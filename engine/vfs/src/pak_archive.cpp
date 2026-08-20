#include "engine/vfs/pak_archive.h"
#include "engine/vfs/vfs.h"
#include "engine/core/log.h"
#include <filesystem>
#include <cstring>

namespace engine::vfs {

PakArchiveMountPoint::PakArchiveMountPoint(std::string pak_path)
    : m_pak_path(std::move(pak_path)) {
    m_file_stream.open(m_pak_path, std::ios::binary);
    if (!m_file_stream.is_open()) {
        LOG_ERROR("VFS", "Failed to open Pak archive: {}", m_pak_path);
        return;
    }

    PakHeader header{};
    m_file_stream.read(reinterpret_cast<char*>(&header), sizeof(PakHeader));
    if (std::memcmp(header.magic, "EPAK", 4) != 0 || header.version != 1) {
        LOG_ERROR("VFS", "Invalid Pak archive header in: {}", m_pak_path);
        return;
    }

    m_file_stream.seekg(static_cast<std::streamoff>(header.toc_offset), std::ios::beg);
    std::vector<PakEntry> entries(header.entry_count);
    m_file_stream.read(reinterpret_cast<char*>(entries.data()), sizeof(PakEntry) * header.entry_count);

    for (const auto& entry : entries) {
        std::string path_str(entry.path);
        m_entries[VFS::normalize_path(path_str)] = entry;
    }

    m_valid = true;
    LOG_INFO("VFS", "Loaded Pak archive '{}' with {} entries", m_pak_path, header.entry_count);
}

PakArchiveMountPoint::~PakArchiveMountPoint() {
    if (m_file_stream.is_open()) {
        m_file_stream.close();
    }
}

bool PakArchiveMountPoint::read_bytes(const std::string& relative_path, std::vector<uint8_t>& out_bytes) {
    if (!m_valid) return false;

    std::string norm = VFS::normalize_path(relative_path);
    while (!norm.empty() && norm.front() == '/') norm.erase(0, 1);

    auto it = m_entries.find(norm);
    if (it == m_entries.end()) return false;

    const auto& entry = it->second;
    std::lock_guard<std::mutex> lock(m_file_mutex);

    m_file_stream.clear();
    m_file_stream.seekg(static_cast<std::streamoff>(entry.offset), std::ios::beg);
    out_bytes.resize(static_cast<size_t>(entry.size));
    if (entry.size > 0) {
        m_file_stream.read(reinterpret_cast<char*>(out_bytes.data()), static_cast<std::streamsize>(entry.size));
    }

    return m_file_stream.good();
}

bool PakArchiveMountPoint::read_string(const std::string& relative_path, std::string& out_str) {
    std::vector<uint8_t> bytes;
    if (!read_bytes(relative_path, bytes)) return false;
    out_str.assign(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    return true;
}

bool PakArchiveMountPoint::write_bytes(const std::string& /*relative_path*/, const void* /*data*/, size_t /*size*/) {
    return false;
}

bool PakArchiveMountPoint::write_string(const std::string& /*relative_path*/, std::string_view /*content*/) {
    return false;
}

bool PakArchiveMountPoint::file_exists(const std::string& relative_path) {
    if (!m_valid) return false;
    std::string norm = VFS::normalize_path(relative_path);
    while (!norm.empty() && norm.front() == '/') norm.erase(0, 1);
    return m_entries.contains(norm);
}

size_t PakArchiveMountPoint::get_file_size(const std::string& relative_path) {
    if (!m_valid) return 0;
    std::string norm = VFS::normalize_path(relative_path);
    while (!norm.empty() && norm.front() == '/') norm.erase(0, 1);

    auto it = m_entries.find(norm);
    if (it != m_entries.end()) {
        return static_cast<size_t>(it->second.size);
    }
    return 0;
}

std::string PakArchiveMountPoint::get_physical_path(const std::string& /*relative_path*/) const {
    return m_pak_path;
}

bool PakArchiveMountPoint::create_pak(const std::string& output_pak_path, 
                                     const std::vector<std::pair<std::string, std::string>>& relative_to_physical_files) {
    std::filesystem::path p(output_pak_path);
    std::error_code ec;
    std::filesystem::create_directories(p.parent_path(), ec);

    std::ofstream out(output_pak_path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        LOG_ERROR("VFS", "Failed to create pak file: {}", output_pak_path);
        return false;
    }

    PakHeader header{};
    header.entry_count = static_cast<uint32_t>(relative_to_physical_files.size());
    out.write(reinterpret_cast<const char*>(&header), sizeof(PakHeader));

    std::vector<PakEntry> entries;
    entries.reserve(relative_to_physical_files.size());

    for (const auto& [rel_path, phys_path] : relative_to_physical_files) {
        std::ifstream src(phys_path, std::ios::binary | std::ios::ate);
        if (!src.is_open()) {
            LOG_WARN("VFS", "Skipping missing file for pak packaging: {}", phys_path);
            continue;
        }

        std::streamsize size = src.tellg();
        src.seekg(0, std::ios::beg);

        std::vector<uint8_t> buffer(static_cast<size_t>(size));
        src.read(reinterpret_cast<char*>(buffer.data()), size);

        PakEntry entry{};
        std::string norm_rel = VFS::normalize_path(rel_path);
        while (!norm_rel.empty() && norm_rel.front() == '/') norm_rel.erase(0, 1);

        uint64_t current_offset = static_cast<uint64_t>(out.tellp());
        std::memset(entry.path, 0, sizeof(entry.path));
        size_t len = std::min(norm_rel.length(), sizeof(entry.path) - 1);
        std::memcpy(entry.path, norm_rel.data(), len);
        entry.offset = current_offset;
        entry.size = static_cast<uint64_t>(size);
        entry.flags = 0;

        out.write(reinterpret_cast<const char*>(buffer.data()), size);
        entries.push_back(entry);
    }

    uint64_t toc_offset = static_cast<uint64_t>(out.tellp());
    uint64_t toc_size = sizeof(PakEntry) * entries.size();

    out.write(reinterpret_cast<const char*>(entries.data()), static_cast<std::streamsize>(toc_size));

    // Rewind and write header with TOC info
    header.entry_count = static_cast<uint32_t>(entries.size());
    header.toc_offset = toc_offset;
    header.toc_size = toc_size;

    out.seekp(0, std::ios::beg);
    out.write(reinterpret_cast<const char*>(&header), sizeof(PakHeader));

    LOG_INFO("VFS", "Created Pak archive '{}' with {} files (TOC size: {} bytes)", output_pak_path, entries.size(), toc_size);
    return true;
}

} // namespace engine::vfs
