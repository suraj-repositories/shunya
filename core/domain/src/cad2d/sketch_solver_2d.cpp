#include "shunya/cad2d/sketch_solver_2d.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <numeric>

namespace shunya::cad2d {

namespace {

struct LinearizedSystem {
    size_t m{0};
    size_t n_free{0};
    std::vector<size_t> free_to_param;
    std::vector<int> param_to_free;
    std::vector<size_t> row_to_constraint_id;
    std::vector<double> r;
    std::vector<double> J; // m x n_free row-major
};

void build_free_map(const ConstraintSystem2D& sys, LinearizedSystem& out) {
    size_t n_total = sys.param_count();
    out.param_to_free.assign(n_total, -1);
    out.free_to_param.clear();
    const auto& fixed = sys.fixed_flags();
    for (size_t i = 0; i < n_total; ++i) {
        if (!fixed[i]) {
            out.param_to_free[i] = static_cast<int>(out.free_to_param.size());
            out.free_to_param.push_back(i);
        }
    }
    out.n_free = out.free_to_param.size();
}

void assemble_residuals(const ConstraintSystem2D& sys, const std::vector<double>& params, LinearizedSystem& out) {
    size_t m = sys.total_residual_rows();
    out.m = m;
    out.r.assign(m, 0.0);
    out.row_to_constraint_id.resize(m);

    size_t row = 0;
    for (const auto& c : sys.constraints()) {
        if (!c || !c->enabled) continue;
        size_t rc = c->residual_count();
        c->evaluate(params, &out.r[row]);
        for (size_t k = 0; k < rc; ++k) {
            out.row_to_constraint_id[row + k] = c->id;
        }
        row += rc;
    }
}

void assemble_jacobian(const ConstraintSystem2D& sys, const std::vector<double>& params, LinearizedSystem& out) {
    out.J.assign(out.m * out.n_free, 0.0);
    if (out.m == 0 || out.n_free == 0) return;

    std::vector<JacobianTriplet> triplets;
    triplets.reserve(out.m * 6);

    size_t row = 0;
    for (const auto& c : sys.constraints()) {
        if (!c || !c->enabled) continue;
        c->accumulate_jacobian(params, row, triplets);
        row += c->residual_count();
    }

    for (const auto& tr : triplets) {
        if (tr.row < out.m && tr.col < out.param_to_free.size()) {
            int fc = out.param_to_free[tr.col];
            if (fc >= 0) {
                out.J[tr.row * out.n_free + static_cast<size_t>(fc)] += tr.value;
            }
        }
    }
}

double vec_norm2(const std::vector<double>& v) noexcept {
    double s = 0.0;
    for (double x : v) s += x * x;
    return std::sqrt(s);
}

bool solve_symmetric_linear(std::vector<double>& A, std::vector<double>& b, size_t n) {
    // Gaussian elimination with partial pivoting (numerically stable for small-medium damped normal equations)
    for (size_t k = 0; k < n; ++k) {
        size_t piv = k;
        double max_val = std::abs(A[k * n + k]);
        for (size_t i = k + 1; i < n; ++i) {
            double v = std::abs(A[i * n + k]);
            if (v > max_val) {
                max_val = v;
                piv = i;
            }
        }
        if (max_val < 1e-18) return false;

        if (piv != k) {
            for (size_t j = k; j < n; ++j) std::swap(A[k * n + j], A[piv * n + j]);
            std::swap(b[k], b[piv]);
        }

        double inv_diag = 1.0 / A[k * n + k];
        for (size_t i = k + 1; i < n; ++i) {
            double factor = A[i * n + k] * inv_diag;
            if (factor == 0.0) continue;
            A[i * n + k] = 0.0;
            for (size_t j = k + 1; j < n; ++j) {
                A[i * n + j] -= factor * A[k * n + j];
            }
            b[i] -= factor * b[k];
        }
    }

    for (size_t idx = n; idx > 0; --idx) {
        size_t i = idx - 1;
        double sum = b[i];
        for (size_t j = i + 1; j < n; ++j) {
            sum -= A[i * n + j] * b[j];
        }
        b[i] = sum / A[i * n + i];
    }
    return true;
}

} // namespace

// =============================================================================
// DofAnalyzer Implementation (Householder QR with Column Pivoting)
// =============================================================================

size_t DofAnalyzer::compute_rank(
    std::vector<double> A,
    size_t rows,
    size_t cols,
    double rel_tol
) {
    if (rows == 0 || cols == 0) return 0;

    size_t k_max = std::min(rows, cols);
    double max_norm = 0.0;
    size_t rank = 0;

    for (size_t k = 0; k < k_max; ++k) {
        // Compute remaining column norms squared in A[k:rows, k:cols]
        size_t piv = k;
        double best_n2 = -1.0;
        for (size_t j = k; j < cols; ++j) {
            double n2 = 0.0;
            for (size_t i = k; i < rows; ++i) {
                double v = A[i * cols + j];
                n2 += v * v;
            }
            if (n2 > best_n2) {
                best_n2 = n2;
                piv = j;
            }
        }

        double col_norm = std::sqrt(std::max(0.0, best_n2));
        if (k == 0) max_norm = col_norm;
        if (col_norm <= rel_tol * std::max(1.0, max_norm)) {
            break;
        }

        // Swap columns k and piv
        if (piv != k) {
            for (size_t i = 0; i < rows; ++i) {
                std::swap(A[i * cols + k], A[i * cols + piv]);
            }
        }

        // Householder vector v for A[k:rows, k]
        double x0 = A[k * cols + k];
        double alpha = (x0 >= 0.0) ? -col_norm : col_norm;
        double v0 = x0 - alpha;

        std::vector<double> v(rows - k, 0.0);
        v[0] = v0;
        double v_n2 = v0 * v0;
        for (size_t i = k + 1; i < rows; ++i) {
            v[i - k] = A[i * cols + k];
            v_n2 += v[i - k] * v[i - k];
        }

        if (v_n2 > 1e-30) {
            double beta = 2.0 / v_n2;
            A[k * cols + k] = alpha;
            for (size_t i = k + 1; i < rows; ++i) A[i * cols + k] = 0.0;

            for (size_t j = k + 1; j < cols; ++j) {
                double dot = 0.0;
                for (size_t i = k; i < rows; ++i) {
                    dot += v[i - k] * A[i * cols + j];
                }
                double scale = beta * dot;
                for (size_t i = k; i < rows; ++i) {
                    A[i * cols + j] -= scale * v[i - k];
                }
            }
        }

        ++rank;
    }

    return rank;
}

DofAnalysisResult DofAnalyzer::analyze(const ConstraintSystem2D& system, double conflict_residual_tol) {
    DofAnalysisResult res;
    LinearizedSystem lin;
    build_free_map(system, lin);
    assemble_residuals(system, system.params(), lin);
    assemble_jacobian(system, system.params(), lin);

    res.free_parameters = lin.n_free;
    res.constraint_rows = lin.m;
    res.jacobian_rank = compute_rank(lin.J, lin.m, lin.n_free);
    res.remaining_dof = (lin.n_free >= res.jacobian_rank) ? (lin.n_free - res.jacobian_rank) : 0;
    res.redundant_rows = (lin.m >= res.jacobian_rank) ? (lin.m - res.jacobian_rank) : 0;

    // Collect conflicting constraint IDs when residuals remain above tolerance
    for (size_t i = 0; i < lin.m; ++i) {
        if (std::abs(lin.r[i]) > conflict_residual_tol) {
            size_t cid = lin.row_to_constraint_id[i];
            if (std::find(res.conflicting_constraint_ids.begin(), res.conflicting_constraint_ids.end(), cid) ==
                res.conflicting_constraint_ids.end()) {
                res.conflicting_constraint_ids.push_back(cid);
            }
        }
    }

    if (!res.conflicting_constraint_ids.empty()) {
        res.status = DofStatus::OverConstrainedConflict;
    } else if (res.redundant_rows > 0) {
        res.status = DofStatus::RedundantConsistent;
    } else if (res.remaining_dof == 0) {
        res.status = DofStatus::FullyConstrained;
    } else {
        res.status = DofStatus::UnderConstrained;
    }

    return res;
}

// =============================================================================
// SketchSolver2D Implementation (Trust-Region Levenberg-Marquardt)
// =============================================================================

SketchSolveResult SketchSolver2D::solve(
    ConstraintSystem2D& system,
    const SketchSolverConfig& config
) {
    auto t0 = std::chrono::high_resolution_clock::now();
    SketchSolveResult result;

    LinearizedSystem lin;
    build_free_map(system, lin);
    assemble_residuals(system, system.params(), lin);

    if (lin.m == 0 || lin.n_free == 0) {
        result.residual_norm = vec_norm2(lin.r);
        result.converged = (result.residual_norm <= config.residual_tolerance);
        result.dof = DofAnalyzer::analyze(system);
        result.solve_time_ms = std::chrono::duration<double, std::milli>(
            std::chrono::high_resolution_clock::now() - t0
        ).count();
        return result;
    }

    assemble_jacobian(system, system.params(), lin);
    double r_norm = vec_norm2(lin.r);
    double lambda = config.initial_lambda;
    size_t n = lin.n_free;
    size_t m = lin.m;

    std::vector<double> H(n * n, 0.0);
    std::vector<double> g(n, 0.0);
    std::vector<double> trial_params = system.params();

    for (size_t iter = 0; iter < config.max_iterations; ++iter) {
        if (r_norm <= config.residual_tolerance) {
            result.converged = true;
            break;
        }
        result.iterations = iter + 1;

        // Form normal equations H = J^T J and g = -J^T r
        std::fill(H.begin(), H.end(), 0.0);
        std::fill(g.begin(), g.end(), 0.0);

        for (size_t r_idx = 0; r_idx < m; ++r_idx) {
            const double* jrow = &lin.J[r_idx * n];
            double r_val = lin.r[r_idx];
            for (size_t i = 0; i < n; ++i) {
                double jri = jrow[i];
                if (jri == 0.0) continue;
                g[i] -= jri * r_val;
                for (size_t j = i; j < n; ++j) {
                    H[i * n + j] += jri * jrow[j];
                }
            }
        }
        for (size_t i = 0; i < n; ++i) {
            for (size_t j = 0; j < i; ++j) {
                H[i * n + j] = H[j * n + i];
            }
        }

        bool accepted = false;
        double step_norm = 0.0;

        for (int lm_try = 0; lm_try < 12; ++lm_try) {
            std::vector<double> H_damped = H;
            std::vector<double> dx = g;

            for (size_t i = 0; i < n; ++i) {
                double diag_scale = std::max(1.0, H[i * n + i]);
                H_damped[i * n + i] += lambda * diag_scale + 1e-13;
            }

            if (!solve_symmetric_linear(H_damped, dx, n)) {
                lambda *= 10.0;
                continue;
            }

            step_norm = vec_norm2(dx);
            trial_params = system.params();
            for (size_t i = 0; i < n; ++i) {
                trial_params[lin.free_to_param[i]] += dx[i];
            }

            LinearizedSystem trial_lin;
            trial_lin.m = m;
            assemble_residuals(system, trial_params, trial_lin);
            double trial_norm = vec_norm2(trial_lin.r);

            if (trial_norm < r_norm || trial_norm <= config.residual_tolerance) {
                system.params() = std::move(trial_params);
                lin.r = std::move(trial_lin.r);
                r_norm = trial_norm;
                lambda = std::max(1e-12, lambda * 0.25);
                accepted = true;
                break;
            } else {
                lambda = std::min(1e8, lambda * 4.0);
            }
        }

        if (!accepted || step_norm <= config.step_tolerance) {
            if (r_norm <= config.residual_tolerance) result.converged = true;
            break;
        }

        assemble_jacobian(system, system.params(), lin);
    }

    if (r_norm <= config.residual_tolerance) {
        result.converged = true;
    }

    result.residual_norm = r_norm;
    result.dof = DofAnalyzer::analyze(system);
    result.solve_time_ms = std::chrono::duration<double, std::milli>(
        std::chrono::high_resolution_clock::now() - t0
    ).count();

    return result;
}

} // namespace shunya::cad2d
