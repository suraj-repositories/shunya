#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/core/domain/sparse_matrix.hpp"
#include "shunya/core/domain/thermal.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace shunya::phy {

struct ThermalGridSpec3D {
    size_t nx{21};
    size_t ny{3};
    size_t nz{3};
    double lx_m{1.0};
    double ly_m{0.1};
    double lz_m{0.1};

    double density_kg_m3{2700.0};
    double specific_heat_j_kg_k{900.0};
    double conductivity_w_m_k{200.0};
    double initial_temp_k{300.0};
};

struct ThermalNodeOverride {
    size_t ix{0}, iy{0}, iz{0};
    bool is_dirichlet{false};
    double fixed_temp_k{300.0};
    double volumetric_heat_w_m3{0.0};
};

/**
 * @brief 3D transient heat conduction finite-difference Crank-Nicolson implicit solver with Sparse CG.
 */
class ThermalSolver3D {
public:
    explicit ThermalSolver3D(ThermalGridSpec3D spec);

    [[nodiscard]] size_t node_count() const noexcept { return spec_.nx * spec_.ny * spec_.nz; }
    [[nodiscard]] size_t index(size_t ix, size_t iy, size_t iz) const noexcept {
        return (iz * spec_.ny + iy) * spec_.nx + ix;
    }

    void set_dirichlet_node(size_t ix, size_t iy, size_t iz, double fixed_temp_k);
    void set_dirichlet_x_min_plane(double fixed_temp_k);
    void set_dirichlet_x_max_plane(double fixed_temp_k);
    void set_volumetric_source(size_t ix, size_t iy, size_t iz, double q_w_m3);

    void set_temperature(size_t ix, size_t iy, size_t iz, double temp_k);
    [[nodiscard]] double temperature(size_t ix, size_t iy, size_t iz) const noexcept;
    [[nodiscard]] const std::vector<double>& temperature_field() const noexcept { return temp_; }

    /**
     * @brief Advances 3D transient heat diffusion equation by dt_s using Crank-Nicolson implicit integration.
     */
    [[nodiscard]] core::base::Result<void> step_crank_nicolson(double dt_s);

    [[nodiscard]] core::base::Result<void> advance(double total_time_s, double dt_s);

private:
    ThermalGridSpec3D spec_;
    std::vector<double> temp_;
    std::vector<double> q_gen_;
    std::vector<bool> is_dirichlet_;
    std::vector<double> dirichlet_temp_;
};

} // namespace shunya::phy
