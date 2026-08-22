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

#include "../../shaders/depth_prepass_vert.spv"
inline const size_t DEPTH_PREPASS_VERT_SPV_SIZE = sizeof(DEPTH_PREPASS_VERT_SPV);

#include "../../shaders/depth_prepass_frag.spv"
inline const size_t DEPTH_PREPASS_FRAG_SPV_SIZE = sizeof(DEPTH_PREPASS_FRAG_SPV);

#include "../../shaders/wboit_vert.spv"
inline const size_t WBOIT_VERT_SPV_SIZE = sizeof(WBOIT_VERT_SPV);

#include "../../shaders/wboit_accum_frag.spv"
inline const size_t WBOIT_ACCUM_FRAG_SPV_SIZE = sizeof(WBOIT_ACCUM_FRAG_SPV);

#include "../../shaders/auto_exposure_comp.spv"
inline const size_t AUTO_EXPOSURE_COMP_SPV_SIZE = sizeof(AUTO_EXPOSURE_COMP_SPV);

#include "../../shaders/taa_resolve_frag.spv"
inline const size_t TAA_RESOLVE_FRAG_SPV_SIZE = sizeof(TAA_RESOLVE_FRAG_SPV);

#include "../../shaders/oit_resolve_frag.spv"
inline const size_t OIT_RESOLVE_FRAG_SPV_SIZE = sizeof(OIT_RESOLVE_FRAG_SPV);

#include "../../shaders/bloom_prefilter_frag.spv"
inline const size_t BLOOM_PREFILTER_FRAG_SPV_SIZE = sizeof(BLOOM_PREFILTER_FRAG_SPV);

#include "../../shaders/bloom_downsample_frag.spv"
inline const size_t BLOOM_DOWNSAMPLE_FRAG_SPV_SIZE = sizeof(BLOOM_DOWNSAMPLE_FRAG_SPV);

#include "../../shaders/bloom_upsample_frag.spv"
inline const size_t BLOOM_UPSAMPLE_FRAG_SPV_SIZE = sizeof(BLOOM_UPSAMPLE_FRAG_SPV);

#include "../../shaders/volumetric_fog_frag.spv"
inline const size_t VOLUMETRIC_FOG_FRAG_SPV_SIZE = sizeof(VOLUMETRIC_FOG_FRAG_SPV);

#include "../../shaders/composite_frag.spv"
inline const size_t COMPOSITE_FRAG_SPV_SIZE = sizeof(COMPOSITE_FRAG_SPV);

} // namespace engine::shaders
