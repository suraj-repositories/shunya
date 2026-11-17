#pragma once

#include "shunya/core/base/math.hpp"
#include "shunya/eng/physical_object.hpp"
#include "shunya/mat/epistemic_value.hpp"
#include "shunya/mat/property_models/material_property_bundle.hpp"

namespace shunya::mat {

struct ConstraintBoundaryCondition {
    double constraint_x{0.0}; // 0.0 = unconstrained free expansion, 1.0 = fully rigid wall constraint
    double constraint_y{0.0};
    double constraint_z{0.0};

    [[nodiscard]] constexpr bool is_unconstrained() const noexcept {
        return constraint_x == 0.0 && constraint_y == 0.0 && constraint_z == 0.0;
    }

    [[nodiscard]] constexpr bool is_fully_clamped() const noexcept {
        return constraint_x >= 0.99 && constraint_y >= 0.99 && constraint_z >= 0.99;
    }
};

struct ThermalDeformationState {
    double operating_temperature_k{293.15};
    double delta_t_k{0.0};
    core::base::math::Vector3<double> thermal_strain{0.0, 0.0, 0.0};
    core::base::math::Vector3<double> deformed_dimensions_m{0.0, 0.0, 0.0};
    double initial_volume_m3{0.0};
    double deformed_volume_m3{0.0};
    core::base::math::Vector3<double> thermal_stress_pa{0.0, 0.0, 0.0};
    double von_mises_thermal_stress_pa{0.0};
    bool exceeds_yield_strength{false};
    double thermal_safety_factor{10.0};
};

/**
 * @brief Engine connecting temperature changes and thermal expansion coefficients
 *        directly to 3D geometry dimensions and induced thermal stress states.
 * (Roadmap task PH4-03.04.004)
 */
class ThermalDeformationEngine {
public:
    static constexpr double REFERENCE_TEMPERATURE_K = 293.15;

    /**
     * @brief Computes 3D dimensional expansion, strains, and thermal stresses.
     */
    [[nodiscard]] static ThermalDeformationState compute_thermal_deformation(
        const eng::PhysicalObject& object,
        const MaterialPropertyBundle& material,
        double target_temperature_k,
        const ConstraintBoundaryCondition& boundary = {}) noexcept;

    /**
     * @brief Creates a mutated physical object whose 3D geometry reflects thermal expansion.
     */
    [[nodiscard]] static eng::PhysicalObject update_geometry_with_thermal_expansion(
        const eng::PhysicalObject& object,
        const MaterialPropertyBundle& material,
        double target_temperature_k) noexcept;

    /**
     * @brief Returns induced thermal stress as an EpistemicScalar.
     */
    [[nodiscard]] static EpistemicScalar evaluate_thermal_stress(
        const eng::PhysicalObject& object,
        const MaterialPropertyBundle& material,
        double target_temperature_k,
        const ConstraintBoundaryCondition& boundary) noexcept;
};

} // namespace shunya::mat
