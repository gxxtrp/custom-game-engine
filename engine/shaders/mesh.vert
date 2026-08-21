#version 450

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;
layout(location = 3) in vec4 in_color;

layout(location = 0) out vec3 out_world_pos;
layout(location = 1) out vec3 out_normal;
layout(location = 2) out vec2 out_uv;
layout(location = 3) out vec4 out_color;

layout(push_constant) uniform MeshPushConstants {
    mat4 view_proj;
    mat4 model;
    vec4 base_color;
    vec4 light_dir_intensity;  // xyz: light_dir, w: light_intensity
    vec4 light_color_ambient;   // xyz: light_color, w: ambient_intensity
    vec4 camera_pos_roughness;  // xyz: camera_pos, w: roughness
    vec4 material_params;       // x: metallic, y: emissive_strength, zw: unused
} pc;

void main() {
    vec4 world_pos = pc.model * vec4(in_position, 1.0);
    out_world_pos = world_pos.xyz;

    mat3 normal_matrix = transpose(inverse(mat3(pc.model)));
    out_normal = normalize(normal_matrix * in_normal);

    out_uv = in_uv;
    out_color = pc.base_color * in_color;

    gl_Position = pc.view_proj * world_pos;
}
