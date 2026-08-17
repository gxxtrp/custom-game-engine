#include "engine/core/memory.h"
#include "engine/core/log.h"
#include <cstdlib>
#include <cstring>

#if defined(ENGINE_PLATFORM_WINDOWS)
#include <malloc.h>
#endif

namespace engine::core {

// Global Allocator
struct Header {
    size_t size;
    size_t alignment;
};

GlobalAllocator& GlobalAllocator::instance() {
    static GlobalAllocator s_instance;
    return s_instance;
}

GlobalAllocator::GlobalAllocator() : Allocator("GlobalAllocator") {}

GlobalAllocator::~GlobalAllocator() {
    dump_leaks();
}

void* GlobalAllocator::allocate(size_t size, size_t alignment) {
    if (size == 0) return nullptr;
    if (alignment < alignof(void*)) alignment = alignof(void*);

#if defined(ENGINE_PLATFORM_WINDOWS)
    void* ptr = _aligned_malloc(size, alignment);
#else
    void* ptr = nullptr;
    if (posix_memalign(&ptr, alignment, size) != 0) {
        ptr = nullptr;
    }
#endif

    if (ptr) {
        m_active_bytes.fetch_add(size, std::memory_order_relaxed);
        m_alloc_count.fetch_add(1, std::memory_order_relaxed);
        size_t current = m_active_bytes.load(std::memory_order_relaxed);
        size_t peak = m_peak_bytes.load(std::memory_order_relaxed);
        while (current > peak && !m_peak_bytes.compare_exchange_weak(peak, current, std::memory_order_relaxed)) {}
    }
    return ptr;
}

void GlobalAllocator::deallocate(void* ptr) {
    if (!ptr) return;
    m_dealloc_count.fetch_add(1, std::memory_order_relaxed);

#if defined(ENGINE_PLATFORM_WINDOWS)
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}

void GlobalAllocator::dump_leaks() {
    size_t active = m_active_bytes.load(std::memory_order_relaxed);
    size_t allocs = m_alloc_count.load(std::memory_order_relaxed);
    size_t deallocs = m_dealloc_count.load(std::memory_order_relaxed);
    size_t peak = m_peak_bytes.load(std::memory_order_relaxed);

    LOG_INFO("Memory", "--- Memory Stats ---");
    LOG_INFO("Memory", "Allocations: {}, Deallocations: {}, Active: {}", allocs, deallocs, (allocs - deallocs));
    LOG_INFO("Memory", "Active Bytes: {} bytes, Peak Bytes: {} bytes", active, peak);

    if (allocs != deallocs) {
        LOG_WARN("Memory", "[WARNING] Possible memory leak detected: {} un-freed allocations!", allocs - deallocs);
    } else {
        LOG_INFO("Memory", "[CLEAN] All allocations were properly released.");
    }
}

// Linear (Arena) Allocator
LinearAllocator::LinearAllocator(size_t capacity, Allocator* backing_allocator, const char* name)
    : Allocator(name), m_capacity(capacity), m_backing_allocator(backing_allocator) {
    if (!m_backing_allocator) m_backing_allocator = &GlobalAllocator::instance();
    m_memory = static_cast<uint8_t*>(m_backing_allocator->allocate(capacity, alignof(std::max_align_t)));
    m_owns_memory = (m_memory != nullptr);
}

LinearAllocator::~LinearAllocator() {
    if (m_owns_memory && m_memory && m_backing_allocator) {
        m_backing_allocator->deallocate(m_memory);
        m_memory = nullptr;
    }
}

void* LinearAllocator::allocate(size_t size, size_t alignment) {
    if (size == 0 || !m_memory) return nullptr;

    uintptr_t current_address = reinterpret_cast<uintptr_t>(m_memory + m_offset);
    uintptr_t aligned_address = align_forward(current_address, alignment);
    size_t padding = aligned_address - current_address;

    if (m_offset + padding + size > m_capacity) {
        LOG_ERROR("Memory", "[LinearAllocator '{}'] Out of memory! Capacity: {} bytes, requested: {} bytes", m_name, m_capacity, size);
        return nullptr;
    }

    m_offset += padding + size;
    m_stats.total_allocated = m_offset;
    m_stats.active_allocated = m_offset;
    if (m_offset > m_stats.peak_allocated) {
        m_stats.peak_allocated = m_offset;
    }
    m_stats.allocation_count++;

    return reinterpret_cast<void*>(aligned_address);
}

void LinearAllocator::deallocate(void* /*ptr*/) {
    // Linear allocator does not support individual deallocations.
}

void LinearAllocator::reset() {
    m_offset = 0;
    m_stats.active_allocated = 0;
}

void LinearAllocator::rewind_to_marker(Marker marker) {
    ENGINE_ASSERT(marker <= m_capacity, "Invalid marker");
    if (marker < m_offset) {
        m_offset = marker;
        m_stats.active_allocated = m_offset;
    }
}

// Stack Allocator
StackAllocator::StackAllocator(size_t capacity, Allocator* backing_allocator, const char* name)
    : Allocator(name), m_capacity(capacity), m_backing_allocator(backing_allocator) {
    if (!m_backing_allocator) m_backing_allocator = &GlobalAllocator::instance();
    m_memory = static_cast<uint8_t*>(m_backing_allocator->allocate(capacity, alignof(std::max_align_t)));
}

StackAllocator::~StackAllocator() {
    if (m_memory && m_backing_allocator) {
        m_backing_allocator->deallocate(m_memory);
        m_memory = nullptr;
    }
}

void* StackAllocator::allocate(size_t size, size_t alignment) {
    if (size == 0 || !m_memory) return nullptr;

    uintptr_t current_address = reinterpret_cast<uintptr_t>(m_memory + m_offset);
    uintptr_t header_address = current_address + sizeof(Header);
    uintptr_t aligned_address = align_forward(header_address, alignment);
    size_t padding = aligned_address - current_address;

    if (m_offset + padding + size > m_capacity) {
        LOG_ERROR("Memory", "[StackAllocator '{}'] Out of memory! Capacity: {} bytes, requested: {} bytes", m_name, m_capacity, size);
        return nullptr;
    }

    Header* header = reinterpret_cast<Header*>(aligned_address - sizeof(Header));
    header->padding = static_cast<uint8_t>(padding);

    m_offset += padding + size;
    m_stats.active_allocated = m_offset;
    if (m_offset > m_stats.peak_allocated) {
        m_stats.peak_allocated = m_offset;
    }
    m_stats.allocation_count++;

    return reinterpret_cast<void*>(aligned_address);
}

void StackAllocator::deallocate(void* ptr) {
    if (!ptr || !m_memory) return;

    uintptr_t p = reinterpret_cast<uintptr_t>(ptr);
    Header* header = reinterpret_cast<Header*>(p - sizeof(Header));
    size_t padding = header->padding;

    // Check if this is the top of the stack
    uintptr_t block_start = p - padding;
    if (block_start < reinterpret_cast<uintptr_t>(m_memory + m_offset)) {
        m_offset = block_start - reinterpret_cast<uintptr_t>(m_memory);
        m_stats.active_allocated = m_offset;
        m_stats.deallocation_count++;
    }
}

void StackAllocator::free_to_marker(Marker marker) {
    if (marker < m_offset) {
        m_offset = marker;
        m_stats.active_allocated = m_offset;
    }
}

void StackAllocator::reset() {
    m_offset = 0;
    m_stats.active_allocated = 0;
}

// Pool Allocator
PoolAllocator::PoolAllocator(size_t block_size, size_t block_count, size_t alignment, Allocator* backing_allocator, const char* name)
    : Allocator(name), m_block_size(block_size), m_block_count(block_count), m_alignment(alignment),
      m_free_count(block_count), m_backing_allocator(backing_allocator) {
    if (!m_backing_allocator) m_backing_allocator = &GlobalAllocator::instance();

    size_t actual_block_size = (m_block_size < sizeof(Node)) ? sizeof(Node) : m_block_size;
    actual_block_size = align_forward(actual_block_size, m_alignment);
    m_block_size = actual_block_size;

    size_t total_size = m_block_size * m_block_count;
    m_memory = static_cast<uint8_t*>(m_backing_allocator->allocate(total_size, m_alignment));

    // Link all blocks into free list
    m_free_list = reinterpret_cast<Node*>(m_memory);
    Node* current = m_free_list;
    for (size_t i = 0; i < m_block_count - 1; ++i) {
        Node* next = reinterpret_cast<Node*>(reinterpret_cast<uint8_t*>(current) + m_block_size);
        current->next = next;
        current = next;
    }
    current->next = nullptr;
}

PoolAllocator::~PoolAllocator() {
    if (m_memory && m_backing_allocator) {
        m_backing_allocator->deallocate(m_memory);
        m_memory = nullptr;
    }
}

void* PoolAllocator::allocate(size_t size, size_t alignment) {
    ENGINE_ASSERT(size <= m_block_size, "Requested size exceeds pool block size");
    ENGINE_ASSERT(alignment <= m_alignment, "Requested alignment exceeds pool alignment");

    if (!m_free_list) {
        LOG_ERROR("Memory", "[PoolAllocator '{}'] No free blocks available!", m_name);
        return nullptr;
    }

    Node* node = m_free_list;
    m_free_list = m_free_list->next;
    m_free_count--;
    m_stats.allocation_count++;
    m_stats.active_allocated = (m_block_count - m_free_count) * m_block_size;
    if (m_stats.active_allocated > m_stats.peak_allocated) {
        m_stats.peak_allocated = m_stats.active_allocated;
    }

    return reinterpret_cast<void*>(node);
}

void PoolAllocator::deallocate(void* ptr) {
    if (!ptr || !m_memory) return;

    uintptr_t start = reinterpret_cast<uintptr_t>(m_memory);
    uintptr_t end = start + (m_block_size * m_block_count);
    uintptr_t p = reinterpret_cast<uintptr_t>(ptr);

    ENGINE_ASSERT(p >= start && p < end, "Pointer does not belong to this pool allocator");

    Node* node = reinterpret_cast<Node*>(ptr);
    node->next = m_free_list;
    m_free_list = node;
    m_free_count++;
    m_stats.deallocation_count++;
    m_stats.active_allocated = (m_block_count - m_free_count) * m_block_size;
}

} // namespace engine::core
