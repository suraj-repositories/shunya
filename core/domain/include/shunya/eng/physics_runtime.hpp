#pragma once

#include "shunya/core/base/math.hpp"
#include "shunya/core/base/result.hpp"
#include "shunya/core/base/uuid.hpp"
#include "shunya/eng/physical_object.hpp"

#include <vector>

namespace shunya::eng {

struct SimulatedObjectBody {
    core::base::Uuid object_id;
    std::string name;
    double mass_kg{1.0};
    double volume_m3{1e-3};
    double specific_heat_j_kgk{500.0};
    double thermal_conductivity_w_mk{10.0};
    double thermal_expansion_1_k{1.2e-5};
    double young_modulus_pa{70e9};
    double surface_area_m2{0.06};
    double restitution{0.45};
    bool is_static{false};

    // Dynamic state
    core::base::math::Vector3<double> position{0.0, 0.0, 0.0};
    core::base::math::Vector3<double> velocity{0.0, 0.0, 0.0};
    core::base::math::Vector3<double> applied_force_n{0.0, 0.0, 0.0};
    double temperature_k{298.15};
    double initial_temperature_k{298.15};
    double heat_source_w{0.0};
    double thermal_strain{0.0};
    double axial_stress_pa{0.0};
    double axial_deformation_m{0.0};
    double half_height_m{0.05};
    double cross_section_m2{0.01};
    double char_length_m{0.1};
    bool collision_active{false};
};

struct PhysicsStepSnapshot {
    double time_s{0.0};
    double total_kinetic_energy_j{0.0};
    double total_potential_energy_j{0.0};
    double average_temperature_k{298.15};
    int active_collisions{0};
    std::vector<SimulatedObjectBody> bodies;
};

/**
 * @brief Material-coupled 2D/3D Physics & Environmental Simulation Runtime:
 *        gravity, rigid-body motion, AABB/sphere collision with restitution,
 *        thermal conduction/convection, thermal expansion, and axial elastic deformation.
 */
class PhysicsSimulationRuntime {
public:
    PhysicsSimulationRuntime() = default;

    void initialize_from_objects(
        const PhysicalObjectManager& obj_mgr,
        double ambient_temp_k = 298.15,
        double gravity_m_s2 = 9.80665,
        double convection_h_w_m2k = 10.0);

    void set_body_force(const core::base::Uuid& object_id, const core::base::math::Vector3<double>& force_n);
    void set_body_heat_source(const core::base::Uuid& object_id, double watts);
    void set_body_static(const core::base::Uuid& object_id, bool is_static);

    [[nodiscard]] core::base::Result<PhysicsStepSnapshot> step(double dt_s);
    void reset();

    [[nodiscard]] double time_s() const noexcept { return time_s_; }
    [[nodiscard]] const std::vector<SimulatedObjectBody>& bodies() const noexcept { return bodies_; }
    [[nodiscard]] const SimulatedObjectBody* find_body(const core::base::Uuid& object_id) const noexcept;

private:
    double time_s_{0.0};
    double ambient_temp_k_{298.15};
    double gravity_m_s2_{9.80665};
    double convection_h_{10.0};
    double ground_y_{0.0};
    std::vector<SimulatedObjectBody> bodies_;
    std::vector<SimulatedObjectBody> initial_bodies_;
};

} // namespace shunya::eng
