#pragma once

#include "shunya/core/domain/circuit.hpp"

#include <cstddef>
#include <vector>

namespace shunya::cirsim {

struct MatrixEntryCOO {
    size_t row{0};
    size_t col{0};
    double value{0.0};
};

class SparseMatrixCSC;

/**
 * @brief Compressed Sparse Row (CSR) matrix representation.
 * Fulfills roadmap task PH4-08.01.004.
 */
class SparseMatrixCSR {
public:
    SparseMatrixCSR(size_t rows = 0, size_t cols = 0);

    [[nodiscard]] size_t rows() const noexcept { return rows_; }
    [[nodiscard]] size_t cols() const noexcept { return cols_; }
    [[nodiscard]] size_t non_zeros() const noexcept { return values_.size(); }

    [[nodiscard]] const std::vector<size_t>& row_ptr() const noexcept { return row_ptr_; }
    [[nodiscard]] const std::vector<size_t>& col_indices() const noexcept { return col_indices_; }
    [[nodiscard]] const std::vector<double>& values() const noexcept { return values_; }

    [[nodiscard]] double get(size_t r, size_t c) const noexcept;

    /**
     * @brief High performance Sparse Matrix-Vector multiplication (SpMV): y = A * x.
     */
    [[nodiscard]] std::vector<double> spmv(const std::vector<double>& x) const;

    [[nodiscard]] double density() const noexcept;
    [[nodiscard]] size_t memory_bytes() const noexcept;
    [[nodiscard]] size_t dense_equivalent_bytes() const noexcept;
    [[nodiscard]] double memory_savings_percent() const noexcept;

    [[nodiscard]] static SparseMatrixCSR from_coo(
        size_t rows, size_t cols,
        const std::vector<MatrixEntryCOO>& entries
    );

    [[nodiscard]] static SparseMatrixCSR from_dense(
        const core::domain::DenseMatrix& dense,
        double zero_tolerance = 1.0e-15
    );

    [[nodiscard]] SparseMatrixCSC to_csc() const;

private:
    size_t rows_{0};
    size_t cols_{0};
    std::vector<size_t> row_ptr_;
    std::vector<size_t> col_indices_;
    std::vector<double> values_;
};

/**
 * @brief Compressed Sparse Column (CSC) matrix representation.
 */
class SparseMatrixCSC {
public:
    SparseMatrixCSC(size_t rows = 0, size_t cols = 0);

    [[nodiscard]] size_t rows() const noexcept { return rows_; }
    [[nodiscard]] size_t cols() const noexcept { return cols_; }
    [[nodiscard]] size_t non_zeros() const noexcept { return values_.size(); }

    [[nodiscard]] const std::vector<size_t>& col_ptr() const noexcept { return col_ptr_; }
    [[nodiscard]] const std::vector<size_t>& row_indices() const noexcept { return row_indices_; }
    [[nodiscard]] const std::vector<double>& values() const noexcept { return values_; }

    [[nodiscard]] double get(size_t r, size_t c) const noexcept;

    [[nodiscard]] std::vector<double> spmv(const std::vector<double>& x) const;

    [[nodiscard]] static SparseMatrixCSC from_csr(const SparseMatrixCSR& csr);

    [[nodiscard]] SparseMatrixCSR to_csr() const;

private:
    size_t rows_{0};
    size_t cols_{0};
    std::vector<size_t> col_ptr_;
    std::vector<size_t> row_indices_;
    std::vector<double> values_;
};

} // namespace shunya::cirsim
