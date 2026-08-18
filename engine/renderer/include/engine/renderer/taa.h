#pragma once

#include "engine/core/config.h"
#include "engine/core/math.h"
#include "engine/rhi/rhi_texture.h"
#include <array>

namespace engine::renderer {

struct TaaParams {
    float feedback_min{0.88f};
    float feedback_max{0.97f};
    float variance_clipping_gamma{1.25f};
    float motion_sharpening{0.2f};
};

class TaaSystem {
public:
    static constexpr uint32_t JITTER_PHASE_COUNT = 16;

    TaaSystem() = default;
    ~TaaSystem();

    bool init(uint32_t width, uint32_t height);
    void resize(uint32_t width, uint32_t height);
    void destroy();

    // Halton (2, 3) low discrepancy sequence
    static float halton(uint32_t index, uint32_t base);
    static core::Vec2 get_jitter(uint32_t frame_index);
    static core::Mat4 apply_jitter(const core::Mat4& proj, const core::Vec2& jitter, uint32_t width, uint32_t height);

    // Reference Variance Clipping in YCoCg space
    static core::Vec3 clip_aabb_ycocg(const core::Vec3& history_ycocg,
                                      const std::array<core::Vec3, 9>& neighborhood_ycocg,
                                      float gamma = 1.25f);

    const rhi::RhiTexture& get_history_texture(uint32_t index) const { return m_history_textures[index % 2]; }
    uint32_t get_current_history_index() const { return m_current_history_idx; }
    void advance_frame() { m_current_history_idx = (m_current_history_idx + 1) % 2; }

private:
    uint32_t m_width{1280};
    uint32_t m_height{720};
    uint32_t m_current_history_idx{0};

    rhi::RhiTexture m_history_textures[2];
};

} // namespace engine::renderer
