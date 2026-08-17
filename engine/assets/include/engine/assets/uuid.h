#pragma once

#include "engine/core/config.h"
#include <cstdint>
#include <string>
#include <string_view>
#include <random>
#include <format>

namespace engine::assets {

struct UUID {
    uint64_t high{0};
    uint64_t low{0};

    constexpr UUID() = default;
    constexpr UUID(uint64_t h, uint64_t l) : high(h), low(l) {}

    static UUID generate() {
        static thread_local std::random_device rd;
        static thread_local std::mt19937_64 gen(rd());
        static thread_local std::uniform_int_distribution<uint64_t> dis;

        uint64_t h = dis(gen);
        uint64_t l = dis(gen);

        // RFC 4122 v4 variant
        h = (h & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL; // version 4
        l = (l & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL; // variant 1

        return UUID(h, l);
    }

    static UUID from_string(std::string_view str) {
        if (str.length() != 36) return UUID{};

        // Format: 8-4-4-4-12 (xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx)
        uint64_t h = 0;
        uint64_t l = 0;

        auto hex_val = [](char c) -> uint64_t {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return 0;
        };

        size_t hex_idx = 0;
        for (char c : str) {
            if (c == '-') continue;
            if (hex_idx < 16) {
                h = (h << 4) | hex_val(c);
            } else if (hex_idx < 32) {
                l = (l << 4) | hex_val(c);
            }
            hex_idx++;
        }

        return UUID(h, l);
    }

    std::string to_string() const {
        return std::format("{:08x}-{:04x}-{:04x}-{:04x}-{:012x}",
            static_cast<uint32_t>(high >> 32),
            static_cast<uint16_t>((high >> 16) & 0xFFFF),
            static_cast<uint16_t>(high & 0xFFFF),
            static_cast<uint16_t>(low >> 48),
            static_cast<uint64_t>(low & 0x0000FFFFFFFFFFFFULL)
        );
    }

    constexpr bool is_valid() const { return high != 0 || low != 0; }
    constexpr bool operator==(const UUID& other) const { return high == other.high && low == other.low; }
    constexpr bool operator!=(const UUID& other) const { return !(*this == other); }
    constexpr bool operator<(const UUID& other) const {
        if (high != other.high) return high < other.high;
        return low < other.low;
    }
};

} // namespace engine::assets

namespace std {
    template<>
    struct hash<engine::assets::UUID> {
        size_t operator()(const engine::assets::UUID& u) const noexcept {
            return std::hash<uint64_t>{}(u.high) ^ (std::hash<uint64_t>{}(u.low) << 1);
        }
    };
}
