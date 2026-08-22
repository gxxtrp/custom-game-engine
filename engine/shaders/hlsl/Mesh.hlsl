// Forward-shaded PBR opaque pass: Cook-Torrance GGX + 4-split cascaded shadow
// mapping (PCF). Frame data comes from the shared UBO (set 0, b0); the CSM depth
// array is a combined comparison sampler at (set 0, b1). HLSL 2021 / Vulkan 1.3.
#include "frame_uniforms.hlsli"

[[vk::binding(1, 0)]] Texture2DArray<float> shadow_map : register(t1);
[[vk::binding(1, 0)]] SamplerComparisonState shadow_sampler : register(s1);

struct VSInput {
    [[vk::location(0)]] float3 position : POSITION;
    [[vk::location(1)]] float3 normal   : NORMAL;
    [[vk::location(2)]] float2 uv       : TEXCOORD0;
    [[vk::location(3)]] float4 color    : COLOR0;
};

struct VSOutput {
    float4 clip_pos                      : SV_Position;
    [[vk::location(0)]] float3 world_pos : POSITION;
    [[vk::location(1)]] float3 normal    : NORMAL;
    [[vk::location(2)]] float2 uv        : TEXCOORD0;
    [[vk::location(3)]] float4 color     : COLOR0;
};

struct MeshPushConstants {
    float4x4 model;
    float4 base_color;
    float4 material_params; // x=metallic, y=emissive_strength, z=roughness
};

[[vk::push_constant]]
ConstantBuffer<MeshPushConstants> pc : register(b0);

static const float PI = 3.14159265359f;

// 1. Trowbridge-Reitz GGX Normal Distribution Function
float distribution_ggx(float3 N, float3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0f);
    float NdotH2 = NdotH * NdotH;
    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0f) + 1.0f);
    denom = PI * denom * denom;
    return num / max(denom, 0.0000001f);
}

// 2. Schlick-GGX Geometry Function
float geometry_schlick_ggx(float NdotV, float roughness) {
    float r = (roughness + 1.0f);
    float k = (r * r) / 8.0f;
    float num = NdotV;
    float denom = NdotV * (1.0f - k) + k;
    return num / max(denom, 0.0000001f);
}

// 3. Smith's Method for Geometry Shadowing
float geometry_smith(float3 N, float3 V, float3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0f);
    float NdotL = max(dot(N, L), 0.0f);
    float ggx2 = geometry_schlick_ggx(NdotV, roughness);
    float ggx1 = geometry_schlick_ggx(NdotL, roughness);
    return ggx1 * ggx2;
}

// 4. Fresnel-Schlick Approximation
float3 fresnel_schlick(float cos_theta, float3 F0) {
    return F0 + (1.0f - F0) * pow(clamp(1.0f - cos_theta, 0.0f, 1.0f), 5.0f);
}

// 4-split CSM with 3x3 PCF
float cascade_shadow_factor(float3 world_pos, float3 N, float3 L) {
    if (frame.shadow_params.y < 0.5f) return 1.0f;

    float view_z = mul(frame.view, float4(world_pos, 1.0f)).z;
    float depth = -view_z;

    uint cascade = 0u;
    if (depth > frame.cascade_splits.x) cascade = 1u;
    if (depth > frame.cascade_splits.y) cascade = 2u;
    if (depth > frame.cascade_splits.z) cascade = 3u;

    float4 shadow_clip = mul(frame.cascade_view_proj[cascade], float4(world_pos, 1.0f));
    float3 shadow_uvw = shadow_clip.xyz / shadow_clip.w;
    shadow_uvw = shadow_uvw * 0.5f + 0.5f;

    if (any(shadow_uvw.xy < 0.0f) || any(shadow_uvw.xy > 1.0f) ||
        shadow_uvw.z < 0.0f || shadow_uvw.z > 1.0f) {
        return 1.0f;
    }

    float bias = frame.shadow_params.x * (1.0f + 4.0f * (1.0f - saturate(dot(N, L))));
    float depth_in_light = shadow_uvw.z - bias;
    float texel = max(frame.shadow_params.z, 0.0001f);

    float shadow = 0.0f;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            float2 uv = shadow_uvw.xy + float2(x, y) * texel;
            shadow += shadow_map.SampleCmpLevelZero(shadow_sampler, float3(uv, cascade), depth_in_light);
        }
    }
    return shadow / 9.0f;
}

VSOutput VSMain(VSInput input) {
    VSOutput output;
    float4 world_pos = mul(pc.model, float4(input.position, 1.0f));
    output.world_pos = world_pos.xyz;

    float3x3 normal_matrix = (float3x3)pc.model;
    output.normal = normalize(mul(normal_matrix, input.normal));

    output.uv = input.uv;
    output.color = pc.base_color * input.color;
    output.clip_pos = mul(frame.view_proj, world_pos);
    return output;
}

[[vk::location(0)]]
float4 PSMain(VSOutput input) : SV_Target {
    float3 N = normalize(input.normal);
    float3 V = normalize(frame.camera_pos.xyz - input.world_pos);
    float3 L = normalize(frame.dir_light_dir_intensity.xyz);
    float3 H = normalize(V + L);

    float3 albedo = input.color.rgb;
    float roughness = clamp(pc.material_params.z, 0.04f, 1.0f);
    float metallic = clamp(pc.material_params.x, 0.0f, 1.0f);
    float emissive_strength = max(pc.material_params.y, 0.0f);

    float light_intensity = frame.dir_light_dir_intensity.w;
    float3 light_color = frame.dir_light_color_ambient.xyz;
    float ambient_factor = max(frame.dir_light_color_ambient.w, 0.05f);

    float3 F0 = float3(0.04f, 0.04f, 0.04f);
    F0 = lerp(F0, albedo, metallic);

    float NdotL = max(dot(N, L), 0.0f);
    float NdotV = max(dot(N, V), 0.0f);

    float3 Lo = float3(0.0f, 0.0f, 0.0f);
    if (NdotL > 0.0f) {
        float NDF = distribution_ggx(N, H, roughness);
        float G = geometry_smith(N, V, L, roughness);
        float3 F = fresnel_schlick(max(dot(H, V), 0.0f), F0);

        float3 numerator = NDF * G * F;
        float denominator = 4.0f * NdotV * NdotL + 0.0001f;
        float3 specular = numerator / denominator;

        float3 kS = F;
        float3 kD = float3(1.0f, 1.0f, 1.0f) - kS;
        kD *= (1.0f - metallic);

        Lo = (kD * albedo / PI + specular) * light_color * (light_intensity * NdotL);
    }

    float3 ambient = float3(ambient_factor, ambient_factor, ambient_factor) * albedo * (1.0f - metallic * 0.5f);
    float3 emissive = albedo * emissive_strength;

    float shadow = cascade_shadow_factor(input.world_pos, N, L);
    float3 final_color = ambient + (Lo * shadow) + emissive;
    return float4(final_color, input.color.a);
}
