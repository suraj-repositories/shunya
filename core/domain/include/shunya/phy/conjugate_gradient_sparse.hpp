#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/core/domain/sparse_matrix.hpp"

#include <cmath>
#include <vector>

namespace shunya::phy {

struct CgSolveStats {
    size_t iterations{0};
    double residual_norm{0.0};
    bool converged{false};
};

/**
 * @brief Jacobi-preconditioned Conjugate Gradient solver for symmetric positive-definite CSR systems.
 */
class SparseConjugateGradient {
public:
    [[nodiscard]] static core::base::Result<CgSolveStats> solve(
        const core::domain::SparseMatrixCSR& A,
        const std::vector<double>& b,
        std::vector<double>& x,
        double tolerance = 1e-10,
        size_t max_iterations = 2000) {
        using namespace core::domain::vector_ops;

        const size_t n = A.rows();
        if (n == 0 || A.cols() != n || b.size() != n) {
            return core::base::Result<CgSolveStats>::err(
                core::base::Error{60, core::base::ErrorDomain::Validation, core::base::Severity::Error,
                                  "Sparse CG requires square non-empty matrix matching RHS size", ""});
        }
        if (x.size() != n) x.assign(n, 0.0);

        std::vector<double> inv_diag(n, 1.0);
        for (size_t i = 0; i < n; ++i) {
            const double d = A.coeff(i, i);
            inv_diag[i] = (std::abs(d) > 1e-18) ? (1.0 / d) : 1.0;
        }

        std::vector<double> Ax(n, 0.0);
        A.multiply(x, Ax);

        std::vector<double> r(n, 0.0);
        std::vector<double> z(n, 0.0);
        std::vector<double> p(n, 0.0);
        std::vector<double> Ap(n, 0.0);

        for (size_t i = 0; i < n; ++i) {
            r[i] = b[i] - Ax[i];
            z[i] = inv_diag[i] * r[i];
            p[i] = z[i];
        }

        double rz_old = dot(r, z);
        const double b_norm = std::max(1.0, norm_2(b));
        double r_norm = norm_2(r);

        CgSolveStats stats;
        stats.residual_norm = r_norm;
        if (r_norm <= tolerance * b_norm) {
            stats.converged = true;
            return core::base::Result<CgSolveStats>::ok(stats);
        }

        for (size_t iter = 1; iter <= max_iterations; ++iter) {
            A.multiply(p, Ap);
            const double pAp = dot(p, Ap);
            if (pAp <= 1e-24) {
                stats.iterations = iter;
                stats.residual_norm = r_norm;
                stats.converged = (r_norm <= tolerance * b_norm);
                return core::base::Result<CgSolveStats>::ok(stats);
            }

            const double alpha = rz_old / pAp;
            axpy(alpha, p, x);
            axpy(-alpha, Ap, r);

            r_norm = norm_2(r);
            stats.iterations = iter;
            stats.residual_norm = r_norm;
            if (r_norm <= tolerance * b_norm) {
                stats.converged = true;
                return core::base::Result<CgSolveStats>::ok(stats);
            }

            for (size_t i = 0; i < n; ++i) {
                z[i] = inv_diag[i] * r[i];
            }
            const double rz_new = dot(r, z);
            const double beta = rz_new / std::max(1e-30, rz_old);
            for (size_t i = 0; i < n; ++i) {
                p[i] = z[i] + beta * p[i];
            }
            rz_old = rz_new;
        }

        stats.converged = (r_norm <= tolerance * b_norm * 10.0);
        return core::base::Result<CgSolveStats>::ok(stats);
    }
};

} // namespace shunya::phy
