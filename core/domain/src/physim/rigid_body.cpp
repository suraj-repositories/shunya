#include "shunya/physim/rigid_body.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace shunya::physim {

void RigidBodyState::set_mass(double mass) noexcept {
    if (mass <= 0.0) {
        mass_kg = 0.0;
        inv_mass = 0.0;
    } else {
        mass_kg = mass;
        inv_mass = 1.0 / mass;
    }
}

void RigidBodyState::set_local_inertia(const Matrix3x3& I_body) noexcept {
    local_inertia = I_body;
    inv_local_inertia = I_body.inverse();
}

Vector3d RigidBodyState::linear_velocity() const noexcept {
    if (inv_mass <= 0.0) {
        return kinematic_linear_velocity;
    }
    return linear_momentum * inv_mass;
}

void RigidBodyState::set_linear_velocity(const Vector3d& v) noexcept {
    kinematic_linear_velocity = v;
    if (inv_mass <= 0.0) {
        linear_momentum = {0.0, 0.0, 0.0};
    } else {
        linear_momentum = v * mass_kg;
    }
}

Matrix3x3 RigidBodyState::world_inertia_tensor() const noexcept {
    const Matrix3x3 R = Matrix3x3::from_quaternion(orientation.normalized());
    return R * local_inertia * R.transpose();
}

Matrix3x3 RigidBodyState::inv_world_inertia_tensor() const noexcept {
    if (inv_mass <= 0.0) {
        return Matrix3x3::zero();
    }
    const Matrix3x3 R = Matrix3x3::from_quaternion(orientation.normalized());
    return R * inv_local_inertia * R.transpose();
}

Vector3d RigidBodyState::angular_velocity() const noexcept {
    if (inv_mass <= 0.0) {
        return kinematic_angular_velocity;
    }
    return inv_world_inertia_tensor() * angular_momentum;
}

void RigidBodyState::set_angular_velocity(const Vector3d& omega_world) noexcept {
    kinematic_angular_velocity = omega_world;
    if (inv_mass <= 0.0) {
        angular_momentum = {0.0, 0.0, 0.0};
    } else {
        angular_momentum = world_inertia_tensor() * omega_world;
    }
}

double RigidBodyState::kinetic_energy() const noexcept {
    return linear_kinetic_energy() + rotational_kinetic_energy();
}

double RigidBodyState::linear_kinetic_energy() const noexcept {
    if (inv_mass <= 0.0) return 0.0;
    return 0.5 * linear_velocity().dot(linear_momentum);
}

double RigidBodyState::rotational_kinetic_energy() const noexcept {
    if (inv_mass <= 0.0) return 0.0;
    return 0.5 * angular_velocity().dot(angular_momentum);
}

Vector3d RigidBodyState::local_to_world_point(const Vector3d& p_local) const noexcept {
    return position + orientation.normalized().rotate(p_local);
}

Vector3d RigidBodyState::world_to_local_point(const Vector3d& p_world) const noexcept {
    return orientation.normalized().conjugate().rotate(p_world - position);
}

Vector3d RigidBodyState::local_to_world_vector(const Vector3d& v_local) const noexcept {
    return orientation.normalized().rotate(v_local);
}

Vector3d RigidBodyState::world_to_local_vector(const Vector3d& v_world) const noexcept {
    return orientation.normalized().conjugate().rotate(v_world);
}

Vector3d RigidBodyState::point_velocity(const Vector3d& p_world) const noexcept {
    const Vector3d r = p_world - position;
    return linear_velocity() + angular_velocity().cross(r);
}

void RigidBodyState::apply_force_at_com(const Vector3d& force) noexcept {
    force_accumulator += force;
}

void RigidBodyState::apply_force_at_world_point(const Vector3d& force, const Vector3d& world_point) noexcept {
    force_accumulator += force;
    const Vector3d r = world_point - position;
    torque_accumulator += r.cross(force);
}

void RigidBodyState::apply_torque(const Vector3d& torque) noexcept {
    torque_accumulator += torque;
}

void RigidBodyState::apply_linear_impulse(const Vector3d& impulse) noexcept {
    if (inv_mass > 0.0) {
        linear_momentum += impulse;
    }
}

void RigidBodyState::apply_angular_impulse(const Vector3d& angular_impulse) noexcept {
    if (inv_mass > 0.0) {
        angular_momentum += angular_impulse;
    }
}

void RigidBodyState::apply_impulse_at_world_point(const Vector3d& impulse, const Vector3d& world_point) noexcept {
    if (inv_mass > 0.0) {
        linear_momentum += impulse;
        const Vector3d r = world_point - position;
        angular_momentum += r.cross(impulse);
    }
}

void RigidBodyState::clear_accumulators() noexcept {
    force_accumulator = {0.0, 0.0, 0.0};
    torque_accumulator = {0.0, 0.0, 0.0};
}

// =============================================================================
// MeshInertiaCalculator (PH4-09.01.002)
// =============================================================================

core::base::Result<MeshInertiaProperties> MeshInertiaCalculator::compute_from_mesh(
    const TriangleMesh& mesh,
    double density_kg_m3) {
    if (mesh.faces.empty() || mesh.vertices.empty() || density_kg_m3 <= 0.0) {
        return core::base::Result<MeshInertiaProperties>::err(
            core::base::Error{101, core::base::ErrorDomain::Validation, core::base::Severity::Error,
                              "Mesh inertia computation requires non-empty mesh and positive density", ""});
    }

    double vol6_sum = 0.0;
    Vector3d com_weighted{0.0, 0.0, 0.0};
    std::array<double, 6> p_moments{0.0, 0.0, 0.0, 0.0, 0.0, 0.0}; // xx, yy, zz, xy, xz, yz

    for (const auto& f : mesh.faces) {
        if (f.v0 >= mesh.vertices.size() || f.v1 >= mesh.vertices.size() || f.v2 >= mesh.vertices.size()) {
            return core::base::Result<MeshInertiaProperties>::err(
                core::base::Error{102, core::base::ErrorDomain::Validation, core::base::Severity::Error,
                                  "TriangleMesh face vertex index out of bounds", ""});
        }
        const auto& a = mesh.vertices[f.v0];
        const auto& b = mesh.vertices[f.v1];
        const auto& c = mesh.vertices[f.v2];

        // 6 * signed volume of tetrahedron formed with origin
        const double vol6 = a.dot(b.cross(c));
        vol6_sum += vol6;

        com_weighted += (a + b + c) * (vol6 / 24.0);

        auto second_moment = [&](double aj, double bj, double cj,
                                 double ak, double bk, double ck) -> double {
            const double sym = aj * ak + bj * bk + cj * ck;
            const double sum_jk = (aj + bj + cj) * (ak + bk + ck);
            return (vol6 / 120.0) * (sym + sum_jk);
        };

        p_moments[0] += second_moment(a.x, b.x, c.x, a.x, b.x, c.x); // xx
        p_moments[1] += second_moment(a.y, b.y, c.y, a.y, b.y, c.y); // yy
        p_moments[2] += second_moment(a.z, b.z, c.z, a.z, b.z, c.z); // zz
        p_moments[3] += second_moment(a.x, b.x, c.x, a.y, b.y, c.y); // xy
        p_moments[4] += second_moment(a.x, b.x, c.x, a.z, b.z, c.z); // xz
        p_moments[5] += second_moment(a.y, b.y, c.y, a.z, b.z, c.z); // yz
    }

    const double volume = vol6_sum / 6.0;
    if (std::abs(volume) < 1e-15) {
        return core::base::Result<MeshInertiaProperties>::err(
            core::base::Error{103, core::base::ErrorDomain::Validation, core::base::Severity::Error,
                              "Mesh has zero or degenerate signed volume", ""});
    }

    const double sign = (volume < 0.0) ? -1.0 : 1.0;
    const double abs_vol = std::abs(volume);
    const double mass = abs_vol * density_kg_m3;
    const Vector3d com = com_weighted / volume;

    const double p_xx = p_moments[0] * sign * density_kg_m3;
    const double p_yy = p_moments[1] * sign * density_kg_m3;
    const double p_zz = p_moments[2] * sign * density_kg_m3;
    const double p_xy = p_moments[3] * sign * density_kg_m3;
    const double p_xz = p_moments[4] * sign * density_kg_m3;
    const double p_yz = p_moments[5] * sign * density_kg_m3;

    // Inertia relative to origin
    const double ixx_0 = p_yy + p_zz;
    const double iyy_0 = p_xx + p_zz;
    const double izz_0 = p_xx + p_yy;
    const double ixy_0 = -p_xy;
    const double ixz_0 = -p_xz;
    const double iyz_0 = -p_yz;

    Matrix3x3 I_origin(
        ixx_0, ixy_0, ixz_0,
        ixy_0, iyy_0, iyz_0,
        ixz_0, iyz_0, izz_0
    );

    // Parallel Axis Theorem to shift inertia tensor to COM
    const double ixx_com = ixx_0 - mass * (com.y * com.y + com.z * com.z);
    const double iyy_com = iyy_0 - mass * (com.x * com.x + com.z * com.z);
    const double izz_com = izz_0 - mass * (com.x * com.x + com.y * com.y);
    const double ixy_com = ixy_0 + mass * com.x * com.y;
    const double ixz_com = ixz_0 + mass * com.x * com.z;
    const double iyz_com = iyz_0 + mass * com.y * com.z;

    Matrix3x3 I_com(
        ixx_com, ixy_com, ixz_com,
        ixy_com, iyy_com, iyz_com,
        ixz_com, iyz_com, izz_com
    );

    MeshInertiaProperties res;
    res.volume_m3 = abs_vol;
    res.mass_kg = mass;
    res.center_of_mass = com;
    res.inertia_origin = I_origin;
    res.inertia_com = I_com;
    res.principal_moments = {ixx_com, iyy_com, izz_com};

    return core::base::Result<MeshInertiaProperties>::ok(res);
}

MeshInertiaProperties MeshInertiaCalculator::compute_solid_box(
    double mass_kg, const Vector3d& size_m) {
    MeshInertiaProperties p;
    p.mass_kg = mass_kg;
    p.volume_m3 = size_m.x * size_m.y * size_m.z;
    p.center_of_mass = {0.0, 0.0, 0.0};
    const double c = mass_kg / 12.0;
    const double ixx = c * (size_m.y * size_m.y + size_m.z * size_m.z);
    const double iyy = c * (size_m.x * size_m.x + size_m.z * size_m.z);
    const double izz = c * (size_m.x * size_m.x + size_m.y * size_m.y);
    p.inertia_origin = Matrix3x3::diagonal(ixx, iyy, izz);
    p.inertia_com = p.inertia_origin;
    p.principal_moments = {ixx, iyy, izz};
    return p;
}

MeshInertiaProperties MeshInertiaCalculator::compute_solid_sphere(
    double mass_kg, double radius_m) {
    MeshInertiaProperties p;
    p.mass_kg = mass_kg;
    p.volume_m3 = (4.0 / 3.0) * std::numbers::pi * radius_m * radius_m * radius_m;
    p.center_of_mass = {0.0, 0.0, 0.0};
    const double i_val = 0.4 * mass_kg * radius_m * radius_m;
    p.inertia_origin = Matrix3x3::diagonal(i_val, i_val, i_val);
    p.inertia_com = p.inertia_origin;
    p.principal_moments = {i_val, i_val, i_val};
    return p;
}

MeshInertiaProperties MeshInertiaCalculator::compute_solid_cylinder(
    double mass_kg, double radius_m, double height_m) {
    MeshInertiaProperties p;
    p.mass_kg = mass_kg;
    p.volume_m3 = std::numbers::pi * radius_m * radius_m * height_m;
    p.center_of_mass = {0.0, 0.0, 0.0};
    const double izz = 0.5 * mass_kg * radius_m * radius_m;
    const double i_trans = (1.0 / 12.0) * mass_kg * (3.0 * radius_m * radius_m + height_m * height_m);
    p.inertia_origin = Matrix3x3::diagonal(i_trans, i_trans, izz);
    p.inertia_com = p.inertia_origin;
    p.principal_moments = {i_trans, i_trans, izz};
    return p;
}

// =============================================================================
// EnvironmentForces (PH4-09.01.005)
// =============================================================================

Vector3d EnvironmentForces::compute_gravity(
    const RigidBodyState& state,
    const Vector3d& gravity_vector) noexcept {
    if (state.inv_mass <= 0.0) return {0.0, 0.0, 0.0};
    return gravity_vector * state.mass_kg;
}

Vector3d EnvironmentForces::compute_buoyancy(
    const AerodynamicProperties& aero,
    const Vector3d& gravity_vector) noexcept {
    if (!aero.enable_buoyancy || aero.volume_m3 <= 0.0) {
        return {0.0, 0.0, 0.0};
    }
    // F_buoyancy = - rho_fluid * Volume * g
    const double fluid_mass = aero.fluid_density_kg_m3 * aero.volume_m3;
    return gravity_vector * (-fluid_mass);
}

Vector3d EnvironmentForces::compute_aerodynamic_drag(
    const RigidBodyState& state,
    const AerodynamicProperties& aero) noexcept {
    if (!aero.enable_drag || state.inv_mass <= 0.0) {
        return {0.0, 0.0, 0.0};
    }
    const Vector3d v = state.linear_velocity();
    const double speed = v.length();
    if (speed < 1e-12) {
        return {0.0, 0.0, 0.0};
    }

    // Quadratic drag: F_drag = -0.5 * rho * Cd * A * |v| * v
    const double quad_coeff = 0.5 * aero.fluid_density_kg_m3 * aero.drag_coefficient * aero.cross_section_area_m2;
    const Vector3d f_quad = v * (-quad_coeff * speed);

    // Linear damping: F_lin = -gamma_lin * v
    const Vector3d f_lin = v * (-aero.linear_damping);

    return f_quad + f_lin;
}

Vector3d EnvironmentForces::compute_rotational_drag(
    const RigidBodyState& state,
    const AerodynamicProperties& aero) noexcept {
    if (!aero.enable_drag || state.inv_mass <= 0.0) {
        return {0.0, 0.0, 0.0};
    }
    const Vector3d w = state.angular_velocity();
    return w * (-aero.angular_damping);
}

void EnvironmentForces::apply_environmental_forces(
    RigidBodyState& state,
    const Vector3d& gravity_vector,
    const AerodynamicProperties& aero) noexcept {
    if (state.inv_mass <= 0.0) return;

    const Vector3d f_grav = compute_gravity(state, gravity_vector);
    const Vector3d f_buoy = compute_buoyancy(aero, gravity_vector);
    const Vector3d f_drag = compute_aerodynamic_drag(state, aero);
    const Vector3d tau_drag = compute_rotational_drag(state, aero);

    state.apply_force_at_com(f_grav + f_buoy + f_drag);
    state.apply_torque(tau_drag);
}

// =============================================================================
// RigidBodyEntity (PH4-09.01.004)
// =============================================================================

void RigidBodyEntity::set_type(BodyType type) noexcept {
    type_ = type;
    if (type_ != BodyType::Dynamic) {
        state_.set_mass(0.0);
        state_.inv_mass = 0.0;
        state_.local_inertia = Matrix3x3::zero();
        state_.inv_local_inertia = Matrix3x3::zero();
        state_.linear_momentum = {0.0, 0.0, 0.0};
        state_.angular_momentum = {0.0, 0.0, 0.0};
    }
}

} // namespace shunya::physim
