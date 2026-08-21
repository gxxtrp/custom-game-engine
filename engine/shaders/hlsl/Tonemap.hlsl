// Modern HLSL 2021 Post-Processing & Tonemapping Shader for Vulkan 1.3 SPIR-V
struct VSOutput {
    float4 clip_pos             : SV_Position;
    [[vk::location(0)]] float2 uv : TEXCOORD0;
};

struct TonemapPushConstants {
    float exposure;
    int tone_mapper; // 0 = ACES, 1 = AgX, 2 = Reinhard, 3 = Linear
    float vignette_intensity;
    float pad;
};

[[vk::push_constant]]
ConstantBuffer<TonemapPushConstants> pc : register(b0);

// Fullscreen triangle generated from vertex ID (0, 1, 2)
VSOutput VSMain(uint vertex_id : SV_VertexID) {
    VSOutput output;
    float2 uv = float2((vertex_id << 1) & 2, vertex_id & 2);
    output.uv = uv;
    output.clip_pos = float4(uv * 2.0f - 1.0f, 0.0f, 1.0f);
    return output;
}

// Stephen Hill's ACES RRT + ODT Fit for sRGB
float3 aces_fitted(float3 color) {
    float3x3 m1 = float3x3(
        0.59719f, 0.35458f, 0.04823f,
        0.07600f, 0.90834f, 0.01566f,
        0.02840f, 0.13383f, 0.83777f
    );
    float3 v = mul(m1, color);

    float3 a = v * (v + 0.0245786f) - 0.000090537f;
    float3 b = v * (0.983729f * v + 0.4329510f) + 0.238081f;
    float3 r = a / b;

    float3x3 m2 = float3x3(
        1.60475f, -0.53108f, -0.07367f,
        -0.10208f, 1.10813f, -0.00605f,
        -0.00327f, -0.07276f, 1.07602f
    );
    return clamp(mul(m2, r), 0.0f, 1.0f);
}

// AgX Tonemapping (Troy Sobotka)
float3 agx_fitted(float3 color) {
    float3 log_col = clamp(log2(color * 0.18f + 1e-5f) * 0.0625f + 0.5f, 0.0f, 1.0f);
    float3 s = log_col * log_col * (3.0f - 2.0f * log_col);
    return clamp(pow(s, float3(2.2f, 2.2f, 2.2f)), 0.0f, 1.0f);
}

// Reinhard Extended
float3 reinhard(float3 color) {
    float l_white_sq = 16.0f;
    return (color * (1.0f + color / l_white_sq)) / (1.0f + color);
}

[[vk::location(0)]]
float4 PSMain(VSOutput input) : SV_Target {
    float3 color = float3(0.08f, 0.09f, 0.11f) * max(pc.exposure, 0.001f);

    float3 mapped = color;
    if (pc.tone_mapper == 0) {
        mapped = aces_fitted(color);
    } else if (pc.tone_mapper == 1) {
        mapped = agx_fitted(color);
    } else if (pc.tone_mapper == 2) {
        mapped = reinhard(color);
    } else {
        mapped = clamp(color, 0.0f, 1.0f);
    }

    if (pc.vignette_intensity > 0.0f) {
        float2 uv = input.uv * (1.0f - input.uv.yx);
        float vig = uv.x * uv.y * 15.0f;
        vig = clamp(pow(vig, pc.vignette_intensity * 0.5f), 0.0f, 1.0f);
        mapped *= vig;
    }

    float dither = frac(sin(dot(input.uv.xy, float2(12.9898f, 78.233f))) * 43758.5453f);
    mapped += (dither - 0.5f) / 255.0f;

    return float4(clamp(mapped, 0.0f, 1.0f), 1.0f);
}
