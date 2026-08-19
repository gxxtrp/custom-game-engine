#pragma once

#include <cstdint>
#include <cstddef>

namespace engine::shaders {

#include "../shaders/mesh_vert.spv"
inline const size_t MESH_VERT_SPV_SIZE = sizeof(MESH_VERT_SPV);

#include "../shaders/mesh_frag.spv"
inline const size_t MESH_FRAG_SPV_SIZE = sizeof(MESH_FRAG_SPV);

inline const uint32_t TRIANGLE_VERT_SPV[] = 
#include "../../../../../triangle_vert.spv"
;

inline const size_t TRIANGLE_VERT_SPV_SIZE = sizeof(TRIANGLE_VERT_SPV);

inline const uint32_t TRIANGLE_FRAG_SPV[] = 
#include "../../../../../triangle_frag.spv"
;

inline const size_t TRIANGLE_FRAG_SPV_SIZE = sizeof(TRIANGLE_FRAG_SPV);

} // namespace engine::shaders
