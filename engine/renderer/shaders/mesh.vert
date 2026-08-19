#version 450

// Vertex layout matching engine::renderer::MeshVertex
layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec4 in_tangent;
layout(location = 3) in vec2 in_uv;
layout(location = 4) in vec4 in_color;

layout(location = 0) out vec3 out_world_pos;
layout(location = 1) out vec3 out_normal;
layout(location = 2) out vec2 out_uv;
layout(location = 3) out vec4 out_color;

// Push Constants (192 bytes)
layout(push_constant) uniform PushConstants {
    mat4 view_proj;
    mat4 model;
    vec4 base_color;
    vec4 light_dir_intensity;  // xyz: light_dir, w: light_intensity
    vec4 light_color_ambient;   // xyz: light_color, w: ambient_intensity
    vec4 camera_pos_roughness;  // xyz: camera_pos, w: roughness
} pc;

void main() {
    vec4 world_pos = pc.model * vec4(in_position, 1.0);
    gl_Position = pc.view_proj * world_pos;

    out_world_pos = world_pos.xyz;
    
    // Normal transform (using 3x3 model part)
    mat3 normal_matrix = mat3(pc.model);
    out_normal = normalize(normal_matrix * in_normal);

    out_uv = in_uv;
    out_color = in_color * pc.base_color;
}
