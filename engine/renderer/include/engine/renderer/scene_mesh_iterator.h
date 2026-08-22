#pragma once

#include "engine/core/config.h"
#include "engine/core/math.h"
#include "engine/scene/scene.h"
#include "engine/scene/components.h"
#include "engine/renderer/mesh.h"
#include "engine/renderer/mesh_resolver.h"
#include "engine/renderer/render_feature.h"
#include <algorithm>

namespace engine::renderer {

// Compact per-entity draw record produced by scene traversal.
struct MeshDrawRecord {
    std::shared_ptr<Mesh> mesh;
    core::Mat4 world{core::Mat4::identity()};
    core::Vec4 base_color{1.0f, 1.0f, 1.0f, 1.0f};
    float metallic{0.0f};
    float roughness{0.5f};
    float emissive_strength{0.0f};
    uint32_t blend_mode{0}; // 0 = Opaque, 1 = Masked, 2 = Transparent
};

// Iterates visible mesh entities. `include_predicate(mat, renderer_component)`
// filters by blend mode / shadow flags; `cull` enables frustum culling
// (stats counted when provided).
template <typename IncludeFn, typename Fn>
void for_each_mesh_draw(const scene::Scene& scene,
                        MeshResolver& meshes,
                        const core::Frustum& frustum,
                        bool cull,
                        SceneRenderStats* stats,
                        IncludeFn&& include_predicate,
                        Fn&& fn) {
    // flecs query_builder is non-const; iteration here is read-only (same
    // const_cast pattern as Scene::get_entity_count).
    const_cast<flecs::world&>(scene.get_world())
        .query_builder<const scene::TransformComponent, const scene::MeshRendererComponent>()
        .build()
        .each([&](flecs::entity e, const scene::TransformComponent& trans, const scene::MeshRendererComponent& mr) {
            if (!mr.is_visible) return;

            // flecs get() returns const T& on const entities; resolve optional components via has().
            const scene::MaterialComponent* mat =
                e.has<scene::MaterialComponent>() ? &e.get<scene::MaterialComponent>() : nullptr;
            if (!include_predicate(mat, mr)) return;

            // Frustum culling (bounding sphere enclosing unit box scaled by entity scale)
            if (cull) {
                float max_scale = std::max({ std::abs(trans.scale.x), std::abs(trans.scale.y), std::abs(trans.scale.z) });
                float bounding_radius = max_scale * 0.866f;
                if (!frustum.intersects_sphere(trans.position, bounding_radius)) {
                    if (stats) stats->culled_meshes++;
                    return;
                }
            }

            std::string_view entity_name = e.has<scene::TagComponent>()
                ? std::string_view(e.get<scene::TagComponent>().name)
                : std::string_view(e.name().c_str() ? e.name().c_str() : "");
            std::shared_ptr<Mesh> mesh = meshes.resolve(mr.mesh_uuid, entity_name);
            if (!mesh || mesh->get_indices().empty()) return;
            // Lazily upload procedural/imported meshes that arrived after GPU init.
            if (!mesh->is_gpu_uploaded() && !mesh->upload_to_gpu()) return;

            core::Mat4 world = trans.get_local_matrix();
            const scene::WorldTransformComponent* wt =
                e.has<scene::WorldTransformComponent>() ? &e.get<scene::WorldTransformComponent>() : nullptr;
            if (wt) world = wt->matrix;

            MeshDrawRecord rec{};
            rec.mesh = mesh;
            rec.world = world;
            if (mat) {
                rec.base_color = mat->base_color;
                rec.metallic = mat->metallic;
                rec.roughness = mat->roughness;
                rec.emissive_strength = mat->emissive_strength;
                rec.blend_mode = mat->blend_mode;
            }
            fn(rec);
        });
}

} // namespace engine::renderer
