#include "engine/vfs/mount_point.h"
#include "engine/vfs/vfs.h"
#include "engine/core/log.h"
#include <filesystem>
#include <fstream>

namespace engine::vfs {

PhysicalMountPoint::PhysicalMountPoint(std::string physical_root, bool read_only)
    : m_physical_root(VFS::normalize_path(physical_root)), m_read_only(read_only) {
    if (!std::filesystem::exists(m_physical_root)) {
        std::error_code ec;
        std::filesystem::create_directories(m_physical_root, ec);
    }
}

std::string PhysicalMountPoint::resolve_path(const std::string& relative_path) const {
    std::string norm_rel = VFS::normalize_path(relative_path);
    while (!norm_rel.empty() && norm_rel.front() == '/') {
        norm_rel.erase(0, 1);
    }

    std::filesystem::path root(m_physical_root);
    std::filesystem::path rel(norm_rel);
    return (root / rel).lexically_normal().string();
}

std::string PhysicalMountPoint::get_physical_path(const std::string& relative_path) const {
    return resolve_path(relative_path);
}

bool PhysicalMountPoint::read_bytes(const std::string& relative_path, std::vector<uint8_t>& out_bytes) {
    std::string full_path = resolve_path(relative_path);
    std::ifstream file(full_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    out_bytes.resize(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(out_bytes.data()), size)) {
        out_bytes.clear();
        return false;
    }
    return true;
}

bool PhysicalMountPoint::read_string(const std::string& relative_path, std::string& out_str) {
    std::vector<uint8_t> bytes;
    if (!read_bytes(relative_path, bytes)) return false;
    out_str.assign(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    return true;
}

bool PhysicalMountPoint::write_bytes(const std::string& relative_path, const void* data, size_t size) {
    if (m_read_only) return false;

    std::string full_path = resolve_path(relative_path);
    std::filesystem::path p(full_path);
    std::error_code ec;
    std::filesystem::create_directories(p.parent_path(), ec);

    std::ofstream file(full_path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) return false;

    file.write(reinterpret_cast<const char*>(data), size);
    return file.good();
}

bool PhysicalMountPoint::write_string(const std::string& relative_path, std::string_view content) {
    if (m_read_only) return false;

    std::string full_path = resolve_path(relative_path);
    std::filesystem::path p(full_path);
    std::error_code ec;
    std::filesystem::create_directories(p.parent_path(), ec);

    std::ofstream file(full_path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) return false;

    file.write(content.data(), content.size());
    file.flush();
    return file.good();
}

bool PhysicalMountPoint::file_exists(const std::string& relative_path) {
    return std::filesystem::exists(resolve_path(relative_path));
}

size_t PhysicalMountPoint::get_file_size(const std::string& relative_path) {
    std::string path = resolve_path(relative_path);
    if (!std::filesystem::exists(path)) return 0;
    return std::filesystem::file_size(path);
}

} // namespace engine::vfs
