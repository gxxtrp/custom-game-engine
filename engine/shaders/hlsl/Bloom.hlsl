// Bloom: bright-pass prefilter (soft knee + Karis), 13-tap downsample, and
// 9-tap tent upsample. All passes share one fullscreen vertex shader.
// HLSL 2021 / Vulkan 1.3.

struct BloomPushConstants {
    float4 params; // x=threshold, y=soft knee, z=intensity, w=filter radius
    float2 texel;  // 1/width, 1/height
    float2 pad;
};

[[vk::push_constant]]
ConstantBuffer<BloomPushConstants> pc : register(b0);

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

// ---- Prefilter (bright pass with soft knee + Karis average) ----
[[vk::binding(0, 0)]] Texture2D<float4> prefilter_input : register(t0);
[[vk::binding(0, 0)]] SamplerState prefilter_sampler : register(s0);

[[vk::location(0)]]
float4 BloomPrefilterPS(VSOutput input) : SV_Target {
    float3 color = prefilter_input.SampleLevel(prefilter_sampler, input.uv, 0).rgb;

    float luma = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
    float knee = pc.params.y * max(pc.params.x, 1e-4f);
    float soft = clamp((luma - pc.params.x + knee) / max(2.0f * knee, 1e-4f), 0.0f, 1.0f);
    float brightness = max(luma - pc.params.x, 0.0f);
    float w = clamp((brightness * (1.0f + knee / max(luma, 1e-4f))) / max(luma, 1e-4f), 0.0f, 1.0f);
    w = max(w, soft * soft); // soft-knee blend

    float3 karis = color * w / (1.0f + luma); // Karis average weight
    return float4(karis * pc.params.z, 1.0f);
}

// ---- 13-tap downsample (tent filter) ----
[[vk::binding(0, 0)]] Texture2D<float4> downsample_input : register(t0);
[[vk::binding(0, 0)]] SamplerState downsample_sampler : register(s0);

[[vk::location(0)]]
float4 BloomDownsamplePS(VSOutput input) : SV_Target {
    // A: 1,1 / 1,-1 / -1,1 / -1,-1 ; B: 2,0 / 0,2 / -2,0 / 0,-2 ; C: center
    float2 t = pc.texel;
    float3 a = downsample_input.SampleLevel(downsample_sampler, input.uv + float2(1, 1) * t, 0).rgb
             + downsample_input.SampleLevel(downsample_sampler, input.uv + float2(1, -1) * t, 0).rgb
             + downsample_input.SampleLevel(downsample_sampler, input.uv + float2(-1, 1) * t, 0).rgb
             + downsample_input.SampleLevel(downsample_sampler, input.uv + float2(-1, -1) * t, 0).rgb;
    float3 b = downsample_input.SampleLevel(downsample_sampler, input.uv + float2(2, 0) * t, 0).rgb
             + downsample_input.SampleLevel(downsample_sampler, input.uv + float2(0, 2) * t, 0).rgb
             + downsample_input.SampleLevel(downsample_sampler, input.uv + float2(-2, 0) * t, 0).rgb
             + downsample_input.SampleLevel(downsample_sampler, input.uv + float2(0, -2) * t, 0).rgb;
    float3 c = downsample_input.SampleLevel(downsample_sampler, input.uv, 0).rgb;
    return float4((a * 0.125f + b * 0.03125f + c * 0.5f), 1.0f);
}

// ---- 9-tap tent upsample (accumulates detail from the lower mip) ----
[[vk::binding(0, 0)]] Texture2D<float4> upsample_lower : register(t0);
[[vk::binding(0, 0)]] SamplerState upsample_lower_sampler : register(s0);
[[vk::binding(1, 0)]] Texture2D<float4> upsample_detail : register(t1);
[[vk::binding(1, 0)]] SamplerState upsample_detail_sampler : register(s1);

[[vk::location(0)]]
float4 BloomUpsamplePS(VSOutput input) : SV_Target {
    float2 t = pc.texel;
    float3 result = upsample_lower.SampleLevel(upsample_lower_sampler, input.uv + float2(-1, -1) * t, 0).rgb
                  + upsample_lower.SampleLevel(upsample_lower_sampler, input.uv + float2(0, -1) * t, 0).rgb * 2.0f
                  + upsample_lower.SampleLevel(upsample_lower_sampler, input.uv + float2(1, -1) * t, 0).rgb
                  + upsample_lower.SampleLevel(upsample_lower_sampler, input.uv + float2(-1, 0) * t, 0).rgb * 2.0f
                  + upsample_lower.SampleLevel(upsample_lower_sampler, input.uv, 0).rgb * 4.0f
                  + upsample_lower.SampleLevel(upsample_lower_sampler, input.uv + float2(1, 0) * t, 0).rgb * 2.0f
                  + upsample_lower.SampleLevel(upsample_lower_sampler, input.uv + float2(-1, 1) * t, 0).rgb
                  + upsample_lower.SampleLevel(upsample_lower_sampler, input.uv + float2(0, 1) * t, 0).rgb * 2.0f
                  + upsample_lower.SampleLevel(upsample_lower_sampler, input.uv + float2(1, 1) * t, 0).rgb;
    result /= 16.0f;

    float3 detail = upsample_detail.SampleLevel(upsample_detail_sampler, input.uv, 0).rgb;
    return float4(result + detail, 1.0f);
}
