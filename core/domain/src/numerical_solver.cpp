#include "shunya/core/domain/numerical_solver.hpp"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <regex>
#include <sstream>

namespace shunya::core::domain {

namespace {

base::Error make_solver_error(std::string msg, int32_t code = 2700) {
    return base::Error{
        .code = code,
        .domain = base::ErrorDomain::Simulation,
        .severity = base::Severity::Error,
        .message = std::move(msg),
        .details = ""
    };
}

} // namespace

// =============================================================================
// SOL-01: Problem Types & Config
// =============================================================================

std::optional<ProblemType> problem_type_from_string(std::string_view str) noexcept {
    if (str == "Linear") return ProblemType::Linear;
    if (str == "Nonlinear") return ProblemType::Nonlinear;
    if (str == "ODE") return ProblemType::ODE;
    if (str == "DAE") return ProblemType::DAE;
    if (str == "PDE") return ProblemType::PDE;
    if (str == "DiscreteEvent") return ProblemType::DiscreteEvent;
    return std::nullopt;
}

std::string SolverConfig::to_json() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"relative_tolerance\": " << relative_tolerance << ",\n";
    ss << "  \"absolute_tolerance\": " << absolute_tolerance << ",\n";
    ss << "  \"max_iterations\": " << max_iterations << ",\n";
    ss << "  \"damping_factor\": " << damping_factor << ",\n";
    ss << "  \"initial_step_size\": " << initial_step_size << ",\n";
    ss << "  \"min_step_size\": " << min_step_size << ",\n";
    ss << "  \"max_step_size\": " << max_step_size << ",\n";
    ss << "  \"use_preconditioning\": " << (use_preconditioning ? "true" : "false") << ",\n";
    ss << "  \"enable_line_search\": " << (enable_line_search ? "true" : "false") << "\n";
    ss << "}";
    return ss.str();
}

base::Result<SolverConfig> SolverConfig::from_json(std::string_view json_str) {
    std::string s(json_str);

    auto extract_num = [&](const std::string& key) -> std::optional<double> {
        std::regex re("\"" + key + R"re("\s*:\s*([-+]?[0-9]*\.?[0-9]+(?:[eE][-+]?[0-9]+)?))re");
        std::smatch m;
        if (std::regex_search(s, m, re)) return std::stod(m[1].str());
        return std::nullopt;
    };

    auto extract_u64 = [&](const std::string& key) -> std::optional<size_t> {
        std::regex re("\"" + key + R"re("\s*:\s*([0-9]+))re");
        std::smatch m;
        if (std::regex_search(s, m, re)) return std::stoull(m[1].str());
        return std::nullopt;
    };

    auto extract_bool = [&](const std::string& key) -> std::optional<bool> {
        std::regex re("\"" + key + R"re("\s*:\s*(true|false))re");
        std::smatch m;
        if (std::regex_search(s, m, re)) return m[1].str() == "true";
        return std::nullopt;
    };

    SolverConfig cfg;
    if (auto v = extract_num("relative_tolerance")) cfg.relative_tolerance = *v;
    if (auto v = extract_num("absolute_tolerance")) cfg.absolute_tolerance = *v;
    if (auto v = extract_u64("max_iterations")) cfg.max_iterations = *v;
    if (auto v = extract_num("damping_factor")) cfg.damping_factor = *v;
    if (auto v = extract_num("initial_step_size")) cfg.initial_step_size = *v;
    if (auto v = extract_num("min_step_size")) cfg.min_step_size = *v;
    if (auto v = extract_num("max_step_size")) cfg.max_step_size = *v;
    if (auto v = extract_bool("use_preconditioning")) cfg.use_preconditioning = *v;
    if (auto v = extract_bool("enable_line_search")) cfg.enable_line_search = *v;

    return cfg;
}

// =============================================================================
// SOL-09: Solver Diagnostics
// =============================================================================

void SolverDiagnostics::reset() noexcept {
    total_iterations = 0;
    step_count = 0;
    accepted_steps = 0;
    rejected_steps = 0;
    initial_residual = 0.0;
    final_residual = 0.0;
    residual_reduction_ratio = 0.0;
    matrix_condition_estimate = 1.0;
    assembly_time_ms = 0.0;
    solve_time_ms = 0.0;
    total_runtime_ms = 0.0;
    peak_memory_bytes = 0;
    stagnation_detected = false;
}

std::string SolverDiagnostics::to_json() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"total_iterations\": " << total_iterations << ",\n";
    ss << "  \"step_count\": " << step_count << ",\n";
    ss << "  \"accepted_steps\": " << accepted_steps << ",\n";
    ss << "  \"rejected_steps\": " << rejected_steps << ",\n";
    ss << "  \"initial_residual\": " << initial_residual << ",\n";
    ss << "  \"final_residual\": " << final_residual << ",\n";
    ss << "  \"residual_reduction_ratio\": " << residual_reduction_ratio << ",\n";
    ss << "  \"matrix_condition_estimate\": " << matrix_condition_estimate << ",\n";
    ss << "  \"solve_time_ms\": " << solve_time_ms << ",\n";
    ss << "  \"total_runtime_ms\": " << total_runtime_ms << ",\n";
    ss << "  \"stagnation_detected\": " << (stagnation_detected ? "true" : "false") << "\n";
    ss << "}";
    return ss.str();
}

base::Result<SolverDiagnostics> SolverDiagnostics::from_json(std::string_view json_str) {
    std::string s(json_str);

    auto extract_num = [&](const std::string& key) -> std::optional<double> {
        std::regex re("\"" + key + R"re("\s*:\s*([-+]?[0-9]*\.?[0-9]+(?:[eE][-+]?[0-9]+)?))re");
        std::smatch m;
        if (std::regex_search(s, m, re)) return std::stod(m[1].str());
        return std::nullopt;
    };

    auto extract_u64 = [&](const std::string& key) -> std::optional<size_t> {
        std::regex re("\"" + key + R"re("\s*:\s*([0-9]+))re");
        std::smatch m;
        if (std::regex_search(s, m, re)) return std::stoull(m[1].str());
        return std::nullopt;
    };

    auto extract_bool = [&](const std::string& key) -> std::optional<bool> {
        std::regex re("\"" + key + R"re("\s*:\s*(true|false))re");
        std::smatch m;
        if (std::regex_search(s, m, re)) return m[1].str() == "true";
        return std::nullopt;
    };

    SolverDiagnostics diag;
    if (auto v = extract_u64("total_iterations")) diag.total_iterations = *v;
    if (auto v = extract_u64("step_count")) diag.step_count = *v;
    if (auto v = extract_u64("accepted_steps")) diag.accepted_steps = *v;
    if (auto v = extract_u64("rejected_steps")) diag.rejected_steps = *v;
    if (auto v = extract_num("initial_residual")) diag.initial_residual = *v;
    if (auto v = extract_num("final_residual")) diag.final_residual = *v;
    if (auto v = extract_num("residual_reduction_ratio")) diag.residual_reduction_ratio = *v;
    if (auto v = extract_num("matrix_condition_estimate")) diag.matrix_condition_estimate = *v;
    if (auto v = extract_num("solve_time_ms")) diag.solve_time_ms = *v;
    if (auto v = extract_num("total_runtime_ms")) diag.total_runtime_ms = *v;
    if (auto v = extract_bool("stagnation_detected")) diag.stagnation_detected = *v;

    return diag;
}

// =============================================================================
// SOL-03: Preconditioners
// =============================================================================

void IdentityPreconditioner::apply(std::span<const double> r, std::span<double> z) const noexcept {
    const size_t n = std::min(r.size(), z.size());
    std::copy_n(r.data(), n, z.data());
}

base::Result<void> JacobiPreconditioner::setup(const SparseMatrixCSR& A) {
    const size_t n = A.rows();
    inv_diag_.resize(n, 1.0);
    for (size_t i = 0; i < n; ++i) {
        const double d = A.coeff(i, i);
        inv_diag_[i] = (std::abs(d) > 1e-15) ? (1.0 / d) : 1.0;
    }
    return {};
}

void JacobiPreconditioner::apply(std::span<const double> r, std::span<double> z) const noexcept {
    const size_t n = std::min({r.size(), z.size(), inv_diag_.size()});
    for (size_t i = 0; i < n; ++i) {
        z[i] = r[i] * inv_diag_[i];
    }
}

base::Result<void> ILU0Preconditioner::setup(const SparseMatrixCSR& A) {
    auto ilu_res = A.compute_ilu0();
    if (!ilu_res.is_ok()) return ilu_res.error();
    lu_factored_ = std::move(ilu_res.value());
    return {};
}

void ILU0Preconditioner::apply(std::span<const double> r, std::span<double> z) const noexcept {
    lu_factored_.solve_lu(r, z);
}

// =============================================================================
// SOL-03: Linear Solvers
// =============================================================================

base::Result<LinearSolveReport> ConjugateGradientSolver::solve(
    const SparseMatrixCSR& A,
    std::span<const double> b,
    std::span<double> x,
    const SolverConfig& config
) {
    const size_t n = A.rows();
    if (n != A.cols() || n != b.size() || n != x.size()) {
        return make_solver_error("ConjugateGradient dimension mismatch", 2710);
    }

    LinearSolveReport report;
    std::vector<double> r(n, 0.0);
    std::vector<double> Ax(n, 0.0);
    A.multiply(x, Ax);

    for (size_t i = 0; i < n; ++i) {
        r[i] = b[i] - Ax[i];
    }

    const double b_norm = vector_ops::norm_2(b);
    report.initial_residual = vector_ops::norm_2(r);

    const double target_tol = std::max(config.absolute_tolerance, config.relative_tolerance * (b_norm > 0 ? b_norm : 1.0));
    if (report.initial_residual <= target_tol) {
        report.converged = true;
        report.final_residual = report.initial_residual;
        report.relative_residual = 0.0;
        return report;
    }

    std::vector<double> z(n, 0.0);
    if (precond_) {
        precond_->apply(r, z);
    } else {
        z = r;
    }

    std::vector<double> p = z;
    double rho = vector_ops::dot(r, z);
    std::vector<double> Ap(n, 0.0);

    for (size_t iter = 0; iter < config.max_iterations; ++iter) {
        ++report.iterations;
        A.multiply(p, Ap);

        const double pAp = vector_ops::dot(p, Ap);
        if (pAp <= 1e-18) {
            // Indefinite or breakdown
            break;
        }

        const double alpha = rho / pAp;
        vector_ops::axpy(alpha, p, x);
        vector_ops::axpy(-alpha, Ap, r);

        const double cur_res = vector_ops::norm_2(r);
        if (cur_res <= target_tol) {
            report.converged = true;
            report.final_residual = cur_res;
            report.relative_residual = (b_norm > 0) ? (cur_res / b_norm) : cur_res;
            return report;
        }

        if (precond_) {
            precond_->apply(r, z);
        } else {
            z = r;
        }

        const double rho_new = vector_ops::dot(r, z);
        const double beta = rho_new / rho;
        for (size_t i = 0; i < n; ++i) {
            p[i] = z[i] + beta * p[i];
        }
        rho = rho_new;
    }

    report.final_residual = vector_ops::norm_2(r);
    report.relative_residual = (b_norm > 0) ? (report.final_residual / b_norm) : report.final_residual;
    report.converged = (report.final_residual <= target_tol);
    return report;
}

base::Result<LinearSolveReport> BiCGSTABSolver::solve(
    const SparseMatrixCSR& A,
    std::span<const double> b,
    std::span<double> x,
    const SolverConfig& config
) {
    const size_t n = A.rows();
    if (n != A.cols() || n != b.size() || n != x.size()) {
        return make_solver_error("BiCGSTAB dimension mismatch", 2711);
    }

    LinearSolveReport report;
    std::vector<double> r(n, 0.0);
    std::vector<double> Ax(n, 0.0);
    A.multiply(x, Ax);

    for (size_t i = 0; i < n; ++i) {
        r[i] = b[i] - Ax[i];
    }

    const double b_norm = vector_ops::norm_2(b);
    report.initial_residual = vector_ops::norm_2(r);

    const double target_tol = std::max(config.absolute_tolerance, config.relative_tolerance * (b_norm > 0 ? b_norm : 1.0));
    if (report.initial_residual <= target_tol) {
        report.converged = true;
        report.final_residual = report.initial_residual;
        return report;
    }

    std::vector<double> r_star = r;
    std::vector<double> p = r;
    std::vector<double> v(n, 0.0);
    std::vector<double> s(n, 0.0);
    std::vector<double> t(n, 0.0);
    std::vector<double> p_hat(n, 0.0);
    std::vector<double> s_hat(n, 0.0);

    double rho = 1.0;
    double alpha = 1.0;
    double omega = 1.0;

    for (size_t iter = 0; iter < config.max_iterations; ++iter) {
        ++report.iterations;
        const double rho_new = vector_ops::dot(r_star, r);
        if (std::abs(rho_new) < 1e-18) {
            report.message = "BiCGSTAB breakdown (rho == 0)";
            break;
        }

        if (iter > 0) {
            const double beta = (rho_new / rho) * (alpha / omega);
            for (size_t i = 0; i < n; ++i) {
                p[i] = r[i] + beta * (p[i] - omega * v[i]);
            }
        } else {
            p = r;
        }

        if (precond_) {
            precond_->apply(p, p_hat);
        } else {
            p_hat = p;
        }

        A.multiply(p_hat, v);
        const double r_star_v = vector_ops::dot(r_star, v);
        if (std::abs(r_star_v) < 1e-18) {
            report.message = "BiCGSTAB breakdown (r_star . v == 0)";
            break;
        }

        alpha = rho_new / r_star_v;
        for (size_t i = 0; i < n; ++i) {
            s[i] = r[i] - alpha * v[i];
        }

        if (vector_ops::norm_2(s) <= target_tol) {
            vector_ops::axpy(alpha, p_hat, x);
            report.converged = true;
            report.final_residual = vector_ops::norm_2(s);
            return report;
        }

        if (precond_) {
            precond_->apply(s, s_hat);
        } else {
            s_hat = s;
        }

        A.multiply(s_hat, t);
        const double tt = vector_ops::dot(t, t);
        if (std::abs(tt) < 1e-18) {
            omega = 0.0;
        } else {
            omega = vector_ops::dot(t, s) / tt;
        }

        for (size_t i = 0; i < n; ++i) {
            x[i] += alpha * p_hat[i] + omega * s_hat[i];
            r[i] = s[i] - omega * t[i];
        }

        const double cur_res = vector_ops::norm_2(r);
        if (cur_res <= target_tol) {
            report.converged = true;
            report.final_residual = cur_res;
            report.relative_residual = (b_norm > 0) ? (cur_res / b_norm) : cur_res;
            return report;
        }

        if (std::abs(omega) < 1e-18) {
            break;
        }

        rho = rho_new;
    }

    report.final_residual = vector_ops::norm_2(r);
    report.relative_residual = (b_norm > 0) ? (report.final_residual / b_norm) : report.final_residual;
    report.converged = (report.final_residual <= target_tol);
    return report;
}

base::Result<LinearSolveReport> DirectLUSolver::solve(
    const SparseMatrixCSR& A,
    std::span<const double> b,
    std::span<double> x,
    const SolverConfig& config
) {
    (void)config;
    const size_t n = A.rows();
    if (n != A.cols() || n != b.size() || n != x.size()) {
        return make_solver_error("DirectLU dimension mismatch", 2712);
    }

    LinearSolveReport report;
    report.iterations = 1;

    // Convert to dense matrix for stable partial-pivoting LU
    std::vector<std::vector<double>> M(n, std::vector<double>(n, 0.0));
    for (size_t r = 0; r < n; ++r) {
        const size_t start = A.row_ptr()[r];
        const size_t end = A.row_ptr()[r + 1];
        for (size_t idx = start; idx < end; ++idx) {
            M[r][A.col_indices()[idx]] = A.values()[idx];
        }
    }

    std::vector<double> rhs(b.begin(), b.end());
    std::vector<size_t> p(n);
    for (size_t i = 0; i < n; ++i) p[i] = i;

    // Gaussian elimination with partial pivoting
    for (size_t k = 0; k < n; ++k) {
        size_t max_row = k;
        double max_val = std::abs(M[k][k]);
        for (size_t i = k + 1; i < n; ++i) {
            if (std::abs(M[i][k]) > max_val) {
                max_val = std::abs(M[i][k]);
                max_row = i;
            }
        }

        if (max_val < 1e-15) {
            return make_solver_error("Singular matrix in DirectLU at column " + std::to_string(k), 2713);
        }

        if (max_row != k) {
            std::swap(M[k], M[max_row]);
            std::swap(rhs[k], rhs[max_row]);
            std::swap(p[k], p[max_row]);
        }

        for (size_t i = k + 1; i < n; ++i) {
            const double factor = M[i][k] / M[k][k];
            M[i][k] = factor;
            for (size_t j = k + 1; j < n; ++j) {
                M[i][j] -= factor * M[k][j];
            }
            rhs[i] -= factor * rhs[k];
        }
    }

    // Back substitution
    for (int i = static_cast<int>(n) - 1; i >= 0; --i) {
        double sum = rhs[i];
        for (size_t j = static_cast<size_t>(i) + 1; j < n; ++j) {
            sum -= M[i][j] * x[j];
        }
        x[i] = sum / M[i][i];
    }

    report.converged = true;
    std::vector<double> Ax(n, 0.0);
    A.multiply(x, Ax);
    double res = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const double diff = b[i] - Ax[i];
        res += diff * diff;
    }
    report.final_residual = std::sqrt(res);
    return report;
}

// =============================================================================
// SOL-04: Nonlinear Solver (Newton-Raphson)
// =============================================================================

NewtonRaphsonSolver::NewtonRaphsonSolver(std::shared_ptr<ILinearSolver> linear_solver)
    : linear_solver_(std::move(linear_solver)) {
    if (!linear_solver_) {
        linear_solver_ = std::make_shared<DirectLUSolver>();
    }
}

base::Result<NonlinearSolveReport> NewtonRaphsonSolver::solve(
    NonlinearResidualFn residual_fn,
    std::optional<NonlinearJacobianFn> jacobian_fn,
    std::span<double> x,
    const SolverConfig& config
) {
    const size_t n = x.size();
    NonlinearSolveReport report;

    auto F = residual_fn(x);
    if (F.size() != n) {
        return make_solver_error("Residual function size mismatch", 2720);
    }

    report.initial_residual_norm = vector_ops::norm_inf(F);
    report.residual_history.push_back(report.initial_residual_norm);

    if (report.initial_residual_norm <= config.absolute_tolerance) {
        report.converged = true;
        report.final_residual_norm = report.initial_residual_norm;
        return report;
    }

    std::vector<double> delta_x(n, 0.0);
    std::vector<double> neg_F(n, 0.0);

    for (size_t iter = 0; iter < config.max_iterations; ++iter) {
        ++report.iterations;

        // Compute Jacobian
        SparseMatrixCSR J;
        if (jacobian_fn.has_value()) {
            J = (*jacobian_fn)(x);
        } else {
            // Numerical finite differences (central difference)
            std::vector<Triplet> triplets;
            triplets.reserve(n * n);
            const double h = 1e-7;

            std::vector<double> x_perturbed(x.begin(), x.end());
            for (size_t j = 0; j < n; ++j) {
                const double orig = x_perturbed[j];

                x_perturbed[j] = orig + h;
                auto F_plus = residual_fn(x_perturbed);

                x_perturbed[j] = orig - h;
                auto F_minus = residual_fn(x_perturbed);

                x_perturbed[j] = orig;

                for (size_t i = 0; i < n; ++i) {
                    const double deriv = (F_plus[i] - F_minus[i]) / (2.0 * h);
                    if (std::abs(deriv) > 1e-15) {
                        triplets.push_back({i, j, deriv});
                    }
                }
            }
            J = SparseMatrixCSR::from_triplets(n, n, triplets);
        }

        // Solve J * delta_x = -F(x)
        for (size_t i = 0; i < n; ++i) {
            neg_F[i] = -F[i];
            delta_x[i] = 0.0;
        }

        auto lin_res = linear_solver_->solve(J, neg_F, delta_x, config);
        if (!lin_res.is_ok() || !lin_res.value().converged) {
            report.message = "Linear solver failed inside Newton step: " +
                             (lin_res.is_ok() ? lin_res.value().message : lin_res.error().message);
            break;
        }

        // Line search / Armijo backtracking
        double alpha = config.damping_factor;
        const double cur_norm = vector_ops::norm_inf(F);

        if (config.enable_line_search) {
            std::vector<double> x_trial(n);
            while (alpha > 1e-4) {
                for (size_t i = 0; i < n; ++i) {
                    x_trial[i] = x[i] + alpha * delta_x[i];
                }
                auto F_trial = residual_fn(x_trial);
                const double trial_norm = vector_ops::norm_inf(F_trial);
                if (trial_norm < (1.0 - 1e-4 * alpha) * cur_norm) {
                    break;
                }
                alpha *= 0.5;
            }
        }

        // Update x
        for (size_t i = 0; i < n; ++i) {
            x[i] += alpha * delta_x[i];
        }

        F = residual_fn(x);
        const double new_norm = vector_ops::norm_inf(F);
        report.residual_history.push_back(new_norm);

        if (new_norm <= config.absolute_tolerance) {
            report.converged = true;
            report.final_residual_norm = new_norm;
            return report;
        }

        // Check for stagnation
        if (alpha * vector_ops::norm_inf(delta_x) < 1e-14) {
            report.stagnation_detected = true;
            break;
        }
    }

    report.final_residual_norm = vector_ops::norm_inf(F);
    report.converged = (report.final_residual_norm <= config.absolute_tolerance);
    return report;
}

// =============================================================================
// SOL-05: ODE Solvers
// =============================================================================

base::Result<ODEStepResult> ExplicitEulerSolver::step(
    ODERhsFn rhs, double t, double dt, std::span<double> y, const SolverConfig& config
) {
    (void)config;
    const size_t n = y.size();
    auto dydt = rhs(t, y);
    if (dydt.size() != n) {
        return make_solver_error("ODE RHS size mismatch", 2730);
    }

    for (size_t i = 0; i < n; ++i) {
        y[i] += dt * dydt[i];
    }

    ODEStepResult res;
    res.step_accepted = true;
    res.next_dt = dt;
    res.estimated_error = 0.0;
    return res;
}

base::Result<ODEStepResult> RungeKutta4Solver::step(
    ODERhsFn rhs, double t, double dt, std::span<double> y, const SolverConfig& config
) {
    (void)config;
    const size_t n = y.size();
    auto k1 = rhs(t, y);
    if (k1.size() != n) return make_solver_error("ODE RHS size mismatch", 2730);

    std::vector<double> y_tmp(n);
    for (size_t i = 0; i < n; ++i) y_tmp[i] = y[i] + 0.5 * dt * k1[i];
    auto k2 = rhs(t + 0.5 * dt, y_tmp);

    for (size_t i = 0; i < n; ++i) y_tmp[i] = y[i] + 0.5 * dt * k2[i];
    auto k3 = rhs(t + 0.5 * dt, y_tmp);

    for (size_t i = 0; i < n; ++i) y_tmp[i] = y[i] + dt * k3[i];
    auto k4 = rhs(t + dt, y_tmp);

    for (size_t i = 0; i < n; ++i) {
        y[i] += (dt / 6.0) * (k1[i] + 2.0 * k2[i] + 2.0 * k3[i] + k4[i]);
    }

    ODEStepResult res;
    res.step_accepted = true;
    res.next_dt = dt;
    res.estimated_error = 0.0;
    return res;
}

base::Result<ODEStepResult> AdaptiveRK45Solver::step(
    ODERhsFn rhs, double t, double dt, std::span<double> y, const SolverConfig& config
) {
    const size_t n = y.size();

    // Dormand-Prince 5(4) Butcher Tableau
    auto k1 = rhs(t, y);
    if (k1.size() != n) return make_solver_error("ODE RHS size mismatch", 2730);

    std::vector<double> y_tmp(n);

    // k2
    for (size_t i = 0; i < n; ++i) y_tmp[i] = y[i] + dt * (1.0 / 5.0) * k1[i];
    auto k2 = rhs(t + (1.0 / 5.0) * dt, y_tmp);

    // k3
    for (size_t i = 0; i < n; ++i) y_tmp[i] = y[i] + dt * ((3.0 / 40.0) * k1[i] + (9.0 / 40.0) * k2[i]);
    auto k3 = rhs(t + (3.0 / 10.0) * dt, y_tmp);

    // k4
    for (size_t i = 0; i < n; ++i)
        y_tmp[i] = y[i] + dt * ((44.0 / 45.0) * k1[i] - (56.0 / 15.0) * k2[i] + (32.0 / 9.0) * k3[i]);
    auto k4 = rhs(t + (4.0 / 5.0) * dt, y_tmp);

    // k5
    for (size_t i = 0; i < n; ++i)
        y_tmp[i] = y[i] + dt * ((19372.0 / 6561.0) * k1[i] - (25360.0 / 2187.0) * k2[i] +
                               (64448.0 / 6561.0) * k3[i] - (212.0 / 729.0) * k4[i]);
    auto k5 = rhs(t + (8.0 / 9.0) * dt, y_tmp);

    // k6
    for (size_t i = 0; i < n; ++i)
        y_tmp[i] = y[i] + dt * ((9017.0 / 3168.0) * k1[i] - (355.0 / 33.0) * k2[i] +
                               (46732.0 / 5247.0) * k3[i] + (49.0 / 176.0) * k4[i] -
                               (5103.0 / 18656.0) * k5[i]);
    auto k6 = rhs(t + dt, y_tmp);

    // 5th order solution
    std::vector<double> y5(n);
    for (size_t i = 0; i < n; ++i) {
        y5[i] = y[i] + dt * ((35.0 / 384.0) * k1[i] + (500.0 / 1113.0) * k3[i] +
                             (125.0 / 192.0) * k4[i] - (2187.0 / 6784.0) * k5[i] +
                             (11.0 / 84.0) * k6[i]);
    }

    // 4th order solution
    std::vector<double> y4(n);
    auto k7 = rhs(t + dt, y5);
    for (size_t i = 0; i < n; ++i) {
        y4[i] = y[i] + dt * ((5179.0 / 57600.0) * k1[i] + (7571.0 / 16695.0) * k3[i] +
                             (393.0 / 640.0) * k4[i] - (92097.0 / 339200.0) * k5[i] +
                             (187.0 / 2100.0) * k6[i] + (1.0 / 40.0) * k7[i]);
    }

    // Error norm
    double max_err = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const double scale = config.absolute_tolerance + config.relative_tolerance * std::max(std::abs(y[i]), std::abs(y5[i]));
        const double err = std::abs(y5[i] - y4[i]) / (scale > 0 ? scale : 1.0);
        if (err > max_err) max_err = err;
    }

    ODEStepResult result;
    result.estimated_error = max_err;

    if (max_err <= 1.0) {
        result.step_accepted = true;
        std::copy(y5.begin(), y5.end(), y.begin());
        double factor = 0.9 * std::pow(1.0 / (max_err > 1e-10 ? max_err : 1e-10), 0.2);
        factor = std::clamp(factor, 0.2, 2.0);
        result.next_dt = std::clamp(dt * factor, config.min_step_size, config.max_step_size);
    } else {
        result.step_accepted = false;
        double factor = 0.9 * std::pow(1.0 / max_err, 0.25);
        factor = std::clamp(factor, 0.2, 0.9);
        result.next_dt = std::clamp(dt * factor, config.min_step_size, config.max_step_size);
        result.message = "Local truncation error exceeded tolerance in AdaptiveRK45";
    }

    return result;
}

ImplicitEulerSolver::ImplicitEulerSolver(std::shared_ptr<INonlinearSolver> nonlinear_solver)
    : nonlinear_solver_(std::move(nonlinear_solver)) {
    if (!nonlinear_solver_) {
        nonlinear_solver_ = std::make_shared<NewtonRaphsonSolver>();
    }
}

base::Result<ODEStepResult> ImplicitEulerSolver::step(
    ODERhsFn rhs, double t, double dt, std::span<double> y, const SolverConfig& config
) {
    const size_t n = y.size();
    std::vector<double> y_prev(y.begin(), y.end());
    std::vector<double> y_next(y.begin(), y.end()); // initial guess

    // Residual function: G(y_next) = y_next - y_prev - dt * rhs(t + dt, y_next) = 0
    auto G = [&](std::span<const double> yn) -> std::vector<double> {
        auto dydt = rhs(t + dt, yn);
        std::vector<double> res(n);
        for (size_t i = 0; i < n; ++i) {
            res[i] = yn[i] - y_prev[i] - dt * dydt[i];
        }
        return res;
    };

    auto n_res = nonlinear_solver_->solve(G, std::nullopt, y_next, config);
    if (!n_res.is_ok() || !n_res.value().converged) {
        return make_solver_error("ImplicitEuler Newton iteration failed to converge", 2731);
    }

    std::copy(y_next.begin(), y_next.end(), y.begin());
    ODEStepResult step_res;
    step_res.step_accepted = true;
    step_res.next_dt = dt;
    step_res.estimated_error = n_res.value().final_residual_norm;
    return step_res;
}

// =============================================================================
// SOL-06: Discretization Mesh
// =============================================================================

DiscretizationMesh DiscretizationMesh::generate_1d_uniform(double x0, double x1, size_t num_cells) {
    DiscretizationMesh mesh;
    if (num_cells == 0 || x1 <= x0) return mesh;

    const size_t num_nodes = num_cells + 1;
    const double dx = (x1 - x0) / static_cast<double>(num_cells);

    mesh.nodes_.reserve(num_nodes);
    for (size_t i = 0; i < num_nodes; ++i) {
        MeshNode node;
        node.id = i;
        node.x = x0 + static_cast<double>(i) * dx;
        node.is_boundary = (i == 0 || i == num_cells);
        mesh.nodes_.push_back(node);
    }

    mesh.elements_.reserve(num_cells);
    for (size_t i = 0; i < num_cells; ++i) {
        MeshElement el;
        el.id = i;
        el.type = ElementType::Line1D;
        el.node_ids = {i, i + 1};
        mesh.elements_.push_back(std::move(el));
    }

    return mesh;
}

DiscretizationMesh DiscretizationMesh::generate_2d_uniform_grid(
    double x0, double x1, double y0, double y1, size_t nx, size_t ny
) {
    DiscretizationMesh mesh;
    if (nx == 0 || ny == 0) return mesh;

    const double dx = (x1 - x0) / static_cast<double>(nx);
    const double dy = (y1 - y0) / static_cast<double>(ny);

    const size_t total_nodes = (nx + 1) * (ny + 1);
    mesh.nodes_.reserve(total_nodes);

    for (size_t j = 0; j <= ny; ++j) {
        for (size_t i = 0; i <= nx; ++i) {
            MeshNode node;
            node.id = j * (nx + 1) + i;
            node.x = x0 + static_cast<double>(i) * dx;
            node.y = y0 + static_cast<double>(j) * dy;
            node.is_boundary = (i == 0 || i == nx || j == 0 || j == ny);
            mesh.nodes_.push_back(node);
        }
    }

    mesh.elements_.reserve(nx * ny);
    for (size_t j = 0; j < ny; ++j) {
        for (size_t i = 0; i < nx; ++i) {
            MeshElement el;
            el.id = j * nx + i;
            el.type = ElementType::Quad2D;
            const size_t n0 = j * (nx + 1) + i;
            const size_t n1 = n0 + 1;
            const size_t n2 = (j + 1) * (nx + 1) + i + 1;
            const size_t n3 = (j + 1) * (nx + 1) + i;
            el.node_ids = {n0, n1, n2, n3};
            mesh.elements_.push_back(std::move(el));
        }
    }

    return mesh;
}

void DiscretizationMesh::add_boundary_condition(BoundaryCondition bc) {
    boundary_conditions_.push_back(bc);
}

std::pair<SparseMatrixCSR, std::vector<double>> DiscretizationMesh::assemble_1d_poisson(
    size_t n, double L, double left_val, double right_val, std::function<double(double)> source
) {
    // 1D Poisson: -u''(x) = f(x)
    // h = L / (n + 1)
    // Central difference: (-u_{i-1} + 2u_i - u_{i+1}) / h^2 = f_i
    const double h = L / static_cast<double>(n + 1);
    const double h2 = h * h;

    std::vector<Triplet> triplets;
    triplets.reserve(3 * n);
    std::vector<double> b(n, 0.0);

    for (size_t i = 0; i < n; ++i) {
        const double xi = (static_cast<double>(i) + 1.0) * h;
        const double f_val = source ? source(xi) : 0.0;
        b[i] = f_val * h2;

        triplets.push_back({i, i, 2.0});

        if (i > 0) {
            triplets.push_back({i, i - 1, -1.0});
        } else {
            b[i] += left_val; // Dirichlet boundary
        }

        if (i + 1 < n) {
            triplets.push_back({i, i + 1, -1.0});
        } else {
            b[i] += right_val; // Dirichlet boundary
        }
    }

    return {SparseMatrixCSR::from_triplets(n, n, triplets), b};
}

// =============================================================================
// SOL-07: Solver Capability Registry
// =============================================================================

SolverRegistry& SolverRegistry::instance() {
    static SolverRegistry reg;
    return reg;
}

SolverRegistry::SolverRegistry() {
    // Linear
    solvers_.push_back({
        .name = "ConjugateGradient",
        .version = base::Version(1, 0, 0, "final"),
        .supported_problems = {ProblemType::Linear},
        .supports_adaptive_stepping = false,
        .supports_sparse_matrices = true,
        .is_stiff_stable = false,
        .accuracy_order = 2
    });
    solvers_.push_back({
        .name = "BiCGSTAB",
        .version = base::Version(1, 0, 0, "final"),
        .supported_problems = {ProblemType::Linear},
        .supports_adaptive_stepping = false,
        .supports_sparse_matrices = true,
        .is_stiff_stable = false,
        .accuracy_order = 2
    });
    solvers_.push_back({
        .name = "DirectLU",
        .version = base::Version(1, 0, 0, "final"),
        .supported_problems = {ProblemType::Linear},
        .supports_adaptive_stepping = false,
        .supports_sparse_matrices = true,
        .is_stiff_stable = false,
        .accuracy_order = 2
    });

    // Nonlinear
    solvers_.push_back({
        .name = "NewtonRaphson",
        .version = base::Version(1, 0, 0, "final"),
        .supported_problems = {ProblemType::Nonlinear},
        .supports_adaptive_stepping = false,
        .supports_sparse_matrices = true,
        .is_stiff_stable = false,
        .accuracy_order = 2
    });

    // ODE
    solvers_.push_back({
        .name = "ExplicitEuler",
        .version = base::Version(1, 0, 0, "final"),
        .supported_problems = {ProblemType::ODE},
        .supports_adaptive_stepping = false,
        .supports_sparse_matrices = false,
        .is_stiff_stable = false,
        .accuracy_order = 1
    });
    solvers_.push_back({
        .name = "RungeKutta4",
        .version = base::Version(1, 0, 0, "final"),
        .supported_problems = {ProblemType::ODE},
        .supports_adaptive_stepping = false,
        .supports_sparse_matrices = false,
        .is_stiff_stable = false,
        .accuracy_order = 4
    });
    solvers_.push_back({
        .name = "AdaptiveRK45",
        .version = base::Version(1, 0, 0, "final"),
        .supported_problems = {ProblemType::ODE},
        .supports_adaptive_stepping = true,
        .supports_sparse_matrices = false,
        .is_stiff_stable = false,
        .accuracy_order = 5
    });
    solvers_.push_back({
        .name = "ImplicitEuler",
        .version = base::Version(1, 0, 0, "final"),
        .supported_problems = {ProblemType::ODE, ProblemType::DAE},
        .supports_adaptive_stepping = false,
        .supports_sparse_matrices = true,
        .is_stiff_stable = true,
        .accuracy_order = 1
    });
}

void SolverRegistry::register_solver(SolverDescriptor descriptor) {
    solvers_.push_back(std::move(descriptor));
}

std::vector<SolverDescriptor> SolverRegistry::get_available_solvers(ProblemType problem) const {
    std::vector<SolverDescriptor> out;
    for (const auto& s : solvers_) {
        for (auto p : s.supported_problems) {
            if (p == problem) {
                out.push_back(s);
                break;
            }
        }
    }
    return out;
}

std::optional<SolverDescriptor> SolverRegistry::find_best_solver(
    ProblemType problem, bool stiff_required, bool adaptive_required
) const {
    auto candidates = get_available_solvers(problem);
    std::optional<SolverDescriptor> best;

    for (const auto& c : candidates) {
        if (stiff_required && !c.is_stiff_stable) continue;
        if (adaptive_required && !c.supports_adaptive_stepping) continue;

        if (!best.has_value() || c.accuracy_order > best->accuracy_order) {
            best = c;
        }
    }
    return best;
}

base::Result<void> SolverRegistry::validate_compatibility(
    const SolverDescriptor& solver, ProblemType problem, bool stiff, size_t dim
) const {
    (void)dim;
    bool problem_matched = false;
    for (auto p : solver.supported_problems) {
        if (p == problem) {
            problem_matched = true;
            break;
        }
    }
    if (!problem_matched) {
        return make_solver_error("Solver '" + solver.name + "' does not support problem class " +
                                 std::string(problem_type_to_string(problem)), 2740);
    }
    if (stiff && !solver.is_stiff_stable) {
        return make_solver_error("Solver '" + solver.name + "' is not stable for stiff systems", 2741);
    }
    return {};
}

void SolverRegistry::clear() noexcept {
    solvers_.clear();
}

// =============================================================================
// SOL-08: Numerical Benchmark Runner
// =============================================================================

BenchmarkResult NumericalBenchmarkRunner::run_harmonic_oscillator_benchmark(
    double omega, double t_end, double dt, IODESolver& solver, double tol
) {
    BenchmarkResult res;
    res.benchmark_name = "HarmonicOscillator_" + solver.name();

    const auto start_tp = std::chrono::steady_clock::now();

    // System: y' = v, v' = -omega^2 * y
    // Exact: y(t) = cos(omega * t), v(t) = -omega * sin(omega * t)
    auto rhs = [omega](double, std::span<const double> state) -> std::vector<double> {
        return {state[1], -omega * omega * state[0]};
    };

    std::vector<double> state = {1.0, 0.0};
    double t = 0.0;
    SolverConfig cfg;
    cfg.relative_tolerance = 1e-5;
    cfg.absolute_tolerance = 1e-7;

    double max_err = 0.0;
    double l2_accum = 0.0;
    size_t steps = 0;

    while (t < t_end) {
        const double step_dt = (t + dt > t_end) ? (t_end - t) : dt;
        auto step_res = solver.step(rhs, t, step_dt, state, cfg);
        if (!step_res.is_ok()) {
            res.message = "Harmonic step failed: " + step_res.error().message;
            return res;
        }

        t += step_dt;
        ++steps;

        const double exact_y = std::cos(omega * t);
        const double err = std::abs(state[0] - exact_y);
        if (err > max_err) max_err = err;
        l2_accum += err * err;
    }

    const auto end_tp = std::chrono::steady_clock::now();
    res.runtime_us = std::chrono::duration<double, std::micro>(end_tp - start_tp).count();
    res.linf_error = max_err;
    res.l2_error = steps > 0 ? std::sqrt(l2_accum / static_cast<double>(steps)) : 0.0;
    res.tolerance_passed = (res.linf_error <= tol);
    return res;
}

BenchmarkResult NumericalBenchmarkRunner::run_stiff_decay_benchmark(
    double lambda, double t_end, double dt, IODESolver& solver, double tol
) {
    BenchmarkResult res;
    res.benchmark_name = "StiffDecay_" + solver.name();

    const auto start_tp = std::chrono::steady_clock::now();

    // dy/dt = -lambda * y, exact: y(t) = exp(-lambda * t)
    auto rhs = [lambda](double, std::span<const double> state) -> std::vector<double> {
        return {-lambda * state[0]};
    };

    std::vector<double> state = {1.0};
    double t = 0.0;
    SolverConfig cfg;
    cfg.absolute_tolerance = 1e-8;

    double max_err = 0.0;
    double l2_accum = 0.0;
    size_t steps = 0;

    while (t < t_end) {
        const double step_dt = (t + dt > t_end) ? (t_end - t) : dt;
        auto step_res = solver.step(rhs, t, step_dt, state, cfg);
        if (!step_res.is_ok()) {
            res.message = "Stiff step failed: " + step_res.error().message;
            return res;
        }

        t += step_dt;
        ++steps;

        const double exact = std::exp(-lambda * t);
        const double err = std::abs(state[0] - exact);
        if (err > max_err) max_err = err;
        l2_accum += err * err;
    }

    const auto end_tp = std::chrono::steady_clock::now();
    res.runtime_us = std::chrono::duration<double, std::micro>(end_tp - start_tp).count();
    res.linf_error = max_err;
    res.l2_error = steps > 0 ? std::sqrt(l2_accum / static_cast<double>(steps)) : 0.0;
    res.tolerance_passed = (res.linf_error <= tol);
    return res;
}

BenchmarkResult NumericalBenchmarkRunner::run_nonlinear_root_benchmark(
    INonlinearSolver& solver, double tol
) {
    BenchmarkResult res;
    res.benchmark_name = "NonlinearRoot_" + solver.name();

    const auto start_tp = std::chrono::steady_clock::now();

    // F(x) = x^3 - x - 2 = 0, exact root is x ~ 1.52137970680457
    auto F = [](std::span<const double> x) -> std::vector<double> {
        return {x[0] * x[0] * x[0] - x[0] - 2.0};
    };

    std::vector<double> x = {2.0}; // initial guess
    SolverConfig cfg;
    cfg.absolute_tolerance = tol;

    auto solve_res = solver.solve(F, std::nullopt, x, cfg);

    const auto end_tp = std::chrono::steady_clock::now();
    res.runtime_us = std::chrono::duration<double, std::micro>(end_tp - start_tp).count();

    if (solve_res.is_ok() && solve_res.value().converged) {
        const double exact = 1.521379706804568;
        res.linf_error = std::abs(x[0] - exact);
        res.l2_error = res.linf_error;
        res.tolerance_passed = (res.linf_error <= tol);
    } else {
        res.tolerance_passed = false;
        res.message = "Nonlinear solver did not converge";
    }

    return res;
}

BenchmarkResult NumericalBenchmarkRunner::run_poisson_1d_benchmark(
    size_t n, ILinearSolver& solver, double tol
) {
    BenchmarkResult res;
    res.benchmark_name = "Poisson1D_" + solver.name();

    const auto start_tp = std::chrono::steady_clock::now();

    // -u'' = pi^2 * sin(pi * x), exact: u(x) = sin(pi * x), u(0) = 0, u(1) = 0
    auto source = [](double x) {
        return 3.14159265358979323846 * 3.14159265358979323846 * std::sin(3.14159265358979323846 * x);
    };

    auto [A, b] = DiscretizationMesh::assemble_1d_poisson(n, 1.0, 0.0, 0.0, source);

    std::vector<double> x(n, 0.0);
    SolverConfig cfg;
    cfg.relative_tolerance = 1e-7;
    cfg.absolute_tolerance = 1e-9;
    cfg.max_iterations = 2000;

    auto solve_res = solver.solve(A, b, x, cfg);

    const auto end_tp = std::chrono::steady_clock::now();
    res.runtime_us = std::chrono::duration<double, std::micro>(end_tp - start_tp).count();

    if (solve_res.is_ok() && solve_res.value().converged) {
        const double h = 1.0 / static_cast<double>(n + 1);
        double max_err = 0.0;
        double l2_sum = 0.0;
        for (size_t i = 0; i < n; ++i) {
            const double xi = (static_cast<double>(i) + 1.0) * h;
            const double exact = std::sin(3.14159265358979323846 * xi);
            const double err = std::abs(x[i] - exact);
            if (err > max_err) max_err = err;
            l2_sum += err * err;
        }
        res.linf_error = max_err;
        res.l2_error = std::sqrt(l2_sum / static_cast<double>(n));
        res.tolerance_passed = (res.linf_error <= tol);
    } else {
        res.tolerance_passed = false;
        res.message = "Poisson solve did not converge";
    }

    return res;
}

std::vector<BenchmarkResult> NumericalBenchmarkRunner::run_all_reference_benchmarks() {
    std::vector<BenchmarkResult> results;

    // 1. Harmonic oscillator with RK4
    RungeKutta4Solver rk4;
    results.push_back(run_harmonic_oscillator_benchmark(1.0, 6.28, 0.01, rk4, 1e-4));

    // 2. Stiff decay with ImplicitEuler
    ImplicitEulerSolver implicit_euler;
    results.push_back(run_stiff_decay_benchmark(100.0, 0.1, 0.001, implicit_euler, 0.03));

    // 3. Nonlinear root with NewtonRaphson
    NewtonRaphsonSolver newton;
    results.push_back(run_nonlinear_root_benchmark(newton, 1e-6));

    // 4. 1D Poisson with BiCGSTAB
    BiCGSTABSolver bicgstab;
    results.push_back(run_poisson_1d_benchmark(50, bicgstab, 1e-3));

    return results;
}

} // namespace shunya::core::domain
