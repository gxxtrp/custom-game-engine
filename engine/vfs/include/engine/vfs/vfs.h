#pragma once

#include "engine/core/config.h"
#include "engine/vfs/mount_point.h"
#include "engine/jobs/job_system.h"
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <shared_mutex>

namespace engine::vfs {

struct MountEntry {
    std::string virtual_prefix;
    std::shared_ptr<IMountPoint> provider;
    int32_t priority{0};
};

class VFS {
public:
    static VFS& instance();

    bool mount(std::string_view virtual_prefix, std::shared_ptr<IMountPoint> provider, int32_t priority = 0);
    bool mount_physical(std::string_view virtual_prefix, const std::string& physical_path, int32_t priority = 0, bool read_only = false);
    bool mount_pak(std::string_view virtual_prefix, const std::string& pak_path, int32_t priority = 0);
    void unmount(std::string_view virtual_prefix);
    void unmount_all();

    bool read_bytes(std::string_view virtual_path, std::vector<uint8_t>& out_bytes);
    bool read_string(std::string_view virtual_path, std::string& out_str);

    bool write_bytes(std::string_view virtual_path, const void* data, size_t size);
    bool write_string(std::string_view virtual_path, std::string_view content);

    bool file_exists(std::string_view virtual_path);
    size_t get_file_size(std::string_view virtual_path);

    std::string resolve_physical_path(std::string_view virtual_path);

    // Asynchronous I/O via JobSystem
    jobs::JobHandle read_async(std::string_view virtual_path,
                               std::function<void(std::vector<uint8_t> data, bool success)> callback,
                               jobs::JobPriority priority = jobs::JobPriority::Normal);

    static std::string normalize_path(std::string_view path);

private:
    VFS();
    ~VFS();

    bool find_mount_point(std::string_view virtual_path, std::shared_ptr<IMountPoint>& out_provider, std::string& out_relative_path);

    std::vector<MountEntry> m_mounts;
    std::shared_mutex m_mutex;
};

} // namespace engine::vfs
