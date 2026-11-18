#include "shunya/cirsim/matrix_conditioner.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace shunya::cirsim {

ConditioningReport MatrixConditioner::condition_system(
    const core::domain::DenseMatrix& A,
    const std::vector<double>& z,
    const std::vector<std::string>& row_labels
) {
    ConditioningReport rep;
    const size_t n_rows = A.rows();
    const size_t n_cols = A.cols();

    rep.scaled_matrix.resize(n_rows, n_cols);
    rep.scaled_rhs.resize(n_rows, 0.0);
    rep.row_scale_factors.assign(n_rows, 1.0);

    if (n_rows == 0 || n_cols == 0) {
        rep.is_singular = true;
        rep.diagnostic_messages.push_back("Matrix dimensions are zero.");
        return rep;
    }

    double max_norm = 0.0;
    double min_norm = std::numeric_limits<double>::max();

    // 1. Calculate row infinity norms
    for (size_t r = 0; r < n_rows; ++r) {
        double r_norm = 0.0;
        for (size_t c = 0; c < n_cols; ++c) {
            r_norm = std::max(r_norm, std::abs(A(r, c)));
        }

        if (r_norm < 1.0e-14) {
            rep.is_singular = true;
            std::string lbl = (r < row_labels.size()) ? row_labels[r] : ("Row " + std::to_string(r));
            rep.diagnostic_messages.push_back("Singular zero row detected at " + lbl + " (unconnected node or redundant equation).");
            rep.row_scale_factors[r] = 1.0;
        } else {
            rep.row_scale_factors[r] = 1.0 / r_norm;
            max_norm = std::max(max_norm, r_norm);
            min_norm = std::min(min_norm, r_norm);
        }
    }

    // 2. Check for zero columns (variables with zero impact)
    for (size_t c = 0; c < n_cols; ++c) {
        double c_norm = 0.0;
        for (size_t r = 0; r < n_rows; ++r) {
            c_norm = std::max(c_norm, std::abs(A(r, c)));
        }
        if (c_norm < 1.0e-14) {
            rep.is_singular = true;
            rep.diagnostic_messages.push_back("Singular zero column detected at Column " + std::to_string(c) + ".");
        }
    }

    // 3. Condition number indicator estimate
    rep.max_row_norm = max_norm;
    rep.min_row_norm = (min_norm == std::numeric_limits<double>::max()) ? 0.0 : min_norm;
    if (rep.min_row_norm > 1.0e-14) {
        rep.condition_number_estimate = rep.max_row_norm / rep.min_row_norm;
    } else {
        rep.condition_number_estimate = std::numeric_limits<double>::infinity();
        rep.is_singular = true;
    }

    // 4. Apply row equilibration scaling
    for (size_t r = 0; r < n_rows; ++r) {
        const double s = rep.row_scale_factors[r];
        for (size_t c = 0; c < n_cols; ++c) {
            rep.scaled_matrix(r, c) = A(r, c) * s;
        }
        if (r < z.size()) {
            rep.scaled_rhs[r] = z[r] * s;
        }
    }

    return rep;
}

ConditioningReport MatrixConditioner::analyze_mna_system(
    const MnaBlockMatrix& mna_sys
) {
    std::vector<std::string> all_labels = mna_sys.node_labels;
    for (const auto& b : mna_sys.branch_labels) {
        all_labels.push_back("BRANCH:" + b);
    }

    auto rep = condition_system(mna_sys.A, mna_sys.z, all_labels);

    // Topological singularity checks:
    // Check parallel voltage sources or short circuit loop
    const size_t m = mna_sys.num_branches;
    for (size_t i = 0; i < m; ++i) {
        for (size_t j = i + 1; j < m; ++j) {
            bool exact_match = true;
            for (size_t c = 0; c < mna_sys.num_nodes; ++c) {
                if (std::abs(mna_sys.C(i, c) - mna_sys.C(j, c)) > 1.0e-6 &&
                    std::abs(mna_sys.C(i, c) + mna_sys.C(j, c)) > 1.0e-6) {
                    exact_match = false;
                    break;
                }
            }
            if (exact_match && mna_sys.num_nodes > 0) {
                rep.is_singular = true;
                rep.diagnostic_messages.push_back(
                    "Singular parallel voltage loop between branches '" +
                    mna_sys.branch_labels[i] + "' and '" + mna_sys.branch_labels[j] + "'."
                );
            }
        }
    }

    return rep;
}

} // namespace shunya::cirsim
