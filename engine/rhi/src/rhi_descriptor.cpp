#include "engine/rhi/rhi_descriptor.h"
#include "engine/rhi/rhi_context.h"
#include "engine/core/log.h"

namespace engine::rhi {

RhiDescriptorSetLayout::~RhiDescriptorSetLayout() {
    destroy();
}

bool RhiDescriptorSetLayout::init(const std::vector<DescriptorBinding>& bindings) {
    destroy();

    std::vector<VkDescriptorSetLayoutBinding> vk_bindings;
    vk_bindings.reserve(bindings.size());
    for (const auto& b : bindings) {
        VkDescriptorSetLayoutBinding vk_b{};
        vk_b.binding = b.binding;
        vk_b.descriptorType = b.type;
        vk_b.descriptorCount = b.descriptor_count;
        vk_b.stageFlags = b.stage_flags;
        vk_b.pImmutableSamplers = nullptr;
        vk_bindings.push_back(vk_b);
    }

    VkDescriptorSetLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = static_cast<uint32_t>(vk_bindings.size());
    info.pBindings = vk_bindings.empty() ? nullptr : vk_bindings.data();

    VkDevice device = RhiContext::instance().get_device();
    VkResult res = vkCreateDescriptorSetLayout(device, &info, nullptr, &m_layout);
    if (res != VK_SUCCESS) {
        LOG_FATAL("RHI", "Failed to create descriptor set layout: {}", static_cast<int>(res));
        return false;
    }
    return true;
}

void RhiDescriptorSetLayout::destroy() {
    if (m_layout != VK_NULL_HANDLE) {
        VkDevice device = RhiContext::instance().get_device();
        if (device != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device, m_layout, nullptr);
        }
        m_layout = VK_NULL_HANDLE;
    }
}

RhiDescriptorPool::~RhiDescriptorPool() {
    destroy();
}

bool RhiDescriptorPool::init(uint32_t max_sets, const std::vector<std::pair<VkDescriptorType, uint32_t>>& pool_sizes) {
    destroy();

    std::vector<VkDescriptorPoolSize> sizes;
    sizes.reserve(pool_sizes.size());
    for (const auto& [type, count] : pool_sizes) {
        VkDescriptorPoolSize s{};
        s.type = type;
        s.descriptorCount = count;
        sizes.push_back(s);
    }

    VkDescriptorPoolCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    info.flags = 0;
    info.maxSets = max_sets;
    info.poolSizeCount = static_cast<uint32_t>(sizes.size());
    info.pPoolSizes = sizes.empty() ? nullptr : sizes.data();

    VkDevice device = RhiContext::instance().get_device();
    VkResult res = vkCreateDescriptorPool(device, &info, nullptr, &m_pool);
    if (res != VK_SUCCESS) {
        LOG_FATAL("RHI", "Failed to create descriptor pool: {}", static_cast<int>(res));
        return false;
    }
    return true;
}

void RhiDescriptorPool::destroy() {
    if (m_pool != VK_NULL_HANDLE) {
        VkDevice device = RhiContext::instance().get_device();
        if (device != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device, m_pool, nullptr);
        }
        m_pool = VK_NULL_HANDLE;
    }
}

VkDescriptorSet RhiDescriptorPool::allocate_set(VkDescriptorSetLayout layout) {
    VkDescriptorSetAllocateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    info.descriptorPool = m_pool;
    info.descriptorSetCount = 1;
    info.pSetLayouts = &layout;

    VkDescriptorSet set = VK_NULL_HANDLE;
    VkResult res = vkAllocateDescriptorSets(RhiContext::instance().get_device(), &info, &set);
    if (res != VK_SUCCESS) {
        LOG_ERROR("RHI", "Failed to allocate descriptor set: {}", static_cast<int>(res));
        return VK_NULL_HANDLE;
    }
    return set;
}

RhiDescriptorSet::~RhiDescriptorSet() {
    // Sets are freed with their pool; no individual destroy needed.
}

bool RhiDescriptorSet::init(RhiDescriptorPool& pool, VkDescriptorSetLayout layout) {
    m_set = pool.allocate_set(layout);
    return m_set != VK_NULL_HANDLE;
}

void RhiDescriptorSet::destroy(RhiDescriptorPool&) {
    m_set = VK_NULL_HANDLE; // pool owns the set
}

void RhiDescriptorSet::update_combined_image_sampler(uint32_t binding, VkImageView image_view, VkSampler sampler, VkImageLayout image_layout) {
    VkDescriptorImageInfo image_info{};
    image_info.sampler = sampler;
    image_info.imageView = image_view;
    image_info.imageLayout = image_layout;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_set;
    write.dstBinding = binding;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &image_info;

    vkUpdateDescriptorSets(RhiContext::instance().get_device(), 1, &write, 0, nullptr);
}

void RhiDescriptorSet::update_uniform_buffer(uint32_t binding, VkBuffer buffer, VkDeviceSize range) {
    VkDescriptorBufferInfo buffer_info{};
    buffer_info.buffer = buffer;
    buffer_info.offset = 0;
    buffer_info.range = range;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_set;
    write.dstBinding = binding;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.pBufferInfo = &buffer_info;

    vkUpdateDescriptorSets(RhiContext::instance().get_device(), 1, &write, 0, nullptr);
}

void RhiDescriptorSet::update_storage_buffer(uint32_t binding, VkBuffer buffer, VkDeviceSize range) {
    VkDescriptorBufferInfo buffer_info{};
    buffer_info.buffer = buffer;
    buffer_info.offset = 0;
    buffer_info.range = range;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_set;
    write.dstBinding = binding;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write.pBufferInfo = &buffer_info;

    vkUpdateDescriptorSets(RhiContext::instance().get_device(), 1, &write, 0, nullptr);
}

void RhiDescriptorSet::update_storage_image(uint32_t binding, VkImageView image_view, VkImageLayout image_layout) {
    VkDescriptorImageInfo image_info{};
    image_info.sampler = VK_NULL_HANDLE;
    image_info.imageView = image_view;
    image_info.imageLayout = image_layout;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_set;
    write.dstBinding = binding;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    write.pImageInfo = &image_info;

    vkUpdateDescriptorSets(RhiContext::instance().get_device(), 1, &write, 0, nullptr);
}

void RhiDescriptorSet::update_sampled_image(uint32_t binding, VkImageView image_view, VkImageLayout image_layout) {
    VkDescriptorImageInfo image_info{};
    image_info.sampler = VK_NULL_HANDLE;
    image_info.imageView = image_view;
    image_info.imageLayout = image_layout;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_set;
    write.dstBinding = binding;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    write.pImageInfo = &image_info;

    vkUpdateDescriptorSets(RhiContext::instance().get_device(), 1, &write, 0, nullptr);
}

} // namespace engine::rhi
