#pragma once

#include "shunya/core/domain/rendering.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace shunya::core::domain {

[[nodiscard]] std::string_view colormap_type_to_string(ColormapType type) noexcept;
[[nodiscard]] std::optional<ColormapType> colormap_type_from_string(std::string_view str) noexcept;

struct ColorRGBf {
    float r{0.0f};
    float g{0.0f};
    float b{0.0f};
};

struct ColorRGB {
    uint8_t r{0};
    uint8_t g{0};
    uint8_t b{0};

    [[nodiscard]] std::string to_hex() const;
};

struct FieldProbeResult {
    double raw_value{0.0};
    double normalized{0.0}; // Clamped [0, 1]
    ColorRGB color{0, 0, 0};
    std::string formatted_label;
};

/**
 * @brief Scientific colormaps with 256-entry lookup tables and continuous linear sampling.
 */
class ColormapTables {
public:
    /**
     * @brief Samples normalized scalar t in [0.0, 1.0] into continuous RGB floats [0.0, 1.0].
     */
    [[nodiscard]] static ColorRGBf sample_float(ColormapType type, double t) noexcept;

    /**
     * @brief Samples normalized scalar t in [0.0, 1.0] into 8-bit sRGB channels [0, 255].
     */
    [[nodiscard]] static ColorRGB sample_rgb8(ColormapType type, double t) noexcept;

    /**
     * @brief Builds a full 256-entry 8-bit RGB lookup texture table.
     */
    [[nodiscard]] static std::array<ColorRGB, 256> generate_lut256(ColormapType type) noexcept;

    /**
     * @brief Maps a physical scalar value within [min_val, max_val] to color and formatted tooltip text.
     */
    [[nodiscard]] static FieldProbeResult probe_scalar(
        ColormapType type,
        double value,
        double min_val,
        double max_val,
        std::string_view unit_symbol = ""
    );
};

} // namespace shunya::core::domain
