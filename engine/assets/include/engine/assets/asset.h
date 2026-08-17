#pragma once

#include "engine/core/config.h"
#include "engine/assets/uuid.h"
#include <string>
#include <string_view>
#include <memory>
#include <atomic>

namespace engine::assets {

enum class AssetType : uint8_t {
    Unknown = 0,
    Mesh,
    Texture,
    Material,
    Shader,
    Scene,
    Prefab,
    Audio,
    Raw
};

const char* asset_type_to_string(AssetType type);
AssetType string_to_asset_type(std::string_view str);

struct AssetMeta {
    UUID uuid;
    AssetType type{AssetType::Unknown};
    std::string virtual_path;
    std::string source_file;
    uint64_t imported_timestamp{0};
};

enum class AssetStatus : uint8_t {
    Unloaded = 0,
    Loading,
    Ready,
    Failed
};

class IAsset {
public:
    explicit IAsset(UUID uuid, AssetType type) : m_uuid(uuid), m_type(type) {}
    virtual ~IAsset() = default;

    UUID get_uuid() const { return m_uuid; }
    AssetType get_type() const { return m_type; }

protected:
    UUID m_uuid;
    AssetType m_type{AssetType::Unknown};
};

template<typename T>
class AssetHandle {
public:
    AssetHandle() = default;
    AssetHandle(UUID uuid, std::shared_ptr<T> asset, AssetStatus status = AssetStatus::Ready)
        : m_uuid(uuid), m_asset(std::move(asset)), m_status(status) {}

    UUID get_uuid() const { return m_uuid; }
    AssetStatus get_status() const { return m_status; }
    bool is_ready() const { return m_status == AssetStatus::Ready && m_asset != nullptr; }

    T* get() const { return m_asset.get(); }
    T* operator->() const { return m_asset.get(); }
    T& operator*() const { return *m_asset; }
    explicit operator bool() const { return is_ready(); }

    bool operator==(const AssetHandle<T>& other) const { return m_uuid == other.m_uuid; }
    bool operator!=(const AssetHandle<T>& other) const { return !(*this == other); }

private:
    UUID m_uuid{};
    std::shared_ptr<T> m_asset{nullptr};
    AssetStatus m_status{AssetStatus::Unloaded};
};

} // namespace engine::assets
