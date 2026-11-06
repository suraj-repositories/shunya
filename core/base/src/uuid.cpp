#include "shunya/core/base/uuid.hpp"

#include <random>
#include <iomanip>
#include <sstream>

namespace shunya::core::base {

namespace {

char to_hex_nibble(uint8_t nibble) noexcept {
    return nibble < 10 ? static_cast<char>('0' + nibble) : static_cast<char>('a' + (nibble - 10));
}

int from_hex_char(char c) noexcept {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

} // namespace

Uuid Uuid::generate_random() {
    thread_local std::random_device rd;
    thread_local std::mt19937_64 gen(rd());
    thread_local std::uniform_int_distribution<uint64_t> dist;

    Bytes bytes;
    uint64_t high = dist(gen);
    uint64_t low = dist(gen);

    for (std::size_t i = 0; i < 8; ++i) {
        bytes[i] = static_cast<uint8_t>((high >> (i * 8)) & 0xFF);
        bytes[i + 8] = static_cast<uint8_t>((low >> (i * 8)) & 0xFF);
    }

    // RFC 4122 v4: set version bits (0100 in high 4 bits of byte 6)
    bytes[6] = static_cast<uint8_t>((bytes[6] & 0x0F) | 0x40);
    // RFC 4122 variant 1: set variant bits (10 in high 2 bits of byte 8)
    bytes[8] = static_cast<uint8_t>((bytes[8] & 0x3F) | 0x80);

    return Uuid(bytes);
}

std::optional<Uuid> Uuid::from_string(std::string_view str) noexcept {
    // Expected format: 8-4-4-4-12 = 36 characters
    if (str.length() != 36) {
        return std::nullopt;
    }

    if (str[8] != '-' || str[13] != '-' || str[18] != '-' || str[23] != '-') {
        return std::nullopt;
    }

    Bytes bytes{};
    std::size_t byte_idx = 0;

    for (std::size_t i = 0; i < str.length(); ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            continue;
        }

        int high = from_hex_char(str[i]);
        if (high < 0 || ++i >= str.length()) return std::nullopt;
        int low = from_hex_char(str[i]);
        if (low < 0) return std::nullopt;

        bytes[byte_idx++] = static_cast<uint8_t>((high << 4) | low);
    }

    if (byte_idx != 16) {
        return std::nullopt;
    }

    return Uuid(bytes);
}

std::string Uuid::to_string() const {
    std::string out;
    out.reserve(36);

    for (std::size_t i = 0; i < 16; ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) {
            out.push_back('-');
        }
        out.push_back(to_hex_nibble(bytes_[i] >> 4));
        out.push_back(to_hex_nibble(bytes_[i] & 0x0F));
    }

    return out;
}

} // namespace shunya::core::base
