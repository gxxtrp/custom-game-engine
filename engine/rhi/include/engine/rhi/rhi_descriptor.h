#pragma once

#include "engine/core/config.h"
#include <vulkan/vulkan.h>
#include <vector>

namespace engine::rhi {

// ==========================================
// Descriptor Set Abstractions
// ==========================================

struct DescriptorBinding {
    uint32_t binding{0};
    VkDescriptorType type{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER};
    uint32_t descriptor_count{1};
    VkShaderStageFlags stage_flags{VK_SHADER_STAGE_ALL};
};

class RhiDescriptorSetLayout {
public:
    RhiDescriptorSetLayout() = default;
    ~RhiDescriptorSetLayout();

    bool init(const std::vector<DescriptorBinding>& bindings);
    void destroy();

    VkDescriptorSetLayout get_handle() const { return m_layout; }
    bool is_valid() const { return m_layout != VK_NULL_HANDLE; }

private:
    VkDescriptorSetLayout m_layout{VK_NULL_HANDLE};
};

class RhiDescriptorPool {
public:
    RhiDescriptorPool() = default;
    ~RhiDescriptorPool();

    // pool_sizes: count per descriptor type the pool must support
    bool init(uint32_t max_sets, const std::vector<std::pair<VkDescriptorType, uint32_t>>& pool_sizes);
    void destroy();

    VkDescriptorPool get_handle() const { return m_pool; }
    bool is_valid() const { return m_pool != VK_NULL_HANDLE; }

    // Allocates one descriptor set from the pool against the given layout.
    // Returns VK_NULL_HANDLE on failure.
    VkDescriptorSet allocate_set(VkDescriptorSetLayout layout);

private:
    VkDescriptorPool m_pool{VK_NULL_HANDLE};
};

// RAII wrapper around a single allocated descriptor set.
class RhiDescriptorSet {
public:
    RhiDescriptorSet() = default;
    ~RhiDescriptorSet();

    bool init(RhiDescriptorPool& pool, VkDescriptorSetLayout layout);
    void destroy(RhiDescriptorPool& pool);

    // Binds a combined image sampler (texture + sampler) at `binding`.
    void update_combined_image_sampler(uint32_t binding, VkImageView image_view, VkSampler sampler, VkImageLayout image_layout);
    // Binds a uniform buffer at `binding`.
    void update_uniform_buffer(uint32_t binding, VkBuffer buffer, VkDeviceSize range);
    // Binds a storage buffer at `binding`.
    void update_storage_buffer(uint32_t binding, VkBuffer buffer, VkDeviceSize range);
    // Binds a storage image at `binding`.
    void update_storage_image(uint32_t binding, VkImageView image_view, VkImageLayout image_layout);
    // Binds a sampled image (no sampler; Texture2D::Load style access) at `binding`.
    void update_sampled_image(uint32_t binding, VkImageView image_view, VkImageLayout image_layout);

    VkDescriptorSet get_handle() const { return m_set; }
    bool is_valid() const { return m_set != VK_NULL_HANDLE; }

private:
    VkDescriptorSet m_set{VK_NULL_HANDLE};
};

} // namespace engine::rhi
