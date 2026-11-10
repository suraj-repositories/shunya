#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/core/domain/circuit.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

namespace shunya::chem {

/**
 * @brief L-stable 2-stage Rosenbrock (ROS2) implicit solver for stiff chemical kinetics ODEs y' = f(t, y).
 */
class StiffOdeSolver {
public:
    using RhsFn = std::function<void(double t, const std::vector<double>& y, std::vector<double>& dydt)>;

    struct Options {
        bool clamp_non_negative{true};
        double jacobian_eps{1e-7};
    };

    [[nodiscard]] static core::base::Result<void> step_ros2(
        double t,
        double dt,
        std::vector<double>& y,
        const RhsFn& rhs_fn,
        const Options& opts) {
        using core::domain::DenseMatrix;

        const size_t n = y.size();
        if (n == 0 || dt <= 0.0 || !rhs_fn) {
            return core::base::Result<void>::err(
                core::base::Error{90, core::base::ErrorDomain::Validation, core::base::Severity::Error,
                                  "StiffOdeSolver requires non-empty state, positive dt, and RHS function", ""});
        }

        // L-stable ROS2 parameter: gamma = 1 - 1 / sqrt(2)
        constexpr double kGamma = 0.29289321881345247560;

        std::vector<double> f0(n, 0.0);
        rhs_fn(t, y, f0);

        // Numerical Jacobian J = df / dy
        DenseMatrix M(n, n);
        std::vector<double> y_pert = y;
        std::vector<double> f_pert(n, 0.0);
        for (size_t j = 0; j < n; ++j) {
            const double h = opts.jacobian_eps * std::max(1.0, std::abs(y[j]));
            const double orig = y[j];
            y_pert[j] = orig + h;
            rhs_fn(t, y_pert, f_pert);
            y_pert[j] = orig;

            for (size_t i = 0; i < n; ++i) {
                const double J_ij = (f_pert[i] - f0[i]) / h;
                M(i, j) = ((i == j) ? 1.0 : 0.0) - kGamma * dt * J_ij;
            }
        }

        // Stage 1: M * k1 = f0
        auto k1_res = M.solve_lu(f0);
        if (!k1_res.is_ok()) {
            return core::base::Result<void>::err(k1_res.error());
        }
        const auto& k1 = k1_res.value();

        // Stage 2: M * k2 = f(t + dt, y + dt * k1) - 2 * k1
        std::vector<double> y_stage(n, 0.0);
        for (size_t i = 0; i < n; ++i) {
            y_stage[i] = y[i] + dt * k1[i];
            if (opts.clamp_non_negative) y_stage[i] = std::max(0.0, y_stage[i]);
        }
        std::vector<double> f1(n, 0.0);
        rhs_fn(t + dt, y_stage, f1);

        std::vector<double> rhs2(n, 0.0);
        for (size_t i = 0; i < n; ++i) {
            rhs2[i] = f1[i] - 2.0 * k1[i];
        }
        auto k2_res = M.solve_lu(rhs2);
        if (!k2_res.is_ok()) {
            return core::base::Result<void>::err(k2_res.error());
        }
        const auto& k2 = k2_res.value();

        for (size_t i = 0; i < n; ++i) {
            y[i] += 1.5 * dt * k1[i] + 0.5 * dt * k2[i];
            if (opts.clamp_non_negative && y[i] < 0.0) {
                y[i] = 0.0;
            }
        }
        return core::base::Result<void>::ok();
    }

    [[nodiscard]] static core::base::Result<std::vector<std::vector<double>>> integrate(
        double t_end,
        double dt,
        std::vector<double>& y,
        const RhsFn& rhs_fn) {
        return integrate(t_end, dt, y, rhs_fn, Options{});
    }

    [[nodiscard]] static core::base::Result<std::vector<std::vector<double>>> integrate(
        double t_end,
        double dt,
        std::vector<double>& y,
        const RhsFn& rhs_fn,
        const Options& opts) {
        if (t_end <= 0.0 || dt <= 0.0) {
            return core::base::Result<std::vector<std::vector<double>>>::err(
                core::base::Error{91, core::base::ErrorDomain::Validation, core::base::Severity::Error,
                                  "Integration t_end and dt must be positive", ""});
        }
        const size_t steps = static_cast<size_t>(std::ceil(t_end / dt));
        const double actual_dt = t_end / static_cast<double>(std::max<size_t>(1, steps));
        std::vector<std::vector<double>> history;
        history.reserve(steps + 1);
        history.push_back(y);

        for (size_t s = 0; s < steps; ++s) {
            const double t = static_cast<double>(s) * actual_dt;
            auto res = step_ros2(t, actual_dt, y, rhs_fn, opts);
            if (!res.is_ok()) {
                return core::base::Result<std::vector<std::vector<double>>>::err(res.error());
            }
            history.push_back(y);
        }
        return core::base::Result<std::vector<std::vector<double>>>::ok(std::move(history));
    }
};

} // namespace shunya::chem
