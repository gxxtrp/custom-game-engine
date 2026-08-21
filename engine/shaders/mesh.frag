#version 450

layout(location = 0) in vec3 in_world_pos;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;
layout(location = 3) in vec4 in_color;

layout(location = 0) out vec4 out_color;

layout(push_constant) uniform MeshPushConstants {
    mat4 view_proj;
    mat4 model;
    vec4 base_color;
    vec4 light_dir_intensity;  // xyz: light_dir, w: light_intensity
    vec4 light_color_ambient;   // xyz: light_color, w: ambient_intensity
    vec4 camera_pos_roughness;  // xyz: camera_pos, w: roughness
    vec4 material_params;       // x: metallic, y: emissive_strength, zw: unused
} pc;

const float PI = 3.14159265359;

// 1. Trowbridge-Reitz GGX Normal Distribution Function
float distribution_ggx(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / max(denom, 0.0000001);
}

// 2. Schlick-GGX Geometry Function
float geometry_schlick_ggx(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / max(denom, 0.0000001);
}

// 3. Smith's Method for Geometry Shadowing
float geometry_smith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = geometry_schlick_ggx(NdotV, roughness);
    float ggx1 = geometry_schlick_ggx(NdotL, roughness);

    return ggx1 * ggx2;
}

// 4. Fresnel-Schlick Approximation
vec3 fresnel_schlick(float cos_theta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
}

void main() {
    vec3 N = normalize(in_normal);
    vec3 V = normalize(pc.camera_pos_roughness.xyz - in_world_pos);
    vec3 L = normalize(pc.light_dir_intensity.xyz);
    vec3 H = normalize(V + L);

    vec3 albedo = in_color.rgb;
    float roughness = clamp(pc.camera_pos_roughness.w, 0.04, 1.0);
    float metallic = clamp(pc.material_params.x, 0.0, 1.0);
    float emissive_strength = max(pc.material_params.y, 0.0);

    float light_intensity = pc.light_dir_intensity.w;
    vec3 light_color = pc.light_color_ambient.xyz;
    float ambient_factor = max(pc.light_color_ambient.w, 0.05);

    // Dielectric base reflectivity = 0.04, metallic uses albedo
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    // Direct Lighting (Cook-Torrance BRDF)
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);

    vec3 Lo = vec3(0.0);
    if (NdotL > 0.0) {
        float NDF = distribution_ggx(N, H, roughness);
        float G = geometry_smith(N, V, L, roughness);
        vec3 F = fresnel_schlick(max(dot(H, V), 0.0), F0);

        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * NdotV * NdotL + 0.0001;
        vec3 specular = numerator / denominator;

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;

        Lo = (kD * albedo / PI + specular) * light_color * (light_intensity * NdotL);
    }

    // Ambient Lighting
    vec3 ambient = vec3(ambient_factor) * albedo * (1.0 - metallic * 0.5);

    // Emissive
    vec3 emissive = albedo * emissive_strength;

    vec3 final_color = ambient + Lo + emissive;
    out_color = vec4(final_color, in_color.a);
}
