// TAA resolve: motion-vector reprojection + YCoCg neighborhood clamp. Writes the
// resolved color (SV_Target0) and the new history frame (SV_Target1).
// HLSL 2021 / Vulkan 1.3.

[[vk::binding(0, 0)]] Texture2D<float4> current_tex : register(t0);
[[vk::binding(0, 0)]] SamplerState current_sampler : register(s0);
[[vk::binding(1, 0)]] Texture2D<float4> history_tex : register(t1);
[[vk::binding(1, 0)]] SamplerState history_sampler : register(s1);
[[vk::binding(2, 0)]] Texture2D<float2> velocity_tex : register(t2);
[[vk::binding(2, 0)]] SamplerState velocity_sampler : register(s2);

struct TaaPushConstants {
    float2 jitter; // applied projection jitter (UV)
    float2 texel;  // 1/size
};

[[vk::push_constant]]
ConstantBuffer<TaaPushConstants> pc : register(b0);

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

float3 rgb_to_ycocg(float3 rgb) {
    float co = rgb.r - rgb.b;
    float t = rgb.b + co * 0.5f;
    float cg = rgb.g - t;
    float y = t + cg * 0.5f;
    return float3(y, co, cg);
}

float3 ycocg_to_rgb(float3 ycocg) {
    float t = ycocg.x - ycocg.z * 0.5f;
    float g = ycocg.z + t;
    float b = t - ycocg.y * 0.5f;
    float r = b + ycocg.y;
    return float3(r, g, b);
}

struct PSOutput {
    [[vk::location(0)]] float4 resolved : SV_Target0;
    [[vk::location(1)]] float4 history  : SV_Target1;
};

PSOutput PSMain(VSOutput input) {
    PSOutput output;

    float3 current = current_tex.SampleLevel(current_sampler, input.uv, 0).rgb;
    float2 velocity = velocity_tex.SampleLevel(velocity_sampler, input.uv, 0).xy;
    float2 history_uv = input.uv - velocity;
    float3 history = history_tex.SampleLevel(history_sampler, history_uv, 0).rgb;

    // YCoCg neighborhood clamp of history against current (variance clipping).
    float3 min_c = current;
    float3 max_c = current;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            float3 n = current_tex.SampleLevel(current_sampler, input.uv + float2(x, y) * pc.texel, 0).rgb;
            min_c = min(min_c, n);
            max_c = max(max_c, n);
        }
    }
    float3 hist_ycocg = rgb_to_ycocg(history);
    float3 min_ycocg = rgb_to_ycocg(min_c);
    float3 max_ycocg = rgb_to_ycocg(max_c);
    // Expand the AABB by the clipping gamma.
    float3 center = (min_ycocg + max_ycocg) * 0.5f;
    float3 extent = (max_ycocg - min_ycocg) * 0.5f * 1.25f + 1e-4f;
    hist_ycocg = clamp(hist_ycocg, center - extent, center + extent);
    history = ycocg_to_rgb(hist_ycocg);

    float3 resolved = lerp(history, current, 0.05f);

    output.resolved = float4(resolved, 1.0f);
    output.history = float4(resolved, 1.0f);
    return output;
}
