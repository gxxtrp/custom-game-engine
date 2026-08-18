#pragma once

#include "engine/core/config.h"
#include "engine/assets/uuid.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <shared_mutex>
#include <string>

namespace engine::assets {

class AssetDependencyGraph {
public:
    static AssetDependencyGraph& instance();

    void add_dependency(UUID parent, UUID child);
    void remove_dependency(UUID parent, UUID child);
    void set_dependencies(UUID parent, const std::vector<UUID>& children);
    void clear_dependencies(UUID parent);
    void remove_asset(UUID asset);

    std::vector<UUID> get_direct_dependencies(UUID parent) const;
    std::vector<UUID> get_direct_dependents(UUID child) const;

    // Collect all transitive dependencies in topological load order (leaf dependencies first)
    bool get_topological_load_order(UUID root_asset, std::vector<UUID>& out_order, bool& out_has_cycle) const;

    // Collect all assets that directly or indirectly depend on this asset (for cascading hot-reload/invalidation)
    void get_all_transitive_dependents(UUID asset, std::unordered_set<UUID>& out_dependents) const;

    // Collect all transitive dependencies needed to cook/bundle root assets
    void get_bundle_dependencies(const std::vector<UUID>& root_assets, std::unordered_set<UUID>& out_all_assets) const;

    bool has_cycle() const;
    void clear();

    std::string export_dot_graph() const;

private:
    AssetDependencyGraph() = default;

    mutable std::shared_mutex m_mutex;
    // parent -> set of children (forward dependencies)
    std::unordered_map<UUID, std::unordered_set<UUID>> m_forward_deps;
    // child -> set of parents (reverse references)
    std::unordered_map<UUID, std::unordered_set<UUID>> m_reverse_deps;
};

} // namespace engine::assets
