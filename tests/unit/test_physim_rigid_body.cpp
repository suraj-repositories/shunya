#include "tests/test_framework.hpp"

#include "shunya/physim/integrator.hpp"
#include "shunya/physim/rigid_body.hpp"

#include <cmath>
#include <numbers>

using namespace shunya::physim;
using namespace shunya::core::domain;

namespace {

TriangleMesh make_test_cube(double side_length) {
    const double h = side_length * 0.5;
    TriangleMesh m;
    m.vertices = {
        {-h, -h, -h}, // 0
        { h, -h, -h}, // 1
        { h,  h, -h}, // 2
        {-h,  h, -h}, // 3
        {-h, -h,  h}, // 4
        { h, -h,  h}, // 5
        { h,  h,  h}, // 6
        {-h,  h,  h}  // 7
    };
    // Outward CCW triangles on all 6 faces
    m.faces = {
        {0, 2, 1}, {0, 3, 2}, // -Z
        {4, 5, 6}, {4, 6, 7}, // +Z
        {0, 1, 5}, {0, 5, 4}, // -Y
        {3, 7, 6}, {3, 6, 2}, // +Y
        {0, 4, 7}, {0, 7, 3}, // -X
        {1, 2, 6}, {1, 6, 5}  // +X
    };
    return m;
}

} // namespace

TEST_CASE("PH4-09.01.001: Rigid body state representation x, q, p, L and momentum-velocity relations") {
    RigidBodyState state;
    state.set_mass(2.5); // 2.5 kg
    CHECK(std::abs(state.mass_kg - 2.5) < 1e-12);
    CHECK(std::abs(state.inv_mass - 0.4) < 1e-12);

    // Box inertia: 0.1 x 0.2 x 0.3 m
    const auto box_inertia = MeshInertiaCalculator::compute_solid_box(2.5, {0.1, 0.2, 0.3});
    state.set_local_inertia(box_inertia.inertia_com);

    // Set linear and angular velocities
    state.set_linear_velocity({3.0, -1.5, 2.0});
    CHECK(std::abs(state.linear_momentum.x - 7.5) < 1e-12);
    CHECK(std::abs(state.linear_momentum.y - (-3.75)) < 1e-12);
    CHECK(std::abs(state.linear_momentum.z - 5.0) < 1e-12);

    // Verify recovery of linear velocity from momentum
    const auto v = state.linear_velocity();
    CHECK(std::abs(v.x - 3.0) < 1e-12);
    CHECK(std::abs(v.y - (-1.5)) < 1e-12);
    CHECK(std::abs(v.z - 2.0) < 1e-12);

    // Set angular velocity
    state.set_angular_velocity({10.0, 5.0, -2.0});
    const auto omega = state.angular_velocity();
    CHECK(std::abs(omega.x - 10.0) < 1e-9);
    CHECK(std::abs(omega.y - 5.0) < 1e-9);
    CHECK(std::abs(omega.z - (-2.0)) < 1e-9);

    // Kinetic energy calculation
    const double t_lin = state.linear_kinetic_energy();
    const double t_rot = state.rotational_kinetic_energy();
    const double t_tot = state.kinetic_energy();
    CHECK(t_lin > 0.0);
    CHECK(t_rot > 0.0);
    CHECK(std::abs(t_tot - (t_lin + t_rot)) < 1e-12);

    // Test impulse application
    state.apply_linear_impulse({1.0, 0.0, 0.0});
    CHECK(std::abs(state.linear_velocity().x - 3.4) < 1e-12);

    // Point velocity at offset
    const auto pt_vel = state.point_velocity(state.position + Vector3d{0.0, 0.1, 0.0});
    CHECK(pt_vel.length() > 0.0);
}

TEST_CASE("PH4-09.01.002: Watertight mesh 3D mass inertia tensor via Mirtich's divergence theorem") {
    // 1 m solid cube with density 1000 kg/m^3
    const auto cube_mesh = make_test_cube(1.0);
    const auto res = MeshInertiaCalculator::compute_from_mesh(cube_mesh, 1000.0);

    REQUIRE(res.is_ok());
    const auto& props = res.value();

    CHECK(std::abs(props.volume_m3 - 1.0) < 1e-9);
    CHECK(std::abs(props.mass_kg - 1000.0) < 1e-9);
    CHECK(props.center_of_mass.length() < 1e-9); // centered at origin

    // Exact I_xx = (1/12) * m * (1^2 + 1^2) = (1000 * 2) / 12 = 166.666667 kg*m^2
    const double expected_i = 1000.0 / 6.0;
    CHECK(std::abs(props.inertia_com.at(0, 0) - expected_i) < 1e-4);
    CHECK(std::abs(props.inertia_com.at(1, 1) - expected_i) < 1e-4);
    CHECK(std::abs(props.inertia_com.at(2, 2) - expected_i) < 1e-4);

    // Off-diagonals must be near zero for symmetric cube
    CHECK(std::abs(props.inertia_com.at(0, 1)) < 1e-9);
    CHECK(std::abs(props.inertia_com.at(0, 2)) < 1e-9);
    CHECK(std::abs(props.inertia_com.at(1, 2)) < 1e-9);

    // Test solid cylinder and sphere analytic helpers
    const auto sphere = MeshInertiaCalculator::compute_solid_sphere(10.0, 0.5);
    CHECK(std::abs(sphere.mass_kg - 10.0) < 1e-12);
    // 0.4 * 10 * 0.25 = 1.0
    CHECK(std::abs(sphere.inertia_com.at(0, 0) - 1.0) < 1e-12);

    const auto cyl = MeshInertiaCalculator::compute_solid_cylinder(5.0, 0.2, 1.0);
    // I_zz = 0.5 * 5.0 * 0.04 = 0.1
    CHECK(std::abs(cyl.inertia_com.at(2, 2) - 0.1) < 1e-12);

    // Negative case: empty mesh returns error
    TriangleMesh empty_mesh;
    const auto err_res = MeshInertiaCalculator::compute_from_mesh(empty_mesh, 1000.0);
    CHECK(err_res.is_err());
}

TEST_CASE("PH4-09.01.003: Symplectic Euler and RK4 integrators preserving linear and angular momentum") {
    // Frictionless spinning rigid body with zero external torque
    RigidBodyState state_symp;
    state_symp.set_mass(1.0);
    state_symp.set_local_inertia(Matrix3x3::diagonal(2.0, 2.0, 1.0));
    state_symp.set_linear_velocity({1.0, 2.0, -1.0});
    state_symp.set_angular_velocity({0.0, 0.0, 5.0});

    const Vector3d p0 = state_symp.linear_momentum;
    const Vector3d L0 = state_symp.angular_momentum;
    const double e0 = state_symp.kinetic_energy();

    // Step 500 steps with Symplectic Euler
    for (int i = 0; i < 500; ++i) {
        RigidBodyIntegrator::step_symplectic_euler(state_symp, 0.002);
    }

    // Linear and angular momentum strictly conserved
    CHECK(std::abs(state_symp.linear_momentum.x - p0.x) < 1e-12);
    CHECK(std::abs(state_symp.linear_momentum.y - p0.y) < 1e-12);
    CHECK(std::abs(state_symp.linear_momentum.z - p0.z) < 1e-12);

    CHECK(std::abs(state_symp.angular_momentum.x - L0.x) < 1e-12);
    CHECK(std::abs(state_symp.angular_momentum.y - L0.y) < 1e-12);
    CHECK(std::abs(state_symp.angular_momentum.z - L0.z) < 1e-12);
    CHECK(std::abs(state_symp.kinetic_energy() - e0) < 1e-10);

    // Step 500 steps with RK4
    RigidBodyState state_rk4;
    state_rk4.set_mass(1.0);
    state_rk4.set_local_inertia(Matrix3x3::diagonal(2.0, 2.0, 1.0));
    state_rk4.set_linear_velocity({1.0, 2.0, -1.0});
    state_rk4.set_angular_velocity({0.0, 0.0, 5.0});

    for (int i = 0; i < 500; ++i) {
        RigidBodyIntegrator::step_rk4(state_rk4, 0.002);
    }

    CHECK(std::abs(state_rk4.linear_momentum.x - p0.x) < 1e-12);
    CHECK(std::abs(state_rk4.linear_momentum.y - p0.y) < 1e-12);
    CHECK(std::abs(state_rk4.linear_momentum.z - p0.z) < 1e-12);
    CHECK(std::abs(state_rk4.angular_momentum.z - L0.z) < 1e-12);
    CHECK(std::abs(state_rk4.kinetic_energy() - e0) < 1e-10);
}

TEST_CASE("PH4-09.01.004: Support dynamic, kinematic, and static body types") {
    RigidBodyEntity dyn_body("dyn_1", BodyType::Dynamic);
    dyn_body.state().set_mass(10.0);
    dyn_body.state().set_linear_velocity({1.0, 0.0, 0.0});
    CHECK(dyn_body.is_dynamic());
    CHECK(!dyn_body.is_kinematic());
    CHECK(!dyn_body.is_static());
    CHECK(dyn_body.state().inv_mass > 0.0);

    RigidBodyEntity kin_body("kin_1", BodyType::Kinematic);
    kin_body.state().set_linear_velocity({2.0, 0.0, 0.0});
    CHECK(kin_body.is_kinematic());
    CHECK(kin_body.state().inv_mass == 0.0);

    RigidBodyEntity stat_body("stat_1", BodyType::Static);
    CHECK(stat_body.is_static());
    CHECK(stat_body.state().inv_mass == 0.0);

    std::vector<RigidBodyEntity> bodies;
    bodies.push_back(dyn_body);
    bodies.push_back(kin_body);
    bodies.push_back(stat_body);

    // Step system with gravity
    const Vector3d grav{0.0, -9.81, 0.0};
    RigidBodyIntegrator::step_system(bodies, 0.1, IntegratorScheme::SymplecticEuler, grav);

    // Dynamic body accelerates downward under gravity
    CHECK(bodies[0].state().linear_velocity().y < -0.9);

    // Kinematic body preserves prescribed velocity unaffected by gravity
    CHECK(std::abs(bodies[1].state().linear_velocity().x - 2.0) < 1e-12);
    CHECK(std::abs(bodies[1].state().linear_velocity().y) < 1e-12);
    CHECK(std::abs(bodies[1].state().position.x - 0.2) < 1e-12);

    // Static body remains completely stationary at origin
    CHECK(bodies[2].state().position.length() < 1e-12);
    CHECK(bodies[2].state().linear_velocity().length() < 1e-12);
}

TEST_CASE("PH4-09.01.005: Aerodynamic drag, buoyancy in air, and global gravitational controls") {
    RigidBodyState state;
    state.set_mass(0.5); // 0.5 kg sphere
    state.set_linear_velocity({0.0, -20.0, 0.0}); // falling fast

    AerodynamicProperties aero;
    aero.fluid_density_kg_m3 = 1.225; // air
    aero.volume_m3 = 0.005;           // 5 liters
    aero.cross_section_area_m2 = 0.02;
    aero.drag_coefficient = 0.47;     // sphere
    aero.enable_buoyancy = true;
    aero.enable_drag = true;

    const Vector3d g{0.0, -9.80665, 0.0};

    const auto f_grav = EnvironmentForces::compute_gravity(state, g);
    CHECK(std::abs(f_grav.y - (0.5 * -9.80665)) < 1e-12);

    const auto f_buoy = EnvironmentForces::compute_buoyancy(aero, g);
    // Buoyant force points upward (+Y)
    CHECK(f_buoy.y > 0.0);
    CHECK(std::abs(f_buoy.y - (1.225 * 0.005 * 9.80665)) < 1e-9);

    const auto f_drag = EnvironmentForces::compute_aerodynamic_drag(state, aero);
    // Drag resists falling velocity, points upward (+Y)
    CHECK(f_drag.y > 0.0);
    const double expected_drag = 0.5 * 1.225 * 0.47 * 0.02 * 400.0;
    CHECK(std::abs(f_drag.y - expected_drag) < 1e-6);

    // Apply combined environmental forces
    EnvironmentForces::apply_environmental_forces(state, g, aero);
    CHECK(state.force_accumulator.y != 0.0);
}

TEST_CASE("PH4-09.01.006: Analytical classical mechanics validation: free-fall, projectile, gyroscopic precession") {
    // 1. Analytical free-fall vs Symplectic Euler
    const double y0 = 100.0;
    const double g = 9.80665;
    const double t_total = 2.0;
    const double dt = 0.001;
    const int steps = static_cast<int>(t_total / dt);

    RigidBodyState free_fall_state;
    free_fall_state.set_mass(2.0);
    free_fall_state.position = {0.0, y0, 0.0};

    for (int i = 0; i < steps; ++i) {
        free_fall_state.apply_force_at_com({0.0, -free_fall_state.mass_kg * g, 0.0});
        RigidBodyIntegrator::step_symplectic_euler(free_fall_state, dt);
    }

    const auto ana_fall = RigidBodyIntegrator::analytical_free_fall(y0, 0.0, g, t_total);
    CHECK(std::abs(free_fall_state.position.y - ana_fall.position_y) < 0.05);
    CHECK(std::abs(free_fall_state.linear_velocity().y - ana_fall.velocity_y) < 0.05);

    // 2. Analytical projectile trajectory
    const Vector3d r0{0.0, 0.0, 0.0};
    const Vector3d v0{30.0, 40.0, 0.0}; // launch at angle ~53 deg
    const auto proj = RigidBodyIntegrator::analytical_projectile(r0, v0, g, 2.0);

    CHECK(std::abs(proj.position.x - 60.0) < 1e-12);
    CHECK(std::abs(proj.max_height - (1600.0 / (2.0 * g))) < 1e-12);
    const double t_flight = 80.0 / g;
    CHECK(std::abs(proj.flight_time - t_flight) < 1e-12);
    CHECK(std::abs(proj.range - (30.0 * t_flight)) < 1e-12);

    // 3. Terminal velocity under aerodynamic drag
    const double m = 1.0;
    const double rho = 1.225;
    const double cd = 0.47;
    const double a = 0.05;
    const double v_term = RigidBodyIntegrator::analytical_terminal_velocity(m, g, rho, cd, a);
    CHECK(v_term > 20.0);
    CHECK(v_term < 50.0);

    // 4. Gyroscopic precession of spinning symmetric top
    // mass 0.5 kg, pivot 0.1 m, Izz 0.002 kg*m^2, spin rate 200 rad/s (~1910 RPM)
    const auto prec = RigidBodyIntegrator::analytical_gyroscopic_precession(0.5, g, 0.1, 0.002, 200.0);
    // tau = 0.5 * 9.80665 * 0.1 = 0.4903325 N*m
    // L_z = 0.002 * 200 = 0.4 kg*m^2/s
    // Omega_p = 0.4903325 / 0.4 = 1.2258 rad/s
    CHECK(std::abs(prec.precession_frequency_rad_s - 1.22583) < 1e-4);
    CHECK(std::abs(prec.precession_period_s - (2.0 * std::numbers::pi / 1.22583)) < 1e-3);
}
