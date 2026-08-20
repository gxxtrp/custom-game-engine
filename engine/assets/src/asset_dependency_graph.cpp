#include "engine/assets/asset_dependency_graph.h"
#include <queue>
#include <sstream>

namespace engine::assets {

AssetDependencyGraph& AssetDependencyGraph::instance() {
    static AssetDependencyGraph s_instance;
    return s_instance;
}

void AssetDependencyGraph::add_dependency(UUID parent, UUID child) {
    if (parent == child || parent.is_null() || child.is_null()) return;

    std::unique_lock lock(m_mutex);
    m_forward_deps[parent].insert(child);
    m_reverse_deps[child].insert(parent);
}

void AssetDependencyGraph::remove_dependency(UUID parent, UUID child) {
    std::unique_lock lock(m_mutex);

    auto f_it = m_forward_deps.find(parent);
    if (f_it != m_forward_deps.end()) {
        f_it->second.erase(child);
        if (f_it->second.empty()) {
            m_forward_deps.erase(f_it);
        }
    }

    auto r_it = m_reverse_deps.find(child);
    if (r_it != m_reverse_deps.end()) {
        r_it->second.erase(parent);
        if (r_it->second.empty()) {
            m_reverse_deps.erase(r_it);
        }
    }
}

void AssetDependencyGraph::set_dependencies(UUID parent, const std::vector<UUID>& children) {
    if (parent.is_null()) return;

    std::unique_lock lock(m_mutex);

    // Remove existing reverse references
    auto f_it = m_forward_deps.find(parent);
    if (f_it != m_forward_deps.end()) {
        for (const auto& old_child : f_it->second) {
            auto r_it = m_reverse_deps.find(old_child);
            if (r_it != m_reverse_deps.end()) {
                r_it->second.erase(parent);
                if (r_it->second.empty()) {
                    m_reverse_deps.erase(r_it);
                }
            }
        }
        m_forward_deps.erase(f_it);
    }

    // Insert new children
    for (const auto& child : children) {
        if (!child.is_null() && child != parent) {
            m_forward_deps[parent].insert(child);
            m_reverse_deps[child].insert(parent);
        }
    }
}

void AssetDependencyGraph::clear_dependencies(UUID parent) {
    set_dependencies(parent, {});
}

void AssetDependencyGraph::remove_asset(UUID asset) {
    if (asset.is_null()) return;

    std::unique_lock lock(m_mutex);

    // Remove forward references
    auto f_it = m_forward_deps.find(asset);
    if (f_it != m_forward_deps.end()) {
        for (const auto& child : f_it->second) {
            auto r_it = m_reverse_deps.find(child);
            if (r_it != m_reverse_deps.end()) {
                r_it->second.erase(asset);
                if (r_it->second.empty()) {
                    m_reverse_deps.erase(r_it);
                }
            }
        }
        m_forward_deps.erase(f_it);
    }

    // Remove reverse references
    auto r_it = m_reverse_deps.find(asset);
    if (r_it != m_reverse_deps.end()) {
        for (const auto& parent : r_it->second) {
            auto pf_it = m_forward_deps.find(parent);
            if (pf_it != m_forward_deps.end()) {
                pf_it->second.erase(asset);
                if (pf_it->second.empty()) {
                    m_forward_deps.erase(pf_it);
                }
            }
        }
        m_reverse_deps.erase(r_it);
    }
}

std::vector<UUID> AssetDependencyGraph::get_direct_dependencies(UUID parent) const {
    std::shared_lock lock(m_mutex);
    std::vector<UUID> result;

    auto it = m_forward_deps.find(parent);
    if (it != m_forward_deps.end()) {
        result.assign(it->second.begin(), it->second.end());
    }
    return result;
}

std::vector<UUID> AssetDependencyGraph::get_direct_dependents(UUID child) const {
    std::shared_lock lock(m_mutex);
    std::vector<UUID> result;

    auto it = m_reverse_deps.find(child);
    if (it != m_reverse_deps.end()) {
        result.assign(it->second.begin(), it->second.end());
    }
    return result;
}

bool AssetDependencyGraph::get_topological_load_order(UUID root_asset, std::vector<UUID>& out_order, bool& out_has_cycle) const {
    std::shared_lock lock(m_mutex);
    out_order.clear();
    out_has_cycle = false;

    if (root_asset.is_null()) return false;

    enum class VisitState { Unvisited = 0, Visiting, Visited };
    std::unordered_map<UUID, VisitState> states;

    auto dfs = [&](auto& self, UUID current) -> void {
        states[current] = VisitState::Visiting;

        auto it = m_forward_deps.find(current);
        if (it != m_forward_deps.end()) {
            for (const auto& child : it->second) {
                VisitState child_state = states[child];
                if (child_state == VisitState::Visiting) {
                    out_has_cycle = true;
                } else if (child_state == VisitState::Unvisited) {
                    self(self, child);
                }
            }
        }

        states[current] = VisitState::Visited;
        out_order.push_back(current);
    };

    dfs(dfs, root_asset);
    return !out_has_cycle;
}

void AssetDependencyGraph::get_all_transitive_dependents(UUID asset, std::unordered_set<UUID>& out_dependents) const {
    std::shared_lock lock(m_mutex);
    out_dependents.clear();

    if (asset.is_null()) return;

    std::queue<UUID> queue;
    queue.push(asset);

    while (!queue.empty()) {
        UUID current = queue.front();
        queue.pop();

        auto it = m_reverse_deps.find(current);
        if (it != m_reverse_deps.end()) {
            for (const auto& parent : it->second) {
                if (out_dependents.insert(parent).second) {
                    queue.push(parent);
                }
            }
        }
    }
}

void AssetDependencyGraph::get_bundle_dependencies(const std::vector<UUID>& root_assets, std::unordered_set<UUID>& out_all_assets) const {
    std::shared_lock lock(m_mutex);
    out_all_assets.clear();

    std::queue<UUID> queue;
    for (const auto& root : root_assets) {
        if (!root.is_null() && out_all_assets.insert(root).second) {
            queue.push(root);
        }
    }

    while (!queue.empty()) {
        UUID current = queue.front();
        queue.pop();

        auto it = m_forward_deps.find(current);
        if (it != m_forward_deps.end()) {
            for (const auto& child : it->second) {
                if (out_all_assets.insert(child).second) {
                    queue.push(child);
                }
            }
        }
    }
}

bool AssetDependencyGraph::has_cycle() const {
    std::shared_lock lock(m_mutex);

    enum class VisitState { Unvisited = 0, Visiting, Visited };
    std::unordered_map<UUID, VisitState> states;

    auto dfs = [&](auto& self, UUID current) -> bool {
        states[current] = VisitState::Visiting;

        auto it = m_forward_deps.find(current);
        if (it != m_forward_deps.end()) {
            for (const auto& child : it->second) {
                VisitState s = states[child];
                if (s == VisitState::Visiting) {
                    return true;
                }
                if (s == VisitState::Unvisited && self(self, child)) {
                    return true;
                }
            }
        }

        states[current] = VisitState::Visited;
        return false;
    };

    for (const auto& [parent, _] : m_forward_deps) {
        if (states[parent] == VisitState::Unvisited) {
            if (dfs(dfs, parent)) {
                return true;
            }
        }
    }
    return false;
}

void AssetDependencyGraph::clear() {
    std::unique_lock lock(m_mutex);
    m_forward_deps.clear();
    m_reverse_deps.clear();
}

std::string AssetDependencyGraph::export_dot_graph() const {
    std::shared_lock lock(m_mutex);
    std::stringstream ss;
    ss << "digraph AssetDependencies {\n";
    ss << "  rankdir=LR;\n";
    ss << "  node [shape=box, fontname=\"Arial\"];\n";

    for (const auto& [parent, children] : m_forward_deps) {
        for (const auto& child : children) {
            ss << "  \"" << parent.to_string() << "\" -> \"" << child.to_string() << "\";\n";
        }
    }

    ss << "}\n";
    return ss.str();
}

} // namespace engine::assets
