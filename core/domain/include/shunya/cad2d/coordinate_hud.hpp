#pragma once

#include "shunya/cad2d/curve_2d.hpp"
#include "shunya/core/base/result.hpp"

#include <string>
#include <optional>
#include <string_view>

namespace shunya::cad2d {

enum class HudInputMode {
    PolarDistanceAngle,   // Length < Angle
    RelativeCartesian,    // @dX, dY
    AbsoluteCartesian     // X, Y
};

struct HudDisplayField {
    std::string label;        // e.g. "Length", "Angle", "X", "Y"
    std::string text_buffer;  // Text currently entered by user
    double current_value{0.0};
    bool is_locked{false};
    bool is_active{false};    // Currently receiving keyboard focus
};

struct HudDisplayState {
    HudInputMode mode{HudInputMode::PolarDistanceAngle};
    HudDisplayField field1;   // e.g. Distance / X
    HudDisplayField field2;   // e.g. Angle / Y
    Point2D anchor{};
    Point2D resolved_world_point{};
    Point2D hud_screen_position{};
    bool can_commit{false};
};

/**
 * @brief Precision numeric coordinate input bar (HUD / Dynamic Input) for CAD sketching.
 * Allows exact keyboard entry of length, angle, and coordinates with field locking and Tab navigation.
 */
class PrecisionCoordinateHud {
public:
    explicit PrecisionCoordinateHud(HudInputMode default_mode = HudInputMode::PolarDistanceAngle);

    void set_mode(HudInputMode mode);
    [[nodiscard]] HudInputMode mode() const noexcept { return mode_; }

    void set_anchor(const Point2D& anchor) noexcept;
    [[nodiscard]] Point2D anchor() const noexcept { return anchor_; }

    void update_free_cursor(const Point2D& cursor_world);

    // Keyboard interaction
    bool input_char(char c);
    void handle_tab() noexcept;
    void handle_backspace();
    void reset();

    void lock_field1(double value);
    void lock_field2(double value);
    void unlock_all() noexcept;

    [[nodiscard]] std::optional<Point2D> commit();
    [[nodiscard]] HudDisplayState display_state() const;

    /**
     * @brief Parses a direct command-line coordinate string (e.g. "12.5, 8.0", "@5.0, -2.0", "15<45").
     */
    [[nodiscard]] static core::base::Result<Point2D> parse_coordinate_string(
        std::string_view input,
        const Point2D& anchor = {0.0, 0.0}
    );

private:
    void recompute_resolved_point();

    HudInputMode mode_{HudInputMode::PolarDistanceAngle};
    Point2D anchor_{0.0, 0.0};
    Point2D free_cursor_{0.0, 0.0};
    Point2D resolved_point_{0.0, 0.0};

    int active_field_idx_{0}; // 0 = field1, 1 = field2

    std::string field1_buffer_;
    std::string field2_buffer_;
    double field1_locked_val_{0.0};
    double field2_locked_val_{0.0};
    bool field1_locked_{false};
    bool field2_locked_{false};
};

} // namespace shunya::cad2d
