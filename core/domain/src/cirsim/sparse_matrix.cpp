#include "shunya/cirsim/sparse_matrix.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace shunya::cirsim {

SparseMatrixCSR::SparseMatrixCSR(size_t rows, size_t cols)
    : rows_(rows), cols_(cols), row_ptr_(rows + 1, 0) {}

double SparseMatrixCSR::get(size_t r, size_t c) const noexcept {
    if (r >= rows_ || c >= cols_) return 0.0;
    const size_t start = row_ptr_[r];
    const size_t end = row_ptr_[r + 1];
    for (size_t i = start; i < end; ++i) {
        if (col_indices_[i] == c) return values_[i];
        if (col_indices_[i] > c) break;
    }
    return 0.0;
}

std::vector<double> SparseMatrixCSR::spmv(const std::vector<double>& x) const {
    std::vector<double> y(rows_, 0.0);
    const size_t n = std::min(cols_, x.size());
    (void)n;
    for (size_t r = 0; r < rows_; ++r) {
        double sum = 0.0;
        const size_t start = row_ptr_[r];
        const size_t end = row_ptr_[r + 1];
        for (size_t i = start; i < end; ++i) {
            const size_t c = col_indices_[i];
            if (c < x.size()) {
                sum += values_[i] * x[c];
            }
        }
        y[r] = sum;
    }
    return y;
}

double SparseMatrixCSR::density() const noexcept {
    if (rows_ == 0 || cols_ == 0) return 0.0;
    return static_cast<double>(values_.size()) / static_cast<double>(rows_ * cols_);
}

size_t SparseMatrixCSR::memory_bytes() const noexcept {
    return (row_ptr_.size() * sizeof(size_t)) +
           (col_indices_.size() * sizeof(size_t)) +
           (values_.size() * sizeof(double)) +
           sizeof(*this);
}

size_t SparseMatrixCSR::dense_equivalent_bytes() const noexcept {
    return rows_ * cols_ * sizeof(double);
}

double SparseMatrixCSR::memory_savings_percent() const noexcept {
    const size_t dense = dense_equivalent_bytes();
    if (dense == 0) return 0.0;
    const size_t sparse = memory_bytes();
    if (sparse >= dense) return 0.0;
    return (1.0 - static_cast<double>(sparse) / static_cast<double>(dense)) * 100.0;
}

SparseMatrixCSR SparseMatrixCSR::from_coo(
    size_t rows, size_t cols,
    const std::vector<MatrixEntryCOO>& entries
) {
    SparseMatrixCSR mat(rows, cols);
    if (entries.empty()) return mat;

    // Filter and sort entries by (row, col)
    std::vector<MatrixEntryCOO> sorted;
    sorted.reserve(entries.size());
    for (const auto& e : entries) {
        if (e.row < rows && e.col < cols && std::abs(e.value) > 1e-15) {
            sorted.push_back(e);
        }
    }

    std::sort(sorted.begin(), sorted.end(), [](const MatrixEntryCOO& a, const MatrixEntryCOO& b) {
        if (a.row != b.row) return a.row < b.row;
        return a.col < b.col;
    });

    // Merge duplicates
    std::vector<MatrixEntryCOO> merged;
    merged.reserve(sorted.size());
    for (const auto& e : sorted) {
        if (!merged.empty() && merged.back().row == e.row && merged.back().col == e.col) {
            merged.back().value += e.value;
        } else {
            merged.push_back(e);
        }
    }

    // Build CSR
    mat.values_.reserve(merged.size());
    mat.col_indices_.reserve(merged.size());

    size_t current_row = 0;
    for (const auto& e : merged) {
        while (current_row < e.row) {
            mat.row_ptr_[++current_row] = mat.values_.size();
        }
        mat.col_indices_.push_back(e.col);
        mat.values_.push_back(e.value);
    }
    while (current_row < rows) {
        mat.row_ptr_[++current_row] = mat.values_.size();
    }

    return mat;
}

SparseMatrixCSR SparseMatrixCSR::from_dense(
    const core::domain::DenseMatrix& dense,
    double zero_tolerance
) {
    const size_t r_count = dense.rows();
    const size_t c_count = dense.cols();
    std::vector<MatrixEntryCOO> coo;

    for (size_t r = 0; r < r_count; ++r) {
        for (size_t c = 0; c < c_count; ++c) {
            const double v = dense(r, c);
            if (std::abs(v) > zero_tolerance) {
                coo.push_back(MatrixEntryCOO{r, c, v});
            }
        }
    }
    return from_coo(r_count, c_count, coo);
}

SparseMatrixCSC SparseMatrixCSR::to_csc() const {
    return SparseMatrixCSC::from_csr(*this);
}

// =============================================================================
// SparseMatrixCSC Implementation
// =============================================================================

SparseMatrixCSC::SparseMatrixCSC(size_t rows, size_t cols)
    : rows_(rows), cols_(cols), col_ptr_(cols + 1, 0) {}

double SparseMatrixCSC::get(size_t r, size_t c) const noexcept {
    if (r >= rows_ || c >= cols_) return 0.0;
    const size_t start = col_ptr_[c];
    const size_t end = col_ptr_[c + 1];
    for (size_t i = start; i < end; ++i) {
        if (row_indices_[i] == r) return values_[i];
        if (row_indices_[i] > r) break;
    }
    return 0.0;
}

std::vector<double> SparseMatrixCSC::spmv(const std::vector<double>& x) const {
    std::vector<double> y(rows_, 0.0);
    const size_t n = std::min(cols_, x.size());
    for (size_t c = 0; c < n; ++c) {
        const double xc = x[c];
        if (std::abs(xc) < 1e-18) continue;
        const size_t start = col_ptr_[c];
        const size_t end = col_ptr_[c + 1];
        for (size_t i = start; i < end; ++i) {
            y[row_indices_[i]] += values_[i] * xc;
        }
    }
    return y;
}

SparseMatrixCSC SparseMatrixCSC::from_csr(const SparseMatrixCSR& csr) {
    SparseMatrixCSC csc(csr.rows(), csr.cols());
    const size_t nnz = csr.non_zeros();
    if (nnz == 0) return csc;

    std::vector<size_t> col_counts(csr.cols(), 0);
    for (size_t idx : csr.col_indices()) {
        col_counts[idx]++;
    }

    csc.col_ptr_[0] = 0;
    for (size_t c = 0; c < csr.cols(); ++c) {
        csc.col_ptr_[c + 1] = csc.col_ptr_[c] + col_counts[c];
    }

    csc.row_indices_.resize(nnz);
    csc.values_.resize(nnz);

    std::vector<size_t> current_col_pos = csc.col_ptr_;
    for (size_t r = 0; r < csr.rows(); ++r) {
        const size_t start = csr.row_ptr()[r];
        const size_t end = csr.row_ptr()[r + 1];
        for (size_t i = start; i < end; ++i) {
            const size_t c = csr.col_indices()[i];
            const size_t dest = current_col_pos[c]++;
            csc.row_indices_[dest] = r;
            csc.values_[dest] = csr.values()[i];
        }
    }

    return csc;
}

SparseMatrixCSR SparseMatrixCSC::to_csr() const {
    std::vector<MatrixEntryCOO> coo;
    coo.reserve(values_.size());
    for (size_t c = 0; c < cols_; ++c) {
        const size_t start = col_ptr_[c];
        const size_t end = col_ptr_[c + 1];
        for (size_t i = start; i < end; ++i) {
            coo.push_back(MatrixEntryCOO{row_indices_[i], c, values_[i]});
        }
    }
    return SparseMatrixCSR::from_coo(rows_, cols_, coo);
}

} // namespace shunya::cirsim
