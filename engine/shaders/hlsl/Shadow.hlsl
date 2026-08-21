// Modern HLSL 2021 Cascaded Shadow Map Depth Shader for Vulkan 1.3 SPIR-V
struct VSInput {
    [[vk::location(0)]] float3 position : POSITION;
    [[vk::location(1)]] float3 normal   : NORMAL;
    [[vk::location(2)]] float2 uv       : TEXCOORD0;
    [[vk::location(3)]] float4 color    : COLOR0;
};

struct VSOutput {
    float4 clip_pos : SV_Position;
};

struct ShadowPushConstants {
    float4x4 light_view_proj;
    float4x4 model;
};

[[vk::push_constant]]
ConstantBuffer<ShadowPushConstants> pc : register(b0);

VSOutput VSMain(VSInput input) {
    VSOutput output;
    float4 world_pos = mul(pc.model, float4(input.position, 1.0f));
    output.clip_pos = mul(pc.light_view_proj, world_pos);
    return output;
}

void PSMain(VSOutput input) {
    // Depth-only pass: hardware writes depth automatically
}
