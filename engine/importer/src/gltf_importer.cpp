#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#include "engine/importer/importer.h"
#include "engine/vfs/vfs.h"
#include "engine/core/log.h"
#include <cstring>

namespace engine::importer {

bool GltfImporter::can_import(std::string_view extension) const {
    return extension == ".gltf" || extension == ".glb";
}

bool GltfImporter::import_scene(std::string_view virtual_path, ImportedScene& out_scene) {
    std::vector<uint8_t> buffer;
    if (!vfs::VFS::instance().read_bytes(virtual_path, buffer)) {
        LOG_ERROR("Importer", "Failed to read glTF file from VFS: {}", virtual_path);
        return false;
    }
    return import_from_memory(buffer.data(), buffer.size(), virtual_path, out_scene);
}

bool GltfImporter::import_from_memory(const uint8_t* data, size_t size, std::string_view source_name, ImportedScene& out_scene) {
    cgltf_options options{};
    cgltf_data* gltf_data = nullptr;

    cgltf_result res = cgltf_parse(&options, data, size, &gltf_data);
    if (res != cgltf_result_success) {
        LOG_ERROR("Importer", "cgltf_parse failed for '{}': result = {}", source_name, static_cast<int>(res));
        return false;
    }

    res = cgltf_load_buffers(&options, gltf_data, nullptr);
    if (res != cgltf_result_success && gltf_data->buffers_count > 0 && gltf_data->buffers[0].data == nullptr) {
        // If external buffers are not packed in GLB, note it
        LOG_WARN("Importer", "glTF external buffer loading not fully resolved for '{}'", source_name);
    }

    out_scene.name = gltf_data->asset.generator ? gltf_data->asset.generator : "glTF Scene";

    // 1. Import Materials
    out_scene.materials.reserve(gltf_data->materials_count);
    for (size_t i = 0; i < gltf_data->materials_count; ++i) {
        const auto& mat = gltf_data->materials[i];
        ImportedMaterial m{};
        m.name = mat.name ? mat.name : std::format("Material_{}", i);

        if (mat.has_pbr_metallic_roughness) {
            m.base_color_factor = core::Vec4(
                mat.pbr_metallic_roughness.base_color_factor[0],
                mat.pbr_metallic_roughness.base_color_factor[1],
                mat.pbr_metallic_roughness.base_color_factor[2],
                mat.pbr_metallic_roughness.base_color_factor[3]
            );
            m.metallic_factor = mat.pbr_metallic_roughness.metallic_factor;
            m.roughness_factor = mat.pbr_metallic_roughness.roughness_factor;

            if (mat.pbr_metallic_roughness.base_color_texture.texture &&
                mat.pbr_metallic_roughness.base_color_texture.texture->image &&
                mat.pbr_metallic_roughness.base_color_texture.texture->image->uri) {
                m.base_color_texture = mat.pbr_metallic_roughness.base_color_texture.texture->image->uri;
            }
        }

        m.emissive_factor = core::Vec3(
            mat.emissive_factor[0],
            mat.emissive_factor[1],
            mat.emissive_factor[2]
        );

        if (mat.has_transmission) {
            m.transmission_factor = mat.transmission.transmission_factor;
        }
        if (mat.has_ior) {
            m.ior = mat.ior.ior;
        }
        if (mat.has_clearcoat) {
            m.clearcoat_factor = mat.clearcoat.clearcoat_factor;
        }
        if (mat.has_sheen) {
            m.sheen_factor = mat.sheen.sheen_roughness_factor;
        }
        if (mat.has_emissive_strength) {
            m.emissive_strength = mat.emissive_strength.emissive_strength;
        }

        out_scene.materials.push_back(std::move(m));
    }

    // 2. Import Meshes
    out_scene.meshes.reserve(gltf_data->meshes_count);
    for (size_t m_idx = 0; m_idx < gltf_data->meshes_count; ++m_idx) {
        const auto& mesh = gltf_data->meshes[m_idx];
        ImportedMesh im{};
        im.name = mesh.name ? mesh.name : std::format("Mesh_{}", m_idx);

        for (size_t p_idx = 0; p_idx < mesh.primitives_count; ++p_idx) {
            const auto& prim = mesh.primitives[p_idx];
            if (prim.type != cgltf_primitive_type_triangles) continue;

            ImportedPrimitive ip{};
            if (prim.material) {
                ip.material_index = static_cast<int32_t>(prim.material - gltf_data->materials);
            }

            // Find Position accessor to get vertex count
            size_t vertex_count = 0;
            const cgltf_accessor* pos_acc = nullptr;
            for (size_t a = 0; a < prim.attributes_count; ++a) {
                if (prim.attributes[a].type == cgltf_attribute_type_position) {
                    pos_acc = prim.attributes[a].data;
                    vertex_count = pos_acc->count;
                    break;
                }
            }

            if (vertex_count == 0 || !pos_acc) continue;
            ip.vertices.resize(vertex_count);

            // Read Positions
            for (size_t v = 0; v < vertex_count; ++v) {
                float p[3] = {0, 0, 0};
                cgltf_accessor_read_float(pos_acc, v, p, 3);
                ip.vertices[v].position = core::Vec3(p[0], p[1], p[2]);
                ip.bounds.expand_by_point(ip.vertices[v].position);
            }

            // Read other attributes
            for (size_t a = 0; a < prim.attributes_count; ++a) {
                const auto& attr = prim.attributes[a];
                if (attr.type == cgltf_attribute_type_normal) {
                    for (size_t v = 0; v < vertex_count; ++v) {
                        float n[3];
                        cgltf_accessor_read_float(attr.data, v, n, 3);
                        ip.vertices[v].normal = core::Vec3(n[0], n[1], n[2]);
                    }
                } else if (attr.type == cgltf_attribute_type_tangent) {
                    for (size_t v = 0; v < vertex_count; ++v) {
                        float t[4];
                        cgltf_accessor_read_float(attr.data, v, t, 4);
                        ip.vertices[v].tangent = core::Vec4(t[0], t[1], t[2], t[3]);
                    }
                } else if (attr.type == cgltf_attribute_type_texcoord) {
                    for (size_t v = 0; v < vertex_count; ++v) {
                        float uv[2];
                        cgltf_accessor_read_float(attr.data, v, uv, 2);
                        ip.vertices[v].texcoord = core::Vec2(uv[0], uv[1]);
                    }
                }
            }

            // Read Indices
            if (prim.indices) {
                ip.indices.resize(prim.indices->count);
                for (size_t idx = 0; idx < prim.indices->count; ++idx) {
                    ip.indices[idx] = static_cast<uint32_t>(cgltf_accessor_read_index(prim.indices, idx));
                }
            } else {
                ip.indices.resize(vertex_count);
                for (size_t idx = 0; idx < vertex_count; ++idx) {
                    ip.indices[idx] = static_cast<uint32_t>(idx);
                }
            }

            im.primitives.push_back(std::move(ip));
        }

        out_scene.meshes.push_back(std::move(im));
    }

    // 3. Import Lights (KHR_lights_punctual)
    out_scene.lights.reserve(gltf_data->lights_count);
    for (size_t l_idx = 0; l_idx < gltf_data->lights_count; ++l_idx) {
        const auto& light = gltf_data->lights[l_idx];
        ImportedLight il{};
        il.name = light.name ? light.name : std::format("Light_{}", l_idx);
        il.color = core::Vec3(light.color[0], light.color[1], light.color[2]);
        il.intensity = light.intensity;
        il.range = light.range > 0 ? light.range : 10.0f;

        if (light.type == cgltf_light_type_directional) {
            il.type = LightType::Directional;
        } else if (light.type == cgltf_light_type_point) {
            il.type = LightType::Point;
        } else if (light.type == cgltf_light_type_spot) {
            il.type = LightType::Spot;
            il.inner_cone_angle_deg = core::math::rad_to_deg(light.spot_inner_cone_angle);
            il.outer_cone_angle_deg = core::math::rad_to_deg(light.spot_outer_cone_angle);
        }

        out_scene.lights.push_back(il);
    }

    // 4. Import Cameras
    out_scene.cameras.reserve(gltf_data->cameras_count);
    for (size_t c_idx = 0; c_idx < gltf_data->cameras_count; ++c_idx) {
        const auto& cam = gltf_data->cameras[c_idx];
        ImportedCamera ic{};
        ic.name = cam.name ? cam.name : std::format("Camera_{}", c_idx);
        if (cam.type == cgltf_camera_type_perspective) {
            ic.is_perspective = true;
            ic.fov_y_deg = core::math::rad_to_deg(cam.data.perspective.yfov);
            ic.aspect_ratio = cam.data.perspective.has_aspect_ratio ? cam.data.perspective.aspect_ratio : 1.777f;
            ic.near_z = cam.data.perspective.znear;
            ic.far_z = cam.data.perspective.has_zfar ? cam.data.perspective.zfar : 1000.0f;
        } else if (cam.type == cgltf_camera_type_orthographic) {
            ic.is_perspective = false;
            ic.near_z = cam.data.orthographic.znear;
            ic.far_z = cam.data.orthographic.zfar;
        }
        out_scene.cameras.push_back(ic);
    }

    // 5. Import Nodes & Transforms
    out_scene.nodes.reserve(gltf_data->nodes_count);
    for (size_t n_idx = 0; n_idx < gltf_data->nodes_count; ++n_idx) {
        const auto& node = gltf_data->nodes[n_idx];
        ImportedNode in{};
        in.name = node.name ? node.name : std::format("Node_{}", n_idx);

        if (node.has_translation) {
            in.transform.position = core::Vec3(node.translation[0], node.translation[1], node.translation[2]);
        }
        if (node.has_rotation) {
            in.transform.rotation = core::Quat(node.rotation[0], node.rotation[1], node.rotation[2], node.rotation[3]);
        }
        if (node.has_scale) {
            in.transform.scale = core::Vec3(node.scale[0], node.scale[1], node.scale[2]);
        }

        if (node.mesh) in.mesh_index = static_cast<int32_t>(node.mesh - gltf_data->meshes);
        if (node.light) in.light_index = static_cast<int32_t>(node.light - gltf_data->lights);
        if (node.camera) in.camera_index = static_cast<int32_t>(node.camera - gltf_data->cameras);

        for (size_t c = 0; c < node.children_count; ++c) {
            in.children.push_back(static_cast<uint32_t>(node.children[c] - gltf_data->nodes));
        }

        out_scene.nodes.push_back(std::move(in));
    }

    if (gltf_data->scene && gltf_data->scene->nodes_count > 0) {
        for (size_t i = 0; i < gltf_data->scene->nodes_count; ++i) {
            out_scene.root_nodes.push_back(static_cast<uint32_t>(gltf_data->scene->nodes[i] - gltf_data->nodes));
        }
    }

    LOG_INFO("Importer", "Imported glTF scene '{}' with {} meshes, {} materials, {} lights, {} nodes",
             source_name, out_scene.meshes.size(), out_scene.materials.size(), out_scene.lights.size(), out_scene.nodes.size());

    cgltf_free(gltf_data);
    return true;
}

} // namespace engine::importer
