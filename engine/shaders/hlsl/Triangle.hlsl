// Modern HLSL 2021 Diagnostic Triangle Shader for Vulkan 1.3 SPIR-V
struct VSOutput {
    float4 clip_pos             : SV_Position;
    [[vk::location(0)]] float3 color : COLOR0;
};

static const float2 POSITIONS[3] = {
    float2(0.0f, -0.5f),
    float2(0.5f, 0.5f),
    float2(-0.5f, 0.5f)
};

static const float3 COLORS[3] = {
    float3(1.0f, 0.0f, 0.0f),
    float3(0.0f, 1.0f, 0.0f),
    float3(0.0f, 0.0f, 1.0f)
};

VSOutput VSMain(uint vertex_id : SV_VertexID) {
    VSOutput output;
    output.clip_pos = float4(POSITIONS[vertex_id], 0.0f, 1.0f);
    output.color = COLORS[vertex_id];
    return output;
}

[[vk::location(0)]]
float4 PSMain(VSOutput input) : SV_Target {
    return float4(input.color, 1.0f);
}
