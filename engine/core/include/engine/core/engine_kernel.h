#pragma once

#include "engine/core/config.h"
#include "engine/core/subsystem.h"
#include "engine/core/engine_context.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <typeindex>

namespace engine::core {

class EngineKernel;

class KernelBuilder {
public:
    KernelBuilder() = default;
    ~KernelBuilder() = default;

    template <typename T, typename... Args>
    KernelBuilder& add_subsystem(Args&&... args) {
        auto sub = std::make_unique<T>(std::forward<Args>(args)...);
        m_entries.push_back(Entry{
            .type_id = std::type_index(typeid(T)),
            .subsystem = std::move(sub)
        });
        return *this;
    }

    KernelBuilder& add_subsystem_instance(std::type_index type_id, std::unique_ptr<ISubsystem> subsystem) {
        m_entries.push_back(Entry{
            .type_id = type_id,
            .subsystem = std::move(subsystem)
        });
        return *this;
    }

    [[nodiscard]] std::unique_ptr<EngineKernel> build();

private:
    struct Entry {
        std::type_index type_id;
        std::unique_ptr<ISubsystem> subsystem;
    };
    std::vector<Entry> m_entries;
};

class EngineKernel {
public:
    EngineKernel();
    ~EngineKernel();

    EngineKernel(const EngineKernel&) = delete;
    EngineKernel& operator=(const EngineKernel&) = delete;

    [[nodiscard]] bool initialize();
    void tick(ExecutionPhase phase, float dt);
    void step_frame(float dt);
    void shutdown();

    [[nodiscard]] bool is_initialized() const noexcept { return m_initialized; }
    [[nodiscard]] EngineContext& get_context() noexcept { return m_context; }
    [[nodiscard]] const EngineContext& get_context() const noexcept { return m_context; }

    [[nodiscard]] const std::vector<std::unique_ptr<ISubsystem>>& get_subsystems() const noexcept {
        return m_sorted_subsystems;
    }

private:
    friend class KernelBuilder;

    struct SubsystemNode {
        std::type_index type_id;
        std::unique_ptr<ISubsystem> subsystem;
    };

    [[nodiscard]] bool topological_sort(std::vector<SubsystemNode>& input_nodes);

    EngineContext m_context;
    std::vector<SubsystemNode> m_raw_nodes;
    std::vector<std::unique_ptr<ISubsystem>> m_sorted_subsystems;
    std::unordered_map<std::type_index, size_t> m_type_to_sorted_index;
    bool m_initialized{false};
};

} // namespace engine::core
