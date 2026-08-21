#include "engine/scene/map_serializer.h"
#include "engine/scene/components.h"
#include "engine/core/log.h"
#include "engine/core/reflection.h"
#include "engine/vfs/vfs.h"
#include "engine/physics/physics_components.h"
#include "engine/audio/audio_components.h"
#include "engine/scripting/script_components.h"
#include <toml++/toml.hpp>
#include <sstream>
#include <vector>

namespace engine::scene {

namespace {

struct TomlWriterVisitor {
    toml::table& table;

    template <typename T>
    void visit(const char* name, const T& value, const char* label = nullptr, const char* tooltip = nullptr) {
        (void)label;
        (void)tooltip;
        using CleanT = std::remove_cvref_t<T>;

        if constexpr (std::is_same_v<CleanT, bool>) {
            table.insert(name, value);
        } else if constexpr (std::is_integral_v<CleanT>) {
            table.insert(name, static_cast<int64_t>(value));
        } else if constexpr (std::is_floating_point_v<CleanT>) {
            table.insert(name, static_cast<double>(value));
        } else if constexpr (std::is_same_v<CleanT, std::string>) {
            table.insert(name, value);
        } else if constexpr (std::is_same_v<CleanT, core::Vec2>) {
            table.insert(name, toml::array{ value.x, value.y });
        } else if constexpr (std::is_same_v<CleanT, core::Vec3>) {
            table.insert(name, toml::array{ value.x, value.y, value.z });
        } else if constexpr (std::is_same_v<CleanT, core::Vec4>) {
            table.insert(name, toml::array{ value.x, value.y, value.z, value.w });
        } else if constexpr (std::is_same_v<CleanT, core::Quat>) {
            table.insert(name, toml::array{ value.x, value.y, value.z, value.w });
        } else if constexpr (std::is_same_v<CleanT, assets::UUID>) {
            if (value.is_valid()) {
                table.insert(name, value.to_string());
            }
        } else if constexpr (std::is_enum_v<CleanT>) {
            table.insert(name, static_cast<int64_t>(value));
        }
    }
};

struct TomlReaderVisitor {
    const toml::table& table;

    template <typename T>
    void visit(const char* name, T& value, const char* label = nullptr, const char* tooltip = nullptr) {
        (void)label;
        (void)tooltip;
        using CleanT = std::remove_cvref_t<T>;

        if constexpr (std::is_same_v<CleanT, bool>) {
            if (auto* b = table[name].as_boolean()) value = b->get();
        } else if constexpr (std::is_integral_v<CleanT>) {
            if (auto* i = table[name].as_integer()) value = static_cast<CleanT>(i->get());
        } else if constexpr (std::is_floating_point_v<CleanT>) {
            if (auto* f = table[name].as_floating_point()) value = static_cast<CleanT>(f->get());
            else if (auto* i = table[name].as_integer()) value = static_cast<CleanT>(i->get());
        } else if constexpr (std::is_same_v<CleanT, std::string>) {
            if (auto* s = table[name].as_string()) value = s->get();
        } else if constexpr (std::is_same_v<CleanT, core::Vec2>) {
            if (auto* arr = table[name].as_array(); arr && arr->size() >= 2) {
                if (auto* v0 = arr->get(0)->as_floating_point()) value.x = static_cast<float>(v0->get());
                else if (auto* i0 = arr->get(0)->as_integer()) value.x = static_cast<float>(i0->get());
                if (auto* v1 = arr->get(1)->as_floating_point()) value.y = static_cast<float>(v1->get());
                else if (auto* i1 = arr->get(1)->as_integer()) value.y = static_cast<float>(i1->get());
            }
        } else if constexpr (std::is_same_v<CleanT, core::Vec3>) {
            if (auto* arr = table[name].as_array(); arr && arr->size() >= 3) {
                if (auto* v0 = arr->get(0)->as_floating_point()) value.x = static_cast<float>(v0->get());
                else if (auto* i0 = arr->get(0)->as_integer()) value.x = static_cast<float>(i0->get());
                if (auto* v1 = arr->get(1)->as_floating_point()) value.y = static_cast<float>(v1->get());
                else if (auto* i1 = arr->get(1)->as_integer()) value.y = static_cast<float>(i1->get());
                if (auto* v2 = arr->get(2)->as_floating_point()) value.z = static_cast<float>(v2->get());
                else if (auto* i2 = arr->get(2)->as_integer()) value.z = static_cast<float>(i2->get());
            }
        } else if constexpr (std::is_same_v<CleanT, core::Vec4>) {
            if (auto* arr = table[name].as_array(); arr && arr->size() >= 4) {
                if (auto* v0 = arr->get(0)->as_floating_point()) value.x = static_cast<float>(v0->get());
                else if (auto* i0 = arr->get(0)->as_integer()) value.x = static_cast<float>(i0->get());
                if (auto* v1 = arr->get(1)->as_floating_point()) value.y = static_cast<float>(v1->get());
                else if (auto* i1 = arr->get(1)->as_integer()) value.y = static_cast<float>(i1->get());
                if (auto* v2 = arr->get(2)->as_floating_point()) value.z = static_cast<float>(v2->get());
                else if (auto* i2 = arr->get(2)->as_integer()) value.z = static_cast<float>(i2->get());
                if (auto* v3 = arr->get(3)->as_floating_point()) value.w = static_cast<float>(v3->get());
                else if (auto* i3 = arr->get(3)->as_integer()) value.w = static_cast<float>(i3->get());
            }
        } else if constexpr (std::is_same_v<CleanT, core::Quat>) {
            if (auto* arr = table[name].as_array(); arr && arr->size() >= 4) {
                if (auto* v0 = arr->get(0)->as_floating_point()) value.x = static_cast<float>(v0->get());
                else if (auto* i0 = arr->get(0)->as_integer()) value.x = static_cast<float>(i0->get());
                if (auto* v1 = arr->get(1)->as_floating_point()) value.y = static_cast<float>(v1->get());
                else if (auto* i1 = arr->get(1)->as_integer()) value.y = static_cast<float>(i1->get());
                if (auto* v2 = arr->get(2)->as_floating_point()) value.z = static_cast<float>(v2->get());
                else if (auto* i2 = arr->get(2)->as_integer()) value.z = static_cast<float>(i2->get());
                if (auto* v3 = arr->get(3)->as_floating_point()) value.w = static_cast<float>(v3->get());
                else if (auto* i3 = arr->get(3)->as_integer()) value.w = static_cast<float>(i3->get());
            }
        } else if constexpr (std::is_same_v<CleanT, assets::UUID>) {
            if (auto* u = table[name].as_string()) value = assets::UUID::from_string(u->get());
        } else if constexpr (std::is_enum_v<CleanT>) {
            if (auto* i = table[name].as_integer()) value = static_cast<CleanT>(i->get());
        }
    }
};

template <typename TComponent>
void serialize_reflected_component(flecs::entity e, const char* table_name, toml::table& entity_tbl) {
    if (const auto* comp = e.try_get<TComponent>()) {
        toml::table comp_tbl;
        TomlWriterVisitor visitor{comp_tbl};
        core::reflect_visit(*comp, visitor);
        entity_tbl.insert(table_name, comp_tbl);
    }
}

template <typename TComponent>
void deserialize_reflected_component(const toml::table& entity_tbl, const char* table_name, Entity& entity) {
    if (auto* comp_node = entity_tbl[table_name].as_table()) {
        TComponent comp{};
        TomlReaderVisitor visitor{*comp_node};
        core::reflect_visit(comp, visitor);
        entity.set(comp);
    }
}

} // anonymous namespace

bool MapSerializer::serialize_to_toml(Scene& scene, std::string& out_toml) {
    toml::table root;

    // 1. Map Header
    toml::table header_tbl;
    header_tbl.insert("name", std::string(scene.get_name()));
    header_tbl.insert("version", "1.0.0");
    root.insert("map", header_tbl);

    // 2. Entities Array
    toml::array entities_arr;

    scene.get_world().query_builder<const TagComponent, const UUIDComponent, const TransformComponent>()
        .build()
        .each([&](flecs::entity e, const TagComponent& tag, const UUIDComponent& uuid_comp, const TransformComponent& trans) {
            toml::table entity_tbl;
            entity_tbl.insert("name", tag.name);
            entity_tbl.insert("uuid", uuid_comp.uuid.to_string());

            Entity ent(e, &scene);
            Entity parent = ent.get_parent();
            if (parent.is_valid()) {
                entity_tbl.insert("parent_uuid", parent.get_uuid().to_string());
            }

            // Reflected Components
            serialize_reflected_component<TransformComponent>(e, "transform", entity_tbl);
            serialize_reflected_component<MeshRendererComponent>(e, "mesh_renderer", entity_tbl);
            serialize_reflected_component<DirectionalLightComponent>(e, "directional_light", entity_tbl);
            serialize_reflected_component<PointLightComponent>(e, "point_light", entity_tbl);
            serialize_reflected_component<SpotLightComponent>(e, "spot_light", entity_tbl);
            serialize_reflected_component<CameraComponent>(e, "camera", entity_tbl);
            serialize_reflected_component<MaterialComponent>(e, "material", entity_tbl);
            serialize_reflected_component<physics::RigidBodyComponent>(e, "rigid_body", entity_tbl);
            serialize_reflected_component<physics::ColliderComponent>(e, "collider", entity_tbl);
            serialize_reflected_component<scripting::ScriptComponent>(e, "script", entity_tbl);
            serialize_reflected_component<audio::AudioSourceComponent>(e, "audio_source", entity_tbl);
            serialize_reflected_component<audio::AudioListenerComponent>(e, "audio_listener", entity_tbl);

            entities_arr.push_back(entity_tbl);
        });

    root.insert("entities", entities_arr);

    std::stringstream ss;
    ss << root;
    out_toml = ss.str();
    return true;
}

bool MapSerializer::deserialize_from_toml(std::string_view toml_content, Scene& out_scene) {
    try {
        auto tbl = toml::parse(toml_content);
        out_scene.clear();

        if (auto* map_tbl = tbl["map"].as_table()) {
            if (auto* name_node = (*map_tbl)["name"].as_string()) {
                out_scene.set_name(name_node->get());
            }
        }

        auto* entities = tbl["entities"].as_array();
        if (!entities) return true;

        struct ParentLink {
            Entity child;
            assets::UUID parent_uuid;
        };
        std::vector<ParentLink> parent_links;

        for (size_t i = 0; i < entities->size(); ++i) {
            auto* ent_node = (*entities)[i].as_table();
            if (!ent_node) continue;

            std::string name = "Entity";
            assets::UUID uuid = assets::UUID::generate();

            if (auto* n = (*ent_node)["name"].as_string()) name = n->get();
            if (auto* u = (*ent_node)["uuid"].as_string()) uuid = assets::UUID::from_string(u->get());

            Entity entity = out_scene.create_entity(name, uuid);

            if (auto* p = (*ent_node)["parent_uuid"].as_string()) {
                assets::UUID p_uuid = assets::UUID::from_string(p->get());
                if (p_uuid.is_valid()) {
                    parent_links.push_back({ entity, p_uuid });
                }
            }

            // Reflected Components
            deserialize_reflected_component<TransformComponent>(*ent_node, "transform", entity);
            deserialize_reflected_component<MeshRendererComponent>(*ent_node, "mesh_renderer", entity);
            deserialize_reflected_component<DirectionalLightComponent>(*ent_node, "directional_light", entity);
            deserialize_reflected_component<PointLightComponent>(*ent_node, "point_light", entity);
            deserialize_reflected_component<SpotLightComponent>(*ent_node, "spot_light", entity);
            deserialize_reflected_component<CameraComponent>(*ent_node, "camera", entity);
            deserialize_reflected_component<MaterialComponent>(*ent_node, "material", entity);
            deserialize_reflected_component<physics::RigidBodyComponent>(*ent_node, "rigid_body", entity);
            deserialize_reflected_component<physics::ColliderComponent>(*ent_node, "collider", entity);
            deserialize_reflected_component<scripting::ScriptComponent>(*ent_node, "script", entity);
            deserialize_reflected_component<audio::AudioSourceComponent>(*ent_node, "audio_source", entity);
            deserialize_reflected_component<audio::AudioListenerComponent>(*ent_node, "audio_listener", entity);
        }

        // Pass 2: Reconstruct Parent-Child Hierarchy
        for (const auto& link : parent_links) {
            Entity parent = out_scene.find_entity_by_uuid(link.parent_uuid);
            if (parent.is_valid()) {
                Entity child = link.child;
                child.set_parent(parent);
            }
        }

        out_scene.update(0.0f);
        LOG_INFO("Scene", "Loaded map '{}' with {} entities", out_scene.get_name(), out_scene.get_entity_count());
        return true;
    } catch (const toml::parse_error& err) {
        LOG_ERROR("Scene", "Failed to deserialize map: {}", err.description());
        return false;
    }
}

bool MapSerializer::save_map(Scene& scene, std::string_view virtual_path) {
    std::string toml_content;
    if (!serialize_to_toml(scene, toml_content)) return false;
    return vfs::VFS::instance().write_string(virtual_path, toml_content);
}

bool MapSerializer::load_map(std::string_view virtual_path, Scene& out_scene) {
    std::string toml_content;
    if (!vfs::VFS::instance().read_string(virtual_path, toml_content)) {
        LOG_ERROR("Scene", "Failed to read map file: {}", virtual_path);
        return false;
    }
    return deserialize_from_toml(toml_content, out_scene);
}

} // namespace engine::scene
