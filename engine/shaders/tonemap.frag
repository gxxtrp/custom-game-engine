#version 450

layout(location = 0) in vec2 in_uv;
layout(location = 0) out vec4 out_color;

layout(push_constant) uniform TonemapPushConstants {
    float exposure;
    int tone_mapper; // 0 = ACES, 1 = AgX, 2 = Reinhard, 3 = Linear
    float vignette_intensity;
    float pad;
} pc;

// Stephen Hill's ACES RRT + ODT Fit for sRGB
vec3 aces_fitted(vec3 color) {
    vec3 v = mat3(
        0.59719, 0.35458, 0.04823,
        0.07600, 0.90834, 0.01566,
        0.02840, 0.13383, 0.83777
    ) * color;

    vec3 a = v * (v + 0.0245786) - 0.000090537;
    vec3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
    vec3 r = a / b;

    return clamp(mat3(
        1.60475, -0.53108, -0.07367,
        -0.10208, 1.10813, -0.00605,
        -0.00327, -0.07276, 1.07602
    ) * r, 0.0, 1.0);
}

// AgX Tonemapping (Troy Sobotka)
vec3 agx_fitted(vec3 color) {
    vec3 log_col = clamp(log2(color * 0.18 + 1e-5) * 0.0625 + 0.5, 0.0, 1.0);
    vec3 s = log_col * log_col * (3.0 - 2.0 * log_col);
    return clamp(pow(s, vec3(2.2)), 0.0, 1.0);
}

// Reinhard Extended
vec3 reinhard(vec3 color) {
    float l_white_sq = 16.0;
    return (color * (1.0 + color / l_white_sq)) / (1.0 + color);
}

void main() {
    vec3 color = vec3(0.08, 0.09, 0.11) * max(pc.exposure, 0.001);

    vec3 mapped = color;
    if (pc.tone_mapper == 0) {
        mapped = aces_fitted(color);
    } else if (pc.tone_mapper == 1) {
        mapped = agx_fitted(color);
    } else if (pc.tone_mapper == 2) {
        mapped = reinhard(color);
    } else {
        mapped = clamp(color, 0.0, 1.0);
    }

    if (pc.vignette_intensity > 0.0) {
        vec2 uv = in_uv * (1.0 - in_uv.yx);
        float vig = uv.x * uv.y * 15.0;
        vig = clamp(pow(vig, pc.vignette_intensity * 0.5), 0.0, 1.0);
        mapped *= vig;
    }

    float dither = fract(sin(dot(in_uv.xy, vec2(12.9898, 78.233))) * 43758.5453);
    mapped += (dither - 0.5) / 255.0;

    out_color = vec4(clamp(mapped, 0.0, 1.0), 1.0);
}
