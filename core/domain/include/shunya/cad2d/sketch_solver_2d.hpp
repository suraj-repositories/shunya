#pragma once

#include "shunya/cad2d/constraint_graph.hpp"
#include "shunya/cad2d/dof_analyzer.hpp"

#include <cstddef>
#include <vector>

namespace shunya::cad2d {

struct SketchSolverConfig {
    size_t max_iterations{100};
    double residual_tolerance{1e-10};
    double step_tolerance{1e-12};
    double initial_lambda{1e-6};
};

struct SketchSolveResult {
    bool converged{false};
    size_t iterations{0};
    double residual_norm{0.0};
    double solve_time_ms{0.0};
    DofAnalysisResult dof;
};

/**
 * @brief Non-linear Levenberg-Marquardt & Newton-Raphson 2D geometric constraint solver with DOF tracking.
 */
class SketchSolver2D {
public:
    /**
     * @brief Solves F(p) = 0 in place on unfixed parameters and analyzes final DOF/conflict status.
     */
    [[nodiscard]] static SketchSolveResult solve(
        ConstraintSystem2D& system,
        const SketchSolverConfig& config = SketchSolverConfig{}
    );
};

} // namespace shunya::cad2d
