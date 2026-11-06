#pragma once

#include <shunya/core/base/error.hpp>
#include <shunya/core/base/result.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace shunya::core::domain {

enum class CurveVariable {
    Temperature,
    Frequency,
    Strain,
    StrainRate,
    Pressure,
    Time,
    ElectricField,
    MagneticField,
    Custom
};

[[nodiscard]] constexpr std::string_view curve_variable_to_string(CurveVariable var) noexcept {
    switch (var) {
    case CurveVariable::Temperature:   return "Temperature";
    case CurveVariable::Frequency:     return "Frequency";
    case CurveVariable::Strain:        return "Strain";
    case CurveVariable::StrainRate:    return "StrainRate";
    case CurveVariable::Pressure:      return "Pressure";
    case CurveVariable::Time:          return "Time";
    case CurveVariable::ElectricField: return "ElectricField";
    case CurveVariable::MagneticField: return "MagneticField";
    case CurveVariable::Custom:        return "Custom";
    }
    return "Custom";
}

[[nodiscard]] base::Result<CurveVariable> curve_variable_from_string(std::string_view str);

enum class InterpolationMode {
    Linear,
    CubicSpline,
    Nearest,
    Step
};

[[nodiscard]] constexpr std::string_view interpolation_mode_to_string(InterpolationMode mode) noexcept {
    switch (mode) {
    case InterpolationMode::Linear:      return "Linear";
    case InterpolationMode::CubicSpline: return "CubicSpline";
    case InterpolationMode::Nearest:     return "Nearest";
    case InterpolationMode::Step:        return "Step";
    }
    return "Linear";
}

[[nodiscard]] base::Result<InterpolationMode> interpolation_mode_from_string(std::string_view str);

enum class ExtrapolationPolicy {
    ClampToBoundary,
    LinearExtrapolate,
    ConstantZero,
    ErrorOnExtrapolate
};

[[nodiscard]] constexpr std::string_view extrapolation_policy_to_string(ExtrapolationPolicy policy) noexcept {
    switch (policy) {
    case ExtrapolationPolicy::ClampToBoundary:    return "ClampToBoundary";
    case ExtrapolationPolicy::LinearExtrapolate:  return "LinearExtrapolate";
    case ExtrapolationPolicy::ConstantZero:       return "ConstantZero";
    case ExtrapolationPolicy::ErrorOnExtrapolate: return "ErrorOnExtrapolate";
    }
    return "ClampToBoundary";
}

[[nodiscard]] base::Result<ExtrapolationPolicy> extrapolation_policy_from_string(std::string_view str);

struct CurvePoint {
    double x{0.0};
    double y{0.0};

    bool operator==(const CurvePoint& other) const noexcept {
        return x == other.x && y == other.y;
    }
};

class MaterialCurve {
public:
    MaterialCurve() = default;
    explicit MaterialCurve(std::string name,
                           CurveVariable var = CurveVariable::Temperature,
                           InterpolationMode interp = InterpolationMode::Linear,
                           ExtrapolationPolicy extrap = ExtrapolationPolicy::ClampToBoundary);

    void add_point(double x, double y);
    void set_points(std::vector<CurvePoint> points);
    [[nodiscard]] const std::vector<CurvePoint>& points() const noexcept { return points_; }
    [[nodiscard]] size_t point_count() const noexcept { return points_.size(); }
    void clear() noexcept { points_.clear(); }

    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    void set_name(std::string name) { name_ = std::move(name); }

    [[nodiscard]] CurveVariable variable() const noexcept { return variable_; }
    void set_variable(CurveVariable v) noexcept { variable_ = v; }

    [[nodiscard]] InterpolationMode interpolation_mode() const noexcept { return interp_mode_; }
    void set_interpolation_mode(InterpolationMode m) noexcept { interp_mode_ = m; }

    [[nodiscard]] ExtrapolationPolicy extrapolation_policy() const noexcept { return extrap_policy_; }
    void set_extrapolation_policy(ExtrapolationPolicy p) noexcept { extrap_policy_ = p; }

    [[nodiscard]] base::Result<double> evaluate(double x) const;

    [[nodiscard]] std::string to_json() const;
    static base::Result<MaterialCurve> from_json(std::string_view json_str);

private:
    std::string name_{"unnamed_curve"};
    CurveVariable variable_{CurveVariable::Temperature};
    InterpolationMode interp_mode_{InterpolationMode::Linear};
    ExtrapolationPolicy extrap_policy_{ExtrapolationPolicy::ClampToBoundary};
    std::vector<CurvePoint> points_;

    void sort_and_deduplicate();
    double interpolate_cubic_spline(double x) const;
};

} // namespace shunya::core::domain
