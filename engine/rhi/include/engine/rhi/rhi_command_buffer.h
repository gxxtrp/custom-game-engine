#pragma once

#include "engine/core/config.h"
#include "engine/rhi/rhi_types.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>

namespace engine::rhi {

class RhiCommandPool {
public:
    RhiCommandPool() = default;
    ~RhiCommandPool();

    bool init(uint32_t queue_family_index, VkCommandPoolCreateFlags flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    void destroy();

    VkCommandPool get_handle() const { return m_pool; }

private:
    VkCommandPool m_pool{VK_NULL_HANDLE};
};

class RhiCommandBuffer {
public:
    RhiCommandBuffer() = default;
    ~RhiCommandBuffer();

    bool init(VkCommandPool pool, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    void destroy(VkCommandPool pool);

    bool begin(VkCommandBufferUsageFlags flags = 0);
    bool end();
    void reset();

    // Vulkan 1.3 Dynamic Rendering
    void begin_rendering(const RenderingDesc& desc);
    void end_rendering();

    void transition_image_layout(VkImage image, 
                                 VkImageLayout old_layout, 
                                 VkImageLayout new_layout, 
                                 VkImageAspectFlags aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT);

    void set_viewport(const Viewport& vp);
    void set_scissor(const Rect2D& sc);

    void bind_pipeline(VkPipeline pipeline, VkPipelineBindPoint bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS);
    void bind_vertex_buffer(uint32_t binding, VkBuffer buffer, VkDeviceSize offset = 0);
    void bind_index_buffer(VkBuffer buffer, VkDeviceSize offset = 0, VkIndexType index_type = VK_INDEX_TYPE_UINT32);

    void draw(uint32_t vertex_count, uint32_t instance_count = 1, uint32_t first_vertex = 0, uint32_t first_instance = 0);
    void draw_indexed(uint32_t index_count, uint32_t instance_count = 1, uint32_t first_index = 0, int32_t vertex_offset = 0, uint32_t first_instance = 0);
    void push_constants(VkPipelineLayout layout, VkShaderStageFlags stages, uint32_t offset, uint32_t size, const void* values);

    VkCommandBuffer get_handle() const { return m_cmd; }

private:
    VkCommandBuffer m_cmd{VK_NULL_HANDLE};
    bool m_is_recording{false};
    bool m_is_rendering{false};
};

} // namespace engine::rhi
