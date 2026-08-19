#version 450

layout(location = 0) in vec3 in_world_pos;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;
layout(location = 3) in vec4 in_color;

layout(location = 0) out vec4 out_color;

layout(push_constant) uniform PushConstants {
    mat4 view_proj;
    mat4 model;
    vec4 base_color;
    vec4 light_dir_intensity;  // xyz: light_dir, w: light_intensity
    vec4 light_color_ambient;   // xyz: light_color, w: ambient_intensity
    vec4 camera_pos_roughness;  // xyz: camera_pos, w: roughness
} pc;

void main() {
    vec3 N = normalize(in_normal);
    vec3 L = normalize(pc.light_dir_intensity.xyz);
    vec3 V = normalize(pc.camera_pos_roughness.xyz - in_world_pos);
    vec3 H = normalize(L + V);

    float light_intensity = pc.light_dir_intensity.w;
    vec3 light_color = pc.light_color_ambient.xyz;
    float ambient_factor = pc.light_color_ambient.w;
    float roughness = clamp(pc.camera_pos_roughness.w, 0.04, 1.0);

    // Diffuse (Half-Lambert / Lambertian)
    float NdotL = max(dot(N, L), 0.0);
    vec3 diffuse = in_color.rgb * light_color * (NdotL * light_intensity);

    // Specular (Blinn-Phong)
    float shininess = (2.0 / (roughness * roughness)) - 2.0;
    shininess = clamp(shininess, 1.0, 256.0);
    float NdotH = max(dot(N, H), 0.0);
    float spec = pow(NdotH, shininess);
    vec3 specular = light_color * (spec * light_intensity * 0.4);

    // Ambient
    vec3 ambient = in_color.rgb * (ambient_factor > 0.0 ? ambient_factor : 0.15);

    vec3 final_color = ambient + diffuse + specular;
    out_color = vec4(final_color, in_color.a);
}
