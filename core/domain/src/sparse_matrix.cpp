#include "shunya/core/domain/sparse_matrix.hpp"

#include <algorithm>
#include <map>
#include <stdexcept>

namespace shunya::core::domain {

namespace vector_ops {

double dot(std::span<const double> a, std::span<const double> b) noexcept {
    const size_t n = std::min(a.size(), b.size());
    double sum = 0.0;
    for (size_t i = 0; i < n; ++i) {
        sum += a[i] * b[i];
    }
    return sum;
}

double norm_2(std::span<const double> v) noexcept {
    return std::sqrt(dot(v, v));
}

double norm_inf(std::span<const double> v) noexcept {
    double max_val = 0.0;
    for (double x : v) {
        const double ax = std::abs(x);
        if (ax > max_val) max_val = ax;
    }
    return max_val;
}

void axpy(double alpha, std::span<const double> x, std::span<double> y) noexcept {
    const size_t n = std::min(x.size(), y.size());
    for (size_t i = 0; i < n; ++i) {
        y[i] += alpha * x[i];
    }
}

void scale(double alpha, std::span<double> v) noexcept {
    for (double& x : v) {
        x *= alpha;
    }
}

} // namespace vector_ops

SparseMatrixCSR::SparseMatrixCSR(size_t rows, size_t cols)
    : rows_(rows), cols_(cols), row_ptr_(rows + 1, 0) {}

SparseMatrixCSR SparseMatrixCSR::from_triplets(size_t rows, size_t cols, const std::vector<Triplet>& triplets) {
    SparseMatrixCSR mat(rows, cols);
    if (rows == 0 || cols == 0 || triplets.empty()) {
        return mat;
    }

    // Accumulate duplicate entries at same (row, col)
    std::map<std::pair<size_t, size_t>, double> accumulated;
    for (const auto& t : triplets) {
        if (t.row < rows && t.col < cols && std::abs(t.value) > 1e-15) {
            accumulated[{t.row, t.col}] += t.value;
        }
    }

    // Count entries per row
    std::vector<size_t> row_counts(rows, 0);
    for (const auto& [pos, val] : accumulated) {
        if (std::abs(val) > 1e-15) {
            ++row_counts[pos.first];
        }
    }

    mat.row_ptr_.resize(rows + 1, 0);
    for (size_t r = 0; r < rows; ++r) {
        mat.row_ptr_[r + 1] = mat.row_ptr_[r] + row_counts[r];
    }

    const size_t total_nz = mat.row_ptr_[rows];
    mat.col_indices_.reserve(total_nz);
    mat.values_.reserve(total_nz);

    for (const auto& [pos, val] : accumulated) {
        if (std::abs(val) > 1e-15) {
            mat.col_indices_.push_back(pos.second);
            mat.values_.push_back(val);
        }
    }

    return mat;
}

double SparseMatrixCSR::sparsity_ratio() const noexcept {
    if (rows_ == 0 || cols_ == 0) return 0.0;
    const double total_entries = static_cast<double>(rows_) * static_cast<double>(cols_);
    return 1.0 - (static_cast<double>(values_.size()) / total_entries);
}

double SparseMatrixCSR::coeff(size_t row, size_t col) const noexcept {
    if (row >= rows_ || col >= cols_) return 0.0;
    const size_t start = row_ptr_[row];
    const size_t end = row_ptr_[row + 1];

    for (size_t idx = start; idx < end; ++idx) {
        if (col_indices_[idx] == col) {
            return values_[idx];
        }
        if (col_indices_[idx] > col) {
            break;
        }
    }
    return 0.0;
}

void SparseMatrixCSR::set_coeff(size_t row, size_t col, double val) {
    if (row >= rows_ || col >= cols_) return;
    const size_t start = row_ptr_[row];
    const size_t end = row_ptr_[row + 1];

    for (size_t idx = start; idx < end; ++idx) {
        if (col_indices_[idx] == col) {
            values_[idx] = val;
            return;
        }
        if (col_indices_[idx] > col) {
            col_indices_.insert(col_indices_.begin() + idx, col);
            values_.insert(values_.begin() + idx, val);
            for (size_t r = row + 1; r <= rows_; ++r) {
                ++row_ptr_[r];
            }
            return;
        }
    }

    // Insert at end of this row
    col_indices_.insert(col_indices_.begin() + end, col);
    values_.insert(values_.begin() + end, val);
    for (size_t r = row + 1; r <= rows_; ++r) {
        ++row_ptr_[r];
    }
}

void SparseMatrixCSR::multiply(std::span<const double> x, std::span<double> y) const noexcept {
    const size_t n_rows = std::min(rows_, y.size());
    for (size_t r = 0; r < n_rows; ++r) {
        double sum = 0.0;
        const size_t start = row_ptr_[r];
        const size_t end = row_ptr_[r + 1];
        for (size_t idx = start; idx < end; ++idx) {
            const size_t c = col_indices_[idx];
            if (c < x.size()) {
                sum += values_[idx] * x[c];
            }
        }
        y[r] = sum;
    }
}

std::vector<double> SparseMatrixCSR::multiply(const std::vector<double>& x) const {
    std::vector<double> y(rows_, 0.0);
    multiply(x, y);
    return y;
}

SparseMatrixCSR SparseMatrixCSR::transpose() const {
    std::vector<Triplet> triplets;
    triplets.reserve(values_.size());
    for (size_t r = 0; r < rows_; ++r) {
        const size_t start = row_ptr_[r];
        const size_t end = row_ptr_[r + 1];
        for (size_t idx = start; idx < end; ++idx) {
            triplets.push_back({col_indices_[idx], r, values_[idx]});
        }
    }
    return from_triplets(cols_, rows_, triplets);
}

double SparseMatrixCSR::frobenius_norm() const noexcept {
    double sum = 0.0;
    for (double v : values_) {
        sum += v * v;
    }
    return std::sqrt(sum);
}

double SparseMatrixCSR::trace() const noexcept {
    const size_t n = std::min(rows_, cols_);
    double sum = 0.0;
    for (size_t i = 0; i < n; ++i) {
        sum += coeff(i, i);
    }
    return sum;
}

base::Result<SparseMatrixCSR> SparseMatrixCSR::compute_ilu0() const {
    if (rows_ != cols_) {
        return base::Error{
            .code = 2701,
            .domain = base::ErrorDomain::Simulation,
            .severity = base::Severity::Error,
            .message = "ILU(0) requires a square matrix",
            .details = ""
        };
    }

    SparseMatrixCSR lu = *this;
    const size_t n = rows_;

    // Lookup index helper
    auto find_idx = [&](size_t r, size_t c) -> std::optional<size_t> {
        const size_t start = lu.row_ptr_[r];
        const size_t end = lu.row_ptr_[r + 1];
        for (size_t idx = start; idx < end; ++idx) {
            if (lu.col_indices_[idx] == c) return idx;
            if (lu.col_indices_[idx] > c) break;
        }
        return std::nullopt;
    };

    for (size_t i = 1; i < n; ++i) {
        const size_t i_start = lu.row_ptr_[i];
        const size_t i_end = lu.row_ptr_[i + 1];

        for (size_t k_idx = i_start; k_idx < i_end; ++k_idx) {
            const size_t k = lu.col_indices_[k_idx];
            if (k >= i) break; // lower triangular only

            auto diag_k_opt = find_idx(k, k);
            if (!diag_k_opt.has_value() || std::abs(lu.values_[*diag_k_opt]) < 1e-15) {
                return base::Error{
                    .code = 2702,
                    .domain = base::ErrorDomain::Simulation,
                    .severity = base::Severity::Error,
                    .message = "Zero pivot in ILU(0) factorization at row " + std::to_string(k),
                    .details = ""
                };
            }

            lu.values_[k_idx] /= lu.values_[*diag_k_opt];
            const double lik = lu.values_[k_idx];

            const size_t k_start = lu.row_ptr_[k];
            const size_t k_end = lu.row_ptr_[k + 1];

            for (size_t j_idx = k_start; j_idx < k_end; ++j_idx) {
                const size_t j = lu.col_indices_[j_idx];
                if (j <= k) continue;

                auto ij_idx_opt = find_idx(i, j);
                if (ij_idx_opt.has_value()) {
                    lu.values_[*ij_idx_opt] -= lik * lu.values_[j_idx];
                }
            }
        }
    }

    return lu;
}

void SparseMatrixCSR::solve_lu(std::span<const double> b, std::span<double> x) const noexcept {
    const size_t n = std::min(rows_, b.size());
    std::vector<double> y(n, 0.0);

    // Forward substitution: L * y = b (L has unit diagonal)
    for (size_t i = 0; i < n; ++i) {
        double sum = b[i];
        const size_t start = row_ptr_[i];
        const size_t end = row_ptr_[i + 1];
        for (size_t idx = start; idx < end; ++idx) {
            const size_t j = col_indices_[idx];
            if (j < i) {
                sum -= values_[idx] * y[j];
            } else {
                break;
            }
        }
        y[i] = sum;
    }

    // Backward substitution: U * x = y
    for (int i = static_cast<int>(n) - 1; i >= 0; --i) {
        double sum = y[i];
        double diag = 1.0;
        const size_t start = row_ptr_[i];
        const size_t end = row_ptr_[i + 1];

        for (size_t idx = start; idx < end; ++idx) {
            const size_t j = col_indices_[idx];
            if (j == static_cast<size_t>(i)) {
                diag = values_[idx];
            } else if (j > static_cast<size_t>(i) && j < n) {
                sum -= values_[idx] * x[j];
            }
        }

        if (std::abs(diag) > 1e-15) {
            x[i] = sum / diag;
        } else {
            x[i] = sum;
        }
    }
}

void SparseMatrixCSR::clear() noexcept {
    rows_ = 0;
    cols_ = 0;
    row_ptr_ = {0};
    col_indices_.clear();
    values_.clear();
}

} // namespace shunya::core::domain
