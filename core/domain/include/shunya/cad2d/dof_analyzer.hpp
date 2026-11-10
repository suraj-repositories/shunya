#pragma once

#include "shunya/cad2d/constraint_graph.hpp"

#include <cstddef>
#include <vector>

namespace shunya::cad2d {

enum class DofStatus {
    UnderConstrained,
    FullyConstrained,
    RedundantConsistent,
    OverConstrainedConflict
};

struct DofAnalysisResult {
    DofStatus status{DofStatus::UnderConstrained};
    size_t free_parameters{0};
    size_t constraint_rows{0};
    size_t jacobian_rank{0};
    size_t remaining_dof{0};
    size_t redundant_rows{0};
    std::vector<size_t> conflicting_constraint_ids;
};

/**
 * @brief Computes Jacobian matrix rank, remaining Degrees of Freedom (DOF), and conflicting constraints.
 */
class DofAnalyzer {
public:
    /**
     * @brief Computes numerical rank of an m x n row-major Jacobian via Householder QR with column pivoting.
     */
    [[nodiscard]] static size_t compute_rank(
        std::vector<double> matrix_row_major,
        size_t rows,
        size_t cols,
        double rel_tol = 1e-7
    );

    /**
     * @brief Performs full DOF & conflict analysis on a ConstraintSystem2D at its current state.
     */
    [[nodiscard]] static DofAnalysisResult analyze(
        const ConstraintSystem2D& system,
        double conflict_residual_tol = 1e-6
    );
};

} // namespace shunya::cad2d
