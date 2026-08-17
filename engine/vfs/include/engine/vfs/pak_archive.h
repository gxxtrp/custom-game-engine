#pragma once

#include "engine/vfs/mount_point.h"
#include <unordered_map>
#include <fstream>
#include <mutex>

namespace engine::vfs {

#pragma pack(push, 1)
struct PakHeader {
    char magic[4] = {'E', 'P', 'A', 'K'};
    uint32_t version = 1;
    uint32_t entry_count = 0;
    uint64_t toc_offset = 0;
    uint64_t toc_size = 0;
};

struct PakEntry {
    char path[128] = {0};
    uint64_t offset = 0;
    uint64_t size = 0;
    uint32_t flags = 0; // 0 = uncompressed
};
#pragma pack(pop)

class PakArchiveMountPoint : public IMountPoint {
public:
    explicit PakArchiveMountPoint(std::string pak_path);
    ~PakArchiveMountPoint() override;

    bool is_valid() const { return m_valid; }

    bool read_bytes(const std::string& relative_path, std::vector<uint8_t>& out_bytes) override;
    bool read_string(const std::string& relative_path, std::string& out_str) override;
    bool write_bytes(const std::string& relative_path, const void* data, size_t size) override;
    bool write_string(const std::string& relative_path, std::string_view content) override;
    bool file_exists(const std::string& relative_path) override;
    size_t get_file_size(const std::string& relative_path) override;
    std::string get_physical_path(const std::string& relative_path) const override;
    bool is_read_only() const override { return true; }

    // Static helper to create a pak file from a directory/files
    static bool create_pak(const std::string& output_pak_path, 
                           const std::vector<std::pair<std::string, std::string>>& relative_to_physical_files);

private:
    std::string m_pak_path;
    bool m_valid{false};
    std::unordered_map<std::string, PakEntry> m_entries;
    mutable std::ifstream m_file_stream;
    mutable std::mutex m_file_mutex;
};

} // namespace engine::vfs
