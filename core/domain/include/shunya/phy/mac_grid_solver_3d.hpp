#pragma once

#include "shunya/core/base/math.hpp"
#include "shunya/core/base/result.hpp"
#include "shunya/phy/pressure_poisson.hpp"
#include "shunya/phy/semi_lagrangian.hpp"

#include <cstddef>
#include <vector>

namespace shunya::phy {

struct MacGridConfig {
    size_t nx{32};
    size_t ny{32};
    size_t nz{1}; // nz == 1 runs 2D planar slice cleanly with w = 0
    double dx_m{1.0 / 32.0};
    double density_kg_m3{1.0};
    double kinematic_viscosity_m2_s{0.01};
    double lid_top_u_m_s{0.0}; // Optional moving top lid velocity (for lid-driven cavity benchmark)
};

/**
 * @brief Staggered Marker-and-Cell (MAC) Eulerian Navier-Stokes fluid solver with BFECC & MIC(0) PCG projection.
 */
class MacGridFluidSolver3D {
public:
    explicit MacGridFluidSolver3D(MacGridConfig cfg = {});

    void set_cell_type(size_t i, size_t j, size_t k, CellFluidKind kind);
    void set_dye(size_t i, size_t j, size_t k, double val);
    [[nodiscard]] double dye(size_t i, size_t j, size_t k) const noexcept;

    [[nodiscard]] double u(size_t i, size_t j, size_t k) const noexcept;
    [[nodiscard]] double v(size_t i, size_t j, size_t k) const noexcept;
    [[nodiscard]] double w(size_t i, size_t j, size_t k) const noexcept;
    [[nodiscard]] double pressure(size_t i, size_t j, size_t k) const noexcept;

    void set_u(size_t i, size_t j, size_t k, double val);
    void set_v(size_t i, size_t j, size_t k, double val);
    void set_w(size_t i, size_t j, size_t k, double val);

    [[nodiscard]] core::base::math::Vector3<double> sample_velocity(double x, double y, double z) const noexcept;
    [[nodiscard]] core::base::Result<PoissonSolveStats> step(double dt_s);
    [[nodiscard]] double max_divergence() const noexcept;

    /**
     * @brief Computes (x, y) streamfunction-peak primary vortex center in normalized domain [0, Lx] x [0, Ly].
     */
    [[nodiscard]] core::base::math::Vector2<double> find_primary_vortex_center_2d() const noexcept;

    [[nodiscard]] const MacGridConfig& config() const noexcept { return cfg_; }
    [[nodiscard]] double total_dye() const noexcept;

private:
    void apply_wall_bcs();
    void apply_viscosity(double dt_s);

    MacGridConfig cfg_;
    std::vector<CellFluidKind> cell_types_;
    std::vector<double> u_, v_, w_;
    std::vector<double> p_;
    std::vector<double> dye_;
};

} // namespace shunya::phy
