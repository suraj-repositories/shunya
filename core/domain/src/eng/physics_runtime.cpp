#include "shunya/eng/physics_runtime.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::eng {

void PhysicsSimulationRuntime::initialize_from_objects(
    const PhysicalObjectManager& obj_mgr,
    double ambient_temp_k,
    double gravity_m_s2,
    double convection_h_w_m2k) {
    time_s_ = 0.0;
    ambient_temp_k_ = ambient_temp_k;
    gravity_m_s2_ = gravity_m_s2;
    convection_h_ = convection_h_w_m2k;
    bodies_.clear();

    for (const auto& obj : obj_mgr.objects()) {
        SimulatedObjectBody b;
        b.object_id = obj.id;
        b.name = obj.name;
        b.mass_kg = std::max(1e-6, obj.computed.mass_kg);
        b.volume_m3 = std::max(1e-9, obj.computed.volume_m3);
        b.specific_heat_j_kgk = std::max(50.0, obj.computed.effective_specific_heat_j_kgk);
        b.thermal_conductivity_w_mk = std::max(0.01, obj.computed.effective_thermal_conductivity_w_mk);
        b.thermal_expansion_1_k = obj.computed.effective_thermal_expansion_1_k;
        b.young_modulus_pa = obj.computed.effective_young_modulus_pa;
        b.surface_area_m2 = std::max(1e-6, obj.computed.surface_area_m2);
        b.position = obj.geometry.position;
        b.temperature_k = obj.operating_temp_k;
        b.initial_temperature_k = obj.operating_temp_k;
        b.cross_section_m2 = std::max(1e-9, obj.geometry.compute_cross_section_area_m2());
        b.char_length_m = std::max(1e-4, obj.geometry.compute_characteristic_length_m());
        if (obj.geometry.kind == ObjectShapeKind::Sphere) {
            b.half_height_m = obj.geometry.radius_m * obj.geometry.scale.y;
        } else {
            b.half_height_m = 0.5 * obj.geometry.dim_y_m * obj.geometry.scale.y;
        }
        bodies_.push_back(std::move(b));
    }
    initial_bodies_ = bodies_;
}

void PhysicsSimulationRuntime::set_body_force(
    const core::base::Uuid& object_id,
    const core::base::math::Vector3<double>& force_n) {
    for (auto& b : bodies_) {
        if (b.object_id == object_id) {
            b.applied_force_n = force_n;
            break;
        }
    }
}

void PhysicsSimulationRuntime::set_body_heat_source(const core::base::Uuid& object_id, double watts) {
    for (auto& b : bodies_) {
        if (b.object_id == object_id) {
            b.heat_source_w = watts;
            break;
        }
    }
}

void PhysicsSimulationRuntime::set_body_static(const core::base::Uuid& object_id, bool is_static) {
    for (auto& b : bodies_) {
        if (b.object_id == object_id) {
            b.is_static = is_static;
            break;
        }
    }
}

void PhysicsSimulationRuntime::reset() {
    time_s_ = 0.0;
    bodies_ = initial_bodies_;
}

const SimulatedObjectBody* PhysicsSimulationRuntime::find_body(const core::base::Uuid& object_id) const noexcept {
    for (const auto& b : bodies_) {
        if (b.object_id == object_id) return &b;
    }
    return nullptr;
}

core::base::Result<PhysicsStepSnapshot> PhysicsSimulationRuntime::step(double dt_s) {
    if (dt_s <= 0.0 || !std::isfinite(dt_s)) {
        return core::base::Error{
            .code = 4701,
            .domain = core::base::ErrorDomain::Simulation,
            .severity = core::base::Severity::Error,
            .message = "Physics step dt_s must be strictly positive",
            .details = ""
        };
    }

    int collisions = 0;

    // 1. Symplectic Euler rigid-body integration (gravity + applied force + ground collision)
    for (auto& b : bodies_) {
        b.collision_active = false;
        if (!b.is_static) {
            const double fy = b.applied_force_n.y - b.mass_kg * gravity_m_s2_;
            const double ax = b.applied_force_n.x / b.mass_kg;
            const double ay = fy / b.mass_kg;
            const double az = b.applied_force_n.z / b.mass_kg;

            b.velocity.x += ax * dt_s;
            b.velocity.y += ay * dt_s;
            b.velocity.z += az * dt_s;

            b.position.x += b.velocity.x * dt_s;
            b.position.y += b.velocity.y * dt_s;
            b.position.z += b.velocity.z * dt_s;

            const double min_y = ground_y_ + b.half_height_m;
            if (b.position.y < min_y) {
                b.position.y = min_y;
                if (b.velocity.y < 0.0) {
                    b.velocity.y = -b.restitution * b.velocity.y;
                    if (std::abs(b.velocity.y) < 0.02) b.velocity.y = 0.0;
                }
                b.velocity.x *= 0.98; // simple contact friction
                b.velocity.z *= 0.98;
                b.collision_active = true;
                ++collisions;
            }
        }
    }

    // 2. Pairwise body-body elastic collision + conductive contact heat transfer
    const size_t N = bodies_.size();
    std::vector<double> contact_heat_w(N, 0.0);
    for (size_t i = 0; i < N; ++i) {
        for (size_t j = i + 1; j < N; ++j) {
            auto& a = bodies_[i];
            auto& b = bodies_[j];
            const double dx = b.position.x - a.position.x;
            const double dy = b.position.y - a.position.y;
            const double dz = b.position.z - a.position.z;
            const double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
            const double min_sep = a.half_height_m + b.half_height_m;
            if (dist < min_sep && dist > 1e-9) {
                a.collision_active = true;
                b.collision_active = true;
                ++collisions;

                const double nx = dx / dist, ny = dy / dist, nz = dz / dist;
                const double overlap = min_sep - dist;
                const double inv_ma = a.is_static ? 0.0 : (1.0 / a.mass_kg);
                const double inv_mb = b.is_static ? 0.0 : (1.0 / b.mass_kg);
                const double inv_sum = inv_ma + inv_mb;
                if (inv_sum > 0.0) {
                    a.position.x -= nx * overlap * (inv_ma / inv_sum);
                    a.position.y -= ny * overlap * (inv_ma / inv_sum);
                    a.position.z -= nz * overlap * (inv_ma / inv_sum);
                    b.position.x += nx * overlap * (inv_mb / inv_sum);
                    b.position.y += ny * overlap * (inv_mb / inv_sum);
                    b.position.z += nz * overlap * (inv_mb / inv_sum);

                    const double rvx = b.velocity.x - a.velocity.x;
                    const double rvy = b.velocity.y - a.velocity.y;
                    const double rvz = b.velocity.z - a.velocity.z;
                    const double vel_along_n = rvx * nx + rvy * ny + rvz * nz;
                    if (vel_along_n < 0.0) {
                        const double e = std::min(a.restitution, b.restitution);
                        const double j_imp = -(1.0 + e) * vel_along_n / inv_sum;
                        a.velocity.x -= j_imp * inv_ma * nx;
                        a.velocity.y -= j_imp * inv_ma * ny;
                        a.velocity.z -= j_imp * inv_ma * nz;
                        b.velocity.x += j_imp * inv_mb * nx;
                        b.velocity.y += j_imp * inv_mb * ny;
                        b.velocity.z += j_imp * inv_mb * nz;
                    }
                }

                // Conductive heat transfer across touching bodies
                const double k_avg = 2.0 * (a.thermal_conductivity_w_mk * b.thermal_conductivity_w_mk) /
                                     std::max(1e-9, a.thermal_conductivity_w_mk + b.thermal_conductivity_w_mk);
                const double area_c = std::min(a.cross_section_m2, b.cross_section_m2);
                const double g_cond = k_avg * area_c / std::max(1e-3, min_sep);
                const double q_ab = g_cond * (a.temperature_k - b.temperature_k);
                contact_heat_w[i] -= q_ab;
                contact_heat_w[j] += q_ab;
            }
        }
    }

    // 3. Thermal energy balance, thermal expansion & elastic deformation
    double ke_sum = 0.0, pe_sum = 0.0, t_sum = 0.0;
    for (size_t i = 0; i < N; ++i) {
        auto& b = bodies_[i];
        const double c_th = std::max(1.0, b.mass_kg * b.specific_heat_j_kgk);
        const double q_conv = convection_h_ * b.surface_area_m2 * (b.temperature_k - ambient_temp_k_);
        const double q_net = b.heat_source_w + contact_heat_w[i] - q_conv;
        b.temperature_k = std::max(1.0, b.temperature_k + (q_net / c_th) * dt_s);

        // Thermal strain & mechanical stress/deformation
        b.thermal_strain = b.thermal_expansion_1_k * (b.temperature_k - b.initial_temperature_k);
        const double f_mag = std::sqrt(b.applied_force_n.x * b.applied_force_n.x +
                                       b.applied_force_n.y * b.applied_force_n.y +
                                       b.applied_force_n.z * b.applied_force_n.z);
        b.axial_stress_pa = f_mag / b.cross_section_m2;
        const double mech_strain = (b.young_modulus_pa > 1.0) ? (b.axial_stress_pa / b.young_modulus_pa) : 0.0;
        b.axial_deformation_m = (b.thermal_strain + mech_strain) * b.char_length_m;

        const double v2 = b.velocity.x * b.velocity.x + b.velocity.y * b.velocity.y + b.velocity.z * b.velocity.z;
        ke_sum += 0.5 * b.mass_kg * v2;
        pe_sum += b.mass_kg * gravity_m_s2_ * std::max(0.0, b.position.y - ground_y_);
        t_sum += b.temperature_k;
    }

    time_s_ += dt_s;
    PhysicsStepSnapshot snap;
    snap.time_s = time_s_;
    snap.total_kinetic_energy_j = ke_sum;
    snap.total_potential_energy_j = pe_sum;
    snap.average_temperature_k = N > 0 ? (t_sum / static_cast<double>(N)) : ambient_temp_k_;
    snap.active_collisions = collisions;
    snap.bodies = bodies_;
    return snap;
}

} // namespace shunya::eng
