// Final composite: HDR scene + bloom + volumetric fog -> color grading ->
// ACES/AgX/Reinhard tonemap -> vignette + film grain -> SDR final target.
// HLSL 2021 / Vulkan 1.3.

[[vk::binding(0, 0)]] Texture2D<float4> scene_tex : register(t0);
[[vk::binding(0, 0)]] SamplerState scene_sampler : register(s0);
[[vk::binding(1, 0)]] Texture2D<float4> bloom_tex : register(t1);
[[vk::binding(1, 0)]] SamplerState bloom_sampler : register(s1);
[[vk::binding(2, 0)]] Texture2D<float4> fog_tex : register(t2);
[[vk::binding(2, 0)]] SamplerState fog_sampler : register(s2);

struct CompositePushConstants {
    float exposure;
    int tone_mapper;          // 0=ACES, 1=AgX, 2=Neutral, 3=Reinhard, 4=Linear
    float bloom_intensity;
    float vignette_intensity;
    float saturation;
    float contrast;
    float grain;
    float fog_enabled;
};

[[vk::push_constant]]
ConstantBuffer<CompositePushConstants> pc : register(b0);

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

float3 reinhard_extended(float3 color) {
    float l_white_sq = 16.0f;
    return (color * (1.0f + color / l_white_sq)) / (1.0f + color);
}

float3 apply_tonemap(float3 color) {
    switch (pc.tone_mapper) {
        case 0: return aces_fitted(color);
        case 1: return agx_fitted(color);
        case 2: return clamp(color, 0.0f, 1.0f); // Neutral (already graded)
        case 3: return reinhard_extended(color);
        default: return clamp(color, 0.0f, 1.0f);
    }
}

[[vk::location(0)]]
float4 PSMain(VSOutput input) : SV_Target {
    float3 color = scene_tex.SampleLevel(scene_sampler, input.uv, 0).rgb;

    // Bloom (pre-multiplied by intensity in the prefilter)
    color += bloom_tex.SampleLevel(bloom_sampler, input.uv, 0).rgb * pc.bloom_intensity;

    // Volumetric fog LUT: rgb = inscatter, a = transmittance
    if (pc.fog_enabled > 0.5f) {
        float4 fog = fog_tex.SampleLevel(fog_sampler, input.uv, 0);
        color = color * fog.a + fog.rgb;
    }

    // Exposure
    color *= max(pc.exposure, 0.001f);

    // Color grading: saturation + contrast around luma
    float luma = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
    color = lerp(float3(luma, luma, luma), color, pc.saturation);
    color = (color - 0.5f) * pc.contrast + 0.5f;

    // Tonemapping
    color = apply_tonemap(color);

    // Vignette
    if (pc.vignette_intensity > 0.0f) {
        float2 uv = input.uv * (1.0f - input.uv.yx);
        float vig = uv.x * uv.y * 15.0f;
        vig = clamp(pow(vig, pc.vignette_intensity * 0.5f), 0.0f, 1.0f);
        color *= vig;
    }

    // Film grain + dither
    float dither = frac(sin(dot(input.uv.xy, float2(12.9898f, 78.233f))) * 43758.5453f);
    color += (dither - 0.5f) * max(pc.grain, 0.0f);

    return float4(clamp(color, 0.0f, 1.0f), 1.0f);
}
