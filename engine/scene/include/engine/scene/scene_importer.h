#pragma once

#include "engine/core/config.h"
#include "engine/scene/scene.h"
#include "engine/importer/importer.h"

namespace engine::scene {

class SceneImporter {
public:
    static bool instantiate_imported_scene(const importer::ImportedScene& imported_scene, Scene& out_scene);
};

} // namespace engine::scene
