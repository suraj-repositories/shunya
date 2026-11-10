#pragma once

#include "shunya/core/base/math.hpp"
#include "shunya/core/base/result.hpp"
#include "shunya/phy/boundary_particles.hpp"
#include "shunya/phy/sph_kernels.hpp"
#include "shunya/phy/sph_surface_tension.hpp"
#include "shunya/phy/sph_viscosity.hpp"

#include <cstddef>
#include <vector>

namespace shunya::phy {

struct SphParticle {
    core::base::math::Vector3<double> position{0.0, 0.0, 0.0};
    core::base::math::Vector3<double> velocity{0.0, 0.0, 0.0};
    double mass_kg{1e-3};
    double density_kg_m3{1000.0};
    double pressure_pa{0.0};
    core::base::math::Vector3<double> accel{0.0, 0.0, 0.0};
};

struct SphSimulationConfig {
    double spacing_m{0.02};
    double smoothing_h_m{0.026};
    double rest_density_kg_m3{1000.0};
    double tait_gamma{7.0};
    double speed_of_sound_m_s{30.0};
    core::base::math::Vector3<double> gravity_m_s2{0.0, -9.80665, 0.0};
    core::base::math::Vector3<double> external_body_accel{0.0, 0.0, 0.0};

    SphViscosityConfig viscosity{};
    SphSurfaceTensionConfig surface_tension{};
    bool enable_surface_tension{false};

    // Optional periodic wrapping along axes (> 0 enables period [0, L))
    double periodic_x_m{0.0};
    double periodic_z_m{0.0};
};

/**
 * @brief Weakly compressible 3D SPH solver with Tait EOS, Akinci boundaries, viscosity, and surface tension.
 */
class SphSolver3D {
public:
    explicit SphSolver3D(SphSimulationConfig cfg = {});

    void add_fluid_box(
        const core::base::math::Vector3<double>& min_pt,
        const core::base::math::Vector3<double>& max_pt);

    void add_boundary_particles(std::vector<SphBoundaryParticle> bpts);

    [[nodiscard]] const std::vector<SphParticle>& particles() const noexcept { return particles_; }
    [[nodiscard]] std::vector<SphParticle>& particles() noexcept { return particles_; }
    [[nodiscard]] const std::vector<SphBoundaryParticle>& boundary_particles() const noexcept { return boundary_; }
    [[nodiscard]] const SphSimulationConfig& config() const noexcept { return cfg_; }

    /**
     * @brief Evaluates Tait equation of state pressure P = B * ((rho/rho0)^gamma - 1) >= 0.
     */
    [[nodiscard]] double tait_pressure(double rho) const noexcept;

    [[nodiscard]] core::base::Result<void> step(double dt_s);
    [[nodiscard]] core::base::Result<void> advance(double total_time_s, double dt_s);

    [[nodiscard]] double max_x_extent() const noexcept;
    [[nodiscard]] core::base::math::Vector3<double> bounding_box_extent() const noexcept;

private:
    SphSimulationConfig cfg_;
    std::vector<SphParticle> particles_;
    std::vector<SphBoundaryParticle> boundary_;
};

} // namespace shunya::phy
