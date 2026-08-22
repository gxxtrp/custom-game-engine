#include "engine/renderer/mesh_resolver.h"
#include "engine/renderer/mesh.h"

namespace engine::renderer {

void MeshResolver::register_mesh(const assets::UUID& uuid, std::shared_ptr<Mesh> mesh) {
    m_registry[uuid] = std::move(mesh);
}

std::shared_ptr<Mesh> MeshResolver::get_mesh(const assets::UUID& uuid) const {
    auto it = m_registry.find(uuid);
    return (it != m_registry.end()) ? it->second : nullptr;
}

void MeshResolver::register_primitive(std::string name, std::shared_ptr<Mesh> mesh) {
    m_primitives[std::move(name)] = std::move(mesh);
}

std::shared_ptr<Mesh> MeshResolver::get_primitive(std::string_view name) const {
    if (name.find("Sphere") != std::string_view::npos || name.find("sphere") != std::string_view::npos) {
        auto it = m_primitives.find("Sphere");
        return (it != m_primitives.end()) ? it->second : nullptr;
    }
    if (name.find("Plane") != std::string_view::npos || name.find("plane") != std::string_view::npos || name.find("Ground") != std::string_view::npos) {
        auto it = m_primitives.find("Plane");
        return (it != m_primitives.end()) ? it->second : nullptr;
    }
    auto it = m_primitives.find("Cube");
    return (it != m_primitives.end()) ? it->second : nullptr;
}

std::shared_ptr<Mesh> MeshResolver::resolve(const assets::UUID& uuid, std::string_view entity_name) const {
    if (uuid.is_valid()) {
        if (auto mesh = get_mesh(uuid)) return mesh;
    }
    return get_primitive(entity_name);
}

void MeshResolver::clear() {
    m_primitives.clear();
    m_registry.clear();
}

} // namespace engine::renderer
