// Volumetric fog: raymarches the view ray from the near plane to the scene
// surface, accumulating transmittance and Henyey-Greenstein sun inscatter into a
// half-resolution LUT (rgb=inscatter, a=transmittance).
// HLSL 2021 / Vulkan 1.3.

[[vk::binding(0, 0)]] Texture2D<float> depth_tex : register(t0);
[[vk::binding(0, 0)]] SamplerState depth_sampler : register(s0);

struct FogPushConstants {
    float4x4 inv_view_proj;
    float4 camera_pos;   // xyz=position, w=near plane
    float4 light_dir;    // xyz=direction (toward light)
    float4 fog_params;   // x=scattering, y=absorption, z=anisotropy, w=height falloff
};

[[vk::push_constant]]
ConstantBuffer<FogPushConstants> pc : register(b0);

struct VSOutput {
    float4 clip_pos                  : SV_Position;
    [[vk::location(0)]] float2 uv    : TEXCOORD0;
};

VSOutput VSMain(uint vertex_id : SV_VertexID) {
    VSOutput output;
    float2 uv = float2((vertex_id << 1) & 2, vertex_id & 2);
    output.uv = uv;
    output.clip_pos = float4(uv * 2.0f - 1.0f, 0.0f, 1.0f);
    return output;
}

static const uint MARCH_STEPS = 32;

float henyey_greenstein(float cos_theta, float g) {
    float g2 = g * g;
    float denom = 1.0f + g2 - 2.0f * g * cos_theta;
    return (1.0f - g2) / max(4.0f * 3.14159265359f * denom * sqrt(denom), 1e-5f);
}

[[vk::location(0)]]
float4 PSMain(VSOutput input) : SV_Target {
    float depth = depth_tex.SampleLevel(depth_sampler, input.uv, 0).r;

    // Reconstruct world position (Vulkan depth [0,1]).
    float4 clip = float4(input.uv * 2.0f - 1.0f, depth, 1.0f);
    float4 world_h = mul(pc.inv_view_proj, clip);
    float3 world = world_h.xyz / world_h.w;

    float3 ray_origin = pc.camera_pos.xyz;
    float3 ray_dir = world - ray_origin;
    float surface_dist = length(ray_dir);
    ray_dir /= max(surface_dist, 1e-5f);

    float3 light_dir = normalize(pc.light_dir.xyz);
    float density = pc.fog_params.x + pc.fog_params.y;
    float step_len = min(surface_dist, 200.0f) / float(MARCH_STEPS);

    float transmittance = 1.0f;
    float3 inscatter = float3(0.0f, 0.0f, 0.0f);

    for (uint i = 0; i < MARCH_STEPS; ++i) {
        float t = (float(i) + 0.5f) * step_len;
        float3 pos = ray_origin + ray_dir * t;

        // Exponential height falloff
        float height_factor = exp(-max(pos.y, 0.0f) * pc.fog_params.w);
        float sample_density = density * height_factor;

        float optical_depth = sample_density * step_len;
        float step_t = exp(-optical_depth);

        // Single-scattering sun inscatter (Henyey-Greenstein)
        float3 view_dir = -ray_dir;
        float cos_theta = dot(view_dir, light_dir);
        float phase = henyey_greenstein(cos_theta, pc.fog_params.z);
        float3 step_inscatter = float3(1.0f, 0.98f, 0.92f) * phase * sample_density * step_t;

        inscatter += transmittance * step_inscatter;
        transmittance *= step_t;

        if (transmittance < 0.001f) break;
    }

    return float4(inscatter, transmittance);
}
