#include "engine/renderer/mesh.h"
#include "engine/core/log.h"
#include <cmath>
#include <numbers>

namespace engine::renderer {

VkVertexInputBindingDescription MeshVertex::get_binding_description() {
    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(MeshVertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return binding;
}

std::vector<VkVertexInputAttributeDescription> MeshVertex::get_attribute_descriptions() {
    std::vector<VkVertexInputAttributeDescription> attributes(5);

    // Position: vec3
    attributes[0].binding = 0;
    attributes[0].location = 0;
    attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[0].offset = offsetof(MeshVertex, position);

    // Normal: vec3
    attributes[1].binding = 0;
    attributes[1].location = 1;
    attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[1].offset = offsetof(MeshVertex, normal);

    // Tangent: vec4
    attributes[2].binding = 0;
    attributes[2].location = 2;
    attributes[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributes[2].offset = offsetof(MeshVertex, tangent);

    // UV: vec2
    attributes[3].binding = 0;
    attributes[3].location = 3;
    attributes[3].format = VK_FORMAT_R32G32_SFLOAT;
    attributes[3].offset = offsetof(MeshVertex, uv);

    // Color: vec4
    attributes[4].binding = 0;
    attributes[4].location = 4;
    attributes[4].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributes[4].offset = offsetof(MeshVertex, color);

    return attributes;
}

Mesh::~Mesh() {
    destroy();
}

void Mesh::destroy() {
    m_vertex_buffer.destroy();
    m_index_buffer.destroy();
    m_meshlet_buffer.destroy();
}

bool Mesh::upload_to_gpu() {
    if (m_vertices.empty() || m_indices.empty()) return false;

    destroy();

    // 1. Upload Vertex Buffer
    rhi::BufferDesc vb_desc{
        .size = m_vertices.size() * sizeof(MeshVertex),
        .usage = rhi::BufferUsage::Vertex | rhi::BufferUsage::Storage | rhi::BufferUsage::TransferDst | rhi::BufferUsage::ShaderDeviceAddress,
        .memory_usage = rhi::MemoryUsage::CpuToGpu,
        .debug_name = name + "_VB"
    };

    if (!m_vertex_buffer.init(vb_desc) || !m_vertex_buffer.upload_data(m_vertices.data(), vb_desc.size)) {
        LOG_ERROR("Mesh", "Failed to upload vertex buffer for '{}'", name);
        return false;
    }

    // 2. Upload Index Buffer
    rhi::BufferDesc ib_desc{
        .size = m_indices.size() * sizeof(uint32_t),
        .usage = rhi::BufferUsage::Index | rhi::BufferUsage::Storage | rhi::BufferUsage::TransferDst | rhi::BufferUsage::ShaderDeviceAddress,
        .memory_usage = rhi::MemoryUsage::CpuToGpu,
        .debug_name = name + "_IB"
    };

    if (!m_index_buffer.init(ib_desc) || !m_index_buffer.upload_data(m_indices.data(), ib_desc.size)) {
        LOG_ERROR("Mesh", "Failed to upload index buffer for '{}'", name);
        return false;
    }

    // 3. Upload Meshlet Buffer if meshlets exist
    if (!m_meshlets.empty()) {
        rhi::BufferDesc mb_desc{
            .size = m_meshlets.size() * sizeof(Meshlet),
            .usage = rhi::BufferUsage::Storage | rhi::BufferUsage::TransferDst | rhi::BufferUsage::ShaderDeviceAddress,
            .memory_usage = rhi::MemoryUsage::CpuToGpu,
            .debug_name = name + "_Meshlets"
        };
        m_meshlet_buffer.init(mb_desc);
        m_meshlet_buffer.upload_data(m_meshlets.data(), mb_desc.size);
    }

    return true;
}

void Mesh::build_meshlets(uint32_t max_vertices, uint32_t max_triangles) {
    m_meshlets.clear();

    for (auto& submesh : m_submeshes) {
        submesh.first_meshlet = static_cast<uint32_t>(m_meshlets.size());
        uint32_t triangle_count = submesh.index_count / 3;

        for (uint32_t t = 0; t < triangle_count; t += max_triangles) {
            uint32_t cur_tri_count = std::min(max_triangles, triangle_count - t);

            Meshlet meshlet{};
            meshlet.triangle_offset = (submesh.first_index / 3) + t;
            meshlet.triangle_count = cur_tri_count;
            meshlet.vertex_offset = submesh.first_vertex;
            meshlet.vertex_count = submesh.vertex_count;

            // Compute bounding sphere for this cluster
            core::AABB cluster_aabb;
            for (uint32_t tri = 0; tri < cur_tri_count; ++tri) {
                uint32_t i0 = m_indices[submesh.first_index + (t + tri) * 3 + 0];
                uint32_t i1 = m_indices[submesh.first_index + (t + tri) * 3 + 1];
                uint32_t i2 = m_indices[submesh.first_index + (t + tri) * 3 + 2];

                cluster_aabb.expand_by_point(m_vertices[i0].position);
                cluster_aabb.expand_by_point(m_vertices[i1].position);
                cluster_aabb.expand_by_point(m_vertices[i2].position);
            }

            meshlet.bounding_center = cluster_aabb.center();
            meshlet.bounding_radius = cluster_aabb.extents().length();

            m_meshlets.push_back(meshlet);
        }

        submesh.meshlet_count = static_cast<uint32_t>(m_meshlets.size()) - submesh.first_meshlet;
    }

    LOG_INFO("Mesh", "Built {} meshlets for mesh '{}'", m_meshlets.size(), name);
}

std::shared_ptr<Mesh> Mesh::create_cube(core::Vec3 size) {
    auto mesh = std::make_shared<Mesh>();
    mesh->name = "Procedural_Cube";

    float hx = size.x * 0.5f;
    float hy = size.y * 0.5f;
    float hz = size.z * 0.5f;

    struct Face {
        core::Vec3 normal;
        core::Vec4 tangent;
        core::Vec3 v0, v1, v2, v3;
    };

    Face faces[6] = {
        // Front (+Z)
        { {0,0,1}, {1,0,0,1}, {-hx,-hy, hz}, { hx,-hy, hz}, { hx, hy, hz}, {-hx, hy, hz} },
        // Back (-Z)
        { {0,0,-1}, {-1,0,0,1}, { hx,-hy,-hz}, {-hx,-hy,-hz}, {-hx, hy,-hz}, { hx, hy,-hz} },
        // Left (-X)
        { {-1,0,0}, {0,0,1,1}, {-hx,-hy,-hz}, {-hx,-hy, hz}, {-hx, hy, hz}, {-hx, hy,-hz} },
        // Right (+X)
        { {1,0,0}, {0,0,-1,1}, { hx,-hy, hz}, { hx,-hy,-hz}, { hx, hy,-hz}, { hx, hy, hz} },
        // Top (+Y)
        { {0,1,0}, {1,0,0,1}, {-hx, hy, hz}, { hx, hy, hz}, { hx, hy,-hz}, {-hx, hy,-hz} },
        // Bottom (-Y)
        { {0,-1,0}, {-1,0,0,1}, {-hx,-hy,-hz}, { hx,-hy,-hz}, { hx,-hy, hz}, {-hx,-hy, hz} }
    };

    mesh->m_vertices.reserve(24);
    mesh->m_indices.reserve(36);

    for (int f = 0; f < 6; ++f) {
        uint32_t base = static_cast<uint32_t>(mesh->m_vertices.size());
        const auto& face = faces[f];

        mesh->m_vertices.push_back({ face.v0, face.normal, face.tangent, {0, 1}, {1,1,1,1} });
        mesh->m_vertices.push_back({ face.v1, face.normal, face.tangent, {1, 1}, {1,1,1,1} });
        mesh->m_vertices.push_back({ face.v2, face.normal, face.tangent, {1, 0}, {1,1,1,1} });
        mesh->m_vertices.push_back({ face.v3, face.normal, face.tangent, {0, 0}, {1,1,1,1} });

        mesh->m_indices.push_back(base + 0);
        mesh->m_indices.push_back(base + 1);
        mesh->m_indices.push_back(base + 2);
        mesh->m_indices.push_back(base + 2);
        mesh->m_indices.push_back(base + 3);
        mesh->m_indices.push_back(base + 0);
    }

    mesh->m_bounds = core::AABB(core::Vec3(-hx, -hy, -hz), core::Vec3(hx, hy, hz));

    Submesh submesh{};
    submesh.name = "Cube_Submesh";
    submesh.first_index = 0;
    submesh.index_count = static_cast<uint32_t>(mesh->m_indices.size());
    submesh.first_vertex = 0;
    submesh.vertex_count = static_cast<uint32_t>(mesh->m_vertices.size());
    submesh.bounds = mesh->m_bounds;
    mesh->m_submeshes.push_back(submesh);

    mesh->build_meshlets();
    mesh->upload_to_gpu();
    return mesh;
}

std::shared_ptr<Mesh> Mesh::create_sphere(float radius, uint32_t segments, uint32_t rings) {
    auto mesh = std::make_shared<Mesh>();
    mesh->name = "Procedural_Sphere";

    mesh->m_vertices.reserve((rings + 1) * (segments + 1));
    mesh->m_indices.reserve(rings * segments * 6);

    for (uint32_t r = 0; r <= rings; ++r) {
        float phi = static_cast<float>(r) * std::numbers::pi_v<float> / static_cast<float>(rings);
        float y = std::cos(phi);
        float sin_phi = std::sin(phi);

        for (uint32_t s = 0; s <= segments; ++s) {
            float theta = static_cast<float>(s) * 2.0f * std::numbers::pi_v<float> / static_cast<float>(segments);
            float x = sin_phi * std::cos(theta);
            float z = sin_phi * std::sin(theta);

            core::Vec3 norm(x, y, z);
            core::Vec3 pos = norm * radius;
            core::Vec2 uv(static_cast<float>(s) / segments, static_cast<float>(r) / rings);
            core::Vec4 tangent(-std::sin(theta), 0.0f, std::cos(theta), 1.0f);

            mesh->m_vertices.push_back({ pos, norm, tangent, uv, {1,1,1,1} });
            mesh->m_bounds.expand_by_point(pos);
        }
    }

    for (uint32_t r = 0; r < rings; ++r) {
        for (uint32_t s = 0; s < segments; ++s) {
            uint32_t cur = r * (segments + 1) + s;
            uint32_t next = cur + segments + 1;

            mesh->m_indices.push_back(cur);
            mesh->m_indices.push_back(next);
            mesh->m_indices.push_back(cur + 1);

            mesh->m_indices.push_back(cur + 1);
            mesh->m_indices.push_back(next);
            mesh->m_indices.push_back(next + 1);
        }
    }

    Submesh submesh{};
    submesh.name = "Sphere_Submesh";
    submesh.first_index = 0;
    submesh.index_count = static_cast<uint32_t>(mesh->m_indices.size());
    submesh.first_vertex = 0;
    submesh.vertex_count = static_cast<uint32_t>(mesh->m_vertices.size());
    submesh.bounds = mesh->m_bounds;
    mesh->m_submeshes.push_back(submesh);

    mesh->build_meshlets();
    mesh->upload_to_gpu();
    return mesh;
}

std::shared_ptr<Mesh> Mesh::create_plane(float width, float height, uint32_t subdivisions) {
    auto mesh = std::make_shared<Mesh>();
    mesh->name = "Procedural_Plane";

    uint32_t v_count = (subdivisions + 1) * (subdivisions + 1);
    mesh->m_vertices.reserve(v_count);
    mesh->m_indices.reserve(subdivisions * subdivisions * 6);

    float half_w = width * 0.5f;
    float half_h = height * 0.5f;
    float dx = width / subdivisions;
    float dz = height / subdivisions;

    for (uint32_t z = 0; z <= subdivisions; ++z) {
        for (uint32_t x = 0; x <= subdivisions; ++x) {
            float px = -half_w + x * dx;
            float pz = -half_h + z * dz;
            core::Vec3 pos(px, 0.0f, pz);
            core::Vec3 norm(0.0f, 1.0f, 0.0f);
            core::Vec4 tangent(1.0f, 0.0f, 0.0f, 1.0f);
            core::Vec2 uv(static_cast<float>(x) / subdivisions, static_cast<float>(z) / subdivisions);

            mesh->m_vertices.push_back({ pos, norm, tangent, uv, {1,1,1,1} });
            mesh->m_bounds.expand_by_point(pos);
        }
    }

    for (uint32_t z = 0; z < subdivisions; ++z) {
        for (uint32_t x = 0; x < subdivisions; ++x) {
            uint32_t row1 = z * (subdivisions + 1) + x;
            uint32_t row2 = (z + 1) * (subdivisions + 1) + x;

            mesh->m_indices.push_back(row1);
            mesh->m_indices.push_back(row2);
            mesh->m_indices.push_back(row1 + 1);

            mesh->m_indices.push_back(row1 + 1);
            mesh->m_indices.push_back(row2);
            mesh->m_indices.push_back(row2 + 1);
        }
    }

    Submesh submesh{};
    submesh.name = "Plane_Submesh";
    submesh.first_index = 0;
    submesh.index_count = static_cast<uint32_t>(mesh->m_indices.size());
    submesh.first_vertex = 0;
    submesh.vertex_count = static_cast<uint32_t>(mesh->m_vertices.size());
    submesh.bounds = mesh->m_bounds;
    mesh->m_submeshes.push_back(submesh);

    mesh->build_meshlets();
    mesh->upload_to_gpu();
    return mesh;
}

std::shared_ptr<Mesh> Mesh::from_imported_mesh(const importer::ImportedMesh& imported_mesh) {
    auto mesh = std::make_shared<Mesh>();
    mesh->name = imported_mesh.name;

    for (const auto& prim : imported_mesh.primitives) {
        Submesh submesh{};
        submesh.first_vertex = static_cast<uint32_t>(mesh->m_vertices.size());
        submesh.vertex_count = static_cast<uint32_t>(prim.vertices.size());
        submesh.first_index = static_cast<uint32_t>(mesh->m_indices.size());
        submesh.index_count = static_cast<uint32_t>(prim.indices.size());
        submesh.material_index = prim.material_index >= 0 ? static_cast<uint32_t>(prim.material_index) : 0;
        submesh.bounds = prim.bounds;

        for (const auto& iv : prim.vertices) {
            MeshVertex v{};
            v.position = iv.position;
            v.normal = iv.normal;
            v.tangent = iv.tangent;
            v.uv = iv.texcoord;
            v.color = iv.color;
            mesh->m_vertices.push_back(v);
            mesh->m_bounds.expand_by_point(v.position);
        }

        for (uint32_t idx : prim.indices) {
            mesh->m_indices.push_back(submesh.first_vertex + idx);
        }

        mesh->m_submeshes.push_back(submesh);
    }

    mesh->build_meshlets();
    mesh->upload_to_gpu();
    return mesh;
}

} // namespace engine::renderer
