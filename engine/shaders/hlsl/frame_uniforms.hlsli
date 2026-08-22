// Shared per-frame uniform block — layout MUST match engine::renderer::FrameUniforms.
// Bound at descriptor set 0, binding 0 (uniform buffer).
struct FrameUniforms {
    float4x4 view_proj;              // offset 0
    float4x4 prev_view_proj;         // offset 64
    float4x4 view;                   // offset 128
    float4 camera_pos;               // offset 192
    float4 dir_light_dir_intensity;  // offset 208
    float4 dir_light_color_ambient;  // offset 224
    float4 cascade_splits;           // offset 240
    float4x4 cascade_view_proj[4];   // offset 256
    float4 shadow_params;            // offset 512: x=bias, y=enabled, z=texel size
};

[[vk::binding(0, 0)]]
ConstantBuffer<FrameUniforms> frame : register(b0);
