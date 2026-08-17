#pragma once

#include "engine/core/config.h"
#include "engine/core/memory.h"
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <utility>
#include <functional>
#include <initializer_list>

namespace engine::core {

// ============================================================================
// FixedVector<T, N> - Stack-allocated fixed capacity array
// ============================================================================
template<typename T, size_t Capacity>
class FixedVector {
public:
    FixedVector() : m_size(0) {}
    ~FixedVector() { clear(); }

    FixedVector(const FixedVector& other) : m_size(0) {
        for (size_t i = 0; i < other.m_size; ++i) {
            push_back(other[i]);
        }
    }

    FixedVector& operator=(const FixedVector& other) {
        if (this != &other) {
            clear();
            for (size_t i = 0; i < other.m_size; ++i) {
                push_back(other[i]);
            }
        }
        return *this;
    }

    FixedVector(FixedVector&& other) noexcept : m_size(0) {
        for (size_t i = 0; i < other.m_size; ++i) {
            push_back(std::move(other[i]));
        }
        other.clear();
    }

    FixedVector& operator=(FixedVector&& other) noexcept {
        if (this != &other) {
            clear();
            for (size_t i = 0; i < other.m_size; ++i) {
                push_back(std::move(other[i]));
            }
            other.clear();
        }
        return *this;
    }

    void push_back(const T& value) {
        ENGINE_ASSERT(m_size < Capacity, "FixedVector capacity exceeded");
        new (&data()[m_size++]) T(value);
    }

    void push_back(T&& value) {
        ENGINE_ASSERT(m_size < Capacity, "FixedVector capacity exceeded");
        new (&data()[m_size++]) T(std::move(value));
    }

    template<typename... Args>
    T& emplace_back(Args&&... args) {
        ENGINE_ASSERT(m_size < Capacity, "FixedVector capacity exceeded");
        T* ptr = new (&data()[m_size++]) T(std::forward<Args>(args)...);
        return *ptr;
    }

    void pop_back() {
        if (m_size > 0) {
            data()[--m_size].~T();
        }
    }

    void clear() {
        for (size_t i = 0; i < m_size; ++i) {
            data()[i].~T();
        }
        m_size = 0;
    }

    T& operator[](size_t index) {
        ENGINE_ASSERT(index < m_size, "Index out of bounds");
        return data()[index];
    }

    const T& operator[](size_t index) const {
        ENGINE_ASSERT(index < m_size, "Index out of bounds");
        return data()[index];
    }

    T* data() { return reinterpret_cast<T*>(m_storage); }
    const T* data() const { return reinterpret_cast<const T*>(m_storage); }

    T* begin() { return data(); }
    const T* begin() const { return data(); }
    T* end() { return data() + m_size; }
    const T* end() const { return data() + m_size; }

    size_t size() const { return m_size; }
    constexpr size_t capacity() const { return Capacity; }
    bool empty() const { return m_size == 0; }

private:
    alignas(alignof(T)) uint8_t m_storage[sizeof(T) * (Capacity > 0 ? Capacity : 1)];
    size_t m_size{0};
};

// ============================================================================
// DynamicArray<T> - Allocator-aware dynamic array
// ============================================================================
template<typename T>
class DynamicArray {
public:
    explicit DynamicArray(Allocator* allocator = nullptr)
        : m_allocator(allocator ? allocator : &GlobalAllocator::instance()) {}

    explicit DynamicArray(size_t initial_capacity, Allocator* allocator = nullptr)
        : m_allocator(allocator ? allocator : &GlobalAllocator::instance()) {
        reserve(initial_capacity);
    }

    ~DynamicArray() {
        clear();
        if (m_data) {
            m_allocator->deallocate(m_data);
            m_data = nullptr;
        }
    }

    DynamicArray(const DynamicArray& other)
        : m_allocator(other.m_allocator) {
        reserve(other.m_size);
        for (size_t i = 0; i < other.m_size; ++i) {
            push_back(other[i]);
        }
    }

    DynamicArray& operator=(const DynamicArray& other) {
        if (this != &other) {
            clear();
            reserve(other.m_size);
            for (size_t i = 0; i < other.m_size; ++i) {
                push_back(other[i]);
            }
        }
        return *this;
    }

    DynamicArray(DynamicArray&& other) noexcept
        : m_data(other.m_data), m_size(other.m_size), m_capacity(other.m_capacity), m_allocator(other.m_allocator) {
        other.m_data = nullptr;
        other.m_size = 0;
        other.m_capacity = 0;
    }

    DynamicArray& operator=(DynamicArray&& other) noexcept {
        if (this != &other) {
            clear();
            if (m_data) m_allocator->deallocate(m_data);

            m_data = other.m_data;
            m_size = other.m_size;
            m_capacity = other.m_capacity;
            m_allocator = other.m_allocator;

            other.m_data = nullptr;
            other.m_size = 0;
            other.m_capacity = 0;
        }
        return *this;
    }

    void reserve(size_t new_capacity) {
        if (new_capacity <= m_capacity) return;

        T* new_data = static_cast<T*>(m_allocator->allocate(sizeof(T) * new_capacity, alignof(T)));
        for (size_t i = 0; i < m_size; ++i) {
            new (&new_data[i]) T(std::move(m_data[i]));
            m_data[i].~T();
        }

        if (m_data) {
            m_allocator->deallocate(m_data);
        }

        m_data = new_data;
        m_capacity = new_capacity;
    }

    void resize(size_t new_size) {
        if (new_size > m_capacity) {
            reserve(new_size);
        }
        if (new_size > m_size) {
            for (size_t i = m_size; i < new_size; ++i) {
                new (&m_data[i]) T();
            }
        } else {
            for (size_t i = new_size; i < m_size; ++i) {
                m_data[i].~T();
            }
        }
        m_size = new_size;
    }

    void push_back(const T& val) {
        if (m_size >= m_capacity) {
            reserve(m_capacity == 0 ? 8 : m_capacity * 2);
        }
        new (&m_data[m_size++]) T(val);
    }

    void push_back(T&& val) {
        if (m_size >= m_capacity) {
            reserve(m_capacity == 0 ? 8 : m_capacity * 2);
        }
        new (&m_data[m_size++]) T(std::move(val));
    }

    template<typename... Args>
    T& emplace_back(Args&&... args) {
        if (m_size >= m_capacity) {
            reserve(m_capacity == 0 ? 8 : m_capacity * 2);
        }
        T* ptr = new (&m_data[m_size++]) T(std::forward<Args>(args)...);
        return *ptr;
    }

    void pop_back() {
        if (m_size > 0) {
            m_data[--m_size].~T();
        }
    }

    void clear() {
        for (size_t i = 0; i < m_size; ++i) {
            m_data[i].~T();
        }
        m_size = 0;
    }

    T& operator[](size_t idx) {
        ENGINE_ASSERT(idx < m_size, "Index out of range");
        return m_data[idx];
    }

    const T& operator[](size_t idx) const {
        ENGINE_ASSERT(idx < m_size, "Index out of range");
        return m_data[idx];
    }

    T* data() { return m_data; }
    const T* data() const { return m_data; }
    size_t size() const { return m_size; }
    size_t capacity() const { return m_capacity; }
    bool empty() const { return m_size == 0; }

    T* begin() { return m_data; }
    const T* begin() const { return m_data; }
    T* end() { return m_data + m_size; }
    const T* end() const { return m_data + m_size; }

private:
    T* m_data{nullptr};
    size_t m_size{0};
    size_t m_capacity{0};
    Allocator* m_allocator{nullptr};
};

// ============================================================================
// RingBuffer<T> - Circular buffer
// ============================================================================
template<typename T>
class RingBuffer {
public:
    explicit RingBuffer(size_t capacity, Allocator* allocator = nullptr)
        : m_capacity(capacity), m_allocator(allocator ? allocator : &GlobalAllocator::instance()) {
        m_data = static_cast<T*>(m_allocator->allocate(sizeof(T) * m_capacity, alignof(T)));
    }

    ~RingBuffer() {
        clear();
        if (m_data) {
            m_allocator->deallocate(m_data);
            m_data = nullptr;
        }
    }

    void push(const T& item) {
        if (m_size == m_capacity) {
            m_data[m_head].~T();
            m_head = (m_head + 1) % m_capacity;
            m_size--;
        }
        new (&m_data[m_tail]) T(item);
        m_tail = (m_tail + 1) % m_capacity;
        m_size++;
    }

    void push(T&& item) {
        if (m_size == m_capacity) {
            m_data[m_head].~T();
            m_head = (m_head + 1) % m_capacity;
            m_size--;
        }
        new (&m_data[m_tail]) T(std::move(item));
        m_tail = (m_tail + 1) % m_capacity;
        m_size++;
    }

    bool pop(T& out_item) {
        if (empty()) return false;
        out_item = std::move(m_data[m_head]);
        m_data[m_head].~T();
        m_head = (m_head + 1) % m_capacity;
        m_size--;
        return true;
    }

    void clear() {
        while (!empty()) {
            m_data[m_head].~T();
            m_head = (m_head + 1) % m_capacity;
            m_size--;
        }
        m_head = 0;
        m_tail = 0;
    }

    size_t size() const { return m_size; }
    size_t capacity() const { return m_capacity; }
    bool empty() const { return m_size == 0; }
    bool full() const { return m_size == m_capacity; }

private:
    T* m_data{nullptr};
    size_t m_capacity{0};
    size_t m_head{0};
    size_t m_tail{0};
    size_t m_size{0};
    Allocator* m_allocator{nullptr};
};

// ============================================================================
// SlotMap<T> - Generational Index Slot Map
// ============================================================================
struct SlotHandle {
    uint32_t id{0xFFFFFFFF};
    uint32_t generation{0};

    bool is_valid() const { return id != 0xFFFFFFFF; }
    bool operator==(const SlotHandle& other) const { return id == other.id && generation == other.generation; }
    bool operator!=(const SlotHandle& other) const { return !(*this == other); }
};

template<typename T>
class SlotMap {
public:
    explicit SlotMap(size_t initial_capacity = 32, Allocator* allocator = nullptr)
        : m_allocator(allocator ? allocator : &GlobalAllocator::instance()),
          m_slots(allocator), m_data(allocator), m_erase_map(allocator) {
        reserve(initial_capacity);
    }

    SlotHandle insert(const T& value) {
        uint32_t slot_idx = allocate_slot();
        uint32_t data_idx = static_cast<uint32_t>(m_data.size());

        m_data.push_back(value);
        m_erase_map.push_back(slot_idx);

        m_slots[slot_idx].data_index = data_idx;

        return SlotHandle{slot_idx, m_slots[slot_idx].generation};
    }

    SlotHandle insert(T&& value) {
        uint32_t slot_idx = allocate_slot();
        uint32_t data_idx = static_cast<uint32_t>(m_data.size());

        m_data.push_back(std::move(value));
        m_erase_map.push_back(slot_idx);

        m_slots[slot_idx].data_index = data_idx;

        return SlotHandle{slot_idx, m_slots[slot_idx].generation};
    }

    bool erase(SlotHandle handle) {
        if (!is_valid(handle)) return false;

        uint32_t slot_idx = handle.id;
        uint32_t data_idx = m_slots[slot_idx].data_index;
        uint32_t last_data_idx = static_cast<uint32_t>(m_data.size() - 1);

        // Swap with last element for dense storage
        if (data_idx != last_data_idx) {
            m_data[data_idx] = std::move(m_data[last_data_idx]);
            uint32_t moved_slot_idx = m_erase_map[last_data_idx];
            m_slots[moved_slot_idx].data_index = data_idx;
            m_erase_map[data_idx] = moved_slot_idx;
        }

        m_data.pop_back();
        m_erase_map.pop_back();

        // Increment generation and recycle slot
        m_slots[slot_idx].generation++;
        m_slots[slot_idx].next_free = m_free_head;
        m_free_head = slot_idx;

        return true;
    }

    T* get(SlotHandle handle) {
        if (!is_valid(handle)) return nullptr;
        return &m_data[m_slots[handle.id].data_index];
    }

    const T* get(SlotHandle handle) const {
        if (!is_valid(handle)) return nullptr;
        return &m_data[m_slots[handle.id].data_index];
    }

    bool is_valid(SlotHandle handle) const {
        if (handle.id >= m_slots.size()) return false;
        return m_slots[handle.id].generation == handle.generation;
    }

    void clear() {
        m_data.clear();
        m_erase_map.clear();
        m_slots.clear();
        m_free_head = 0xFFFFFFFF;
    }

    size_t size() const { return m_data.size(); }
    bool empty() const { return m_data.empty(); }

    T* begin() { return m_data.begin(); }
    const T* begin() const { return m_data.begin(); }
    T* end() { return m_data.end(); }
    const T* end() const { return m_data.end(); }

private:
    struct Slot {
        uint32_t data_index{0xFFFFFFFF};
        uint32_t generation{1};
        uint32_t next_free{0xFFFFFFFF};
    };

    void reserve(size_t cap) {
        m_slots.reserve(cap);
        m_data.reserve(cap);
        m_erase_map.reserve(cap);
    }

    uint32_t allocate_slot() {
        if (m_free_head != 0xFFFFFFFF) {
            uint32_t idx = m_free_head;
            m_free_head = m_slots[idx].next_free;
            return idx;
        }

        uint32_t idx = static_cast<uint32_t>(m_slots.size());
        m_slots.emplace_back(Slot{});
        return idx;
    }

    Allocator* m_allocator{nullptr};
    DynamicArray<Slot> m_slots;
    DynamicArray<T> m_data;
    DynamicArray<uint32_t> m_erase_map;
    uint32_t m_free_head{0xFFFFFFFF};
};

// ============================================================================
// HashMap<K, V> - Robin Hood Open Addressing Hash Map
// ============================================================================
template<typename K, typename V, typename Hash = std::hash<K>, typename KeyEqual = std::equal_to<K>>
class HashMap {
public:
    explicit HashMap(size_t initial_capacity = 16, Allocator* allocator = nullptr)
        : m_allocator(allocator ? allocator : &GlobalAllocator::instance()) {
        reserve(initial_capacity);
    }

    ~HashMap() {
        clear();
        if (m_entries) {
            m_allocator->deallocate(m_entries);
            m_entries = nullptr;
        }
    }

    HashMap(const HashMap& other)
        : m_allocator(other.m_allocator) {
        reserve(other.m_capacity);
        for (size_t i = 0; i < other.m_capacity; ++i) {
            if (other.m_entries[i].dib > 0) {
                insert(other.m_entries[i].key, other.m_entries[i].value);
            }
        }
    }

    HashMap& operator=(const HashMap& other) {
        if (this != &other) {
            clear();
            reserve(other.m_capacity);
            for (size_t i = 0; i < other.m_capacity; ++i) {
                if (other.m_entries[i].dib > 0) {
                    insert(other.m_entries[i].key, other.m_entries[i].value);
                }
            }
        }
        return *this;
    }

    HashMap(HashMap&& other) noexcept
        : m_entries(other.m_entries), m_capacity(other.m_capacity),
          m_size(other.m_size), m_allocator(other.m_allocator) {
        other.m_entries = nullptr;
        other.m_capacity = 0;
        other.m_size = 0;
    }

    HashMap& operator=(HashMap&& other) noexcept {
        if (this != &other) {
            clear();
            if (m_entries) m_allocator->deallocate(m_entries);
            m_entries = other.m_entries;
            m_capacity = other.m_capacity;
            m_size = other.m_size;
            m_allocator = other.m_allocator;

            other.m_entries = nullptr;
            other.m_capacity = 0;
            other.m_size = 0;
        }
        return *this;
    }

    bool insert(const K& key, const V& value) {
        if (m_size >= m_capacity * 0.75f) {
            rehash(m_capacity == 0 ? 16 : m_capacity * 2);
        }

        Entry current_entry{key, value, 1};
        size_t mask = m_capacity - 1;
        size_t idx = Hash{}(key) & mask;

        while (true) {
            if (m_entries[idx].dib == 0) {
                new (&m_entries[idx]) Entry(std::move(current_entry));
                m_size++;
                return true;
            }

            if (KeyEqual{}(m_entries[idx].key, current_entry.key)) {
                m_entries[idx].value = current_entry.value;
                return false; // updated existing
            }

            if (current_entry.dib > m_entries[idx].dib) {
                std::swap(current_entry, m_entries[idx]);
            }

            idx = (idx + 1) & mask;
            current_entry.dib++;
        }
    }

    V* find(const K& key) {
        if (m_size == 0) return nullptr;
        size_t mask = m_capacity - 1;
        size_t idx = Hash{}(key) & mask;
        uint32_t dib = 1;

        while (true) {
            if (m_entries[idx].dib == 0 || dib > m_entries[idx].dib) {
                return nullptr;
            }
            if (KeyEqual{}(m_entries[idx].key, key)) {
                return &m_entries[idx].value;
            }
            idx = (idx + 1) & mask;
            dib++;
        }
    }

    const V* find(const K& key) const {
        if (m_size == 0) return nullptr;
        size_t mask = m_capacity - 1;
        size_t idx = Hash{}(key) & mask;
        uint32_t dib = 1;

        while (true) {
            if (m_entries[idx].dib == 0 || dib > m_entries[idx].dib) {
                return nullptr;
            }
            if (KeyEqual{}(m_entries[idx].key, key)) {
                return &m_entries[idx].value;
            }
            idx = (idx + 1) & mask;
            dib++;
        }
    }

    bool erase(const K& key) {
        if (m_size == 0) return false;
        size_t mask = m_capacity - 1;
        size_t idx = Hash{}(key) & mask;
        uint32_t dib = 1;

        while (true) {
            if (m_entries[idx].dib == 0 || dib > m_entries[idx].dib) {
                return false;
            }
            if (KeyEqual{}(m_entries[idx].key, key)) {
                // Backward shift deletion
                m_entries[idx].~Entry();
                size_t curr = idx;
                size_t next = (curr + 1) & mask;

                while (m_entries[next].dib > 1) {
                    new (&m_entries[curr]) Entry(std::move(m_entries[next]));
                    m_entries[curr].dib--;
                    m_entries[next].~Entry();
                    curr = next;
                    next = (curr + 1) & mask;
                }
                m_entries[curr].dib = 0;
                m_size--;
                return true;
            }
            idx = (idx + 1) & mask;
            dib++;
        }
    }

    V& operator[](const K& key) {
        V* found = find(key);
        if (found) return *found;
        insert(key, V());
        return *find(key);
    }

    void clear() {
        for (size_t i = 0; i < m_capacity; ++i) {
            if (m_entries[i].dib > 0) {
                m_entries[i].~Entry();
                m_entries[i].dib = 0;
            }
        }
        m_size = 0;
    }

    size_t size() const { return m_size; }
    bool empty() const { return m_size == 0; }

private:
    struct Entry {
        K key{};
        V value{};
        uint32_t dib{0}; // Distance From Initial Bucket (0 = empty)
    };

    void reserve(size_t cap) {
        size_t power2 = 16;
        while (power2 < cap) power2 *= 2;
        rehash(power2);
    }

    void rehash(size_t new_capacity) {
        Entry* old_entries = m_entries;
        size_t old_capacity = m_capacity;

        m_entries = static_cast<Entry*>(m_allocator->allocate(sizeof(Entry) * new_capacity, alignof(Entry)));
        for (size_t i = 0; i < new_capacity; ++i) {
            m_entries[i].dib = 0;
        }

        m_capacity = new_capacity;
        m_size = 0;

        if (old_entries) {
            for (size_t i = 0; i < old_capacity; ++i) {
                if (old_entries[i].dib > 0) {
                    insert(std::move(old_entries[i].key), std::move(old_entries[i].value));
                    old_entries[i].~Entry();
                }
            }
            m_allocator->deallocate(old_entries);
        }
    }

    Entry* m_entries{nullptr};
    size_t m_capacity{0};
    size_t m_size{0};
    Allocator* m_allocator{nullptr};
};

} // namespace engine::core
