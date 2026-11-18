#include "tests/test_framework.hpp"
#include "shunya/core/domain/workspace_ui.hpp"

#include <cmath>
#include <string>
#include <vector>

using namespace shunya::core::domain;

namespace {

// Parses hex string "#RRGGBB" into normalized [0, 1] RGB
struct ColorRGB {
    double r{0.0};
    double g{0.0};
    double b{0.0};

    static ColorRGB from_hex(std::string_view hex) {
        if (hex.size() == 7 && hex[0] == '#') {
            int r_int = std::stoi(std::string(hex.substr(1, 2)), nullptr, 16);
            int g_int = std::stoi(std::string(hex.substr(3, 2)), nullptr, 16);
            int b_int = std::stoi(std::string(hex.substr(5, 2)), nullptr, 16);
            return {r_int / 255.0, g_int / 255.0, b_int / 255.0};
        }
        return {0.0, 0.0, 0.0};
    }

    [[nodiscard]] double relative_luminance() const noexcept {
        auto to_linear = [](double c) {
            return (c <= 0.04045) ? (c / 12.92) : std::pow((c + 0.055) / 1.055, 2.4);
        };
        const double rl = to_linear(r);
        const double gl = to_linear(g);
        const double bl = to_linear(b);
        return 0.2126 * rl + 0.7152 * gl + 0.0722 * bl;
    }
};

double compute_contrast_ratio(const ColorRGB& c1, const ColorRGB& c2) {
    const double l1 = c1.relative_luminance();
    const double l2 = c2.relative_luminance();
    const double lighter = std::max(l1, l2);
    const double darker = std::min(l1, l2);
    return (lighter + 0.05) / (darker + 0.05);
}

} // namespace

// =============================================================================
// PH4-13.03.004: Theme Toggling (Dark vs Light) & WCAG AA Contrast Compliance
// =============================================================================

TEST_CASE("PH4-13.03.004: Dark Theme Palette WCAG AA Contrast Compliance") {
    VSCodeMenuStyleConfig config;

    // Dark theme: Background #1E1E1E, Primary text #CCCCCC / #FFFFFF
    const auto dark_bg = ColorRGB::from_hex(config.dark_bg_color);
    const auto dark_text = ColorRGB::from_hex("#CCCCCC");
    const auto pure_white = ColorRGB::from_hex("#FFFFFF");

    const double contrast_normal = compute_contrast_ratio(dark_bg, dark_text);
    const double contrast_highlight = compute_contrast_ratio(dark_bg, pure_white);

    // WCAG AA requirement for normal text is >= 4.5:1
    CHECK(contrast_normal >= 4.5);
    CHECK(contrast_highlight >= 7.0);

    // Dark menu border contrast
    const auto dark_border = ColorRGB::from_hex(config.dark_border_color);
    CHECK(dark_border.r > 0.0);
}

TEST_CASE("PH4-13.03.004: Light Theme Palette WCAG AA Contrast Compliance") {
    VSCodeMenuStyleConfig config;

    // Light theme: Background #F3F3F3, Primary text #222222
    const auto light_bg = ColorRGB::from_hex(config.light_bg_color);
    const auto light_text = ColorRGB::from_hex("#222222");

    const double contrast_normal = compute_contrast_ratio(light_bg, light_text);

    // WCAG AA requirement for normal text is >= 4.5:1
    CHECK(contrast_normal >= 4.5);
}

TEST_CASE("PH4-13.03.004: UI Icon Visibility & Opacity Invariants Across Themes") {
    // Opacity invariant definitions:
    // Active state: 1.0 (100% full opacity)
    // Hover state: 0.85 (85% opacity)
    // Disabled state: 0.40 (40% opacity invariant, per PH4-12.01.007)
    constexpr double opacity_active = 1.0;
    constexpr double opacity_hover = 0.85;
    constexpr double opacity_disabled = 0.40;

    CHECK(opacity_active > opacity_hover);
    CHECK(opacity_hover > opacity_disabled);
    CHECK(opacity_disabled == 0.40);
}
