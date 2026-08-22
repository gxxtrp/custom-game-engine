// Log-luminance histogram compute shader for auto exposure. One thread per pixel;
// atomically accumulates 256 log-luma bins into a storage buffer.
// HLSL 2021 / Vulkan 1.3.

// Sampled (read-only) image: dxc has no syntax for SPIR-V storage image formats,
// so a plain Texture2D::Load avoids the OpTypeImage Format mismatch entirely.
[[vk::binding(0, 0)]] Texture2D<float4> scene_color : register(t0);
[[vk::binding(1, 0)]] RWStructuredBuffer<uint> histogram : register(u1);

[numthreads(16, 16, 1)]
void main(uint3 dtid : SV_DispatchThreadID) {
    uint width, height;
    scene_color.GetDimensions(width, height);
    if (dtid.x >= width || dtid.y >= height) return;

    float3 color = scene_color.Load(int3(dtid.xy, 0)).rgb;
    float luma = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
    float log_luma = log2(max(luma, 1e-4f));

    // Matches AutoExposureSystem CPU reference: [-8, 4] log-luma range.
    float t = (log_luma - (-8.0f)) / (4.0f - (-8.0f));
    uint bin = uint(clamp(t, 0.0f, 1.0f) * 255.0f + 0.5f);

    InterlockedAdd(histogram[bin], 1u);
}
