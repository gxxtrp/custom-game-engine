#include "engine/events/event_bus.h"
#include "engine/core/log.h"

namespace engine::events {

EventBus& EventBus::instance() {
    static EventBus s_instance;
    return s_instance;
}

EventBus::EventBus() = default;

EventBus::~EventBus() {
    clear();
}

void EventBus::dispatch_deferred() {
    std::vector<std::function<void()>> events_to_dispatch;
    {
        std::lock_guard<std::mutex> lock(m_deferred_mutex);
        events_to_dispatch.swap(m_deferred_events);
    }

    for (const auto& dispatch_fn : events_to_dispatch) {
        if (dispatch_fn) {
            dispatch_fn();
        }
    }
}

void EventBus::clear() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_subscribers.clear();
    }
    {
        std::lock_guard<std::mutex> lock(m_deferred_mutex);
        m_deferred_events.clear();
    }
}

} // namespace engine::events
