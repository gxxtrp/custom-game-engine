#include "engine/core/engine_kernel.h"
#include "engine/core/log.h"
#include <queue>
#include <format>

namespace engine::core {

std::unique_ptr<EngineKernel> KernelBuilder::build() {
    auto kernel = std::make_unique<EngineKernel>();
    for (auto& entry : m_entries) {
        kernel->m_raw_nodes.push_back(EngineKernel::SubsystemNode{
            .type_id = entry.type_id,
            .subsystem = std::move(entry.subsystem)
        });
    }
    m_entries.clear();
    return kernel;
}

EngineKernel::EngineKernel() = default;

EngineKernel::~EngineKernel() {
    shutdown();
}

bool EngineKernel::topological_sort(std::vector<SubsystemNode>& input_nodes) {
    const size_t n = input_nodes.size();
    if (n == 0) return true;

    std::unordered_map<std::type_index, size_t> type_to_node_idx;
    for (size_t i = 0; i < n; ++i) {
        type_to_node_idx[input_nodes[i].type_id] = i;
    }

    std::vector<std::vector<size_t>> adj(n);
    std::vector<size_t> in_degree(n, 0);

    for (size_t i = 0; i < n; ++i) {
        SubsystemDependencyBuilder builder;
        input_nodes[i].subsystem->declare_dependencies(builder);

        for (const auto& dep_type : builder.get_dependencies()) {
            auto it = type_to_node_idx.find(dep_type);
            if (it == type_to_node_idx.end()) {
                LOG_FATAL("EngineKernel", "Subsystem '{}' requires an unregistered dependency '{}'!",
                          input_nodes[i].subsystem->get_name(), dep_type.name());
                return false;
            }

            size_t dep_idx = it->second;
            // Edge: dep_idx -> i (dep_idx must initialize before i)
            adj[dep_idx].push_back(i);
            in_degree[i]++;
        }
    }

    std::queue<size_t> q;
    for (size_t i = 0; i < n; ++i) {
        if (in_degree[i] == 0) {
            q.push(i);
        }
    }

    std::vector<size_t> sorted_indices;
    sorted_indices.reserve(n);

    while (!q.empty()) {
        size_t u = q.front();
        q.pop();
        sorted_indices.push_back(u);

        for (size_t v : adj[u]) {
            in_degree[v]--;
            if (in_degree[v] == 0) {
                q.push(v);
            }
        }
    }

    if (sorted_indices.size() != n) {
        LOG_FATAL("EngineKernel", "Cyclic dependency detected among registered subsystems!");
        for (size_t i = 0; i < n; ++i) {
            if (in_degree[i] > 0) {
                LOG_ERROR("EngineKernel", "  -> Cycle member: '{}' (remaining in-degree: {})",
                          input_nodes[i].subsystem->get_name(), in_degree[i]);
            }
        }
        return false;
    }

    m_sorted_subsystems.clear();
    m_sorted_subsystems.reserve(n);
    m_type_to_sorted_index.clear();

    for (size_t i = 0; i < n; ++i) {
        size_t orig_idx = sorted_indices[i];
        m_type_to_sorted_index[input_nodes[orig_idx].type_id] = i;
        m_sorted_subsystems.push_back(std::move(input_nodes[orig_idx].subsystem));
    }

    return true;
}

bool EngineKernel::initialize() {
    if (m_initialized) return true;

    LOG_INFO("EngineKernel", "Sorting and initializing subsystem DAG...");
    if (!topological_sort(m_raw_nodes)) {
        LOG_FATAL("EngineKernel", "Failed to resolve subsystem dependency graph!");
        return false;
    }
    m_raw_nodes.clear();

    // Initialize all subsystems in topological order
    for (size_t i = 0; i < m_sorted_subsystems.size(); ++i) {
        auto& sub = m_sorted_subsystems[i];
        LOG_INFO("EngineKernel", "  [{}/{}] Initializing Subsystem: '{}'",
                 i + 1, m_sorted_subsystems.size(), sub->get_name());

        if (!sub->initialize(m_context)) {
            LOG_FATAL("EngineKernel", "Failed to initialize subsystem '{}'! Shutting down initialized subsystems...",
                      sub->get_name());

            // Reverse shutdown of already initialized subsystems
            for (size_t j = i; j > 0; --j) {
                m_sorted_subsystems[j - 1]->shutdown(m_context);
            }
            m_context.clear();
            return false;
        }
    }

    m_initialized = true;
    LOG_INFO("EngineKernel", "Subsystem DAG initialized successfully ({} subsystems active).",
             m_sorted_subsystems.size());
    return true;
}

void EngineKernel::tick(ExecutionPhase phase, float dt) {
    if (!m_initialized) return;

    for (auto& sub : m_sorted_subsystems) {
        if (sub->participates_in_phase(phase)) {
            sub->tick(m_context, phase, dt);
        }
    }
}

void EngineKernel::step_frame(float dt) {
    if (!m_initialized) return;

    tick(ExecutionPhase::PreTick, dt);
    tick(ExecutionPhase::Simulation, dt);
    tick(ExecutionPhase::PostSimulation, dt);
    tick(ExecutionPhase::Render, dt);
    tick(ExecutionPhase::Present, dt);
}

void EngineKernel::shutdown() {
    if (!m_initialized) return;

    LOG_INFO("EngineKernel", "Shutting down subsystem DAG in reverse topological order...");
    for (size_t i = m_sorted_subsystems.size(); i > 0; --i) {
        auto& sub = m_sorted_subsystems[i - 1];
        LOG_INFO("EngineKernel", "  Shutting down Subsystem: '{}'", sub->get_name());
        sub->shutdown(m_context);
    }

    m_sorted_subsystems.clear();
    m_type_to_sorted_index.clear();
    m_context.clear();
    m_initialized = false;
    LOG_INFO("EngineKernel", "All subsystems shut down cleanly.");
}

} // namespace engine::core
