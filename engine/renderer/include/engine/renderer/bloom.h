#pragma once

#include "engine/core/config.h"
#include "engine/core/math.h"
#include "engine/rhi/rhi_texture.h"
#include <vector>

namespace engine::renderer {

struct BloomParams {
    float threshold{1.0f};
    float soft_knee{0.5f};
    float intensity{0.04f};
    float filter_radius{0.85f};
};

class BloomSystem {
public:
    static constexpr uint32_t MIP_COUNT = 6;

    BloomSystem() = default;
    ~BloomSystem();

    bool init(uint32_t width, uint32_t height);
    void resize(uint32_t width, uint32_t height);
    void destroy();

    static core::Vec3 compute_prefilter(const core::Vec3& color, float threshold, float knee);
    static float karis_average(const core::Vec3& color);

    const rhi::RhiTexture& get_mip_texture(uint32_t index) const { return m_mips[index]; }
    uint32_t get_mip_count() const { return static_cast<uint32_t>(m_mips.size()); }

private:
    uint32_t m_width{1280};
    uint32_t m_height{720};
    std::vector<rhi::RhiTexture> m_mips;
};

} // namespace engine::renderer
