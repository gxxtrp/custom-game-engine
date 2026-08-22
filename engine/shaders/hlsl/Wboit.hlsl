// Weighted Blended OIT: transparent PBR shading with McGuire & Bavoil depth
// weights. Outputs premultiplied accumulation (SV_Target0) and revealage
// (SV_Target1, scalar). Blend states are configured per-attachment in the
// accumulation pipeline. HLSL 2021 / Vulkan 1.3.
#include "frame_uniforms.hlsli"

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

float distribution_ggx(float3 N, float3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0f);
    float NdotH2 = NdotH * NdotH;
    float denom = PI * (NdotH2 * (a2 - 1.0f) + 1.0f);
    denom = denom * denom;
    return a2 / max(denom, 0.0000001f);
}

float geometry_schlick_ggx(float NdotV, float roughness) {
    float r = roughness + 1.0f;
    float k = (r * r) / 8.0f;
    return NdotV / max(NdotV * (1.0f - k) + k, 0.0000001f);
}

float geometry_smith(float3 N, float3 V, float3 L, float roughness) {
    return geometry_schlick_ggx(max(dot(N, V), 0.0f), roughness) *
           geometry_schlick_ggx(max(dot(N, L), 0.0f), roughness);
}

float3 fresnel_schlick(float cos_theta, float3 F0) {
    return F0 + (1.0f - F0) * pow(clamp(1.0f - cos_theta, 0.0f, 1.0f), 5.0f);
}

// McGuire & Bavoil 2013 WBOIT depth weight
float compute_depth_weight(float z, float alpha) {
    float a = min(1.0f, alpha) * 8.0f + 0.01f;
    float b = -clamp(z, 0.0f, 1.0f) * 0.95f + 1.0f;
    return clamp(a * a * a * 1e2f * b * b * b, 1e-2f, 3e3f);
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

struct PSOutput {
    [[vk::location(0)]] float4 accumulation : SV_Target0;
    [[vk::location(1)]] float revealage     : SV_Target1;
};

PSOutput PSMain(VSOutput input) {
    PSOutput output;

    float3 N = normalize(input.normal);
    float3 V = normalize(frame.camera_pos.xyz - input.world_pos);
    float3 L = normalize(frame.dir_light_dir_intensity.xyz);
    float3 H = normalize(V + L);

    float3 albedo = input.color.rgb;
    float alpha = input.color.a;
    float roughness = clamp(pc.material_params.z, 0.04f, 1.0f);
    float metallic = clamp(pc.material_params.x, 0.0f, 1.0f);
    float emissive_strength = max(pc.material_params.y, 0.0f);

    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);

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
        float3 kD = (float3(1.0f, 1.0f, 1.0f) - kS) * (1.0f - metallic);
        Lo = (kD * albedo / PI + specular) * frame.dir_light_color_ambient.xyz * (frame.dir_light_dir_intensity.w * NdotL);
    }

    float ambient_factor = max(frame.dir_light_color_ambient.w, 0.05f);
    float3 ambient = float3(ambient_factor, ambient_factor, ambient_factor) * albedo * (1.0f - metallic * 0.5f);
    float3 emissive = albedo * emissive_strength;
    float3 color = ambient + Lo + emissive;

    float z = input.clip_pos.z / input.clip_pos.w;
    float weight = compute_depth_weight(z, alpha);

    output.accumulation = float4(color.rgb * alpha * weight, alpha * weight);
    output.revealage = alpha * weight;
    return output;
}
