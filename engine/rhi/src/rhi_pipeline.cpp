#include "engine/rhi/rhi_pipeline.h"
#include "engine/rhi/rhi_context.h"
#include "engine/core/log.h"

namespace engine::rhi {

RhiShaderModule::~RhiShaderModule() {
    destroy();
}

bool RhiShaderModule::init_from_spirv(const void* code, size_t size_bytes, ShaderStage stage) {
    m_stage = stage;
    VkShaderModuleCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = size_bytes;
    info.pCode = reinterpret_cast<const uint32_t*>(code);

    VkResult res = vkCreateShaderModule(RhiContext::instance().get_device(), &info, nullptr, &m_module);
    if (res != VK_SUCCESS) {
        LOG_ERROR("RHI", "Failed to create shader module: {}", static_cast<int>(res));
        return false;
    }
    return true;
}

void RhiShaderModule::destroy() {
    if (m_module != VK_NULL_HANDLE) {
        vkDestroyShaderModule(RhiContext::instance().get_device(), m_module, nullptr);
        m_module = VK_NULL_HANDLE;
    }
}

RhiGraphicsPipeline::~RhiGraphicsPipeline() {
    destroy();
}

bool RhiGraphicsPipeline::init(const GraphicsPipelineDesc& desc) {
    VkDevice device = RhiContext::instance().get_device();

    std::vector<VkPipelineShaderStageCreateInfo> shader_stages;
    if (desc.vertex_shader && desc.vertex_shader->get_handle() != VK_NULL_HANDLE) {
        VkPipelineShaderStageCreateInfo stage_info{};
        stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
        stage_info.module = desc.vertex_shader->get_handle();
        stage_info.pName = "main";
        shader_stages.push_back(stage_info);
    }
    if (desc.fragment_shader && desc.fragment_shader->get_handle() != VK_NULL_HANDLE) {
        VkPipelineShaderStageCreateInfo stage_info{};
        stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stage_info.module = desc.fragment_shader->get_handle();
        stage_info.pName = "main";
        shader_stages.push_back(stage_info);
    }

    // Dynamic Viewport & Scissor
    std::vector<VkDynamicState> dynamic_states = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamic_state{};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
    dynamic_state.pDynamicStates = dynamic_states.data();

    // Vertex Input
    VkPipelineVertexInputStateCreateInfo vertex_input_info{};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.vertexBindingDescriptionCount = static_cast<uint32_t>(desc.vertex_bindings.size());
    vertex_input_info.pVertexBindingDescriptions = desc.vertex_bindings.empty() ? nullptr : desc.vertex_bindings.data();
    vertex_input_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(desc.vertex_attributes.size());
    vertex_input_info.pVertexAttributeDescriptions = desc.vertex_attributes.empty() ? nullptr : desc.vertex_attributes.data();

    // Input Assembly
    VkPipelineInputAssemblyStateCreateInfo input_assembly{};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = desc.topology;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    // Viewport State
    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;

    // Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = desc.polygon_mode;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = desc.cull_mode;
    rasterizer.frontFace = desc.front_face;
    rasterizer.depthBiasEnable = VK_FALSE;

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Depth & Stencil
    VkPipelineDepthStencilStateCreateInfo depth_stencil{};
    depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil.depthTestEnable = desc.depth_test_enable ? VK_TRUE : VK_FALSE;
    depth_stencil.depthWriteEnable = desc.depth_write_enable ? VK_TRUE : VK_FALSE;
    depth_stencil.depthCompareOp = desc.depth_compare_op;
    depth_stencil.depthBoundsTestEnable = VK_FALSE;
    depth_stencil.stencilTestEnable = VK_FALSE;

    // Color Blend
    std::vector<VkPipelineColorBlendAttachmentState> color_blend_attachments;
    for (size_t i = 0; i < desc.color_formats.size(); ++i) {
        VkPipelineColorBlendAttachmentState color_blend_attachment{};
        color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        color_blend_attachment.blendEnable = desc.blend_enable ? VK_TRUE : VK_FALSE;
        color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
        color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
        color_blend_attachments.push_back(color_blend_attachment);
    }

    VkPipelineColorBlendStateCreateInfo color_blending{};
    color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.logicOpEnable = VK_FALSE;
    color_blending.attachmentCount = static_cast<uint32_t>(color_blend_attachments.size());
    color_blending.pAttachments = color_blend_attachments.data();

    // Pipeline Layout
    if (desc.layout != VK_NULL_HANDLE) {
        m_layout = desc.layout;
        m_owns_layout = false;
    } else {
        VkPipelineLayoutCreateInfo pipeline_layout_info{};
        pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_info.setLayoutCount = static_cast<uint32_t>(desc.descriptor_set_layouts.size());
        pipeline_layout_info.pSetLayouts = desc.descriptor_set_layouts.empty() ? nullptr : desc.descriptor_set_layouts.data();
        pipeline_layout_info.pushConstantRangeCount = static_cast<uint32_t>(desc.push_constant_ranges.size());
        pipeline_layout_info.pPushConstantRanges = desc.push_constant_ranges.empty() ? nullptr : desc.push_constant_ranges.data();
        if (vkCreatePipelineLayout(device, &pipeline_layout_info, nullptr, &m_layout) != VK_SUCCESS) {
            LOG_FATAL("RHI", "Failed to create default pipeline layout");
            return false;
        }
        m_owns_layout = true;
    }

    // Dynamic Rendering Configuration (Vulkan 1.3)
    std::vector<VkFormat> vk_color_formats;
    for (auto fmt : desc.color_formats) {
        vk_color_formats.push_back(static_cast<VkFormat>(fmt));
    }

    VkPipelineRenderingCreateInfo rendering_create_info{};
    rendering_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    rendering_create_info.colorAttachmentCount = static_cast<uint32_t>(vk_color_formats.size());
    rendering_create_info.pColorAttachmentFormats = vk_color_formats.data();
    rendering_create_info.depthAttachmentFormat = static_cast<VkFormat>(desc.depth_format);

    VkGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.pNext = &rendering_create_info;
    pipeline_info.stageCount = static_cast<uint32_t>(shader_stages.size());
    pipeline_info.pStages = shader_stages.data();
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pDepthStencilState = &depth_stencil;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.pDynamicState = &dynamic_state;
    pipeline_info.layout = m_layout;
    pipeline_info.renderPass = VK_NULL_HANDLE; // Dynamic rendering!

    VkResult res = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &m_pipeline);
    if (res != VK_SUCCESS) {
        LOG_FATAL("RHI", "Failed to create graphics pipeline: {}", static_cast<int>(res));
        return false;
    }

    LOG_INFO("RHI", "Created Vulkan 1.3 Dynamic Rendering Graphics Pipeline");
    return true;
}

void RhiGraphicsPipeline::destroy() {
    VkDevice device = RhiContext::instance().get_device();
    if (device == VK_NULL_HANDLE) return;

    if (m_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_pipeline, nullptr);
        m_pipeline = VK_NULL_HANDLE;
    }
    if (m_owns_layout && m_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, m_layout, nullptr);
        m_layout = VK_NULL_HANDLE;
    }
}

} // namespace engine::rhi
