#include "editor/assets/asset_importer.h"
#include "engine/vfs/vfs.h"
#include "engine/assets/asset_manager.h"
#include "engine/core/platform.h"
#include "engine/core/log.h"
#include <filesystem>
#include <fstream>

namespace editor {

AssetCategory EditorAssetImporter::categorize_file(std::string_view filename) {
    std::filesystem::path p(filename);
    std::string ext = p.extension().string();
    for (char& c : ext) c = static_cast<char>(std::tolower(c));

    if (ext == ".glb" || ext == ".gltf" || ext == ".obj" || ext == ".fbx") return AssetCategory::Model;
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".hdr" || ext == ".tga" || ext == ".dds") return AssetCategory::Texture;
    if (ext == ".wav" || ext == ".mp3" || ext == ".ogg" || ext == ".flac") return AssetCategory::Audio;
    if (ext == ".mat") return AssetCategory::Material;
    if (ext == ".lua") return AssetCategory::Script;
    if (ext == ".map") return AssetCategory::Map;
    if (ext == ".prefab") return AssetCategory::Prefab;

    return AssetCategory::Unknown;
}

std::string EditorAssetImporter::get_default_directory_for_category(AssetCategory category) {
    switch (category) {
        case AssetCategory::Model: return "/assets/models";
        case AssetCategory::Texture: return "/assets/textures";
        case AssetCategory::Audio: return "/assets/audio";
        case AssetCategory::Material: return "/assets/materials";
        case AssetCategory::Script: return "/scripts";
        case AssetCategory::Map: return "/maps";
        case AssetCategory::Prefab: return "/assets/prefabs";
        default: return "/assets";
    }
}

bool EditorAssetImporter::import_physical_file(const std::string& source_path, 
                                               const std::string& target_virtual_directory,
                                               std::string* out_imported_virtual_path) {
    std::filesystem::path src(source_path);
    if (!std::filesystem::exists(src) || std::filesystem::is_directory(src)) {
        LOG_ERROR("AssetImporter", "Source file does not exist: {}", source_path);
        return false;
    }

    std::string filename = src.filename().string();
    AssetCategory category = categorize_file(filename);

    std::string target_dir = target_virtual_directory;
    if (target_dir.empty()) {
        target_dir = get_default_directory_for_category(category);
    }

    if (!target_dir.starts_with("/")) {
        target_dir = "/" + target_dir;
    }
    if (target_dir.ends_with("/")) {
        target_dir.pop_back();
    }

    std::string dest_virtual_path = target_dir + "/" + filename;

    // Read binary source file
    std::ifstream file(src, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        LOG_ERROR("AssetImporter", "Failed to open physical file: {}", source_path);
        return false;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        LOG_ERROR("AssetImporter", "Failed to read physical file bytes: {}", source_path);
        return false;
    }

    // Write to VFS
    if (!engine::vfs::VFS::instance().write_bytes(dest_virtual_path, buffer.data(), buffer.size())) {
        LOG_ERROR("AssetImporter", "Failed to write imported asset to VFS: {}", dest_virtual_path);
        return false;
    }

    // Create & Save metadata
    engine::assets::AssetType engine_asset_type = engine::assets::AssetType::Unknown;
    switch (category) {
        case AssetCategory::Model: engine_asset_type = engine::assets::AssetType::Mesh; break;
        case AssetCategory::Texture: engine_asset_type = engine::assets::AssetType::Texture; break;
        case AssetCategory::Audio: engine_asset_type = engine::assets::AssetType::Audio; break;
        case AssetCategory::Material: engine_asset_type = engine::assets::AssetType::Material; break;
        case AssetCategory::Script: engine_asset_type = engine::assets::AssetType::Raw; break;
        case AssetCategory::Map: engine_asset_type = engine::assets::AssetType::Scene; break;
        case AssetCategory::Prefab: engine_asset_type = engine::assets::AssetType::Prefab; break;
        default: engine_asset_type = engine::assets::AssetType::Unknown; break;
    }

    engine::assets::AssetMeta meta = engine::assets::AssetManager::instance().create_or_get_meta(dest_virtual_path, engine_asset_type);
    engine::assets::AssetManager::instance().save_meta_file(meta);

    LOG_INFO("AssetImporter", "Successfully imported '{}' -> '{}' (UUID: {}, Size: {} bytes)",
             filename, dest_virtual_path, meta.uuid.to_string(), buffer.size());

    if (out_imported_virtual_path) {
        *out_imported_virtual_path = dest_virtual_path;
    }

    return true;
}

bool EditorAssetImporter::rename_asset(std::string_view old_virtual_path, std::string_view new_virtual_path) {
    auto& vfs = engine::vfs::VFS::instance();
    auto& am = engine::assets::AssetManager::instance();

    std::vector<uint8_t> data;
    if (!vfs.read_bytes(old_virtual_path, data)) {
        LOG_ERROR("AssetImporter", "Cannot rename non-existent asset: {}", old_virtual_path);
        return false;
    }

    engine::assets::UUID uuid = am.get_uuid_for_path(old_virtual_path);
    if (!uuid.is_valid()) {
        uuid = engine::assets::UUID::generate();
    }

    // Write new path and remove old physical file
    if (!vfs.write_bytes(new_virtual_path, data.data(), data.size())) {
        LOG_ERROR("AssetImporter", "Failed to write renamed asset to: {}", new_virtual_path);
        return false;
    }

    engine::assets::UUIDRegistry::instance().register_mapping(uuid, new_virtual_path);
    
    // Save new metadata
    engine::assets::AssetMeta meta;
    meta.uuid = uuid;
    meta.virtual_path = new_virtual_path;
    meta.type = categorize_file(new_virtual_path) == AssetCategory::Model ? engine::assets::AssetType::Mesh : engine::assets::AssetType::Texture;
    am.save_meta_file(meta);

    // Delete old physical file if resolved
    std::string old_physical = vfs.resolve_physical_path(old_virtual_path);
    if (!old_physical.empty() && std::filesystem::exists(old_physical)) {
        std::error_code ec;
        std::filesystem::remove(old_physical, ec);
        std::filesystem::remove(old_physical + ".meta", ec);
    }

    LOG_INFO("AssetImporter", "Renamed asset '{}' -> '{}' (Preserved UUID: {})", 
             old_virtual_path, new_virtual_path, uuid.to_string());
    return true;
}

bool EditorAssetImporter::delete_asset(std::string_view virtual_path) {
    auto& vfs = engine::vfs::VFS::instance();
    auto& am = engine::assets::AssetManager::instance();

    engine::assets::UUID uuid = am.get_uuid_for_path(virtual_path);
    if (uuid.is_valid()) {
        engine::assets::UUIDRegistry::instance().unregister_mapping(uuid);
    }

    std::string physical = vfs.resolve_physical_path(virtual_path);
    if (!physical.empty() && std::filesystem::exists(physical)) {
        std::error_code ec;
        std::filesystem::remove(physical, ec);
        std::filesystem::remove(physical + ".meta", ec);
        LOG_INFO("AssetImporter", "Deleted asset file and metadata: {}", virtual_path);
        return true;
    }

    return false;
}

} // namespace editor
