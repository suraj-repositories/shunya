#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/core/base/error.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <span>
#include <string>
#include <string_view>
#include <optional>

namespace shunya::core::domain {

struct Triplet {
    size_t row{0};
    size_t col{0};
    double value{0.0};
};

// Dense vector helpers
namespace vector_ops {
    [[nodiscard]] double dot(std::span<const double> a, std::span<const double> b) noexcept;
    [[nodiscard]] double norm_2(std::span<const double> v) noexcept;
    [[nodiscard]] double norm_inf(std::span<const double> v) noexcept;
    void axpy(double alpha, std::span<const double> x, std::span<double> y) noexcept; // y = alpha * x + y
    void scale(double alpha, std::span<double> v) noexcept;
}

/**
 * @brief High-performance Compressed Sparse Row (CSR) matrix representation.
 *
 * Implements SOL-02: Sparse matrix storage, assembly from triplets,
 * matrix-vector product, ILU(0) factorization, and matrix norms.
 */
class SparseMatrixCSR {
public:
    SparseMatrixCSR() = default;
    SparseMatrixCSR(size_t rows, size_t cols);

    static SparseMatrixCSR from_triplets(size_t rows, size_t cols, const std::vector<Triplet>& triplets);

    [[nodiscard]] size_t rows() const noexcept { return rows_; }
    [[nodiscard]] size_t cols() const noexcept { return cols_; }
    [[nodiscard]] size_t non_zeros() const noexcept { return values_.size(); }
    [[nodiscard]] double sparsity_ratio() const noexcept;

    [[nodiscard]] const std::vector<size_t>& row_ptr() const noexcept { return row_ptr_; }
    [[nodiscard]] const std::vector<size_t>& col_indices() const noexcept { return col_indices_; }
    [[nodiscard]] const std::vector<double>& values() const noexcept { return values_; }
    [[nodiscard]] std::vector<double>& values() noexcept { return values_; }

    [[nodiscard]] double coeff(size_t row, size_t col) const noexcept;
    void set_coeff(size_t row, size_t col, double val);

    // Matrix-vector product: y = A * x
    void multiply(std::span<const double> x, std::span<double> y) const noexcept;
    [[nodiscard]] std::vector<double> multiply(const std::vector<double>& x) const;

    [[nodiscard]] SparseMatrixCSR transpose() const;
    [[nodiscard]] double frobenius_norm() const noexcept;
    [[nodiscard]] double trace() const noexcept;

    // Incomplete LU (ILU0) factorization sharing the exact same sparsity pattern
    [[nodiscard]] base::Result<SparseMatrixCSR> compute_ilu0() const;

    // Triangular solves using this matrix as factored LU (L has unit diagonal)
    void solve_lu(std::span<const double> b, std::span<double> x) const noexcept;

    void clear() noexcept;

private:
    size_t rows_{0};
    size_t cols_{0};
    std::vector<size_t> row_ptr_{0};
    std::vector<size_t> col_indices_{};
    std::vector<double> values_{};
};

} // namespace shunya::core::domain
