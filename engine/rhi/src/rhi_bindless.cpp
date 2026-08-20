#include "engine/rhi/rhi_bindless.h"
#include "engine/rhi/rhi_context.h"
#include "engine/core/log.h"
#include <array>

namespace engine::rhi {

BindlessHeap& BindlessHeap::instance() {
    static BindlessHeap s_instance;
    return s_instance;
}

BindlessHeap::~BindlessHeap() {
    shutdown();
}

bool BindlessHeap::init() {
    if (m_initialized) return true;

    VkDevice device = RhiContext::instance().get_device();
    if (device == VK_NULL_HANDLE) return false;

    // 1. Descriptor Pool with Update-After-Bind
    std::array<VkDescriptorPoolSize, 3> pool_sizes = {{
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, MAX_SAMPLED_IMAGES },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, MAX_STORAGE_BUFFERS },
        { VK_DESCRIPTOR_TYPE_SAMPLER, MAX_SAMPLERS }
    }};

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    pool_info.maxSets = 1;
    pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
    pool_info.pPoolSizes = pool_sizes.data();

    VkResult res = vkCreateDescriptorPool(device, &pool_info, nullptr, &m_descriptor_pool);
    if (res != VK_SUCCESS) {
        LOG_FATAL("RHI", "Failed to create bindless descriptor pool: {}", static_cast<int>(res));
        return false;
    }

    // 2. Descriptor Set Layout with Partially Bound & Update After Bind flags
    std::array<VkDescriptorSetLayoutBinding, 3> bindings{};

    // Binding 0: Sampled Images
    bindings[0].binding = BINDING_SAMPLED_IMAGES;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    bindings[0].descriptorCount = MAX_SAMPLED_IMAGES;
    bindings[0].stageFlags = VK_SHADER_STAGE_ALL;

    // Binding 1: Storage Buffers
    bindings[1].binding = BINDING_STORAGE_BUFFERS;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[1].descriptorCount = MAX_STORAGE_BUFFERS;
    bindings[1].stageFlags = VK_SHADER_STAGE_ALL;

    // Binding 2: Samplers
    bindings[2].binding = BINDING_SAMPLERS;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    bindings[2].descriptorCount = MAX_SAMPLERS;
    bindings[2].stageFlags = VK_SHADER_STAGE_ALL;

    std::array<VkDescriptorBindingFlags, 3> binding_flags = {{
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
    }};

    VkDescriptorSetLayoutBindingFlagsCreateInfo flags_info{};
    flags_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    flags_info.bindingCount = static_cast<uint32_t>(binding_flags.size());
    flags_info.pBindingFlags = binding_flags.data();

    VkDescriptorSetLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    layout_info.bindingCount = static_cast<uint32_t>(bindings.size());
    layout_info.pBindings = bindings.data();
    layout_info.pNext = &flags_info;

    res = vkCreateDescriptorSetLayout(device, &layout_info, nullptr, &m_descriptor_set_layout);
    if (res != VK_SUCCESS) {
        LOG_FATAL("RHI", "Failed to create bindless descriptor set layout: {}", static_cast<int>(res));
        return false;
    }

    // 3. Allocate Global Bindless Descriptor Set
    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = m_descriptor_pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &m_descriptor_set_layout;

    res = vkAllocateDescriptorSets(device, &alloc_info, &m_descriptor_set);
    if (res != VK_SUCCESS) {
        LOG_FATAL("RHI", "Failed to allocate bindless descriptor set: {}", static_cast<int>(res));
        return false;
    }

    m_initialized = true;
    LOG_INFO("RHI", "Initialized Bindless Heap ({} textures, {} buffers, {} samplers)",
             MAX_SAMPLED_IMAGES, MAX_STORAGE_BUFFERS, MAX_SAMPLERS);
    return true;
}

void BindlessHeap::shutdown() {
    if (!m_initialized) return;

    VkDevice device = RhiContext::instance().get_device();
    if (device != VK_NULL_HANDLE) {
        if (m_descriptor_set_layout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device, m_descriptor_set_layout, nullptr);
            m_descriptor_set_layout = VK_NULL_HANDLE;
        }
        if (m_descriptor_pool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device, m_descriptor_pool, nullptr);
            m_descriptor_pool = VK_NULL_HANDLE;
        }
    }

    m_descriptor_set = VK_NULL_HANDLE;
    m_free_texture_slots.clear();
    m_free_buffer_slots.clear();
    m_free_sampler_slots.clear();
    m_next_texture_slot = 0;
    m_next_buffer_slot = 0;
    m_next_sampler_slot = 0;
    m_initialized = false;
}

uint32_t BindlessHeap::register_texture(VkImageView view, VkImageLayout layout) {
    if (!m_initialized || view == VK_NULL_HANDLE) return UINT32_MAX;

    std::lock_guard<std::mutex> lock(m_mutex);

    uint32_t slot = 0;
    if (!m_free_texture_slots.empty()) {
        slot = m_free_texture_slots.back();
        m_free_texture_slots.pop_back();
    } else {
        if (m_next_texture_slot >= MAX_SAMPLED_IMAGES) {
            LOG_ERROR("RHI", "Exceeded max bindless textures!");
            return UINT32_MAX;
        }
        slot = m_next_texture_slot++;
    }

    VkDescriptorImageInfo image_info{};
    image_info.imageView = view;
    image_info.imageLayout = layout;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_descriptor_set;
    write.dstBinding = BINDING_SAMPLED_IMAGES;
    write.dstArrayElement = slot;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    write.pImageInfo = &image_info;

    vkUpdateDescriptorSets(RhiContext::instance().get_device(), 1, &write, 0, nullptr);
    return slot;
}

void BindlessHeap::unregister_texture(uint32_t slot) {
    if (slot == UINT32_MAX) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_free_texture_slots.push_back(slot);
}

uint32_t BindlessHeap::register_storage_buffer(VkBuffer buffer, VkDeviceSize size, VkDeviceSize offset) {
    if (!m_initialized || buffer == VK_NULL_HANDLE) return UINT32_MAX;

    std::lock_guard<std::mutex> lock(m_mutex);

    uint32_t slot = 0;
    if (!m_free_buffer_slots.empty()) {
        slot = m_free_buffer_slots.back();
        m_free_buffer_slots.pop_back();
    } else {
        if (m_next_buffer_slot >= MAX_STORAGE_BUFFERS) {
            LOG_ERROR("RHI", "Exceeded max bindless storage buffers!");
            return UINT32_MAX;
        }
        slot = m_next_buffer_slot++;
    }

    VkDescriptorBufferInfo buffer_info{};
    buffer_info.buffer = buffer;
    buffer_info.offset = offset;
    buffer_info.range = size;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_descriptor_set;
    write.dstBinding = BINDING_STORAGE_BUFFERS;
    write.dstArrayElement = slot;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write.pBufferInfo = &buffer_info;

    vkUpdateDescriptorSets(RhiContext::instance().get_device(), 1, &write, 0, nullptr);
    return slot;
}

void BindlessHeap::unregister_storage_buffer(uint32_t slot) {
    if (slot == UINT32_MAX) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_free_buffer_slots.push_back(slot);
}

uint32_t BindlessHeap::register_sampler(VkSampler sampler) {
    if (!m_initialized || sampler == VK_NULL_HANDLE) return UINT32_MAX;

    std::lock_guard<std::mutex> lock(m_mutex);

    uint32_t slot = 0;
    if (!m_free_sampler_slots.empty()) {
        slot = m_free_sampler_slots.back();
        m_free_sampler_slots.pop_back();
    } else {
        if (m_next_sampler_slot >= MAX_SAMPLERS) {
            LOG_ERROR("RHI", "Exceeded max bindless samplers!");
            return UINT32_MAX;
        }
        slot = m_next_sampler_slot++;
    }

    VkDescriptorImageInfo sampler_info{};
    sampler_info.sampler = sampler;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_descriptor_set;
    write.dstBinding = BINDING_SAMPLERS;
    write.dstArrayElement = slot;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    write.pImageInfo = &sampler_info;

    vkUpdateDescriptorSets(RhiContext::instance().get_device(), 1, &write, 0, nullptr);
    return slot;
}

void BindlessHeap::unregister_sampler(uint32_t slot) {
    if (slot == UINT32_MAX) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_free_sampler_slots.push_back(slot);
}

void BindlessHeap::bind(VkCommandBuffer cmd, VkPipelineLayout layout, VkPipelineBindPoint bind_point, uint32_t set_index) {
    if (!m_initialized || m_descriptor_set == VK_NULL_HANDLE) return;
    vkCmdBindDescriptorSets(cmd, bind_point, layout, set_index, 1, &m_descriptor_set, 0, nullptr);
}

} // namespace engine::rhi
