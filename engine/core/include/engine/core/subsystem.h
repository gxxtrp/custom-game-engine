#pragma once

#include "engine/core/config.h"
#include <cstdint>
#include <string_view>
#include <vector>
#include <typeindex>
#include <typeinfo>

namespace engine::core {

class EngineContext;

enum class ExecutionPhase : uint8_t {
    PreTick = 0,      // Platform events, window processing, input polling, delta calculation
    Simulation,       // Fixed-timestep physics, Flecs scene progress, Lua script controllers
    PostSimulation,   // Transform hierarchy updates, audio emitter sync, camera/frustum extraction
    Render,           // Scene extraction, RenderGraph compilation, GPU command recording
    Present,          // Frame submission to IViewportPresenter, fence synchronization
    Count
};

[[nodiscard]] constexpr const char* execution_phase_to_string(ExecutionPhase phase) noexcept {
    switch (phase) {
        case ExecutionPhase::PreTick:        return "PreTick";
        case ExecutionPhase::Simulation:     return "Simulation";
        case ExecutionPhase::PostSimulation: return "PostSimulation";
        case ExecutionPhase::Render:         return "Render";
        case ExecutionPhase::Present:        return "Present";
        default:                             return "Unknown";
    }
}

class SubsystemDependencyBuilder {
public:
    template <typename TDependency>
    SubsystemDependencyBuilder& require() {
        m_dependencies.push_back(std::type_index(typeid(TDependency)));
        return *this;
    }

    template <typename TDependency>
    SubsystemDependencyBuilder& depends_on() {
        return require<TDependency>();
    }

    [[nodiscard]] const std::vector<std::type_index>& get_dependencies() const noexcept {
        return m_dependencies;
    }

private:
    std::vector<std::type_index> m_dependencies;
};

class ISubsystem {
public:
    virtual ~ISubsystem() = default;

    [[nodiscard]] virtual const char* get_name() const noexcept = 0;

    virtual void declare_dependencies(SubsystemDependencyBuilder& builder) {
        (void)builder;
    }

    virtual bool initialize(EngineContext& context) = 0;

    virtual void tick(EngineContext& context, ExecutionPhase phase, float dt) {
        (void)context;
        (void)phase;
        (void)dt;
    }

    virtual void shutdown(EngineContext& context) = 0;

    [[nodiscard]] virtual bool participates_in_phase(ExecutionPhase phase) const noexcept {
        (void)phase;
        return true;
    }
};

} // namespace engine::core
