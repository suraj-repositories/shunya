#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <optional>
#include <functional>

namespace shunya::core::base {

class Uuid {
public:
    using Bytes = std::array<uint8_t, 16>;

    constexpr Uuid() noexcept : bytes_{} {}
    explicit constexpr Uuid(const Bytes& bytes) noexcept : bytes_(bytes) {}

    [[nodiscard]] static Uuid generate_random();
    [[nodiscard]] static std::optional<Uuid> from_string(std::string_view str) noexcept;

    [[nodiscard]] constexpr bool is_nil() const noexcept {
        for (auto b : bytes_) {
            if (b != 0) return false;
        }
        return true;
    }

    [[nodiscard]] constexpr const Bytes& data() const noexcept { return bytes_; }
    [[nodiscard]] std::string to_string() const;

    constexpr auto operator<=>(const Uuid&) const = default;
    constexpr bool operator==(const Uuid&) const = default;

private:
    Bytes bytes_{};
};

} // namespace shunya::core::base

namespace std {
template <>
struct hash<shunya::core::base::Uuid> {
    std::size_t operator()(const shunya::core::base::Uuid& id) const noexcept {
        std::size_t h = 0;
        const auto& b = id.data();
        for (std::size_t i = 0; i < 16; i += sizeof(std::size_t)) {
            std::size_t chunk = 0;
            for (std::size_t j = 0; j < sizeof(std::size_t) && (i + j) < 16; ++j) {
                chunk |= static_cast<std::size_t>(b[i + j]) << (j * 8);
            }
            h ^= chunk + 0x9e3779b9 + (h << 6) + (h >> 2);
        }
        return h;
    }
};
} // namespace std
