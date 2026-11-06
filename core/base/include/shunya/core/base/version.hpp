#pragma once

#include "shunya/core/base/result.hpp"

#include <string>
#include <string_view>
#include <array>

// Compile-time verification of C++20 standard baseline
#if defined(_MSVC_LANG)
    static_assert(_MSVC_LANG >= 202002L, "Shunya requires C++20 standard baseline or higher.");
#else
    static_assert(__cplusplus >= 202002L, "Shunya requires C++20 standard baseline or higher.");
#endif

namespace shunya::core::base {

struct Version {
    int major = 0;
    int minor = 1;
    int patch = 0;
    std::array<char, 32> suffix{};

    constexpr Version() noexcept {
        suffix[0] = 'd'; suffix[1] = 'e'; suffix[2] = 'v'; suffix[3] = '\0';
    }

    constexpr Version(int maj, int min, int pat, std::string_view suf = "") noexcept
        : major(maj), minor(min), patch(pat) {
        std::size_t i = 0;
        for (; i < suf.size() && i < 31; ++i) {
            suffix[i] = suf[i];
        }
        suffix[i] = '\0';
    }

    [[nodiscard]] std::string as_string() const {
        std::string s = std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
        if (suffix[0] != '\0') {
            s += "-";
            s += suffix.data();
        }
        return s;
    }

    [[nodiscard]] std::string_view suffix_str() const noexcept {
        return std::string_view(suffix.data());
    }

    [[nodiscard]] static Result<Version> parse(std::string_view str);

    constexpr bool operator==(const Version& other) const noexcept {
        if (major != other.major || minor != other.minor || patch != other.patch) return false;
        for (std::size_t i = 0; i < 32; ++i) {
            if (suffix[i] != other.suffix[i]) return false;
            if (suffix[i] == '\0') break;
        }
        return true;
    }
};

inline constexpr Version CURRENT_VERSION{0, 1, 0, "dev"};

} // namespace shunya::core::base
