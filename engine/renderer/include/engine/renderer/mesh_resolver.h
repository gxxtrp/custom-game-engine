#pragma once

#include "engine/core/config.h"
#include "engine/assets/uuid.h"
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace engine::renderer {

class Mesh;

// Owns the primitive-mesh table and the UUID mesh registry, and resolves
// MeshRendererComponent references to GPU-ready meshes. Owned by SceneRenderer;
// shared with features via RenderFeatureServices (read-only resolution).
class MeshResolver {
public:
    MeshResolver() = default;

    void register_mesh(const assets::UUID& uuid, std::shared_ptr<Mesh> mesh);
    std::shared_ptr<Mesh> get_mesh(const assets::UUID& uuid) const;

    void register_primitive(std::string name, std::shared_ptr<Mesh> mesh);
    std::shared_ptr<Mesh> get_primitive(std::string_view name) const;

    // Resolves a renderer component reference: valid UUID -> registry lookup,
    // otherwise -> primitive by entity name.
    std::shared_ptr<Mesh> resolve(const assets::UUID& uuid, std::string_view entity_name) const;

    void clear();

private:
    std::unordered_map<std::string, std::shared_ptr<Mesh>> m_primitives;
    std::unordered_map<assets::UUID, std::shared_ptr<Mesh>> m_registry;
};

} // namespace engine::renderer
