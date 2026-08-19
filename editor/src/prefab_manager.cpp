#include "editor/prefab_manager.h"
#include "editor/command_history.h"
#include "engine/vfs/vfs.h"
#include "engine/scene/map_serializer.h"
#include "engine/core/log.h"
#include <toml++/toml.hpp>

namespace editor {

bool PrefabManager::is_prefab_file(std::string_view path) {
    return path.ends_with(".prefab") || path.ends_with(".prefab.toml");
}

bool PrefabManager::save_prefab(flecs::entity entity, engine::scene::Scene& scene, std::string_view virtual_path) {
    if (!entity.is_valid() || !entity.is_alive()) {
        LOG_ERROR("PrefabManager", "Cannot create prefab from invalid entity");
        return false;
    }

    EntitySnapshot snapshot = EntitySnapshot::capture(entity, scene);
    
    toml::table tbl;
    tbl.insert_or_assign("name", snapshot.name);
    tbl.insert_or_assign("prefab_version", "1.0");

    if (snapshot.transform.has_value()) {
        toml::table t;
        t.insert_or_assign("pos", toml::array{ snapshot.transform->position.x, snapshot.transform->position.y, snapshot.transform->position.z });
        t.insert_or_assign("rot", toml::array{ snapshot.transform->rotation.w, snapshot.transform->rotation.x, snapshot.transform->rotation.y, snapshot.transform->rotation.z });
        t.insert_or_assign("scale", toml::array{ snapshot.transform->scale.x, snapshot.transform->scale.y, snapshot.transform->scale.z });
        tbl.insert_or_assign("transform", t);
    }

    if (snapshot.mesh_renderer.has_value()) {
        toml::table m;
        m.insert_or_assign("cast_shadows", snapshot.mesh_renderer->cast_shadows);
        m.insert_or_assign("receive_shadows", snapshot.mesh_renderer->receive_shadows);
        tbl.insert_or_assign("mesh_renderer", m);
    }

    if (snapshot.directional_light.has_value()) {
        toml::table l;
        l.insert_or_assign("color", toml::array{ snapshot.directional_light->color.x, snapshot.directional_light->color.y, snapshot.directional_light->color.z });
        l.insert_or_assign("intensity", snapshot.directional_light->intensity);
        l.insert_or_assign("cast_shadows", snapshot.directional_light->cast_shadows);
        tbl.insert_or_assign("directional_light", l);
    }

    if (snapshot.point_light.has_value()) {
        toml::table l;
        l.insert_or_assign("color", toml::array{ snapshot.point_light->color.x, snapshot.point_light->color.y, snapshot.point_light->color.z });
        l.insert_or_assign("intensity", snapshot.point_light->intensity);
        l.insert_or_assign("radius", snapshot.point_light->radius);
        tbl.insert_or_assign("point_light", l);
    }

    if (snapshot.spot_light.has_value()) {
        toml::table l;
        l.insert_or_assign("color", toml::array{ snapshot.spot_light->color.x, snapshot.spot_light->color.y, snapshot.spot_light->color.z });
        l.insert_or_assign("intensity", snapshot.spot_light->intensity);
        l.insert_or_assign("range", snapshot.spot_light->range);
        l.insert_or_assign("inner_angle", snapshot.spot_light->inner_cone_angle_deg);
        l.insert_or_assign("outer_angle", snapshot.spot_light->outer_cone_angle_deg);
        tbl.insert_or_assign("spot_light", l);
    }

    if (snapshot.camera.has_value()) {
        toml::table c;
        c.insert_or_assign("fov_deg", snapshot.camera->fov_deg);
        c.insert_or_assign("near_z", snapshot.camera->near_z);
        c.insert_or_assign("far_z", snapshot.camera->far_z);
        tbl.insert_or_assign("camera", c);
    }

    if (snapshot.rigidbody.has_value()) {
        toml::table r;
        r.insert_or_assign("mass", snapshot.rigidbody->mass);
        r.insert_or_assign("friction", snapshot.rigidbody->friction);
        r.insert_or_assign("restitution", snapshot.rigidbody->restitution);
        r.insert_or_assign("motion_type", static_cast<int64_t>(snapshot.rigidbody->motion_type));
        tbl.insert_or_assign("rigidbody", r);
    }

    if (snapshot.collider.has_value()) {
        toml::table col;
        col.insert_or_assign("shape_type", static_cast<int64_t>(snapshot.collider->shape_type));
        col.insert_or_assign("half_extents", toml::array{ snapshot.collider->box_half_extents.x, snapshot.collider->box_half_extents.y, snapshot.collider->box_half_extents.z });
        col.insert_or_assign("radius", snapshot.collider->radius);
        col.insert_or_assign("half_height", snapshot.collider->half_height);
        tbl.insert_or_assign("collider", col);
    }

    if (snapshot.audio_source.has_value()) {
        toml::table a;
        a.insert_or_assign("sound_name", snapshot.audio_source->sound_name);
        a.insert_or_assign("volume", snapshot.audio_source->volume);
        a.insert_or_assign("pitch", snapshot.audio_source->pitch);
        a.insert_or_assign("is_looping", snapshot.audio_source->is_looping);
        tbl.insert_or_assign("audio_source", a);
    }

    if (snapshot.script.has_value()) {
        toml::table s;
        s.insert_or_assign("class_name", snapshot.script->class_name);
        tbl.insert_or_assign("script", s);
    }

    std::stringstream ss;
    ss << tbl;
    std::string toml_content = ss.str();

    if (engine::vfs::VFS::instance().write_string(virtual_path, toml_content)) {
        LOG_INFO("PrefabManager", "Saved prefab '{}' to '{}' ({} bytes)", snapshot.name, virtual_path, toml_content.size());
        return true;
    }

    LOG_ERROR("PrefabManager", "Failed to write prefab to VFS path '{}'", virtual_path);
    return false;
}

flecs::entity PrefabManager::instantiate_prefab(std::string_view virtual_path, 
                                               engine::scene::Scene& scene, 
                                               const engine::core::Vec3& spawn_position) {
    std::string toml_content;
    if (!engine::vfs::VFS::instance().read_string(virtual_path, toml_content)) {
        LOG_ERROR("PrefabManager", "Failed to read prefab from VFS path '{}'", virtual_path);
        return flecs::entity::null();
    }

    toml::table tbl;
    try {
        tbl = toml::parse(toml_content);
    } catch (const toml::parse_error& err) {
        LOG_ERROR("PrefabManager", "Failed to parse prefab TOML: {}", err.description());
        return flecs::entity::null();
    }

    std::string name = tbl["name"].value_or("PrefabEntity");

    engine::scene::Entity entity = scene.create_entity(name);

    if (auto t_node = tbl["transform"].as_table()) {
        engine::scene::TransformComponent t;
        if (auto pos = (*t_node)["pos"].as_array(); pos && pos->size() >= 3) {
            t.position = spawn_position + engine::core::Vec3((*pos)[0].value_or(0.0f), (*pos)[1].value_or(0.0f), (*pos)[2].value_or(0.0f));
        } else {
            t.position = spawn_position;
        }
        if (auto rot = (*t_node)["rot"].as_array(); rot && rot->size() >= 4) {
            t.rotation = engine::core::Quat((*rot)[0].value_or(1.0f), (*rot)[1].value_or(0.0f), (*rot)[2].value_or(0.0f), (*rot)[3].value_or(0.0f));
        }
        if (auto sc = (*t_node)["scale"].as_array(); sc && sc->size() >= 3) {
            t.scale = engine::core::Vec3((*sc)[0].value_or(1.0f), (*sc)[1].value_or(1.0f), (*sc)[2].value_or(1.0f));
        }
        t.is_dirty = true;
        entity.set<engine::scene::TransformComponent>(t);
    } else {
        entity.set<engine::scene::TransformComponent>(engine::scene::TransformComponent{ .position = spawn_position });
    }

    if (auto m_node = tbl["mesh_renderer"].as_table()) {
        engine::scene::MeshRendererComponent m;
        m.cast_shadows = (*m_node)["cast_shadows"].value_or(true);
        m.receive_shadows = (*m_node)["receive_shadows"].value_or(true);
        entity.set<engine::scene::MeshRendererComponent>(m);
    }

    if (auto l_node = tbl["directional_light"].as_table()) {
        engine::scene::DirectionalLightComponent l;
        if (auto c = (*l_node)["color"].as_array(); c && c->size() >= 3) {
            l.color = engine::core::Vec3((*c)[0].value_or(1.0f), (*c)[1].value_or(1.0f), (*c)[2].value_or(1.0f));
        }
        l.intensity = (*l_node)["intensity"].value_or(1.0f);
        l.cast_shadows = (*l_node)["cast_shadows"].value_or(true);
        entity.set<engine::scene::DirectionalLightComponent>(l);
    }

    if (auto l_node = tbl["point_light"].as_table()) {
        engine::scene::PointLightComponent l;
        if (auto c = (*l_node)["color"].as_array(); c && c->size() >= 3) {
            l.color = engine::core::Vec3((*c)[0].value_or(1.0f), (*c)[1].value_or(1.0f), (*c)[2].value_or(1.0f));
        }
        l.intensity = (*l_node)["intensity"].value_or(1.0f);
        l.radius = (*l_node)["radius"].value_or(10.0f);
        entity.set<engine::scene::PointLightComponent>(l);
    }

    if (auto l_node = tbl["spot_light"].as_table()) {
        engine::scene::SpotLightComponent l;
        if (auto c = (*l_node)["color"].as_array(); c && c->size() >= 3) {
            l.color = engine::core::Vec3((*c)[0].value_or(1.0f), (*c)[1].value_or(1.0f), (*c)[2].value_or(1.0f));
        }
        l.intensity = (*l_node)["intensity"].value_or(1.0f);
        l.range = (*l_node)["range"].value_or(10.0f);
        l.inner_cone_angle_deg = (*l_node)["inner_angle"].value_or(25.0f);
        l.outer_cone_angle_deg = (*l_node)["outer_angle"].value_or(35.0f);
        entity.set<engine::scene::SpotLightComponent>(l);
    }

    if (auto c_node = tbl["camera"].as_table()) {
        engine::scene::CameraComponent c;
        c.fov_deg = (*c_node)["fov_deg"].value_or(60.0f);
        c.near_z = (*c_node)["near_z"].value_or(0.1f);
        c.far_z = (*c_node)["far_z"].value_or(1000.0f);
        entity.set<engine::scene::CameraComponent>(c);
    }

    if (auto r_node = tbl["rigidbody"].as_table()) {
        engine::physics::RigidBodyComponent r;
        r.mass = (*r_node)["mass"].value_or(1.0f);
        r.friction = (*r_node)["friction"].value_or(0.5f);
        r.restitution = (*r_node)["restitution"].value_or(0.0f);
        r.motion_type = static_cast<engine::physics::BodyMotionType>((*r_node)["motion_type"].value_or(0));
        entity.set<engine::physics::RigidBodyComponent>(r);
    }

    if (auto col_node = tbl["collider"].as_table()) {
        engine::physics::ColliderComponent col;
        col.shape_type = static_cast<engine::physics::ColliderShapeType>((*col_node)["shape_type"].value_or(0));
        if (auto he = (*col_node)["half_extents"].as_array(); he && he->size() >= 3) {
            col.box_half_extents = engine::core::Vec3((*he)[0].value_or(0.5f), (*he)[1].value_or(0.5f), (*he)[2].value_or(0.5f));
        }
        col.radius = (*col_node)["radius"].value_or(0.5f);
        col.half_height = (*col_node)["half_height"].value_or(0.5f);
        entity.set<engine::physics::ColliderComponent>(col);
    }

    if (auto a_node = tbl["audio_source"].as_table()) {
        engine::audio::AudioSourceComponent a;
        a.sound_name = (*a_node)["sound_name"].value_or("sfx_footstep.wav");
        a.volume = (*a_node)["volume"].value_or(1.0f);
        a.pitch = (*a_node)["pitch"].value_or(1.0f);
        a.is_looping = (*a_node)["is_looping"].value_or(false);
        entity.set<engine::audio::AudioSourceComponent>(a);
    }

    if (auto s_node = tbl["script"].as_table()) {
        engine::scripting::ScriptComponent s;
        s.class_name = (*s_node)["class_name"].value_or("");
        entity.set<engine::scripting::ScriptComponent>(s);
    }

    LOG_INFO("PrefabManager", "Instantiated prefab '{}' into scene at position ({:.2f}, {:.2f}, {:.2f})",
             name, spawn_position.x, spawn_position.y, spawn_position.z);

    return entity.get_raw();
}

} // namespace editor
