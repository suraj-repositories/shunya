#pragma once

#include <cstddef>
#include <vector>

namespace shunya::phy {

struct PoissonSolveStats {
    size_t iterations{0};
    double max_residual{0.0};
    bool converged{false};
};

/**
 * @brief Bridson Modified Incomplete Cholesky (MIC(0)) Preconditioned Conjugate Gradient solver for 3D grids.
 */
class GridPcgSolver {
public:
    [[nodiscard]] static PoissonSolveStats solve_7pt_laplacian(
        size_t nx, size_t ny, size_t nz,
        const std::vector<double>& diag,
        const std::vector<double>& plus_x,
        const std::vector<double>& plus_y,
        const std::vector<double>& plus_z,
        const std::vector<double>& rhs,
        std::vector<double>& x,
        double tolerance = 1e-10,
        size_t max_iterations = 500,
        double mic_tau = 0.97);
};

} // namespace shunya::phy
