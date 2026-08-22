// Depth prepass: writes scene depth + per-pixel motion vectors (previous-frame
// reprojection) for TAA. HLSL 2021 / Vulkan 1.3.
#include "frame_uniforms.hlsli"

struct VSInput {
    [[vk::location(0)]] float3 position : POSITION;
    [[vk::location(1)]] float3 normal   : NORMAL;
    [[vk::location(2)]] float2 uv       : TEXCOORD0;
    [[vk::location(3)]] float4 color    : COLOR0;
};

struct VSOutput {
    float4 clip_pos                          : SV_Position;
    [[vk::location(0)]] float4 prev_clip_pos : TEXCOORD0;
};

struct MeshPushConstants {
    float4x4 model;
};

[[vk::push_constant]]
ConstantBuffer<MeshPushConstants> pc : register(b0);

VSOutput VSMain(VSInput input) {
    VSOutput output;
    float4 world_pos = mul(pc.model, float4(input.position, 1.0f));
    output.clip_pos = mul(frame.view_proj, world_pos);
    output.prev_clip_pos = mul(frame.prev_view_proj, world_pos);
    return output;
}

[[vk::location(0)]]
float2 PSMain(VSOutput input) : SV_Target {
    float2 cur = input.clip_pos.xy / input.clip_pos.w;
    float2 prev = input.prev_clip_pos.xy / input.prev_clip_pos.w;
    return (cur - prev) * 0.5f;
}
