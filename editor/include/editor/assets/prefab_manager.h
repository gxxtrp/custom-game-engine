#pragma once

#include "engine/scene/scene.h"
#include "engine/core/math.h"
#include <string>
#include <string_view>

namespace editor {

class PrefabManager {
public:
    static bool save_prefab(flecs::entity entity, engine::scene::Scene& scene, std::string_view virtual_path);
    static flecs::entity instantiate_prefab(std::string_view virtual_path, 
                                            engine::scene::Scene& scene, 
                                            const engine::core::Vec3& spawn_position = engine::core::Vec3(0.0f, 0.0f, 0.0f));
    static bool is_prefab_file(std::string_view path);
};

} // namespace editor
