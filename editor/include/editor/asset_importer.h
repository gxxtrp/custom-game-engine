#pragma once

#include "engine/assets/uuid.h"
#include "engine/assets/asset.h"
#include <string>
#include <string_view>
#include <vector>

namespace editor {

enum class AssetCategory {
    Model,
    Texture,
    Audio,
    Material,
    Script,
    Map,
    Prefab,
    Unknown
};

class EditorAssetImporter {
public:
    static AssetCategory categorize_file(std::string_view filename);
    static std::string get_default_directory_for_category(AssetCategory category);

    static bool import_physical_file(const std::string& source_path, 
                                     const std::string& target_virtual_directory = "",
                                     std::string* out_imported_virtual_path = nullptr);

    static bool rename_asset(std::string_view old_virtual_path, std::string_view new_virtual_path);
    static bool delete_asset(std::string_view virtual_path);
};

} // namespace editor
