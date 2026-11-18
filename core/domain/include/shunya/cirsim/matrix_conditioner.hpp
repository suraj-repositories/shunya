#pragma once

#include "shunya/core/domain/circuit.hpp"
#include "shunya/cirsim/mna_system_builder.hpp"

#include <string>
#include <vector>

namespace shunya::cirsim {

struct ConditioningReport {
    bool is_singular{false};
    double condition_number_estimate{1.0};
    double min_row_norm{0.0};
    double max_row_norm{0.0};
    std::vector<double> row_scale_factors;
    std::vector<std::string> diagnostic_messages;
    core::domain::DenseMatrix scaled_matrix;
    std::vector<double> scaled_rhs;
};

/**
 * @brief Automated matrix conditioning, row scaling & singularity detection engine.
 * Fulfills roadmap task PH4-08.01.005.
 */
class MatrixConditioner {
public:
    /**
     * @brief Performs row equilibration scaling on MNA system A * x = z.
     * Scales each row so that max_j |A_ij| = 1.0.
     */
    [[nodiscard]] static ConditioningReport condition_system(
        const core::domain::DenseMatrix& A,
        const std::vector<double>& z,
        const std::vector<std::string>& row_labels = {}
    );

    /**
     * @brief Checks MNA Block System for structural topological singularities
     * (e.g. short-circuited voltage loops, floating sub-circuits, zero conductance rows).
     */
    [[nodiscard]] static ConditioningReport analyze_mna_system(
        const MnaBlockMatrix& mna_sys
    );
};

} // namespace shunya::cirsim
