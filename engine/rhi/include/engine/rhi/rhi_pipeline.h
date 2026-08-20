#pragma once

#include "engine/core/config.h"
#include "engine/rhi/rhi_types.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>

namespace engine::rhi {

class RhiShaderModule {
public:
    RhiShaderModule() = default;
    ~RhiShaderModule();

    bool init_from_spirv(const uint32_t* code, size_t size_bytes, ShaderStage stage);
    void destroy();

    VkShaderModule get_handle() const { return m_module; }
    ShaderStage get_stage() const { return m_stage; }

private:
    VkShaderModule m_module{VK_NULL_HANDLE};
    ShaderStage m_stage{ShaderStage::Vertex};
};

struct GraphicsPipelineDesc {
    RhiShaderModule* vertex_shader{nullptr};
    RhiShaderModule* fragment_shader{nullptr};

    std::vector<Format> color_formats;
    Format depth_format{Format::Undefined};

    std::vector<VkVertexInputBindingDescription> vertex_bindings;
    std::vector<VkVertexInputAttributeDescription> vertex_attributes;
    std::vector<VkDescriptorSetLayout> descriptor_set_layouts;
    std::vector<VkPushConstantRange> push_constant_ranges;

    VkPrimitiveTopology topology{VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
    VkPolygonMode polygon_mode{VK_POLYGON_MODE_FILL};
    VkCullModeFlags cull_mode{VK_CULL_MODE_NONE};
    VkFrontFace front_face{VK_FRONT_FACE_COUNTER_CLOCKWISE};

    bool depth_test_enable{false};
    bool depth_write_enable{false};
    VkCompareOp depth_compare_op{VK_COMPARE_OP_LESS_OR_EQUAL};

    bool blend_enable{false};
    VkPipelineLayout layout{VK_NULL_HANDLE};
};

class RhiGraphicsPipeline {
public:
    RhiGraphicsPipeline() = default;
    ~RhiGraphicsPipeline();

    bool init(const GraphicsPipelineDesc& desc);
    void destroy();

    VkPipeline get_pipeline() const { return m_pipeline; }
    VkPipelineLayout get_layout() const { return m_layout; }

private:
    VkPipeline m_pipeline{VK_NULL_HANDLE};
    VkPipelineLayout m_layout{VK_NULL_HANDLE};
    bool m_owns_layout{false};
};

} // namespace engine::rhi
