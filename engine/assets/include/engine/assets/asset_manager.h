#pragma once

#include "engine/core/config.h"
#include "engine/assets/uuid.h"
#include "engine/assets/asset.h"
#include "engine/vfs/vfs.h"
#include <unordered_map>
#include <shared_mutex>

namespace engine::assets {

class UUIDRegistry {
public:
    static UUIDRegistry& instance();

    void register_mapping(UUID uuid, std::string_view virtual_path);
    void unregister_mapping(UUID uuid);

    UUID find_uuid_by_path(std::string_view virtual_path) const;
    std::string find_path_by_uuid(UUID uuid) const;

    bool load_from_toml(std::string_view toml_content);
    std::string save_to_toml() const;

    void clear();

private:
    UUIDRegistry() = default;

    mutable std::shared_mutex m_mutex;
    std::unordered_map<UUID, std::string> m_uuid_to_path;
    std::unordered_map<std::string, UUID> m_path_to_uuid;
};

class AssetManager {
public:
    static AssetManager& instance();

    bool init();
    void shutdown();

    bool save_meta_file(const AssetMeta& meta);
    bool load_meta_file(std::string_view virtual_path_with_meta, AssetMeta& out_meta);

    AssetMeta create_or_get_meta(std::string_view virtual_path, AssetType type);

    std::string get_path_for_uuid(UUID uuid) const {
        return UUIDRegistry::instance().find_path_by_uuid(uuid);
    }

    UUID get_uuid_for_path(std::string_view virtual_path) const {
        return UUIDRegistry::instance().find_uuid_by_path(virtual_path);
    }

private:
    AssetManager() = default;
};

} // namespace engine::assets
