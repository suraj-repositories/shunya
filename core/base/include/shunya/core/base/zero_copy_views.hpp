#pragma once

#include <string_view>
#include <span>
#include <vector>
#include <charconv>
#include <cstdint>
#include <algorithm>
#include <cctype>

namespace shunya::core::base {

/**
 * @brief Zero-copy whitespace trimming returning a sub-view without allocations.
 */
[[nodiscard]] inline constexpr std::string_view trim_view(std::string_view s) noexcept {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r' || s.front() == '\n')) {
        s.remove_prefix(1);
    }
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n')) {
        s.remove_suffix(1);
    }
    return s;
}

/**
 * @brief Zero-copy delimiter-based tokenizer yielding std::string_view slices.
 */
class StringViewTokenizer {
public:
    constexpr StringViewTokenizer(std::string_view text, char delimiter) noexcept
        : text_(text), delimiter_(delimiter) {}

    class Iterator {
    public:
        using value_type = std::string_view;
        using difference_type = std::ptrdiff_t;
        using pointer = const std::string_view*;
        using reference = const std::string_view&;

        constexpr Iterator() noexcept : at_end_(true), has_more_(false) {}
        constexpr explicit Iterator(std::string_view text, char delim) noexcept
            : remaining_(text), delimiter_(delim), at_end_(text.empty()), has_more_(!text.empty()) {
            if (!at_end_) {
                advance();
            }
        }

        [[nodiscard]] constexpr std::string_view operator*() const noexcept { return current_; }
        [[nodiscard]] constexpr const std::string_view* operator->() const noexcept { return &current_; }

        constexpr Iterator& operator++() noexcept {
            advance();
            return *this;
        }

        constexpr bool operator==(const Iterator& other) const noexcept {
            if (at_end_ && other.at_end_) return true;
            return at_end_ == other.at_end_ && remaining_.data() == other.remaining_.data();
        }

        constexpr bool operator!=(const Iterator& other) const noexcept {
            return !(*this == other);
        }

    private:
        std::string_view remaining_{};
        std::string_view current_{};
        char delimiter_{','};
        bool at_end_{true};
        bool has_more_{true};

        constexpr void advance() noexcept {
            if (!has_more_) {
                at_end_ = true;
                current_ = {};
                return;
            }
            auto pos = remaining_.find(delimiter_);
            if (pos == std::string_view::npos) {
                current_ = remaining_;
                remaining_ = {};
                has_more_ = false;
            } else {
                current_ = remaining_.substr(0, pos);
                remaining_.remove_prefix(pos + 1);
            }
        }
    };

    [[nodiscard]] constexpr Iterator begin() const noexcept {
        return Iterator(text_, delimiter_);
    }

    [[nodiscard]] constexpr Iterator end() const noexcept {
        return Iterator();
    }

private:
    std::string_view text_;
    char delimiter_{','};
};

/**
 * @brief Fast zero-allocation double parser from std::string_view using std::from_chars.
 */
[[nodiscard]] inline double parse_double_view(std::string_view s, double fallback = 0.0) noexcept {
    s = trim_view(s);
    if (s.empty()) return fallback;
    double val = fallback;
    auto res = std::from_chars(s.data(), s.data() + s.size(), val);
    return (res.ec == std::errc{}) ? val : fallback;
}

/**
 * @brief Fast zero-allocation 64-bit integer parser from std::string_view using std::from_chars.
 */
[[nodiscard]] inline int64_t parse_int64_view(std::string_view s, int64_t fallback = 0) noexcept {
    s = trim_view(s);
    if (s.empty()) return fallback;
    int64_t val = fallback;
    auto res = std::from_chars(s.data(), s.data() + s.size(), val);
    return (res.ec == std::errc{}) ? val : fallback;
}

/**
 * @brief High-performance inner product over contiguous spans without heap allocation.
 */
[[nodiscard]] inline double dot_product_span(std::span<const double> a, std::span<const double> b) noexcept {
    const size_t n = std::min(a.size(), b.size());
    double sum = 0.0;
    // Unrolled by 4 for SIMD pipelining
    size_t i = 0;
    for (; i + 3 < n; i += 4) {
        sum += a[i] * b[i] + a[i + 1] * b[i + 1] + a[i + 2] * b[i + 2] + a[i + 3] * b[i + 3];
    }
    for (; i < n; ++i) {
        sum += a[i] * b[i];
    }
    return sum;
}

/**
 * @brief Safe byte view extraction over arbitrary trivially copyable objects.
 */
template <typename T>
[[nodiscard]] std::span<const uint8_t> as_byte_span(const T& obj) noexcept requires std::is_trivially_copyable_v<T> {
    return std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(&obj), sizeof(T));
}

} // namespace shunya::core::base
