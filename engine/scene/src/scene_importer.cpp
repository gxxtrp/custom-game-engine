#include "engine/scene/scene_importer.h"
#include "engine/core/log.h"
#include <vector>

namespace engine::scene {

bool SceneImporter::instantiate_imported_scene(const importer::ImportedScene& imported_scene, Scene& out_scene) {
    if (imported_scene.nodes.empty()) return true;

    out_scene.set_name(imported_scene.name);

    std::vector<Entity> created_entities(imported_scene.nodes.size());

    // 1. Create all entities
    for (size_t i = 0; i < imported_scene.nodes.size(); ++i) {
        const auto& node = imported_scene.nodes[i];
        Entity entity = out_scene.create_entity(node.name);

        // Transform
        auto* trans = entity.get_mut<TransformComponent>();
        trans->position = node.transform.position;
        trans->rotation = node.transform.rotation;
        trans->scale = node.transform.scale;
        trans->is_dirty = true;

        // MeshRenderer
        if (node.mesh_index >= 0 && node.mesh_index < static_cast<int32_t>(imported_scene.meshes.size())) {
            const auto& mesh = imported_scene.meshes[node.mesh_index];
            for (size_t p = 0; p < mesh.primitives.size(); ++p) {
                MeshRendererComponent mr{};
                mr.submesh_index = static_cast<uint32_t>(p);
                mr.cast_shadows = true;
                mr.receive_shadows = true;
                mr.is_visible = true;
                entity.set(mr);
            }
        }

        // Light
        if (node.light_index >= 0 && node.light_index < static_cast<int32_t>(imported_scene.lights.size())) {
            const auto& light = imported_scene.lights[node.light_index];
            if (light.type == importer::LightType::Directional) {
                DirectionalLightComponent dl{};
                dl.color = light.color;
                dl.intensity = light.intensity;
                dl.cast_shadows = true;
                entity.set(dl);
            } else if (light.type == importer::LightType::Point) {
                PointLightComponent pl{};
                pl.color = light.color;
                pl.intensity = light.intensity;
                pl.radius = light.range;
                pl.cast_shadows = false;
                entity.set(pl);
            } else if (light.type == importer::LightType::Spot) {
                SpotLightComponent sl{};
                sl.color = light.color;
                sl.intensity = light.intensity;
                sl.range = light.range;
                sl.inner_cone_angle_deg = light.inner_cone_angle_deg;
                sl.outer_cone_angle_deg = light.outer_cone_angle_deg;
                sl.cast_shadows = false;
                entity.set(sl);
            }
        }

        // Camera
        if (node.camera_index >= 0 && node.camera_index < static_cast<int32_t>(imported_scene.cameras.size())) {
            const auto& cam = imported_scene.cameras[node.camera_index];
            CameraComponent c{};
            c.fov_deg = cam.fov_y_deg;
            c.near_z = cam.near_z;
            c.far_z = cam.far_z;
            c.is_orthographic = !cam.is_perspective;
            c.is_primary = true;
            entity.set(c);
        }

        created_entities[i] = entity;
    }

    // 2. Establish Hierarchy
    for (size_t i = 0; i < imported_scene.nodes.size(); ++i) {
        const auto& node = imported_scene.nodes[i];
        Entity parent = created_entities[i];
        for (uint32_t child_idx : node.children) {
            if (child_idx < created_entities.size()) {
                created_entities[child_idx].set_parent(parent);
            }
        }
    }

    out_scene.update(0.0f);
    LOG_INFO("Scene", "Instantiated imported glTF scene '{}' into ECS with {} entities",
             imported_scene.name, created_entities.size());
    return true;
}

} // namespace engine::scene
