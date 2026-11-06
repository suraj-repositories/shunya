#pragma once

#include <string_view>

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
    std::string_view suffix = "dev";

    [[nodiscard]] constexpr std::string_view as_string() const noexcept {
        return "0.1.0-dev";
    }
};

inline constexpr Version CURRENT_VERSION{0, 1, 0, "dev"};

} // namespace shunya::core::base
