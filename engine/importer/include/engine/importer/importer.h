#pragma once

#include "engine/core/config.h"
#include "engine/core/math.h"
#include "engine/assets/uuid.h"
#include <string>
#include <vector>
#include <memory>

namespace engine::importer {

struct ImportedVertex {
    core::Vec3 position{0.0f};
    core::Vec3 normal{0.0f, 1.0f, 0.0f};
    core::Vec4 tangent{1.0f, 0.0f, 0.0f, 1.0f};
    core::Vec2 texcoord{0.0f, 0.0f};
    core::Vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
};

struct ImportedPrimitive {
    std::vector<ImportedVertex> vertices;
    std::vector<uint32_t> indices;
    int32_t material_index{-1};
    core::AABB bounds;
};

struct ImportedMesh {
    std::string name;
    std::vector<ImportedPrimitive> primitives;
};

struct ImportedMaterial {
    std::string name;
    core::Vec4 base_color_factor{1.0f, 1.0f, 1.0f, 1.0f};
    float metallic_factor{1.0f};
    float roughness_factor{1.0f};
    core::Vec3 emissive_factor{0.0f, 0.0f, 0.0f};
    float emissive_strength{1.0f};

    // Textures (URIs or embedded indices)
    std::string base_color_texture;
    std::string normal_texture;
    std::string metallic_roughness_texture;
    std::string emissive_texture;

    // Advanced PBR / KHR Extensions
    float transmission_factor{0.0f}; // > 0 triggers OIT path
    float ior{1.5f};
    float clearcoat_factor{0.0f};
    float sheen_factor{0.0f};
};

enum class LightType : uint8_t {
    Directional = 0,
    Point,
    Spot
};

struct ImportedLight {
    std::string name;
    LightType type{LightType::Directional};
    core::Vec3 color{1.0f, 1.0f, 1.0f};
    float intensity{1.0f};
    float range{10.0f};
    float inner_cone_angle_deg{25.0f};
    float outer_cone_angle_deg{35.0f};
};

struct ImportedCamera {
    std::string name;
    bool is_perspective{true};
    float fov_y_deg{60.0f};
    float aspect_ratio{1.777f};
    float near_z{0.1f};
    float far_z{1000.0f};
};

struct ImportedNode {
    std::string name;
    core::Transform transform;
    int32_t mesh_index{-1};
    int32_t light_index{-1};
    int32_t camera_index{-1};
    std::vector<uint32_t> children;
};

struct ImportedScene {
    std::string name;
    std::vector<uint32_t> root_nodes;
    std::vector<ImportedNode> nodes;
    std::vector<ImportedMesh> meshes;
    std::vector<ImportedMaterial> materials;
    std::vector<ImportedLight> lights;
    std::vector<ImportedCamera> cameras;
};

class IAssetImporter {
public:
    virtual ~IAssetImporter() = default;
    virtual bool can_import(std::string_view extension) const = 0;
    virtual bool import_scene(std::string_view virtual_path, ImportedScene& out_scene) = 0;
};

class GltfImporter : public IAssetImporter {
public:
    GltfImporter() = default;
    ~GltfImporter() override = default;

    bool can_import(std::string_view extension) const override;
    bool import_scene(std::string_view virtual_path, ImportedScene& out_scene) override;
    bool import_from_memory(const uint8_t* data, size_t size, std::string_view source_name, ImportedScene& out_scene);
};

} // namespace engine::importer
