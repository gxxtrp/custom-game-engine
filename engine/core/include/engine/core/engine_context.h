#pragma once

#include "engine/core/config.h"
#include "engine/core/log.h"
#include <unordered_map>
#include <typeindex>
#include <typeinfo>
#include <shared_mutex>
#include <mutex>
#include <format>

namespace engine::core {

class EngineContext {
public:
    EngineContext() = default;
    ~EngineContext() = default;

    EngineContext(const EngineContext&) = delete;
    EngineContext& operator=(const EngineContext&) = delete;

    template <typename T>
    void register_service(T* service) {
        ENGINE_ASSERT(service != nullptr, "Cannot register null service in EngineContext!");
        std::unique_lock lock(m_mutex);
        m_services[std::type_index(typeid(T))] = static_cast<void*>(service);
    }

    template <typename T>
    void unregister_service() {
        std::unique_lock lock(m_mutex);
        m_services.erase(std::type_index(typeid(T)));
    }

    template <typename T>
    [[nodiscard]] T* try_get() const noexcept {
        std::shared_lock lock(m_mutex);
        auto it = m_services.find(std::type_index(typeid(T)));
        if (it == m_services.end()) {
            return nullptr;
        }
        return static_cast<T*>(it->second);
    }

    template <typename T>
    [[nodiscard]] T& get() const {
        T* service = try_get<T>();
        if (!service) {
            LOG_FATAL("EngineContext", "Requested service '{}' is not registered!", typeid(T).name());
            ENGINE_ASSERT(service != nullptr, "Service not found in EngineContext!");
        }
        return *service;
    }

    template <typename T>
    [[nodiscard]] bool has() const noexcept {
        return try_get<T>() != nullptr;
    }

    void clear() {
        std::unique_lock lock(m_mutex);
        m_services.clear();
    }

private:
    mutable std::shared_mutex m_mutex;
    std::unordered_map<std::type_index, void*> m_services;
};

} // namespace engine::core
