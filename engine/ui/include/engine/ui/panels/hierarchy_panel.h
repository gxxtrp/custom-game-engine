#pragma once

#include "engine/core/config.h"
#include "engine/scene/scene.h"

namespace engine::ui {

class HierarchyPanel {
public:
    HierarchyPanel() = default;
    void render(scene::Scene& scene);
};

} // namespace engine::ui
