#include <shunya/core/domain/material_curve.hpp>

#include <algorithm>
#include <cmath>
#include <regex>
#include <sstream>

namespace shunya::core::domain {

namespace {

base::Error make_error(std::string msg, int32_t code = 1) {
    return base::Error{
        .code = code,
        .domain = base::ErrorDomain::Materials,
        .severity = base::Severity::Error,
        .message = std::move(msg),
        .details = ""
    };
}

} // namespace

base::Result<CurveVariable> curve_variable_from_string(std::string_view str) {
    if (str == "Temperature")   return CurveVariable::Temperature;
    if (str == "Frequency")     return CurveVariable::Frequency;
    if (str == "Strain")        return CurveVariable::Strain;
    if (str == "StrainRate")    return CurveVariable::StrainRate;
    if (str == "Pressure")      return CurveVariable::Pressure;
    if (str == "Time")          return CurveVariable::Time;
    if (str == "ElectricField") return CurveVariable::ElectricField;
    if (str == "MagneticField") return CurveVariable::MagneticField;
    if (str == "Custom")        return CurveVariable::Custom;
    return make_error(std::string("Unknown CurveVariable: ") + std::string(str));
}

base::Result<InterpolationMode> interpolation_mode_from_string(std::string_view str) {
    if (str == "Linear")      return InterpolationMode::Linear;
    if (str == "CubicSpline") return InterpolationMode::CubicSpline;
    if (str == "Nearest")     return InterpolationMode::Nearest;
    if (str == "Step")        return InterpolationMode::Step;
    return make_error(std::string("Unknown InterpolationMode: ") + std::string(str));
}

base::Result<ExtrapolationPolicy> extrapolation_policy_from_string(std::string_view str) {
    if (str == "ClampToBoundary")    return ExtrapolationPolicy::ClampToBoundary;
    if (str == "LinearExtrapolate")  return ExtrapolationPolicy::LinearExtrapolate;
    if (str == "ConstantZero")       return ExtrapolationPolicy::ConstantZero;
    if (str == "ErrorOnExtrapolate") return ExtrapolationPolicy::ErrorOnExtrapolate;
    return make_error(std::string("Unknown ExtrapolationPolicy: ") + std::string(str));
}

MaterialCurve::MaterialCurve(std::string name,
                             CurveVariable var,
                             InterpolationMode interp,
                             ExtrapolationPolicy extrap)
    : name_(std::move(name)),
      variable_(var),
      interp_mode_(interp),
      extrap_policy_(extrap) {}

void MaterialCurve::add_point(double x, double y) {
    points_.push_back({x, y});
    sort_and_deduplicate();
}

void MaterialCurve::set_points(std::vector<CurvePoint> points) {
    points_ = std::move(points);
    sort_and_deduplicate();
}

void MaterialCurve::sort_and_deduplicate() {
    if (points_.size() <= 1) return;

    std::stable_sort(points_.begin(), points_.end(), [](const CurvePoint& a, const CurvePoint& b) {
        return a.x < b.x;
    });

    // Remove duplicates with exact same x (keep the last one)
    std::vector<CurvePoint> unique_pts;
    unique_pts.reserve(points_.size());
    for (size_t i = 0; i < points_.size(); ++i) {
        if (!unique_pts.empty() && std::abs(unique_pts.back().x - points_[i].x) < 1e-15) {
            unique_pts.back() = points_[i];
        } else {
            unique_pts.push_back(points_[i]);
        }
    }
    points_ = std::move(unique_pts);
}

base::Result<double> MaterialCurve::evaluate(double x) const {
    if (points_.empty()) {
        return make_error("Cannot evaluate empty curve: " + name_);
    }

    if (points_.size() == 1) {
        return points_.front().y;
    }

    const double x_min = points_.front().x;
    const double x_max = points_.back().x;

    // Boundary checks & extrapolation
    if (x < x_min) {
        switch (extrap_policy_) {
        case ExtrapolationPolicy::ClampToBoundary:
            return points_.front().y;
        case ExtrapolationPolicy::ConstantZero:
            return 0.0;
        case ExtrapolationPolicy::ErrorOnExtrapolate:
            return make_error("x (" + std::to_string(x) + ") is below curve minimum (" + std::to_string(x_min) + ")");
        case ExtrapolationPolicy::LinearExtrapolate: {
            double dx = points_[1].x - points_[0].x;
            if (std::abs(dx) < 1e-15) return points_[0].y;
            double slope = (points_[1].y - points_[0].y) / dx;
            return points_[0].y + slope * (x - points_[0].x);
        }
        }
    }

    if (x > x_max) {
        switch (extrap_policy_) {
        case ExtrapolationPolicy::ClampToBoundary:
            return points_.back().y;
        case ExtrapolationPolicy::ConstantZero:
            return 0.0;
        case ExtrapolationPolicy::ErrorOnExtrapolate:
            return make_error("x (" + std::to_string(x) + ") is above curve maximum (" + std::to_string(x_max) + ")");
        case ExtrapolationPolicy::LinearExtrapolate: {
            size_t n = points_.size();
            double dx = points_[n - 1].x - points_[n - 2].x;
            if (std::abs(dx) < 1e-15) return points_.back().y;
            double slope = (points_[n - 1].y - points_[n - 2].y) / dx;
            return points_[n - 1].y + slope * (x - points_[n - 1].x);
        }
        }
    }

    // Exact boundary matches
    if (std::abs(x - x_min) < 1e-15) return points_.front().y;
    if (std::abs(x - x_max) < 1e-15) return points_.back().y;

    // Find interval [x_i, x_{i+1}]
    auto it = std::upper_bound(points_.begin(), points_.end(), x, [](double val, const CurvePoint& pt) {
        return val < pt.x;
    });
    size_t idx = static_cast<size_t>(std::distance(points_.begin(), it));
    if (idx == 0) idx = 1;
    if (idx >= points_.size()) idx = points_.size() - 1;

    size_t i0 = idx - 1;
    size_t i1 = idx;

    const double x0 = points_[i0].x;
    const double x1 = points_[i1].x;
    const double y0 = points_[i0].y;
    const double y1 = points_[i1].y;

    switch (interp_mode_) {
    case InterpolationMode::Nearest: {
        return (std::abs(x - x0) <= std::abs(x1 - x)) ? y0 : y1;
    }
    case InterpolationMode::Step: {
        return y0;
    }
    case InterpolationMode::Linear: {
        double dx = x1 - x0;
        if (std::abs(dx) < 1e-15) return y0;
        double t = (x - x0) / dx;
        return y0 + t * (y1 - y0);
    }
    case InterpolationMode::CubicSpline: {
        return interpolate_cubic_spline(x);
    }
    }

    return y0;
}

double MaterialCurve::interpolate_cubic_spline(double x) const {
    const size_t n = points_.size();
    if (n < 3) {
        // Fall back to linear for 2 points
        double dx = points_[1].x - points_[0].x;
        if (std::abs(dx) < 1e-15) return points_[0].y;
        double t = (x - points_[0].x) / dx;
        return points_[0].y + t * (points_[1].y - points_[0].y);
    }

    // Solve tridiagonal system for natural cubic spline second derivatives (c)
    // S_i(x) = a_i + b_i(x - x_i) + c_i(x - x_i)^2 + d_i(x - x_i)^3
    std::vector<double> h(n - 1);
    for (size_t i = 0; i < n - 1; ++i) {
        h[i] = points_[i + 1].x - points_[i].x;
        if (h[i] <= 0.0) h[i] = 1e-12;
    }

    std::vector<double> alpha(n, 0.0);
    for (size_t i = 1; i < n - 1; ++i) {
        alpha[i] = (3.0 / h[i]) * (points_[i + 1].y - points_[i].y) -
                   (3.0 / h[i - 1]) * (points_[i].y - points_[i - 1].y);
    }

    std::vector<double> l(n, 1.0);
    std::vector<double> mu(n, 0.0);
    std::vector<double> z(n, 0.0);

    for (size_t i = 1; i < n - 1; ++i) {
        l[i] = 2.0 * (points_[i + 1].x - points_[i - 1].x) - h[i - 1] * mu[i - 1];
        if (std::abs(l[i]) < 1e-15) l[i] = 1e-15;
        mu[i] = h[i] / l[i];
        z[i] = (alpha[i] - h[i - 1] * z[i - 1]) / l[i];
    }

    std::vector<double> c(n, 0.0);
    std::vector<double> b(n - 1, 0.0);
    std::vector<double> d(n - 1, 0.0);

    for (int j = static_cast<int>(n) - 2; j >= 0; --j) {
        c[j] = z[j] - mu[j] * c[j + 1];
        b[j] = (points_[j + 1].y - points_[j].y) / h[j] - (h[j] * (c[j + 1] + 2.0 * c[j])) / 3.0;
        d[j] = (c[j + 1] - c[j]) / (3.0 * h[j]);
    }

    // Find which segment x belongs to
    size_t i = 0;
    while (i < n - 2 && x > points_[i + 1].x) {
        ++i;
    }

    double dx = x - points_[i].x;
    return points_[i].y + b[i] * dx + c[i] * dx * dx + d[i] * dx * dx * dx;
}

std::string MaterialCurve::to_json() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"name\": \"" << name_ << "\",\n";
    ss << "  \"variable\": \"" << curve_variable_to_string(variable_) << "\",\n";
    ss << "  \"interpolation\": \"" << interpolation_mode_to_string(interp_mode_) << "\",\n";
    ss << "  \"extrapolation\": \"" << extrapolation_policy_to_string(extrap_policy_) << "\",\n";
    ss << "  \"points\": [\n";
    for (size_t i = 0; i < points_.size(); ++i) {
        ss << "    {\"x\": " << points_[i].x << ", \"y\": " << points_[i].y << "}";
        if (i + 1 < points_.size()) ss << ",";
        ss << "\n";
    }
    ss << "  ]\n";
    ss << "}";
    return ss.str();
}

base::Result<MaterialCurve> MaterialCurve::from_json(std::string_view json_str) {
    std::string s(json_str);

    auto extract_str = [&](const std::string& key) -> std::string {
        std::regex re("\"" + key + R"re("\s*:\s*"([^"]*)")re");
        std::smatch m;
        if (std::regex_search(s, m, re)) {
            return m[1].str();
        }
        return "";
    };

    std::string name = extract_str("name");
    std::string var_str = extract_str("variable");
    std::string interp_str = extract_str("interpolation");
    std::string extrap_str = extract_str("extrapolation");

    CurveVariable var = CurveVariable::Temperature;
    if (!var_str.empty()) {
        auto res = curve_variable_from_string(var_str);
        if (res.is_ok()) var = res.value();
    }

    InterpolationMode interp = InterpolationMode::Linear;
    if (!interp_str.empty()) {
        auto res = interpolation_mode_from_string(interp_str);
        if (res.is_ok()) interp = res.value();
    }

    ExtrapolationPolicy extrap = ExtrapolationPolicy::ClampToBoundary;
    if (!extrap_str.empty()) {
        auto res = extrapolation_policy_from_string(extrap_str);
        if (res.is_ok()) extrap = res.value();
    }

    MaterialCurve curve(name, var, interp, extrap);

    // Parse points array
    std::regex pt_re(R"re(\{\s*"x"\s*:\s*([-+]?[0-9]*\.?[0-9]+(?:[eE][-+]?[0-9]+)?)\s*,\s*"y"\s*:\s*([-+]?[0-9]*\.?[0-9]+(?:[eE][-+]?[0-9]+)?)\s*\})re");
    auto pt_begin = std::sregex_iterator(s.begin(), s.end(), pt_re);
    auto pt_end = std::sregex_iterator();

    std::vector<CurvePoint> parsed_points;
    for (auto it = pt_begin; it != pt_end; ++it) {
        double px = std::stod((*it)[1].str());
        double py = std::stod((*it)[2].str());
        parsed_points.push_back({px, py});
    }

    curve.set_points(std::move(parsed_points));
    return curve;
}

} // namespace shunya::core::domain
