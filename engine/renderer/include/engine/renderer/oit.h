#pragma once

#include "engine/core/config.h"
#include "engine/core/math.h"
#include "engine/rhi/rhi_texture.h"

namespace engine::renderer {

class WboitRenderer {
public:
    WboitRenderer() = default;
    ~WboitRenderer();

    bool init(uint32_t width, uint32_t height);
    void resize(uint32_t width, uint32_t height);
    void destroy();

    static float compute_depth_weight(float z, float alpha);

    const rhi::RhiTexture& get_accumulation_texture() const { return m_accumulation_texture; }
    const rhi::RhiTexture& get_revealage_texture() const { return m_revealage_texture; }

private:
    uint32_t m_width{1280};
    uint32_t m_height{720};
    rhi::RhiTexture m_accumulation_texture;
    rhi::RhiTexture m_revealage_texture;
};

} // namespace engine::renderer
