#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/phy/pcg_solver.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace shunya::phy {

enum class CellFluidKind : uint8_t {
    Fluid = 0,
    Solid = 1,
    OpenAir = 2 // Dirichlet p = 0
};

/**
 * @brief Incompressible Poisson pressure projection solver for staggered MAC grids enforcing div(u) = 0.
 */
class PressurePoissonSolver {
public:
    [[nodiscard]] static core::base::Result<PoissonSolveStats> project(
        size_t nx, size_t ny, size_t nz,
        double dx_m,
        double density_kg_m3,
        double dt_s,
        const std::vector<CellFluidKind>& cell_types,
        std::vector<double>& u,
        std::vector<double>& v,
        std::vector<double>& w,
        std::vector<double>& p,
        double tolerance = 1e-10);

    [[nodiscard]] static double compute_max_divergence(
        size_t nx, size_t ny, size_t nz,
        double dx_m,
        const std::vector<CellFluidKind>& cell_types,
        const std::vector<double>& u,
        const std::vector<double>& v,
        const std::vector<double>& w) noexcept;
};

} // namespace shunya::phy
