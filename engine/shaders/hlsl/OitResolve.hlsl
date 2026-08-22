// WBOIT resolve: composites weighted accumulation over the opaque HDR color.
// HLSL 2021 / Vulkan 1.3.

[[vk::binding(0, 0)]] Texture2D<float4> opaque_tex : register(t0);
[[vk::binding(0, 0)]] SamplerState opaque_sampler : register(s0);
[[vk::binding(1, 0)]] Texture2D<float4> accum_tex : register(t1);
[[vk::binding(1, 0)]] SamplerState accum_sampler : register(s1);
[[vk::binding(2, 0)]] Texture2D<float> reveal_tex : register(t2);
[[vk::binding(2, 0)]] SamplerState reveal_sampler : register(s2);

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

[[vk::location(0)]]
float4 PSMain(VSOutput input) : SV_Target {
    float3 opaque = opaque_tex.SampleLevel(opaque_sampler, input.uv, 0).rgb;
    float4 accum = accum_tex.SampleLevel(accum_sampler, input.uv, 0);
    float reveal = reveal_tex.SampleLevel(reveal_sampler, input.uv, 0).r;

    float3 transparent = accum.rgb / max(accum.a, 1e-4f);
    float3 color = transparent + opaque * reveal;
    return float4(color, 1.0f);
}
