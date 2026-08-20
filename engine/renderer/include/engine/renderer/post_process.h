#pragma once

#include "engine/core/config.h"
#include "engine/core/math.h"
#include "engine/rhi/rhi_buffer.h"
#include <cstdint>

namespace engine::renderer {

enum class ToneMapper : uint8_t {
    ACES = 0,
    AgX,
    Neutral,
    Reinhard,
    Linear
};

struct alignas(16) PostProcessUniforms {
    float exposure{1.0f};
    float contrast{1.0f};
    float saturation{1.0f};
    uint32_t tone_mapper{0}; // 0 = ACES, 1 = AgX, 2 = Neutral, 3 = Reinhard, 4 = Linear

    core::Vec3 color_filter{1.0f, 1.0f, 1.0f};
    float bloom_intensity{0.05f};

    float vignette_intensity{0.25f};
    float vignette_roundness{0.5f};
    float chromatic_aberration{0.002f};
    float film_grain_intensity{0.02f};
};

class PostProcessSystem {
public:
    PostProcessSystem() = default;
    ~PostProcessSystem();

    bool init();
    void destroy();

    void set_exposure(float exposure) { m_uniforms.exposure = exposure; m_dirty = true; }
    void set_tone_mapper(ToneMapper mapper) { m_uniforms.tone_mapper = static_cast<uint32_t>(mapper); m_dirty = true; }
    void set_contrast(float contrast) { m_uniforms.contrast = contrast; m_dirty = true; }
    void set_saturation(float sat) { m_uniforms.saturation = sat; m_dirty = true; }
    void set_bloom_intensity(float bloom) { m_uniforms.bloom_intensity = bloom; m_dirty = true; }

    void update_gpu_buffer();

    // CPU-side reference color transformations
    static core::Vec3 aces_fitted(const core::Vec3& color);
    static core::Vec3 agx_tonemap(const core::Vec3& color);
    static core::Vec3 rgb_to_ycocg(const core::Vec3& rgb);
    static core::Vec3 ycocg_to_rgb(const core::Vec3& ycocg);

    const rhi::RhiBuffer& get_uniform_buffer() const { return m_uniform_buffer; }
    const PostProcessUniforms& get_uniforms() const { return m_uniforms; }

private:
    PostProcessUniforms m_uniforms{};
    rhi::RhiBuffer m_uniform_buffer;
    bool m_dirty{true};
};

} // namespace engine::renderer
