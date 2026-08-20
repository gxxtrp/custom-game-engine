#pragma once

#include "engine/core/config.h"
#include "engine/rhi/rhi_types.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <mutex>

namespace engine::rhi {

class BindlessHeap {
public:
    static constexpr uint32_t MAX_SAMPLED_IMAGES = 16384;
    static constexpr uint32_t MAX_STORAGE_BUFFERS = 16384;
    static constexpr uint32_t MAX_SAMPLERS = 256;

    static constexpr uint32_t BINDING_SAMPLED_IMAGES = 0;
    static constexpr uint32_t BINDING_STORAGE_BUFFERS = 1;
    static constexpr uint32_t BINDING_SAMPLERS = 2;

    static BindlessHeap& instance();

    bool init();
    void shutdown();

    uint32_t register_texture(VkImageView view, VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    void unregister_texture(uint32_t slot);

    uint32_t register_storage_buffer(VkBuffer buffer, VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    void unregister_storage_buffer(uint32_t slot);

    uint32_t register_sampler(VkSampler sampler);
    void unregister_sampler(uint32_t slot);

    void bind(VkCommandBuffer cmd, VkPipelineLayout layout, VkPipelineBindPoint bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS, uint32_t set_index = 0);

    VkDescriptorSetLayout get_layout() const { return m_descriptor_set_layout; }
    VkDescriptorSet get_descriptor_set() const { return m_descriptor_set; }

private:
    BindlessHeap() = default;
    ~BindlessHeap();

    VkDescriptorPool m_descriptor_pool{VK_NULL_HANDLE};
    VkDescriptorSetLayout m_descriptor_set_layout{VK_NULL_HANDLE};
    VkDescriptorSet m_descriptor_set{VK_NULL_HANDLE};

    std::mutex m_mutex;
    std::vector<uint32_t> m_free_texture_slots;
    std::vector<uint32_t> m_free_buffer_slots;
    std::vector<uint32_t> m_free_sampler_slots;

    uint32_t m_next_texture_slot{0};
    uint32_t m_next_buffer_slot{0};
    uint32_t m_next_sampler_slot{0};
    bool m_initialized{false};
};

} // namespace engine::rhi
