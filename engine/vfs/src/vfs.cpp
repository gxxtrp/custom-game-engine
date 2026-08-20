#include "engine/vfs/vfs.h"
#include "engine/vfs/pak_archive.h"
#include "engine/core/log.h"
#include <algorithm>

namespace engine::vfs {

VFS& VFS::instance() {
    static VFS s_instance;
    return s_instance;
}

VFS::VFS() = default;

VFS::~VFS() {
    unmount_all();
}

std::string VFS::normalize_path(std::string_view path) {
    std::string result;
    result.reserve(path.size());

    for (char c : path) {
        if (c == '\\') {
            result.push_back('/');
        } else {
            result.push_back(c);
        }
    }

    // Collapse multiple consecutive slashes
    std::string collapsed;
    collapsed.reserve(result.size());
    bool last_slash = false;
    for (char c : result) {
        if (c == '/') {
            if (!last_slash) collapsed.push_back(c);
            last_slash = true;
        } else {
            collapsed.push_back(c);
            last_slash = false;
        }
    }

    // Trim trailing slash (unless it's just "/")
    if (collapsed.size() > 1 && collapsed.back() == '/') {
        collapsed.pop_back();
    }

    return collapsed;
}

bool VFS::mount(std::string_view virtual_prefix, std::shared_ptr<IMountPoint> provider, int32_t priority) {
    if (!provider) return false;

    std::string norm_prefix = normalize_path(virtual_prefix);
    if (!norm_prefix.empty() && norm_prefix.front() != '/') {
        norm_prefix = "/" + norm_prefix;
    }

    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_mounts.push_back(MountEntry{
        .virtual_prefix = norm_prefix,
        .provider = std::move(provider),
        .priority = priority
    });

    // Sort mounts by priority descending, then by prefix length descending (most specific first)
    std::sort(m_mounts.begin(), m_mounts.end(), [](const MountEntry& a, const MountEntry& b) {
        if (a.priority != b.priority) return a.priority > b.priority;
        return a.virtual_prefix.length() > b.virtual_prefix.length();
    });

    LOG_INFO("VFS", "Mounted virtual path '{}' (priority: {})", norm_prefix, priority);
    return true;
}

bool VFS::mount_physical(std::string_view virtual_prefix, const std::string& physical_path, int32_t priority, bool read_only) {
    auto mount_point = std::make_shared<PhysicalMountPoint>(physical_path, read_only);
    return mount(virtual_prefix, mount_point, priority);
}

bool VFS::mount_pak(std::string_view virtual_prefix, const std::string& pak_path, int32_t priority) {
    auto pak = std::make_shared<PakArchiveMountPoint>(pak_path);
    if (!pak->is_valid()) return false;
    return mount(virtual_prefix, pak, priority);
}

void VFS::unmount(std::string_view virtual_prefix) {
    std::string norm = normalize_path(virtual_prefix);
    if (!norm.empty() && norm.front() != '/') norm = "/" + norm;

    std::unique_lock<std::shared_mutex> lock(m_mutex);
    std::erase_if(m_mounts, [&](const MountEntry& entry) {
        return entry.virtual_prefix == norm;
    });

    LOG_INFO("VFS", "Unmounted virtual path '{}'", norm);
}

void VFS::unmount_all() {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_mounts.clear();
}

bool VFS::find_mount_point(std::string_view virtual_path, std::shared_ptr<IMountPoint>& out_provider, std::string& out_relative_path) {
    std::string norm = normalize_path(virtual_path);
    if (!norm.empty() && norm.front() != '/') norm = "/" + norm;

    std::shared_lock<std::shared_mutex> lock(m_mutex);

    for (const auto& mount : m_mounts) {
        if (norm.starts_with(mount.virtual_prefix)) {
            std::string_view rel = std::string_view(norm).substr(mount.virtual_prefix.length());
            while (!rel.empty() && rel.front() == '/') rel.remove_prefix(1);

            out_provider = mount.provider;
            out_relative_path = std::string(rel);
            return true;
        }
    }

    return false;
}

bool VFS::read_bytes(std::string_view virtual_path, std::vector<uint8_t>& out_bytes) {
    std::shared_ptr<IMountPoint> provider;
    std::string rel_path;
    if (!find_mount_point(virtual_path, provider, rel_path)) {
        LOG_WARN("VFS", "No mount point found for virtual path: {}", virtual_path);
        return false;
    }
    return provider->read_bytes(rel_path, out_bytes);
}

bool VFS::read_string(std::string_view virtual_path, std::string& out_str) {
    std::shared_ptr<IMountPoint> provider;
    std::string rel_path;
    if (!find_mount_point(virtual_path, provider, rel_path)) {
        LOG_WARN("VFS", "No mount point found for virtual path: {}", virtual_path);
        return false;
    }
    return provider->read_string(rel_path, out_str);
}

bool VFS::write_bytes(std::string_view virtual_path, const void* data, size_t size) {
    std::shared_ptr<IMountPoint> provider;
    std::string rel_path;
    if (!find_mount_point(virtual_path, provider, rel_path)) {
        LOG_ERROR("VFS", "No mount point found for write to: {}", virtual_path);
        return false;
    }
    return provider->write_bytes(rel_path, data, size);
}

bool VFS::write_string(std::string_view virtual_path, std::string_view content) {
    std::shared_ptr<IMountPoint> provider;
    std::string rel_path;
    if (!find_mount_point(virtual_path, provider, rel_path)) {
        LOG_ERROR("VFS", "No mount point found for write to: {}", virtual_path);
        return false;
    }
    return provider->write_string(rel_path, content);
}

bool VFS::file_exists(std::string_view virtual_path) {
    std::shared_ptr<IMountPoint> provider;
    std::string rel_path;
    if (!find_mount_point(virtual_path, provider, rel_path)) return false;
    return provider->file_exists(rel_path);
}

size_t VFS::get_file_size(std::string_view virtual_path) {
    std::shared_ptr<IMountPoint> provider;
    std::string rel_path;
    if (!find_mount_point(virtual_path, provider, rel_path)) return 0;
    return provider->get_file_size(rel_path);
}

std::string VFS::resolve_physical_path(std::string_view virtual_path) {
    std::shared_ptr<IMountPoint> provider;
    std::string rel_path;
    if (!find_mount_point(virtual_path, provider, rel_path)) return "";
    return provider->get_physical_path(rel_path);
}

jobs::JobHandle VFS::read_async(std::string_view virtual_path,
                                std::function<void(std::vector<uint8_t> data, bool success)> callback,
                                jobs::JobPriority priority) {
    std::string path_copy(virtual_path);

    return jobs::JobSystem::instance().dispatch([this, path = std::move(path_copy), cb = std::move(callback)]() {
        std::vector<uint8_t> buffer;
        bool ok = read_bytes(path, buffer);
        if (cb) {
            cb(std::move(buffer), ok);
        }
    }, priority, "VFS_ReadAsync");
}

} // namespace engine::vfs
