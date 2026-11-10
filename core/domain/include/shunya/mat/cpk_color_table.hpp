#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace shunya::mat {

struct CpkRgb {
    uint8_t r{200};
    uint8_t g{200};
    uint8_t b{200};

    [[nodiscard]] std::string to_hex() const;
};

/**
 * @brief Standard Corey-Pauling-Koltun (CPK) atomic color lookup table.
 */
class CpkColorTable {
public:
    [[nodiscard]] static CpkRgb color_for_element(int atomic_number) noexcept {
        switch (atomic_number) {
        case 1:  return {248, 250, 252}; // H: White
        case 2:  return {217, 255, 255}; // He: Cyan-white
        case 6:  return { 45,  45,  48}; // C: Charcoal black
        case 7:  return { 48,  80, 248}; // N: Blue
        case 8:  return {220,  38,  38}; // O: Red
        case 9:  return {144, 224,  80}; // F: Pale green
        case 15: return {255, 128,   0}; // P: Orange
        case 16: return {234, 179,   8}; // S: Yellow
        case 17: return { 31, 240,  31}; // Cl: Green
        case 26: return {224, 102,  51}; // Fe: Rust
        case 29: return {200, 128,  51}; // Cu: Copper
        case 35: return {166,  41,  41}; // Br: Dark red
        case 53: return {148,   0, 148}; // I: Dark violet
        default: return {180, 180, 195}; // Generic silver-gray
        }
    }
};

} // namespace shunya::mat
