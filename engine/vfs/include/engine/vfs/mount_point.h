#pragma once

#include "engine/core/config.h"
#include <string>
#include <string_view>
#include <vector>
#include <cstdint>
#include <memory>

namespace engine::vfs {

class IMountPoint {
public:
    virtual ~IMountPoint() = default;

    virtual bool read_bytes(const std::string& relative_path, std::vector<uint8_t>& out_bytes) = 0;
    virtual bool read_string(const std::string& relative_path, std::string& out_str) = 0;
    virtual bool write_bytes(const std::string& relative_path, const void* data, size_t size) = 0;
    virtual bool write_string(const std::string& relative_path, std::string_view content) = 0;
    virtual bool file_exists(const std::string& relative_path) = 0;
    virtual size_t get_file_size(const std::string& relative_path) = 0;
    virtual std::string get_physical_path(const std::string& relative_path) const = 0;
    virtual bool is_read_only() const = 0;
};

class PhysicalMountPoint : public IMountPoint {
public:
    explicit PhysicalMountPoint(std::string physical_root, bool read_only = false);
    ~PhysicalMountPoint() override = default;

    bool read_bytes(const std::string& relative_path, std::vector<uint8_t>& out_bytes) override;
    bool read_string(const std::string& relative_path, std::string& out_str) override;
    bool write_bytes(const std::string& relative_path, const void* data, size_t size) override;
    bool write_string(const std::string& relative_path, std::string_view content) override;
    bool file_exists(const std::string& relative_path) override;
    size_t get_file_size(const std::string& relative_path) override;
    std::string get_physical_path(const std::string& relative_path) const override;
    bool is_read_only() const override { return m_read_only; }

private:
    std::string resolve_path(const std::string& relative_path) const;

    std::string m_physical_root;
    bool m_read_only{false};
};

} // namespace engine::vfs
