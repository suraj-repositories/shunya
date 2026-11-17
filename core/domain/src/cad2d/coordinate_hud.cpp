#include "shunya/cad2d/coordinate_hud.hpp"
#include "shunya/core/base/math.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <charconv>
#include <iomanip>
#include <sstream>

namespace shunya::cad2d {

namespace {

using shunya::core::base::math::PI;

std::string format_val(double v, int prec = 3) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(prec) << v;
    return ss.str();
}

std::optional<double> parse_double_str(std::string_view sv) {
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.front()))) sv.remove_prefix(1);
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.back()))) sv.remove_suffix(1);
    if (sv.empty()) return std::nullopt;

    std::string str(sv);
    char* end = nullptr;
    double val = std::strtod(str.c_str(), &end);
    if (end != str.c_str() + str.size()) return std::nullopt;
    return val;
}

} // namespace

PrecisionCoordinateHud::PrecisionCoordinateHud(HudInputMode default_mode)
    : mode_(default_mode)
{
    set_mode(default_mode);
}

void PrecisionCoordinateHud::set_mode(HudInputMode mode) {
    mode_ = mode;
    reset();
}

void PrecisionCoordinateHud::set_anchor(const Point2D& anchor) noexcept {
    anchor_ = anchor;
    recompute_resolved_point();
}

void PrecisionCoordinateHud::update_free_cursor(const Point2D& cursor_world) {
    free_cursor_ = cursor_world;
    recompute_resolved_point();
}

void PrecisionCoordinateHud::reset() {
    field1_buffer_.clear();
    field2_buffer_.clear();
    field1_locked_ = false;
    field2_locked_ = false;
    field1_locked_val_ = 0.0;
    field2_locked_val_ = 0.0;
    active_field_idx_ = 0;
    recompute_resolved_point();
}

void PrecisionCoordinateHud::lock_field1(double value) {
    field1_locked_ = true;
    field1_locked_val_ = value;
    field1_buffer_ = format_val(value);
    recompute_resolved_point();
}

void PrecisionCoordinateHud::lock_field2(double value) {
    field2_locked_ = true;
    field2_locked_val_ = value;
    field2_buffer_ = format_val(value);
    recompute_resolved_point();
}

void PrecisionCoordinateHud::unlock_all() noexcept {
    field1_locked_ = false;
    field2_locked_ = false;
    recompute_resolved_point();
}

void PrecisionCoordinateHud::handle_tab() noexcept {
    active_field_idx_ = (active_field_idx_ == 0) ? 1 : 0;
}

void PrecisionCoordinateHud::handle_backspace() {
    auto& buf = (active_field_idx_ == 0) ? field1_buffer_ : field2_buffer_;
    auto& locked = (active_field_idx_ == 0) ? field1_locked_ : field2_locked_;

    if (!buf.empty()) {
        buf.pop_back();
        auto parsed = parse_double_str(buf);
        if (parsed) {
            if (active_field_idx_ == 0) field1_locked_val_ = *parsed;
            else field2_locked_val_ = *parsed;
        } else {
            locked = false;
        }
    } else {
        locked = false;
    }
    recompute_resolved_point();
}

bool PrecisionCoordinateHud::input_char(char c) {
    if (c == '\t') {
        handle_tab();
        return false;
    }
    if (c == '\r' || c == '\n') {
        return true; // Commit requested
    }

    if (active_field_idx_ == 0 && c == '<') {
        mode_ = HudInputMode::PolarDistanceAngle;
        active_field_idx_ = 1;
        return false;
    }
    if (active_field_idx_ == 0 && c == ',') {
        if (mode_ == HudInputMode::PolarDistanceAngle) {
            mode_ = HudInputMode::RelativeCartesian;
        }
        active_field_idx_ = 1;
        return false;
    }

    // Allow digits, signs, and decimal point
    if (std::isdigit(static_cast<unsigned char>(c)) || c == '.' || c == '-' || c == '+') {
        auto& buf = (active_field_idx_ == 0) ? field1_buffer_ : field2_buffer_;
        auto& locked = (active_field_idx_ == 0) ? field1_locked_ : field2_locked_;
        auto& val = (active_field_idx_ == 0) ? field1_locked_val_ : field2_locked_val_;

        buf.push_back(c);
        auto parsed = parse_double_str(buf);
        if (parsed) {
            val = *parsed;
            locked = true;
        }
        recompute_resolved_point();
    }
    return false;
}

void PrecisionCoordinateHud::recompute_resolved_point() {
    Vec2d delta = free_cursor_ - anchor_;
    double free_dist = delta.length();
    double free_angle_deg = std::atan2(delta.y, delta.x) * (180.0 / PI);
    while (free_angle_deg < 0.0) free_angle_deg += 360.0;

    switch (mode_) {
        case HudInputMode::PolarDistanceAngle: {
            double eff_dist = field1_locked_ ? field1_locked_val_ : free_dist;
            double eff_ang_deg = field2_locked_ ? field2_locked_val_ : free_angle_deg;
            double rad = eff_ang_deg * (PI / 180.0);
            resolved_point_ = anchor_ + Vec2d{std::cos(rad), std::sin(rad)} * eff_dist;
            break;
        }

        case HudInputMode::RelativeCartesian: {
            double eff_dx = field1_locked_ ? field1_locked_val_ : delta.x;
            double eff_dy = field2_locked_ ? field2_locked_val_ : delta.y;
            resolved_point_ = anchor_ + Vec2d{eff_dx, eff_dy};
            break;
        }

        case HudInputMode::AbsoluteCartesian: {
            double eff_x = field1_locked_ ? field1_locked_val_ : free_cursor_.x;
            double eff_y = field2_locked_ ? field2_locked_val_ : free_cursor_.y;
            resolved_point_ = {eff_x, eff_y};
            break;
        }
    }
}

std::optional<Point2D> PrecisionCoordinateHud::commit() {
    recompute_resolved_point();
    Point2D pt = resolved_point_;
    reset();
    return pt;
}

HudDisplayState PrecisionCoordinateHud::display_state() const {
    HudDisplayState st;
    st.mode = mode_;
    st.anchor = anchor_;
    st.resolved_world_point = resolved_point_;
    st.can_commit = (field1_locked_ || field2_locked_ || !field1_buffer_.empty() || !field2_buffer_.empty());

    Vec2d delta = free_cursor_ - anchor_;
    double free_dist = delta.length();
    double free_angle_deg = std::atan2(delta.y, delta.x) * (180.0 / PI);
    while (free_angle_deg < 0.0) free_angle_deg += 360.0;

    switch (mode_) {
        case HudInputMode::PolarDistanceAngle:
            st.field1.label = "Length";
            st.field1.current_value = field1_locked_ ? field1_locked_val_ : free_dist;
            st.field1.text_buffer = field1_buffer_;
            st.field1.is_locked = field1_locked_;
            st.field1.is_active = (active_field_idx_ == 0);

            st.field2.label = "Angle";
            st.field2.current_value = field2_locked_ ? field2_locked_val_ : free_angle_deg;
            st.field2.text_buffer = field2_buffer_;
            st.field2.is_locked = field2_locked_;
            st.field2.is_active = (active_field_idx_ == 1);
            break;

        case HudInputMode::RelativeCartesian:
            st.field1.label = "dX";
            st.field1.current_value = field1_locked_ ? field1_locked_val_ : delta.x;
            st.field1.text_buffer = field1_buffer_;
            st.field1.is_locked = field1_locked_;
            st.field1.is_active = (active_field_idx_ == 0);

            st.field2.label = "dY";
            st.field2.current_value = field2_locked_ ? field2_locked_val_ : delta.y;
            st.field2.text_buffer = field2_buffer_;
            st.field2.is_locked = field2_locked_;
            st.field2.is_active = (active_field_idx_ == 1);
            break;

        case HudInputMode::AbsoluteCartesian:
            st.field1.label = "X";
            st.field1.current_value = field1_locked_ ? field1_locked_val_ : free_cursor_.x;
            st.field1.text_buffer = field1_buffer_;
            st.field1.is_locked = field1_locked_;
            st.field1.is_active = (active_field_idx_ == 0);

            st.field2.label = "Y";
            st.field2.current_value = field2_locked_ ? field2_locked_val_ : free_cursor_.y;
            st.field2.text_buffer = field2_buffer_;
            st.field2.is_locked = field2_locked_;
            st.field2.is_active = (active_field_idx_ == 1);
            break;
    }

    return st;
}

core::base::Result<Point2D> PrecisionCoordinateHud::parse_coordinate_string(
    std::string_view input,
    const Point2D& anchor
) {
    while (!input.empty() && std::isspace(static_cast<unsigned char>(input.front()))) input.remove_prefix(1);
    while (!input.empty() && std::isspace(static_cast<unsigned char>(input.back()))) input.remove_suffix(1);

    if (input.empty()) {
        return core::base::Result<Point2D>::err({
            .code = 4501,
            .domain = core::base::ErrorDomain::Validation,
            .severity = core::base::Severity::Error,
            .message = "Empty coordinate input string",
            .details = ""
        });
    }

    bool is_relative = false;
    if (input.front() == '@') {
        is_relative = true;
        input.remove_prefix(1);
    }

    size_t angle_pos = input.find('<');
    if (angle_pos != std::string_view::npos) {
        // Polar syntax: dist < angle
        auto dist_opt = parse_double_str(input.substr(0, angle_pos));
        auto ang_opt = parse_double_str(input.substr(angle_pos + 1));
        if (!dist_opt || !ang_opt) {
            return core::base::Result<Point2D>::err({
                .code = 4502,
                .domain = core::base::ErrorDomain::Validation,
                .severity = core::base::Severity::Error,
                .message = "Malformed polar coordinate string (expected Distance<Angle)",
                .details = ""
            });
        }
        double rad = *ang_opt * (PI / 180.0);
        Vec2d vec{std::cos(rad) * *dist_opt, std::sin(rad) * *dist_opt};
        return core::base::Result<Point2D>::ok(anchor + vec);
    }

    size_t comma_pos = input.find(',');
    if (comma_pos != std::string_view::npos) {
        // Cartesian syntax: x, y
        auto x_opt = parse_double_str(input.substr(0, comma_pos));
        auto y_opt = parse_double_str(input.substr(comma_pos + 1));
        if (!x_opt || !y_opt) {
            return core::base::Result<Point2D>::err({
                .code = 4503,
                .domain = core::base::ErrorDomain::Validation,
                .severity = core::base::Severity::Error,
                .message = "Malformed Cartesian coordinate string (expected X,Y)",
                .details = ""
            });
        }
        if (is_relative) {
            return core::base::Result<Point2D>::ok(anchor + Point2D{*x_opt, *y_opt});
        } else {
            return core::base::Result<Point2D>::ok(Point2D{*x_opt, *y_opt});
        }
    }

    // Single value treated as distance along current direction
    auto single_opt = parse_double_str(input);
    if (single_opt) {
        return core::base::Result<Point2D>::ok(anchor + Point2D{*single_opt, 0.0});
    }

    return core::base::Result<Point2D>::err({
        .code = 4504,
        .domain = core::base::ErrorDomain::Validation,
        .severity = core::base::Severity::Error,
        .message = "Unrecognized coordinate string format",
        .details = ""
    });
}

} // namespace shunya::cad2d
