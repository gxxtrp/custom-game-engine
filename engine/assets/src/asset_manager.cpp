#include "engine/assets/asset_manager.h"
#include "engine/core/log.h"
#include "engine/core/platform.h"
#include <toml++/toml.hpp>
#include <sstream>

namespace engine::assets {

UUIDRegistry& UUIDRegistry::instance() {
    static UUIDRegistry s_instance;
    return s_instance;
}

void UUIDRegistry::register_mapping(UUID uuid, std::string_view virtual_path) {
    if (!uuid.is_valid()) return;
    std::string norm_path = vfs::VFS::normalize_path(virtual_path);

    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_uuid_to_path[uuid] = norm_path;
    m_path_to_uuid[norm_path] = uuid;
}

void UUIDRegistry::unregister_mapping(UUID uuid) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    auto it = m_uuid_to_path.find(uuid);
    if (it != m_uuid_to_path.end()) {
        m_path_to_uuid.erase(it->second);
        m_uuid_to_path.erase(it);
    }
}

UUID UUIDRegistry::find_uuid_by_path(std::string_view virtual_path) const {
    std::string norm = vfs::VFS::normalize_path(virtual_path);
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    auto it = m_path_to_uuid.find(norm);
    if (it != m_path_to_uuid.end()) return it->second;
    return UUID{};
}

std::string UUIDRegistry::find_path_by_uuid(UUID uuid) const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    auto it = m_uuid_to_path.find(uuid);
    if (it != m_uuid_to_path.end()) return it->second;
    return "";
}

bool UUIDRegistry::load_from_toml(std::string_view toml_content) {
    try {
        auto tbl = toml::parse(toml_content);
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_uuid_to_path.clear();
        m_path_to_uuid.clear();

        for (auto&& [uuid_str, path_node] : tbl) {
            if (path_node.is_string()) {
                UUID u = UUID::from_string(uuid_str.str());
                if (u.is_valid()) {
                    std::string p = path_node.as_string()->get();
                    m_uuid_to_path[u] = p;
                    m_path_to_uuid[p] = u;
                }
            }
        }
        return true;
    } catch (const toml::parse_error& err) {
        LOG_ERROR("Assets", "Failed to parse UUID registry: {}", err.description());
        return false;
    }
}

std::string UUIDRegistry::save_to_toml() const {
    toml::table tbl;
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        for (const auto& [uuid, path] : m_uuid_to_path) {
            tbl.insert_or_assign(uuid.to_string(), path);
        }
    }
    std::stringstream ss;
    ss << tbl;
    return ss.str();
}

void UUIDRegistry::clear() {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_uuid_to_path.clear();
    m_path_to_uuid.clear();
}

AssetManager& AssetManager::instance() {
    static AssetManager s_instance;
    return s_instance;
}

bool AssetManager::init() {
    LOG_INFO("Assets", "AssetManager initialized");
    return true;
}

void AssetManager::shutdown() {
    UUIDRegistry::instance().clear();
    LOG_INFO("Assets", "AssetManager shutdown cleanly");
}

bool AssetManager::save_meta_file(const AssetMeta& meta) {
    if (!meta.uuid.is_valid()) return false;

    toml::table root;
    toml::table asset_tbl;
    asset_tbl.insert("uuid", meta.uuid.to_string());
    asset_tbl.insert("type", asset_type_to_string(meta.type));
    asset_tbl.insert("source", meta.source_file);
    asset_tbl.insert("imported_timestamp", static_cast<int64_t>(meta.imported_timestamp));

    root.insert("asset", asset_tbl);

    std::stringstream ss;
    ss << root;

    std::string meta_path = meta.virtual_path + ".meta";
    return vfs::VFS::instance().write_string(meta_path, ss.str());
}

bool AssetManager::load_meta_file(std::string_view virtual_path_with_meta, AssetMeta& out_meta) {
    std::string content;
    if (!vfs::VFS::instance().read_string(virtual_path_with_meta, content)) {
        return false;
    }

    try {
        auto tbl = toml::parse(content);
        if (auto* asset = tbl["asset"].as_table()) {
            if (auto* u = (*asset)["uuid"].as_string()) {
                out_meta.uuid = UUID::from_string(u->get());
            }
            if (auto* t = (*asset)["type"].as_string()) {
                out_meta.type = string_to_asset_type(t->get());
            }
            if (auto* s = (*asset)["source"].as_string()) {
                out_meta.source_file = s->get();
            }
            if (auto* ts = (*asset)["imported_timestamp"].as_integer()) {
                out_meta.imported_timestamp = static_cast<uint64_t>(ts->get());
            }

            // Path is the meta path without the .meta suffix
            std::string meta_p = vfs::VFS::normalize_path(virtual_path_with_meta);
            if (meta_p.ends_with(".meta")) {
                out_meta.virtual_path = meta_p.substr(0, meta_p.length() - 5);
            } else {
                out_meta.virtual_path = meta_p;
            }

            UUIDRegistry::instance().register_mapping(out_meta.uuid, out_meta.virtual_path);
            return true;
        }
    } catch (const toml::parse_error& err) {
        LOG_ERROR("Assets", "Error parsing .meta file '{}': {}", virtual_path_with_meta, err.description());
    }

    return false;
}

AssetMeta AssetManager::create_or_get_meta(std::string_view virtual_path, AssetType type) {
    std::string meta_file = std::string(virtual_path) + ".meta";
    AssetMeta meta{};

    if (load_meta_file(meta_file, meta)) {
        return meta;
    }

    // Create new meta
    meta.uuid = UUID::generate();
    meta.type = type;
    meta.virtual_path = vfs::VFS::normalize_path(virtual_path);
    meta.source_file = "";
    meta.imported_timestamp = core::Clock::get_time_nanoseconds();

    save_meta_file(meta);
    UUIDRegistry::instance().register_mapping(meta.uuid, meta.virtual_path);

    LOG_INFO("Assets", "Created new asset metadata for '{}' (UUID: {})", meta.virtual_path, meta.uuid.to_string());
    return meta;
}

} // namespace engine::assets
