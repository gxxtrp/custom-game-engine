#pragma once

#include "engine/core/config.h"
#include "engine/scene/scene.h"
#include <string>
#include <string_view>

namespace engine::scene {

class MapSerializer {
public:
    static bool serialize_to_toml(Scene& scene, std::string& out_toml);
    static bool deserialize_from_toml(std::string_view toml_content, Scene& out_scene);

    static bool save_map(Scene& scene, std::string_view virtual_path);
    static bool load_map(std::string_view virtual_path, Scene& out_scene);
};

} // namespace engine::scene
