#pragma once

#include <cstdint>
#include <cstddef>

namespace engine::shaders {

#include "../../shaders/triangle_vert.spv"
inline const size_t TRIANGLE_VERT_SPV_SIZE = sizeof(TRIANGLE_VERT_SPV);

#include "../../shaders/triangle_frag.spv"
inline const size_t TRIANGLE_FRAG_SPV_SIZE = sizeof(TRIANGLE_FRAG_SPV);

#include "../../shaders/shadow_vert.spv"
inline const size_t SHADOW_VERT_SPV_SIZE = sizeof(SHADOW_VERT_SPV);

#include "../../shaders/shadow_frag.spv"
inline const size_t SHADOW_FRAG_SPV_SIZE = sizeof(SHADOW_FRAG_SPV);

#include "../../shaders/mesh_vert.spv"
inline const size_t MESH_VERT_SPV_SIZE = sizeof(MESH_VERT_SPV);

#include "../../shaders/mesh_frag.spv"
inline const size_t MESH_FRAG_SPV_SIZE = sizeof(MESH_FRAG_SPV);

#include "../../shaders/tonemap_vert.spv"
inline const size_t TONEMAP_VERT_SPV_SIZE = sizeof(TONEMAP_VERT_SPV);

#include "../../shaders/tonemap_frag.spv"
inline const size_t TONEMAP_FRAG_SPV_SIZE = sizeof(TONEMAP_FRAG_SPV);

} // namespace engine::shaders
