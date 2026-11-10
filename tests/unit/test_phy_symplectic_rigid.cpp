#include "tests/test_framework.hpp"

#include "shunya/phy/energy_auditor.hpp"
#include "shunya/phy/inertia_tensor_calculator.hpp"
#include "shunya/phy/rigid_body_kinematics.hpp"
#include "shunya/phy/symplectic_integrator.hpp"

#include <cmath>
#include <numbers>

using namespace shunya::phy;
using namespace shunya::core::domain;
using namespace shunya::core::domain::mechanics;

namespace {

TriangleMesh make_box_mesh(const Vector3d& center, const Vector3d& size) {
    const Vector3d h = size * 0.5;
    TriangleMesh m;
    m.vertices = {
        {center.x - h.x, center.y - h.y, center.z - h.z}, // 0
        {center.x + h.x, center.y - h.y, center.z - h.z}, // 1
        {center.x + h.x, center.y + h.y, center.z - h.z}, // 2
        {center.x - h.x, center.y + h.y, center.z - h.z}, // 3
        {center.x - h.x, center.y - h.y, center.z + h.z}, // 4
        {center.x + h.x, center.y - h.y, center.z + h.z}, // 5
        {center.x + h.x, center.y + h.y, center.z + h.z}, // 6
        {center.x - h.x, center.y + h.y, center.z + h.z}  // 7
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

TEST_CASE("PHY-01-001-01: Symplectic harmonic oscillator maintains total energy < 0.01% over 10,000 steps") {
    const double k = 10.0;
    const double m = 1.0;

    for (auto scheme : {SymplecticScheme::VelocityVerlet, SymplecticScheme::Leapfrog, SymplecticScheme::YoshidaRk4}) {
        std::vector<DynamicalBodyState> bodies{{ {1.0, 0.0, 0.0}, {0.0, 0.0, 0.0}, m }};
        EnergyAuditor auditor;
        auto force_fn = [k](double, std::span<const Vector3d> pos, std::span<Vector3d> out) {
            out[0] = pos[0] * (-k);
        };
        auto pot_fn = [k](double, std::span<const DynamicalBodyState> b) {
            return 0.5 * k * b[0].position.length_squared();
        };

        auto res = SymplecticIntegrator::integrate(bodies, 10000, 0.005, force_fn, scheme, &auditor, pot_fn, 100);
        REQUIRE(res.is_ok());
        CHECK(auditor.max_relative_drift() < 1e-4); // Strictly < 0.01%
    }
}

TEST_CASE("PHY-01-001-01: Kepler 2-body orbits across eccentricities e=0, 0.5, 0.75 over 100 revolutions") {
    constexpr double kMu = 1.0; // G * M = 1
    constexpr double kA = 1.0;
    constexpr double kOrbitalPeriod = 2.0 * std::numbers::pi;

    for (double ecc : {0.0, 0.5, 0.75}) {
        const double r_peri = kA * (1.0 - ecc);
        const double v_peri = std::sqrt(kMu * (1.0 + ecc) / r_peri);

        std::vector<DynamicalBodyState> bodies{{ {r_peri, 0.0, 0.0}, {0.0, v_peri, 0.0}, 1.0 }};
        const Vector3d L0 = bodies[0].position.cross(bodies[0].velocity);

        EnergyAuditor auditor;
        auto gravity_fn = [](double, std::span<const Vector3d> pos, std::span<Vector3d> out) {
            const double r2 = pos[0].length_squared();
            const double r3 = r2 * std::sqrt(r2);
            out[0] = pos[0] * (-kMu / r3);
        };
        auto pot_fn = [](double, std::span<const DynamicalBodyState> b) {
            return -kMu / b[0].position.length();
        };

        // 100 revolutions with Yoshida 4th-order symplectic integrator (400 steps/rev -> 40,000 steps total)
        const double dt = kOrbitalPeriod / 400.0;
        const size_t total_steps = 40000;
        auto res = SymplecticIntegrator::integrate(
            bodies, total_steps, dt, gravity_fn, SymplecticScheme::YoshidaRk4, &auditor, pot_fn, 400);
        REQUIRE(res.is_ok());

        CHECK(auditor.max_relative_drift() < 1e-4); // < 0.01% over 100 orbits
        const Vector3d L_end = bodies[0].position.cross(bodies[0].velocity);
        CHECK((L_end - L0).length() < 1e-8);
    }
}

TEST_CASE("PHY-01-001-02: Mesh inertia tensor calculation & Dzhanibekov intermediate-axis instability") {
    // 1) Closed triangular box mesh at offset (5, 2, -1) with dimensions (2, 3, 4) m and density 500 kg/m^3
    const auto mesh = make_box_mesh({5.0, 2.0, -1.0}, {2.0, 3.0, 4.0});
    auto props_res = InertiaTensorCalculator::compute_from_closed_mesh(mesh, 500.0);
    REQUIRE(props_res.is_ok());
    const auto& props = props_res.value();

    // V = 24 m^3, M = 12,000 kg
    CHECK(std::abs(props.volume_m3 - 24.0) < 1e-9);
    CHECK(std::abs(props.mass_kg - 12000.0) < 1e-6);
    CHECK((props.center_of_mass - Vector3d{5.0, 2.0, -1.0}).length() < 1e-9);

    // Analytical box CoM inertia: I_x = M/12*(3^2 + 4^2) = 25000, I_y = 20000, I_z = 13000
    CHECK(std::abs(props.inertia_body_com.at(0, 0) - 25000.0) < 1e-4);
    CHECK(std::abs(props.inertia_body_com.at(1, 1) - 20000.0) < 1e-4);
    CHECK(std::abs(props.inertia_body_com.at(2, 2) - 13000.0) < 1e-4);
    CHECK(std::abs(props.inertia_body_com.at(0, 1)) < 1e-6);

    // 2) Torque-free Dzhanibekov effect: I1 < I2 < I3, spin nearly along intermediate axis I2
    RotationalBodyState wingnut;
    wingnut.mass_kg = 1.0;
    wingnut.set_local_inertia(Matrix3x3::diagonal(1.0, 2.5, 4.0));
    wingnut.orientation = Quaterniond::identity();
    wingnut.angular_velocity_world = {0.05, 10.0, 0.02};

    const Vector3d L0 = RigidBodyKinematics::angular_momentum_world(wingnut);
    const double E0 = RigidBodyKinematics::rotational_kinetic_energy(wingnut);

    double min_wy_body = 10.0;
    double max_wy_body = 10.0;
    for (int step = 0; step < 3000; ++step) {
        RigidBodyKinematics::step_rk4(wingnut, 0.002);
        const Vector3d wb = RigidBodyKinematics::world_to_body(wingnut.orientation, wingnut.angular_velocity_world);
        min_wy_body = std::min(min_wy_body, wb.y);
        max_wy_body = std::max(max_wy_body, wb.y);
    }

    // Dzhanibekov theorem: intermediate-axis spin flips completely to opposite sign (~ -10 rad/s)!
    CHECK(min_wy_body < -8.0);
    CHECK(max_wy_body > 8.0);

    // Verify angular momentum, rotational kinetic energy, and quaternion unit norm conservation
    const Vector3d L_end = RigidBodyKinematics::angular_momentum_world(wingnut);
    const double E_end = RigidBodyKinematics::rotational_kinetic_energy(wingnut);
    CHECK((L_end - L0).length() / L0.length() < 1e-5);
    CHECK(std::abs(E_end - E0) / E0 < 1e-5);
    CHECK(std::abs(wingnut.orientation.norm() - 1.0) < 1e-12);
}
