#include "engine/scene/map_serializer.h"
#include "engine/core/log.h"
#include "engine/vfs/vfs.h"
#include <toml++/toml.hpp>
#include <sstream>
#include <vector>

namespace engine::scene {

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

            // Transform
            toml::table trans_tbl;
            toml::array pos_arr;
            pos_arr.push_back(trans.position.x);
            pos_arr.push_back(trans.position.y);
            pos_arr.push_back(trans.position.z);
            trans_tbl.insert("position", pos_arr);

            toml::array rot_arr;
            rot_arr.push_back(trans.rotation.x);
            rot_arr.push_back(trans.rotation.y);
            rot_arr.push_back(trans.rotation.z);
            rot_arr.push_back(trans.rotation.w);
            trans_tbl.insert("rotation", rot_arr);

            toml::array scale_arr;
            scale_arr.push_back(trans.scale.x);
            scale_arr.push_back(trans.scale.y);
            scale_arr.push_back(trans.scale.z);
            trans_tbl.insert("scale", scale_arr);
            entity_tbl.insert("transform", trans_tbl);

            // MeshRenderer
            if (const auto* mr = e.try_get<MeshRendererComponent>()) {
                toml::table mr_tbl;
                mr_tbl.insert("mesh_uuid", mr->mesh_uuid.to_string());
                mr_tbl.insert("submesh_index", static_cast<int64_t>(mr->submesh_index));
                mr_tbl.insert("material_uuid", mr->material_uuid.to_string());
                mr_tbl.insert("cast_shadows", mr->cast_shadows);
                mr_tbl.insert("receive_shadows", mr->receive_shadows);
                mr_tbl.insert("is_visible", mr->is_visible);
                entity_tbl.insert("mesh_renderer", mr_tbl);
            }

            // Directional Light
            if (const auto* dl = e.try_get<DirectionalLightComponent>()) {
                toml::table dl_tbl;
                toml::array col_arr;
                col_arr.push_back(dl->color.x);
                col_arr.push_back(dl->color.y);
                col_arr.push_back(dl->color.z);
                dl_tbl.insert("color", col_arr);
                dl_tbl.insert("intensity", dl->intensity);
                dl_tbl.insert("cast_shadows", dl->cast_shadows);
                dl_tbl.insert("cascade_count", static_cast<int64_t>(dl->cascade_count));
                entity_tbl.insert("directional_light", dl_tbl);
            }

            // Point Light
            if (const auto* pl = e.try_get<PointLightComponent>()) {
                toml::table pl_tbl;
                toml::array col_arr;
                col_arr.push_back(pl->color.x);
                col_arr.push_back(pl->color.y);
                col_arr.push_back(pl->color.z);
                pl_tbl.insert("color", col_arr);
                pl_tbl.insert("intensity", pl->intensity);
                pl_tbl.insert("radius", pl->radius);
                pl_tbl.insert("falloff", pl->falloff);
                pl_tbl.insert("cast_shadows", pl->cast_shadows);
                entity_tbl.insert("point_light", pl_tbl);
            }

            // Spot Light
            if (const auto* sl = e.try_get<SpotLightComponent>()) {
                toml::table sl_tbl;
                toml::array col_arr;
                col_arr.push_back(sl->color.x);
                col_arr.push_back(sl->color.y);
                col_arr.push_back(sl->color.z);
                sl_tbl.insert("color", col_arr);
                sl_tbl.insert("intensity", sl->intensity);
                sl_tbl.insert("range", sl->range);
                sl_tbl.insert("inner_cone_deg", sl->inner_cone_angle_deg);
                sl_tbl.insert("outer_cone_deg", sl->outer_cone_angle_deg);
                sl_tbl.insert("cast_shadows", sl->cast_shadows);
                entity_tbl.insert("spot_light", sl_tbl);
            }

            // Camera
            if (const auto* cam = e.try_get<CameraComponent>()) {
                toml::table cam_tbl;
                cam_tbl.insert("fov_deg", cam->fov_deg);
                cam_tbl.insert("near_z", cam->near_z);
                cam_tbl.insert("far_z", cam->far_z);
                cam_tbl.insert("is_orthographic", cam->is_orthographic);
                cam_tbl.insert("ortho_size", cam->ortho_size);
                cam_tbl.insert("is_primary", cam->is_primary);
                entity_tbl.insert("camera", cam_tbl);
            }

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

            // Transform
            if (auto* t_node = (*ent_node)["transform"].as_table()) {
                auto* trans = entity.get_mut<TransformComponent>();
                if (auto* pos = (*t_node)["position"].as_array(); pos && pos->size() >= 3) {
                    trans->position.x = static_cast<float>(pos->get(0)->as_floating_point() ? pos->get(0)->as_floating_point()->get() : 0.0);
                    trans->position.y = static_cast<float>(pos->get(1)->as_floating_point() ? pos->get(1)->as_floating_point()->get() : 0.0);
                    trans->position.z = static_cast<float>(pos->get(2)->as_floating_point() ? pos->get(2)->as_floating_point()->get() : 0.0);
                }
                if (auto* rot = (*t_node)["rotation"].as_array(); rot && rot->size() >= 4) {
                    trans->rotation.x = static_cast<float>(rot->get(0)->as_floating_point() ? rot->get(0)->as_floating_point()->get() : 0.0);
                    trans->rotation.y = static_cast<float>(rot->get(1)->as_floating_point() ? rot->get(1)->as_floating_point()->get() : 0.0);
                    trans->rotation.z = static_cast<float>(rot->get(2)->as_floating_point() ? rot->get(2)->as_floating_point()->get() : 0.0);
                    trans->rotation.w = static_cast<float>(rot->get(3)->as_floating_point() ? rot->get(3)->as_floating_point()->get() : 1.0);
                }
                if (auto* sc = (*t_node)["scale"].as_array(); sc && sc->size() >= 3) {
                    trans->scale.x = static_cast<float>(sc->get(0)->as_floating_point() ? sc->get(0)->as_floating_point()->get() : 1.0);
                    trans->scale.y = static_cast<float>(sc->get(1)->as_floating_point() ? sc->get(1)->as_floating_point()->get() : 1.0);
                    trans->scale.z = static_cast<float>(sc->get(2)->as_floating_point() ? sc->get(2)->as_floating_point()->get() : 1.0);
                }
                trans->is_dirty = true;
            }

            // MeshRenderer
            if (auto* mr_node = (*ent_node)["mesh_renderer"].as_table()) {
                MeshRendererComponent mr{};
                if (auto* mu = (*mr_node)["mesh_uuid"].as_string()) mr.mesh_uuid = assets::UUID::from_string(mu->get());
                if (auto* s = (*mr_node)["submesh_index"].as_integer()) mr.submesh_index = static_cast<uint32_t>(s->get());
                if (auto* mat = (*mr_node)["material_uuid"].as_string()) mr.material_uuid = assets::UUID::from_string(mat->get());
                if (auto* cs = (*mr_node)["cast_shadows"].as_boolean()) mr.cast_shadows = cs->get();
                if (auto* rs = (*mr_node)["receive_shadows"].as_boolean()) mr.receive_shadows = rs->get();
                if (auto* iv = (*mr_node)["is_visible"].as_boolean()) mr.is_visible = iv->get();
                entity.set(mr);
            }

            // Directional Light
            if (auto* dl_node = (*ent_node)["directional_light"].as_table()) {
                DirectionalLightComponent dl{};
                if (auto* col = (*dl_node)["color"].as_array(); col && col->size() >= 3) {
                    dl.color.x = static_cast<float>(col->get(0)->as_floating_point() ? col->get(0)->as_floating_point()->get() : 1.0);
                    dl.color.y = static_cast<float>(col->get(1)->as_floating_point() ? col->get(1)->as_floating_point()->get() : 1.0);
                    dl.color.z = static_cast<float>(col->get(2)->as_floating_point() ? col->get(2)->as_floating_point()->get() : 1.0);
                }
                if (auto* in = (*dl_node)["intensity"].as_floating_point()) dl.intensity = static_cast<float>(in->get());
                if (auto* cs = (*dl_node)["cast_shadows"].as_boolean()) dl.cast_shadows = cs->get();
                if (auto* cc = (*dl_node)["cascade_count"].as_integer()) dl.cascade_count = static_cast<uint32_t>(cc->get());
                entity.set(dl);
            }

            // Point Light
            if (auto* pl_node = (*ent_node)["point_light"].as_table()) {
                PointLightComponent pl{};
                if (auto* col = (*pl_node)["color"].as_array(); col && col->size() >= 3) {
                    pl.color.x = static_cast<float>(col->get(0)->as_floating_point() ? col->get(0)->as_floating_point()->get() : 1.0);
                    pl.color.y = static_cast<float>(col->get(1)->as_floating_point() ? col->get(1)->as_floating_point()->get() : 1.0);
                    pl.color.z = static_cast<float>(col->get(2)->as_floating_point() ? col->get(2)->as_floating_point()->get() : 1.0);
                }
                if (auto* in = (*pl_node)["intensity"].as_floating_point()) pl.intensity = static_cast<float>(in->get());
                if (auto* r = (*pl_node)["radius"].as_floating_point()) pl.radius = static_cast<float>(r->get());
                if (auto* f = (*pl_node)["falloff"].as_floating_point()) pl.falloff = static_cast<float>(f->get());
                if (auto* cs = (*pl_node)["cast_shadows"].as_boolean()) pl.cast_shadows = cs->get();
                entity.set(pl);
            }

            // Spot Light
            if (auto* sl_node = (*ent_node)["spot_light"].as_table()) {
                SpotLightComponent sl{};
                if (auto* col = (*sl_node)["color"].as_array(); col && col->size() >= 3) {
                    sl.color.x = static_cast<float>(col->get(0)->as_floating_point() ? col->get(0)->as_floating_point()->get() : 1.0);
                    sl.color.y = static_cast<float>(col->get(1)->as_floating_point() ? col->get(1)->as_floating_point()->get() : 1.0);
                    sl.color.z = static_cast<float>(col->get(2)->as_floating_point() ? col->get(2)->as_floating_point()->get() : 1.0);
                }
                if (auto* in = (*sl_node)["intensity"].as_floating_point()) sl.intensity = static_cast<float>(in->get());
                if (auto* r = (*sl_node)["range"].as_floating_point()) sl.range = static_cast<float>(r->get());
                if (auto* ic = (*sl_node)["inner_cone_deg"].as_floating_point()) sl.inner_cone_angle_deg = static_cast<float>(ic->get());
                if (auto* oc = (*sl_node)["outer_cone_deg"].as_floating_point()) sl.outer_cone_angle_deg = static_cast<float>(oc->get());
                if (auto* cs = (*sl_node)["cast_shadows"].as_boolean()) sl.cast_shadows = cs->get();
                entity.set(sl);
            }

            // Camera
            if (auto* cam_node = (*ent_node)["camera"].as_table()) {
                CameraComponent cam{};
                if (auto* f = (*cam_node)["fov_deg"].as_floating_point()) cam.fov_deg = static_cast<float>(f->get());
                if (auto* nz = (*cam_node)["near_z"].as_floating_point()) cam.near_z = static_cast<float>(nz->get());
                if (auto* fz = (*cam_node)["far_z"].as_floating_point()) cam.far_z = static_cast<float>(fz->get());
                if (auto* o = (*cam_node)["is_orthographic"].as_boolean()) cam.is_orthographic = o->get();
                if (auto* os = (*cam_node)["ortho_size"].as_floating_point()) cam.ortho_size = static_cast<float>(os->get());
                if (auto* p = (*cam_node)["is_primary"].as_boolean()) cam.is_primary = p->get();
                entity.set(cam);
            }
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
