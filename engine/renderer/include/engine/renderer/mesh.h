#pragma once

#include "engine/core/config.h"
#include "engine/core/math.h"
#include "engine/assets/uuid.h"
#include "engine/rhi/rhi_buffer.h"
#include "engine/importer/importer.h"
#include <vector>
#include <string>
#include <memory>

namespace engine::renderer {

struct MeshVertex {
    core::Vec3 position{0.0f, 0.0f, 0.0f};
    core::Vec3 normal{0.0f, 1.0f, 0.0f};
    core::Vec4 tangent{1.0f, 0.0f, 0.0f, 1.0f};
    core::Vec2 uv{0.0f, 0.0f};
    core::Vec4 color{1.0f, 1.0f, 1.0f, 1.0f};

    static VkVertexInputBindingDescription get_binding_description();
    static std::vector<VkVertexInputAttributeDescription> get_attribute_descriptions();
};

struct Meshlet {
    uint32_t vertex_offset{0};
    uint32_t triangle_offset{0};
    uint32_t vertex_count{0};
    uint32_t triangle_count{0};

    core::Vec3 bounding_center{0.0f, 0.0f, 0.0f};
    float bounding_radius{0.0f};
    core::Vec3 cone_axis{0.0f, 1.0f, 0.0f};
    float cone_cutoff{1.0f};
};

struct Submesh {
    std::string name{"Submesh"};
    uint32_t first_index{0};
    uint32_t index_count{0};
    uint32_t first_vertex{0};
    uint32_t vertex_count{0};
    uint32_t first_meshlet{0};
    uint32_t meshlet_count{0};
    core::AABB bounds;
    uint32_t material_index{0};
};

class Mesh {
public:
    Mesh() = default;
    ~Mesh();

    bool upload_to_gpu();
    void destroy();

    // Procedural primitives
    static std::shared_ptr<Mesh> create_cube(core::Vec3 size = core::Vec3(1.0f, 1.0f, 1.0f));
    static std::shared_ptr<Mesh> create_sphere(float radius = 0.5f, uint32_t segments = 32, uint32_t rings = 16);
    static std::shared_ptr<Mesh> create_plane(float width = 10.0f, float height = 10.0f, uint32_t subdivisions = 10);
    static std::shared_ptr<Mesh> from_imported_mesh(const importer::ImportedMesh& imported_mesh);

    void build_meshlets(uint32_t max_vertices = 64, uint32_t max_triangles = 126);

    const std::vector<MeshVertex>& get_vertices() const { return m_vertices; }
    const std::vector<uint32_t>& get_indices() const { return m_indices; }
    const std::vector<Meshlet>& get_meshlets() const { return m_meshlets; }
    const std::vector<Submesh>& get_submeshes() const { return m_submeshes; }
    const core::AABB& get_bounds() const { return m_bounds; }

    const rhi::RhiBuffer& get_vertex_buffer() const { return m_vertex_buffer; }
    const rhi::RhiBuffer& get_index_buffer() const { return m_index_buffer; }
    const rhi::RhiBuffer& get_meshlet_buffer() const { return m_meshlet_buffer; }

    bool is_gpu_uploaded() const { return m_vertex_buffer.is_valid() && m_index_buffer.is_valid(); }

    assets::UUID uuid{assets::UUID::generate()};
    std::string name{"Mesh"};

private:
    std::vector<MeshVertex> m_vertices;
    std::vector<uint32_t> m_indices;
    std::vector<Meshlet> m_meshlets;
    std::vector<Submesh> m_submeshes;
    core::AABB m_bounds;

    rhi::RhiBuffer m_vertex_buffer;
    rhi::RhiBuffer m_index_buffer;
    rhi::RhiBuffer m_meshlet_buffer;
};

} // namespace engine::renderer
