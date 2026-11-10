#include "shunya/mat/monotonic_spline.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::mat {

namespace {

core::base::Error make_spline_error(std::string msg) {
    return core::base::Error{
        .code = 4250,
        .domain = core::base::ErrorDomain::Materials,
        .severity = core::base::Severity::Error,
        .message = std::move(msg),
        .details = ""
    };
}

} // namespace

core::base::Result<MonotonicCubicSpline> MonotonicCubicSpline::create(
    std::vector<double> x,
    std::vector<double> y) {
    if (x.size() < 2 || x.size() != y.size()) {
        return make_spline_error("Spline requires at least 2 (x, y) sample points of matching length");
    }
    for (size_t i = 1; i < x.size(); ++i) {
        if (x[i] <= x[i - 1]) {
            return make_spline_error("Spline x coordinates must be strictly increasing");
        }
    }

    const size_t n = x.size();
    std::vector<double> h(n - 1);
    std::vector<double> delta(n - 1);
    for (size_t i = 0; i + 1 < n; ++i) {
        h[i] = x[i + 1] - x[i];
        delta[i] = (y[i + 1] - y[i]) / h[i];
    }

    std::vector<double> m(n, 0.0);
    if (n == 2) {
        m[0] = m[1] = delta[0];
    } else {
        // Endpoints one-sided non-overshooting estimate
        m[0] = ((2.0 * h[0] + h[1]) * delta[0] - h[0] * delta[1]) / (h[0] + h[1]);
        if (m[0] * delta[0] <= 0.0) m[0] = 0.0;
        else if (std::abs(m[0]) > 3.0 * std::abs(delta[0])) m[0] = 3.0 * delta[0];

        m[n - 1] = ((2.0 * h[n - 2] + h[n - 3]) * delta[n - 2] - h[n - 2] * delta[n - 3]) /
                   (h[n - 2] + h[n - 3]);
        if (m[n - 1] * delta[n - 2] <= 0.0) m[n - 1] = 0.0;
        else if (std::abs(m[n - 1]) > 3.0 * std::abs(delta[n - 2])) m[n - 1] = 3.0 * delta[n - 2];

        // Interior knots via weighted harmonic mean + Fritsch-Carlson monotonicity clamp
        for (size_t i = 1; i + 1 < n; ++i) {
            if (delta[i - 1] * delta[i] <= 0.0) {
                m[i] = 0.0;
            } else {
                const double w1 = 2.0 * h[i] + h[i - 1];
                const double w2 = h[i] + 2.0 * h[i - 1];
                m[i] = (w1 + w2) / (w1 / delta[i - 1] + w2 / delta[i]);
            }
        }

        // Strict Fritsch-Carlson overshoot condition: alpha^2 + beta^2 <= 9
        for (size_t i = 0; i + 1 < n; ++i) {
            if (std::abs(delta[i]) <= 1e-15) {
                m[i] = 0.0;
                m[i + 1] = 0.0;
                continue;
            }
            const double alpha = m[i] / delta[i];
            const double beta = m[i + 1] / delta[i];
            const double r2 = alpha * alpha + beta * beta;
            if (r2 > 9.0) {
                const double tau = 3.0 / std::sqrt(r2);
                m[i] = tau * alpha * delta[i];
                m[i + 1] = tau * beta * delta[i];
            }
        }
    }

    MonotonicCubicSpline sp;
    sp.x_ = std::move(x);
    sp.y_ = std::move(y);
    sp.m_ = std::move(m);
    return sp;
}

size_t MonotonicCubicSpline::find_segment(double x) const noexcept {
    if (x <= x_.front()) return 0;
    if (x >= x_.back()) return x_.size() - 2;
    auto it = std::upper_bound(x_.begin(), x_.end(), x);
    size_t idx = static_cast<size_t>(std::distance(x_.begin(), it));
    return (idx > 0) ? (idx - 1) : 0;
}

double MonotonicCubicSpline::evaluate(double x) const noexcept {
    if (x_.empty()) return 0.0;
    if (x <= x_.front()) return y_.front();
    if (x >= x_.back()) return y_.back();

    const size_t i = find_segment(x);
    const double h = x_[i + 1] - x_[i];
    const double t = (x - x_[i]) / h;
    const double t2 = t * t;
    const double t3 = t2 * t;

    const double h00 = 2.0 * t3 - 3.0 * t2 + 1.0;
    const double h10 = t3 - 2.0 * t2 + t;
    const double h01 = -2.0 * t3 + 3.0 * t2;
    const double h11 = t3 - t2;

    return h00 * y_[i] + h10 * h * m_[i] + h01 * y_[i + 1] + h11 * h * m_[i + 1];
}

double MonotonicCubicSpline::derivative(double x) const noexcept {
    if (x_.size() < 2) return 0.0;
    if (x <= x_.front()) return m_.front();
    if (x >= x_.back()) return m_.back();

    const size_t i = find_segment(x);
    const double h = x_[i + 1] - x_[i];
    const double t = (x - x_[i]) / h;
    const double t2 = t * t;

    const double dh00 = 6.0 * t2 - 6.0 * t;
    const double dh10 = 3.0 * t2 - 4.0 * t + 1.0;
    const double dh01 = -6.0 * t2 + 6.0 * t;
    const double dh11 = 3.0 * t2 - 2.0 * t;

    return (dh00 * y_[i] + dh01 * y_[i + 1]) / h + dh10 * m_[i] + dh11 * m_[i + 1];
}

double MonotonicCubicSpline::integrate(double x_a, double x_b) const noexcept {
    if (x_.size() < 2 || std::abs(x_b - x_a) <= 1e-15) return 0.0;
    const double sign = (x_b < x_a) ? -1.0 : 1.0;
    const double lo = std::min(x_a, x_b);
    const double hi = std::max(x_a, x_b);

    auto antideriv_at_segment = [&](size_t i, double u_lo, double u_hi) {
        const double h = x_[i + 1] - x_[i];
        auto poly_F = [&](double t) {
            const double t2 = t * t;
            const double t3 = t2 * t;
            const double t4 = t2 * t2;
            const double i00 = 0.5 * t4 - t3 + t;
            const double i10 = 0.25 * t4 - (2.0 / 3.0) * t3 + 0.5 * t2;
            const double i01 = -0.5 * t4 + t3;
            const double i11 = 0.25 * t4 - (1.0 / 3.0) * t3;
            return h * (i00 * y_[i] + i10 * h * m_[i] + i01 * y_[i + 1] + i11 * h * m_[i + 1]);
        };
        const double t_lo = (u_lo - x_[i]) / h;
        const double t_hi = (u_hi - x_[i]) / h;
        return poly_F(t_hi) - poly_F(t_lo);
    };

    double sum = 0.0;
    if (lo < x_.front()) {
        const double cap = std::min(hi, x_.front());
        sum += y_.front() * (cap - lo);
    }
    for (size_t i = 0; i + 1 < x_.size(); ++i) {
        const double a = std::max(lo, x_[i]);
        const double b = std::min(hi, x_[i + 1]);
        if (b > a) sum += antideriv_at_segment(i, a, b);
    }
    if (hi > x_.back()) {
        const double base = std::max(lo, x_.back());
        sum += y_.back() * (hi - base);
    }
    return sign * sum;
}

} // namespace shunya::mat
