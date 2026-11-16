#include "shunya/core/base/uuid.hpp"

#include <cstring>
#include <random>

namespace shunya::core::base {

namespace {

// High-performance Xoshiro256++ 64-bit PRNG
class FastEntropyPrng {
public:
    FastEntropyPrng() {
        std::random_device rd;
        uint64_t seed1 = (static_cast<uint64_t>(rd()) << 32) | rd();
        uint64_t seed2 = (static_cast<uint64_t>(rd()) << 32) | rd();
        uint64_t seed3 = (static_cast<uint64_t>(rd()) << 32) | rd();
        uint64_t seed4 = (static_cast<uint64_t>(rd()) << 32) | rd();
        
        auto splitmix = [](uint64_t& x) {
            uint64_t z = (x += 0x9e3779b97f4a7c15ULL);
            z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
            z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
            return z ^ (z >> 31);
        };
        s_[0] = splitmix(seed1);
        s_[1] = splitmix(seed2);
        s_[2] = splitmix(seed3);
        s_[3] = splitmix(seed4);
        if ((s_[0] | s_[1] | s_[2] | s_[3]) == 0) {
            s_[0] = 0x8a5cd789635d2dffULL;
        }
    }

    [[nodiscard]] inline uint64_t next() noexcept {
        const uint64_t result = rotl(s_[0] + s_[3], 23) + s_[0];
        const uint64_t t = s_[1] << 17;
        s_[2] ^= s_[0];
        s_[3] ^= s_[1];
        s_[1] ^= s_[2];
        s_[0] ^= s_[3];
        s_[2] ^= t;
        s_[3] = rotl(s_[3], 45);
        return result;
    }

private:
    static inline uint64_t rotl(const uint64_t x, int k) noexcept {
        return (x << k) | (x >> (64 - k));
    }
    uint64_t s_[4];
};

alignas(64) static constexpr char kHexDigits[] = "0123456789abcdef";

alignas(64) static constexpr int8_t kHexDecodeLut[256] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9, -1, -1, -1, -1, -1, -1,
    -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};

} // namespace

Uuid Uuid::generate_random() {
    thread_local FastEntropyPrng prng;
    uint64_t w0 = prng.next();
    uint64_t w1 = prng.next();

    Bytes bytes;
    std::memcpy(&bytes[0], &w0, 8);
    std::memcpy(&bytes[8], &w1, 8);

    // RFC 4122 v4: set version bits (0100 in high 4 bits of byte 6)
    bytes[6] = static_cast<uint8_t>((bytes[6] & 0x0F) | 0x40);
    // RFC 4122 variant 1: set variant bits (10 in high 2 bits of byte 8)
    bytes[8] = static_cast<uint8_t>((bytes[8] & 0x3F) | 0x80);

    return Uuid(bytes);
}

void Uuid::to_chars(char* out) const noexcept {
    auto write_byte = [&](std::size_t byte_idx, std::size_t str_pos) {
        uint8_t b = bytes_[byte_idx];
        out[str_pos] = kHexDigits[b >> 4];
        out[str_pos + 1] = kHexDigits[b & 0x0F];
    };

    write_byte(0, 0);
    write_byte(1, 2);
    write_byte(2, 4);
    write_byte(3, 6);
    out[8] = '-';
    write_byte(4, 9);
    write_byte(5, 11);
    out[13] = '-';
    write_byte(6, 14);
    write_byte(7, 16);
    out[18] = '-';
    write_byte(8, 19);
    write_byte(9, 21);
    out[23] = '-';
    write_byte(10, 24);
    write_byte(11, 26);
    write_byte(12, 28);
    write_byte(13, 30);
    write_byte(14, 32);
    write_byte(15, 34);
}

std::string Uuid::to_string() const {
    std::string out(36, '\0');
    to_chars(out.data());
    return out;
}

std::optional<Uuid> Uuid::from_string(std::string_view str) noexcept {
    if (str.length() != 36) return std::nullopt;
    if (str[8] != '-' || str[13] != '-' || str[18] != '-' || str[23] != '-') return std::nullopt;

    Bytes bytes;
    auto decode_pair = [&](std::size_t str_pos) -> int {
        int8_t hi = kHexDecodeLut[static_cast<uint8_t>(str[str_pos])];
        int8_t lo = kHexDecodeLut[static_cast<uint8_t>(str[str_pos + 1])];
        if (hi < 0 || lo < 0) return -1;
        return (hi << 4) | lo;
    };

    int b0 = decode_pair(0); if (b0 < 0) return std::nullopt; bytes[0] = static_cast<uint8_t>(b0);
    int b1 = decode_pair(2); if (b1 < 0) return std::nullopt; bytes[1] = static_cast<uint8_t>(b1);
    int b2 = decode_pair(4); if (b2 < 0) return std::nullopt; bytes[2] = static_cast<uint8_t>(b2);
    int b3 = decode_pair(6); if (b3 < 0) return std::nullopt; bytes[3] = static_cast<uint8_t>(b3);

    int b4 = decode_pair(9); if (b4 < 0) return std::nullopt; bytes[4] = static_cast<uint8_t>(b4);
    int b5 = decode_pair(11); if (b5 < 0) return std::nullopt; bytes[5] = static_cast<uint8_t>(b5);

    int b6 = decode_pair(14); if (b6 < 0) return std::nullopt; bytes[6] = static_cast<uint8_t>(b6);
    int b7 = decode_pair(16); if (b7 < 0) return std::nullopt; bytes[7] = static_cast<uint8_t>(b7);

    int b8 = decode_pair(19); if (b8 < 0) return std::nullopt; bytes[8] = static_cast<uint8_t>(b8);
    int b9 = decode_pair(21); if (b9 < 0) return std::nullopt; bytes[9] = static_cast<uint8_t>(b9);

    int b10 = decode_pair(24); if (b10 < 0) return std::nullopt; bytes[10] = static_cast<uint8_t>(b10);
    int b11 = decode_pair(26); if (b11 < 0) return std::nullopt; bytes[11] = static_cast<uint8_t>(b11);
    int b12 = decode_pair(28); if (b12 < 0) return std::nullopt; bytes[12] = static_cast<uint8_t>(b12);
    int b13 = decode_pair(30); if (b13 < 0) return std::nullopt; bytes[13] = static_cast<uint8_t>(b13);
    int b14 = decode_pair(32); if (b14 < 0) return std::nullopt; bytes[14] = static_cast<uint8_t>(b14);
    int b15 = decode_pair(34); if (b15 < 0) return std::nullopt; bytes[15] = static_cast<uint8_t>(b15);

    return Uuid(bytes);
}

} // namespace shunya::core::base
