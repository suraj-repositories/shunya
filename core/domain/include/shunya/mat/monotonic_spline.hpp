#pragma once

#include "shunya/core/base/result.hpp"

#include <utility>
#include <vector>

namespace shunya::mat {

/**
 * @brief Fritsch-Carlson monotonicity-preserving piecewise cubic Hermite spline interpolator.
 */
class MonotonicCubicSpline {
public:
    MonotonicCubicSpline() = default;

    [[nodiscard]] static core::base::Result<MonotonicCubicSpline> create(
        std::vector<double> x,
        std::vector<double> y);

    [[nodiscard]] double evaluate(double x) const noexcept;
    [[nodiscard]] double derivative(double x) const noexcept;
    [[nodiscard]] double integrate(double x_a, double x_b) const noexcept;

    [[nodiscard]] double min_x() const noexcept { return x_.empty() ? 0.0 : x_.front(); }
    [[nodiscard]] double max_x() const noexcept { return x_.empty() ? 0.0 : x_.back(); }
    [[nodiscard]] size_t size() const noexcept { return x_.size(); }

private:
    std::vector<double> x_;
    std::vector<double> y_;
    std::vector<double> m_; // First derivatives at knots

    [[nodiscard]] size_t find_segment(double x) const noexcept;
};

} // namespace shunya::mat
