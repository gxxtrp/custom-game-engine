#pragma once

#include "engine/core/config.h"
#include <cstdint>
#include <cstddef>
#include <atomic>
#include <utility>
#include <new>

namespace engine::core {

struct AllocatorStats {
    size_t total_allocated{0};
    size_t active_allocated{0};
    size_t peak_allocated{0};
    size_t allocation_count{0};
    size_t deallocation_count{0};
};

class Allocator {
public:
    explicit Allocator(const char* name = "UnnamedAllocator") : m_name(name) {}
    virtual ~Allocator() = default;

    virtual void* allocate(size_t size, size_t alignment = alignof(std::max_align_t)) = 0;
    virtual void deallocate(void* ptr) = 0;

    const char* get_name() const { return m_name; }
    virtual AllocatorStats get_stats() const { return m_stats; }

protected:
    const char* m_name;
    AllocatorStats m_stats{};
};

// Global / Heap Allocator with tracking
class GlobalAllocator : public Allocator {
public:
    static GlobalAllocator& instance();

    void* allocate(size_t size, size_t alignment = alignof(std::max_align_t)) override;
    void deallocate(void* ptr) override;

    void dump_leaks();

private:
    GlobalAllocator();
    ~GlobalAllocator() override;

    std::atomic<size_t> m_active_bytes{0};
    std::atomic<size_t> m_peak_bytes{0};
    std::atomic<size_t> m_alloc_count{0};
    std::atomic<size_t> m_dealloc_count{0};
};

// Linear (Arena) Allocator - bump pointer, reset all at once (ideal for per-frame scratch)
class LinearAllocator : public Allocator {
public:
    LinearAllocator(size_t capacity, Allocator* backing_allocator = nullptr, const char* name = "LinearAllocator");
    ~LinearAllocator() override;

    void* allocate(size_t size, size_t alignment = alignof(std::max_align_t)) override;
    void deallocate(void* ptr) override; // No-op in linear allocator

    void reset();
    
    using Marker = size_t;
    Marker get_marker() const { return m_offset; }
    void rewind_to_marker(Marker marker);

    size_t get_capacity() const { return m_capacity; }
    size_t get_used_bytes() const { return m_offset; }

private:
    uint8_t* m_memory{nullptr};
    size_t m_capacity{0};
    size_t m_offset{0};
    Allocator* m_backing_allocator{nullptr};
    bool m_owns_memory{true};
};

// Stack Allocator - LIFO allocations with markers
class StackAllocator : public Allocator {
public:
    using Marker = size_t;

    StackAllocator(size_t capacity, Allocator* backing_allocator = nullptr, const char* name = "StackAllocator");
    ~StackAllocator() override;

    void* allocate(size_t size, size_t alignment = alignof(std::max_align_t)) override;
    void deallocate(void* ptr) override;

    Marker get_marker() const { return m_offset; }
    void free_to_marker(Marker marker);
    void reset();

private:
    struct Header {
        uint8_t padding;
    };

    uint8_t* m_memory{nullptr};
    size_t m_capacity{0};
    size_t m_offset{0};
    Allocator* m_backing_allocator{nullptr};
};

// Pool Allocator - O(1) allocation of fixed-size blocks
class PoolAllocator : public Allocator {
public:
    PoolAllocator(size_t block_size, size_t block_count, size_t alignment = alignof(std::max_align_t),
                  Allocator* backing_allocator = nullptr, const char* name = "PoolAllocator");
    ~PoolAllocator() override;

    void* allocate(size_t size, size_t alignment = alignof(std::max_align_t)) override;
    void deallocate(void* ptr) override;

    size_t get_block_size() const { return m_block_size; }
    size_t get_block_count() const { return m_block_count; }
    size_t get_free_count() const { return m_free_count; }

private:
    struct Node {
        Node* next{nullptr};
    };

    uint8_t* m_memory{nullptr};
    size_t m_block_size{0};
    size_t m_block_count{0};
    size_t m_alignment{0};
    size_t m_free_count{0};
    Node* m_free_list{nullptr};
    Allocator* m_backing_allocator{nullptr};
};

// Helper utility functions for alignment
ENGINE_FORCE_INLINE uintptr_t align_forward(uintptr_t ptr, size_t alignment) {
    ENGINE_ASSERT((alignment & (alignment - 1)) == 0, "Alignment must be a power of 2");
    return (ptr + (alignment - 1)) & ~(alignment - 1);
}

ENGINE_FORCE_INLINE void* align_forward_ptr(void* ptr, size_t alignment) {
    return reinterpret_cast<void*>(align_forward(reinterpret_cast<uintptr_t>(ptr), alignment));
}

// Memory Allocation Macros & Template Helpers
template<typename T, typename... Args>
ENGINE_FORCE_INLINE T* engine_new(Allocator* allocator, Args&&... args) {
    if (!allocator) allocator = &GlobalAllocator::instance();
    void* memory = allocator->allocate(sizeof(T), alignof(T));
    if (!memory) return nullptr;
    return new (memory) T(std::forward<Args>(args)...);
}

template<typename T>
ENGINE_FORCE_INLINE void engine_delete(Allocator* allocator, T* ptr) {
    if (!ptr) return;
    if (!allocator) allocator = &GlobalAllocator::instance();
    ptr->~T();
    allocator->deallocate(ptr);
}

template<typename T>
ENGINE_FORCE_INLINE T* engine_alloc_array(Allocator* allocator, size_t count) {
    if (!allocator) allocator = &GlobalAllocator::instance();
    void* memory = allocator->allocate(sizeof(T) * count, alignof(T));
    if (!memory) return nullptr;
    T* array = reinterpret_cast<T*>(memory);
    for (size_t i = 0; i < count; ++i) {
        new (&array[i]) T();
    }
    return array;
}

template<typename T>
ENGINE_FORCE_INLINE void engine_free_array(Allocator* allocator, T* array, size_t count) {
    if (!array) return;
    if (!allocator) allocator = &GlobalAllocator::instance();
    for (size_t i = 0; i < count; ++i) {
        array[i].~T();
    }
    allocator->deallocate(array);
}

#define ENGINE_NEW(allocator, Type, ...) ::engine::core::engine_new<Type>(allocator __VA_OPT__(,) __VA_ARGS__)
#define ENGINE_DELETE(allocator, ptr) ::engine::core::engine_delete(allocator, ptr)
#define ENGINE_ALLOC_ARRAY(allocator, Type, count) ::engine::core::engine_alloc_array<Type>(allocator, count)
#define ENGINE_FREE_ARRAY(allocator, array, count) ::engine::core::engine_free_array(allocator, array, count)

} // namespace engine::core
