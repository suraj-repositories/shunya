#include "shunya/perf/gpu_spmv.hpp"

#include <algorithm>
#include <cmath>
#include <thread>

namespace shunya::perf {

core::base::Result<void> GpuSpMvKernel::multiply(
    const core::domain::SparseMatrixCSR& matrix,
    std::span<const double> x,
    std::span<double> y) {
    if (x.size() != matrix.cols() || y.size() != matrix.rows()) {
        return core::base::Error{
            511, core::base::ErrorDomain::Validation, core::base::Severity::Error,
            "SpMV vector dimension mismatch with CSR matrix", ""
        };
    }

    const size_t n_rows = matrix.rows();
    const auto& rp = matrix.row_ptr();
    const auto& ci = matrix.col_indices();
    const auto& vals = matrix.values();

    const size_t n_threads = (n_rows < 2048) ? 1 : std::min<size_t>(std::max<size_t>(1, std::thread::hardware_concurrency()), 8);
    auto worker = [&](size_t r0, size_t r1) {
        for (size_t r = r0; r < r1; ++r) {
            double acc = 0.0;
            const size_t start = rp[r];
            const size_t end = rp[r + 1];
            for (size_t nz = start; nz < end; ++nz) {
                acc += vals[nz] * x[ci[nz]];
            }
            y[r] = acc;
        }
    };

    if (n_threads <= 1) {
        worker(0, n_rows);
    } else {
        std::vector<std::thread> threads;
        threads.reserve(n_threads - 1);
        const size_t chunk = (n_rows + n_threads - 1) / n_threads;
        for (size_t t = 1; t < n_threads; ++t) {
            const size_t b = t * chunk;
            const size_t e = std::min(n_rows, b + chunk);
            if (b < e) threads.emplace_back(worker, b, e);
        }
        worker(0, std::min(n_rows, chunk));
        for (auto& th : threads) th.join();
    }
    return core::base::Result<void>::ok();
}

double GpuSpMvKernel::parallel_dot(
    std::span<const double> a,
    std::span<const double> b) noexcept {
    const size_t n = std::min(a.size(), b.size());
    double sum0 = 0.0, sum1 = 0.0, sum2 = 0.0, sum3 = 0.0;
    size_t i = 0;
    for (; i + 4 <= n; i += 4) {
        sum0 += a[i + 0] * b[i + 0];
        sum1 += a[i + 1] * b[i + 1];
        sum2 += a[i + 2] * b[i + 2];
        sum3 += a[i + 3] * b[i + 3];
    }
    double tail = 0.0;
    for (; i < n; ++i) tail += a[i] * b[i];
    return (sum0 + sum1) + (sum2 + sum3) + tail;
}

double GpuSpMvKernel::parallel_norm2_sq(std::span<const double> v) noexcept {
    return parallel_dot(v, v);
}

core::base::Result<GpuCgSolveStats> GpuSpMvKernel::solve_jacobi_cg(
    const core::domain::SparseMatrixCSR& matrix,
    std::span<const double> rhs,
    std::span<double> x,
    double tolerance,
    size_t max_iterations) {
    const size_t n = matrix.rows();
    if (n != matrix.cols() || rhs.size() != n || x.size() != n || n == 0) {
        return core::base::Error{
            512, core::base::ErrorDomain::Validation, core::base::Severity::Error,
            "Invalid dimensions for CSR Conjugate Gradient solve", ""
        };
    }

    std::vector<double> inv_diag(n, 1.0);
    for (size_t i = 0; i < n; ++i) {
        const double d = matrix.coeff(i, i);
        inv_diag[i] = (std::abs(d) > 1e-18) ? (1.0 / d) : 1.0;
    }

    std::vector<double> r(n, 0.0), z(n, 0.0), p(n, 0.0), ap(n, 0.0);
    auto m_res = multiply(matrix, x, ap);
    if (!m_res.is_ok()) return m_res.error();

    double max_res = 0.0;
    for (size_t i = 0; i < n; ++i) {
        r[i] = rhs[i] - ap[i];
        z[i] = inv_diag[i] * r[i];
        p[i] = z[i];
        max_res = std::max(max_res, std::abs(r[i]));
    }

    GpuCgSolveStats stats;
    stats.residual_norm_inf = max_res;
    if (max_res <= tolerance) {
        stats.converged = true;
        return stats;
    }

    double rz = parallel_dot(r, z);
    for (size_t iter = 1; iter <= max_iterations; ++iter) {
        auto mult_st = multiply(matrix, p, ap);
        if (!mult_st.is_ok()) return mult_st.error();

        const double pap = parallel_dot(p, ap);
        if (std::abs(pap) <= 1e-30) break;
        const double alpha = rz / pap;

        max_res = 0.0;
        for (size_t i = 0; i < n; ++i) {
            x[i] += alpha * p[i];
            r[i] -= alpha * ap[i];
            max_res = std::max(max_res, std::abs(r[i]));
        }
        stats.iterations = iter;
        stats.residual_norm_inf = max_res;
        if (max_res <= tolerance) {
            stats.converged = true;
            return stats;
        }

        for (size_t i = 0; i < n; ++i) {
            z[i] = inv_diag[i] * r[i];
        }
        const double rz_new = parallel_dot(r, z);
        const double beta = rz_new / std::max(1e-30, rz);
        for (size_t i = 0; i < n; ++i) {
            p[i] = z[i] + beta * p[i];
        }
        rz = rz_new;
    }

    return stats;
}

std::string_view GpuSpMvKernel::spmv_shader_glsl() noexcept {
    return R"(#version 450
layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;
// See core/domain/shaders/spmv_csr.comp
)";
}

} // namespace shunya::perf
