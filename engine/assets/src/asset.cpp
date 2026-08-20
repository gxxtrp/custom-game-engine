#include "engine/assets/asset.h"

namespace engine::assets {

const char* asset_type_to_string(AssetType type) {
    switch (type) {
        case AssetType::Mesh:     return "mesh";
        case AssetType::Texture:  return "texture";
        case AssetType::Material: return "material";
        case AssetType::Shader:   return "shader";
        case AssetType::Scene:    return "scene";
        case AssetType::Prefab:   return "prefab";
        case AssetType::Audio:    return "audio";
        case AssetType::Raw:      return "raw";
        default:                  return "unknown";
    }
}

AssetType string_to_asset_type(std::string_view str) {
    if (str == "mesh") return AssetType::Mesh;
    if (str == "texture") return AssetType::Texture;
    if (str == "material") return AssetType::Material;
    if (str == "shader") return AssetType::Shader;
    if (str == "scene") return AssetType::Scene;
    if (str == "prefab") return AssetType::Prefab;
    if (str == "audio") return AssetType::Audio;
    if (str == "raw") return AssetType::Raw;
    return AssetType::Unknown;
}

} // namespace engine::assets
