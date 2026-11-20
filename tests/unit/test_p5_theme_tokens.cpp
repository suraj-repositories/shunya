#include "tests/test_framework.hpp"
#include <cmath>
#include <string>
#include <string_view>
#include <sstream>
#include <iomanip>
#include <vector>
#include <algorithm>

namespace {

// WCAG 2.1 AA relative luminance & contrast ratio calculation
// Standard formula:
// L = 0.2126 * R_lin + 0.7152 * G_lin + 0.0722 * B_lin
// where C_sRGB <= 0.04045 ? C_sRGB / 12.92 : ((C_sRGB + 0.055) / 1.055) ^ 2.4
// Contrast Ratio = (L1 + 0.05) / (L2 + 0.05) where L1 is the lighter of the two.
struct ColorRGB {
    double r{0.0};
    double g{0.0};
    double b{0.0};

    static ColorRGB from_hex(std::string_view hex) {
        std::string s(hex);
        if (!s.empty() && s[0] == '#') s = s.substr(1);
        if (s.size() == 6) {
            unsigned int ri = 0, gi = 0, bi = 0;
            std::stringstream ss;
            ss << std::hex << s.substr(0, 2);
            ss >> ri;
            ss.clear();
            ss << std::hex << s.substr(2, 2);
            ss >> gi;
            ss.clear();
            ss << std::hex << s.substr(4, 2);
            ss >> bi;
            return ColorRGB{ri / 255.0, gi / 255.0, bi / 255.0};
        }
        return ColorRGB{};
    }

    [[nodiscard]] double channel_to_linear(double c) const noexcept {
        return (c <= 0.04045) ? (c / 12.92) : std::pow((c + 0.055) / 1.055, 2.4);
    }

    [[nodiscard]] double relative_luminance() const noexcept {
        const double r_lin = channel_to_linear(r);
        const double g_lin = channel_to_linear(g);
        const double b_lin = channel_to_linear(b);
        return 0.2126 * r_lin + 0.7152 * g_lin + 0.0722 * b_lin;
    }

    [[nodiscard]] static double contrast_ratio(const ColorRGB& c1, const ColorRGB& c2) noexcept {
        const double l1 = c1.relative_luminance();
        const double l2 = c2.relative_luminance();
        const double lighter = (std::max)(l1, l2);
        const double darker = (std::min)(l1, l2);
        return (lighter + 0.05) / (darker + 0.05);
    }
};

// Theme State Model representation for cross-checking QML contracts
struct ThemeTokenSet {
    std::string bg_primary;
    std::string bg_canvas;
    std::string bg_surface;
    std::string bg_elevated;
    std::string bg_hover;
    std::string bg_active;
    std::string bg_input;

    std::string border_subtle;
    std::string border_default;
    std::string border_strong;
    std::string border_focus;

    std::string text_primary;
    std::string text_secondary;
    std::string text_muted;
    std::string text_subtle;
    std::string text_inverse;

    std::string btn_primary_bg;
    std::string btn_primary_fg;

    std::string accent_primary;
    std::string accent_hover;

    static ThemeTokenSet dark_mode() {
        return ThemeTokenSet{
            .bg_primary = "#0A0A0A",
            .bg_canvas = "#111111",
            .bg_surface = "#171717",
            .bg_elevated = "#202020",
            .bg_hover = "#262626",
            .bg_active = "#2E2E2E",
            .bg_input = "#121212",
            .border_subtle = "#262626",
            .border_default = "#333333",
            .border_strong = "#525252",
            .border_focus = "#FAFAFA",
            .text_primary = "#FAFAFA",
            .text_secondary = "#D4D4D4",
            .text_muted = "#A3A3A3",
            .text_subtle = "#8A8A8A",
            .text_inverse = "#0A0A0A",
            .btn_primary_bg = "#FAFAFA",
            .btn_primary_fg = "#0A0A0A",
            .accent_primary = "#60A5FA",
            .accent_hover = "#93C5FD"
        };
    }

    static ThemeTokenSet light_mode() {
        return ThemeTokenSet{
            .bg_primary = "#FFFFFF",
            .bg_canvas = "#FAFAFA",
            .bg_surface = "#F5F5F5",
            .bg_elevated = "#EFEFEF",
            .bg_hover = "#E5E5E5",
            .bg_active = "#DCDCDC",
            .bg_input = "#FFFFFF",
            .border_subtle = "#E5E5E5",
            .border_default = "#D4D4D4",
            .border_strong = "#A3A3A3",
            .border_focus = "#0A0A0A",
            .text_primary = "#0A0A0A",
            .text_secondary = "#262626",
            .text_muted = "#525252",
            .text_subtle = "#595959",
            .text_inverse = "#FFFFFF",
            .btn_primary_bg = "#0A0A0A",
            .btn_primary_fg = "#FFFFFF",
            .accent_primary = "#2563EB",
            .accent_hover = "#1D4ED8"
        };
    }
};

} // namespace

TEST_CASE("PH5-31: Theme Token Contract & WCAG 2.1 AA Contrast Verification") {
    // 1. Dark Mode WCAG AA Ratios
    {
        const auto dark = ThemeTokenSet::dark_mode();
        const auto bgPrimary = ColorRGB::from_hex(dark.bg_primary);
        const auto bgCanvas = ColorRGB::from_hex(dark.bg_canvas);
        const auto bgSurface = ColorRGB::from_hex(dark.bg_surface);
        const auto bgElevated = ColorRGB::from_hex(dark.bg_elevated);

        const auto textPrimary = ColorRGB::from_hex(dark.text_primary);
        const auto textSecondary = ColorRGB::from_hex(dark.text_secondary);
        const auto textMuted = ColorRGB::from_hex(dark.text_muted);
        const auto textSubtle = ColorRGB::from_hex(dark.text_subtle);

        const double cr_prim = ColorRGB::contrast_ratio(textPrimary, bgPrimary);
        const double cr_sec = ColorRGB::contrast_ratio(textSecondary, bgPrimary);
        const double cr_muted = ColorRGB::contrast_ratio(textMuted, bgPrimary);
        const double cr_subtle = ColorRGB::contrast_ratio(textSubtle, bgPrimary);

        CHECK(cr_prim >= 15.0);
        CHECK(cr_sec >= 10.0);
        CHECK(cr_muted >= 6.0);
        CHECK(cr_subtle >= 4.5);

        CHECK(ColorRGB::contrast_ratio(textPrimary, bgElevated) >= 12.0);
        CHECK(ColorRGB::contrast_ratio(textSecondary, bgElevated) >= 8.0);
        CHECK(ColorRGB::contrast_ratio(textPrimary, bgCanvas) >= 14.0);
        CHECK(ColorRGB::contrast_ratio(textPrimary, bgSurface) >= 13.0);
    }

    // 2. Light Mode WCAG AA Ratios
    {
        const auto light = ThemeTokenSet::light_mode();
        const auto bgPrimary = ColorRGB::from_hex(light.bg_primary);
        const auto bgCanvas = ColorRGB::from_hex(light.bg_canvas);
        const auto bgSurface = ColorRGB::from_hex(light.bg_surface);
        const auto bgElevated = ColorRGB::from_hex(light.bg_elevated);

        const auto textPrimary = ColorRGB::from_hex(light.text_primary);
        const auto textSecondary = ColorRGB::from_hex(light.text_secondary);
        const auto textMuted = ColorRGB::from_hex(light.text_muted);
        const auto textSubtle = ColorRGB::from_hex(light.text_subtle);

        const double cr_prim = ColorRGB::contrast_ratio(textPrimary, bgPrimary);
        const double cr_sec = ColorRGB::contrast_ratio(textSecondary, bgPrimary);
        const double cr_muted = ColorRGB::contrast_ratio(textMuted, bgPrimary);
        const double cr_subtle = ColorRGB::contrast_ratio(textSubtle, bgPrimary);

        CHECK(cr_prim >= 15.0);
        CHECK(cr_sec >= 12.0);
        CHECK(cr_muted >= 6.5);
        CHECK(cr_subtle >= 4.5);

        CHECK(ColorRGB::contrast_ratio(textPrimary, bgElevated) >= 12.0);
        CHECK(ColorRGB::contrast_ratio(textSecondary, bgElevated) >= 8.0);
        CHECK(ColorRGB::contrast_ratio(textPrimary, bgCanvas) >= 14.0);
        CHECK(ColorRGB::contrast_ratio(textPrimary, bgSurface) >= 13.0);
    }

    // 3. Primary Button Contrasts
    {
        const auto dark = ThemeTokenSet::dark_mode();
        const double cr_dark_btn = ColorRGB::contrast_ratio(
            ColorRGB::from_hex(dark.btn_primary_fg),
            ColorRGB::from_hex(dark.btn_primary_bg)
        );
        CHECK(cr_dark_btn >= 15.0);

        const auto light = ThemeTokenSet::light_mode();
        const double cr_light_btn = ColorRGB::contrast_ratio(
            ColorRGB::from_hex(light.btn_primary_fg),
            ColorRGB::from_hex(light.btn_primary_bg)
        );
        CHECK(cr_light_btn >= 15.0);
    }

    // 4. Elevation Hierarchy
    {
        const auto dark = ThemeTokenSet::dark_mode();
        const auto d0 = ColorRGB::from_hex(dark.bg_primary);
        const auto d1 = ColorRGB::from_hex(dark.bg_canvas);
        const auto d2 = ColorRGB::from_hex(dark.bg_surface);
        const auto d3 = ColorRGB::from_hex(dark.bg_elevated);
        const auto d4 = ColorRGB::from_hex(dark.bg_hover);
        const auto d5 = ColorRGB::from_hex(dark.bg_active);

        CHECK(d0.relative_luminance() < d1.relative_luminance());
        CHECK(d1.relative_luminance() < d2.relative_luminance());
        CHECK(d2.relative_luminance() < d3.relative_luminance());
        CHECK(d3.relative_luminance() < d4.relative_luminance());
        CHECK(d4.relative_luminance() < d5.relative_luminance());

        const auto light = ThemeTokenSet::light_mode();
        const auto l0 = ColorRGB::from_hex(light.bg_primary);
        const auto l1 = ColorRGB::from_hex(light.bg_canvas);
        const auto l2 = ColorRGB::from_hex(light.bg_surface);
        const auto l3 = ColorRGB::from_hex(light.bg_elevated);
        const auto l4 = ColorRGB::from_hex(light.bg_hover);
        const auto l5 = ColorRGB::from_hex(light.bg_active);

        CHECK(l0.relative_luminance() > l1.relative_luminance());
        CHECK(l1.relative_luminance() > l2.relative_luminance());
        CHECK(l2.relative_luminance() > l3.relative_luminance());
        CHECK(l3.relative_luminance() > l4.relative_luminance());
        CHECK(l4.relative_luminance() > l5.relative_luminance());
    }

    // 5. Accent Visibility
    {
        const auto dark = ThemeTokenSet::dark_mode();
        const auto dark_accent = ColorRGB::from_hex(dark.accent_primary);
        const auto dark_bg = ColorRGB::from_hex(dark.bg_primary);
        // Accent in dark mode (#60A5FA on #0A0A0A) has high readability (exceeds WCAG AAA 7:1)
        CHECK(ColorRGB::contrast_ratio(dark_accent, dark_bg) >= 7.0);

        const auto light = ThemeTokenSet::light_mode();
        const auto light_accent = ColorRGB::from_hex(light.accent_primary);
        const auto light_bg = ColorRGB::from_hex(light.bg_primary);
        // Accent in light mode (#2563EB on #FFFFFF) meets AA 4.5:1 for UI indicators & links
        CHECK(ColorRGB::contrast_ratio(light_accent, light_bg) >= 4.5);
    }
}
