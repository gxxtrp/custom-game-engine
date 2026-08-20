#pragma once

#include "engine/core/config.h"
#include <functional>
#include <vector>
#include <unordered_map>
#include <typeindex>
#include <memory>
#include <mutex>
#include <atomic>

namespace engine::events {

using SubscriptionId = uint64_t;

class ScopedSubscription;

class EventBus {
public:
    static EventBus& instance();

    template<typename EventType>
    [[nodiscard]] SubscriptionId subscribe(std::function<void(const EventType&)> callback) {
        std::type_index type = std::type_index(typeid(EventType));
        SubscriptionId id = m_next_sub_id.fetch_add(1, std::memory_order_relaxed);

        std::lock_guard<std::mutex> lock(m_mutex);
        auto& handlers = m_subscribers[type];

        handlers.push_back(std::make_shared<TypedHandler<EventType>>(id, std::move(callback)));
        return id;
    }

    template<typename EventType>
    void unsubscribe(SubscriptionId id) {
        std::type_index type = std::type_index(typeid(EventType));
        std::lock_guard<std::mutex> lock(m_mutex);

        auto it = m_subscribers.find(type);
        if (it != m_subscribers.end()) {
            auto& list = it->second;
            for (auto list_it = list.begin(); list_it != list.end(); ++list_it) {
                if ((*list_it)->id == id) {
                    list.erase(list_it);
                    break;
                }
            }
        }
    }

    template<typename EventType>
    void publish(const EventType& event) {
        std::type_index type = std::type_index(typeid(EventType));
        std::vector<std::shared_ptr<IHandler>> handlers_copy;

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_subscribers.find(type);
            if (it != m_subscribers.end()) {
                handlers_copy = it->second;
            }
        }

        for (const auto& handler : handlers_copy) {
            static_cast<TypedHandler<EventType>*>(handler.get())->callback(event);
        }
    }

    template<typename EventType>
    void publish_deferred(const EventType& event) {
        std::lock_guard<std::mutex> lock(m_deferred_mutex);
        m_deferred_events.push_back([this, event]() {
            publish(event);
        });
    }

    template<typename EventType>
    [[nodiscard]] ScopedSubscription subscribe_scoped(std::function<void(const EventType&)> callback);

    void dispatch_deferred();
    void clear();

private:
    EventBus();
    ~EventBus();

    struct IHandler {
        SubscriptionId id{0};
        explicit IHandler(SubscriptionId id_) : id(id_) {}
        virtual ~IHandler() = default;
    };

    template<typename EventType>
    struct TypedHandler : public IHandler {
        std::function<void(const EventType&)> callback;
        TypedHandler(SubscriptionId id_, std::function<void(const EventType&)> cb)
            : IHandler(id_), callback(std::move(cb)) {}
    };

    std::atomic<SubscriptionId> m_next_sub_id{1};
    std::unordered_map<std::type_index, std::vector<std::shared_ptr<IHandler>>> m_subscribers;
    std::mutex m_mutex;

    std::vector<std::function<void()>> m_deferred_events;
    std::mutex m_deferred_mutex;
};

// RAII Scoped Subscription
class ScopedSubscription {
public:
    ScopedSubscription() = default;

    ScopedSubscription(SubscriptionId id, std::function<void()> unsubscriber)
        : m_id(id), m_unsubscriber(std::move(unsubscriber)) {}

    ~ScopedSubscription() {
        unsubscribe();
    }

    ScopedSubscription(const ScopedSubscription&) = delete;
    ScopedSubscription& operator=(const ScopedSubscription&) = delete;

    ScopedSubscription(ScopedSubscription&& other) noexcept
        : m_id(other.m_id), m_unsubscriber(std::move(other.m_unsubscriber)) {
        other.m_id = 0;
        other.m_unsubscriber = nullptr;
    }

    ScopedSubscription& operator=(ScopedSubscription&& other) noexcept {
        if (this != &other) {
            unsubscribe();
            m_id = other.m_id;
            m_unsubscriber = std::move(other.m_unsubscriber);
            other.m_id = 0;
            other.m_unsubscriber = nullptr;
        }
        return *this;
    }

    void unsubscribe() {
        if (m_unsubscriber && m_id != 0) {
            m_unsubscriber();
            m_id = 0;
            m_unsubscriber = nullptr;
        }
    }

    bool is_active() const { return m_id != 0; }

private:
    SubscriptionId m_id{0};
    std::function<void()> m_unsubscriber{nullptr};
};

template<typename EventType>
inline ScopedSubscription EventBus::subscribe_scoped(std::function<void(const EventType&)> callback) {
    SubscriptionId id = subscribe<EventType>(std::move(callback));
    return ScopedSubscription(id, [this, id]() {
        this->unsubscribe<EventType>(id);
    });
}

} // namespace engine::events
