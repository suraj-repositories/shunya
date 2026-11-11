#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/core/domain/sparse_matrix.hpp"

#include <span>
#include <string_view>
#include <vector>

namespace shunya::perf {

struct GpuCgSolveStats {
    size_t iterations{0};
    double residual_norm_inf{0.0};
    bool converged{false};
    double effective_bandwidth_gb_s{0.0};
};

/**
 * @brief Parallel warp/workgroup CSR Sparse Matrix-Vector Multiply (SpMV), reduction, and Conjugate Gradient kernel.
 */
class GpuSpMvKernel {
public:
    static constexpr size_t kWorkgroupSize = 256;

    [[nodiscard]] static core::base::Result<void> multiply(
        const core::domain::SparseMatrixCSR& matrix,
        std::span<const double> x,
        std::span<double> y);

    [[nodiscard]] static double parallel_dot(
        std::span<const double> a,
        std::span<const double> b) noexcept;

    [[nodiscard]] static double parallel_norm2_sq(std::span<const double> v) noexcept;

    [[nodiscard]] static core::base::Result<GpuCgSolveStats> solve_jacobi_cg(
        const core::domain::SparseMatrixCSR& matrix,
        std::span<const double> rhs,
        std::span<double> x,
        double tolerance = 1e-10,
        size_t max_iterations = 2000);

    [[nodiscard]] static std::string_view spmv_shader_glsl() noexcept;
};

} // namespace shunya::perf
