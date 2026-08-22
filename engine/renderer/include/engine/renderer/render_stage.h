#pragma once

#include "engine/core/config.h"
#include <cstdint>
#include <string_view>

namespace engine::renderer {

// Standard frame execution timeline. Features are grouped by stage; the host
// (SceneRenderer) compiles stages in declaration order, and features within a
// stage execute in registration order.
enum class RenderStage : uint8_t {
    DepthPrePass = 0,   // Early-Z, Motion Vectors, Visibility Buffer, Object Picking
    Opaque,             // G-Buffer or forward opaque rasterization
    Lighting,           // Direct lighting, Shadow evaluation, ReSTIR GI
    Translucent,        // OIT, forward transparent blending
    PostProcessStack,   // Auto Exposure, Bloom, TAA, Color Grading, Tonemap
    OverlayDebug        // Physics wireframes, Navmesh, Editor Gizmos, ImGui
};

inline constexpr uint8_t RENDER_STAGE_COUNT = 6;

inline constexpr std::string_view get_render_stage_name(RenderStage stage) {
    switch (stage) {
        case RenderStage::DepthPrePass:     return "DepthPrePass";
        case RenderStage::Opaque:           return "Opaque";
        case RenderStage::Lighting:         return "Lighting";
        case RenderStage::Translucent:      return "Translucent";
        case RenderStage::PostProcessStack: return "PostProcessStack";
        case RenderStage::OverlayDebug:     return "OverlayDebug";
    }
    return "Unknown";
}

} // namespace engine::renderer
