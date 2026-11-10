#include "tests/test_framework.hpp"

#include "shunya/integ/chemo_thermal_connector.hpp"
#include "shunya/integ/electro_thermal_connector.hpp"
#include "shunya/integ/fsi_connector.hpp"
#include "shunya/integ/thermo_mechanical_connector.hpp"

#include <cmath>

using namespace shunya::integ;

TEST_CASE("INT-01-002 ElectroThermalConnector self-heating resistance rise and fuse I^2*t blowout") {
    FuseElementSpec spec;
    spec.cold_resistance_ohm = 0.05;
    spec.temp_coeff_alpha_per_k = 0.004;
    spec.ref_temp_k = 293.15;
    spec.melting_temp_k = 505.0; // Low-melting fuse alloy (~232 C)
    spec.thermal_mass_j_k = 0.50;
    spec.ambient_h_area_w_k = 0.005;

    ElectroThermalConnector fuse(spec);
    const double i_surge = 30.0; // Initial P = 900 * 0.05 = 45 W
    bool blew = false;
    double blow_time = 0.0;
    double prev_r = fuse.current_resistance_ohm();

    for (int step = 0; step < 400; ++step) {
        auto res = fuse.step_current(i_surge, 0.02);
        REQUIRE(res.is_ok());
        if (!res.value().fuse_melted) {
            CHECK(res.value().resistance_ohm > prev_r);
            prev_r = res.value().resistance_ohm;
        } else {
            blew = true;
            blow_time = res.value().time_s;
            break;
        }
    }

    REQUIRE(blew);
    CHECK(blow_time > 1.0);
    CHECK(blow_time < 3.0);
    // Analytical I^2*t rating roughly C * dT / R_avg ~ 0.5 * 212 / 0.071 ~ 1490 A^2*s
    CHECK(fuse.integral_i2t() > 1100.0);
    CHECK(fuse.integral_i2t() < 2000.0);
    CHECK(std::isinf(fuse.current_resistance_ohm()));
}

TEST_CASE("INT-01-002 ThermoMechanicalConnector bimetal deflection, Euler buckling, and Taylor-Quinney heating") {
    BimaterialBeamSpec bimetal;
    const double d_100 = ThermoMechanicalConnector::bimetallic_tip_deflection_m(bimetal, 100.0);
    const double d_200 = ThermoMechanicalConnector::bimetallic_tip_deflection_m(bimetal, 200.0);
    CHECK(d_100 > 0.005); // several mm visible warp
    CHECK(std::abs(d_200 - 2.0 * d_100) < 1e-9);

    SlenderColumnBucklingSpec col;
    const double dt_crit = ThermoMechanicalConnector::euler_critical_buckling_delta_t_k(col);
    // Exact pi^2 * I / (alpha * A * L^2) = pi^2 * 8.333e-10 / (12e-6 * 1e-4 * 1) = 6.85 K
    CHECK(std::abs(dt_crit - 6.854) < 0.05);

    bool buckled = false;
    auto sig_below = ThermoMechanicalConnector::axial_thermal_stress_pa(col, 4.0, buckled);
    REQUIRE(sig_below.is_ok());
    CHECK_FALSE(buckled);

    auto sig_above = ThermoMechanicalConnector::axial_thermal_stress_pa(col, 15.0, buckled);
    REQUIRE(sig_above.is_ok());
    CHECK(buckled);

    // Rapid 20% plastic strain on 400 MPa aluminum (rho=2700, cp=900) with beta=0.9 -> ~29.6 K rise
    const double dt_plastic = ThermoMechanicalConnector::taylor_quinney_plastic_temp_rise_k(
        400e6, 0.20, 2700.0, 900.0, 0.90);
    CHECK(std::abs(dt_plastic - 29.63) < 0.5);
}

TEST_CASE("INT-01-002 FsiConnector Archimedes floating wooden cylinder hydrometer frequency") {
    FloatingCylinderSpec cyl;
    cyl.height_m = 0.30;
    cyl.solid_density_kg_m3 = 600.0;
    cyl.fluid_density_kg_m3 = 1000.0;
    cyl.linear_damping_n_s_m = 0.0;

    // Displace +4 cm deeper than equilibrium (0.18 m -> 0.22 m)
    FsiConnector fsi(cyl, 0.22);
    CHECK(std::abs(fsi.equilibrium_submerged_depth_m() - 0.18) < 1e-9);

    const double expected_freq_hz = fsi.analytical_oscillation_freq_hz();
    // Period T = 2*pi*sqrt(0.18 / 9.80665) ~ 0.851 s -> f ~ 1.175 Hz
    CHECK(std::abs(expected_freq_hz - 1.1746) < 0.01);

    auto traj = fsi.simulate_hydrometer(3.5, 0.002);
    REQUIRE(traj.is_ok());

    // Measure zero-crossing upward times around equilibrium depth 0.18 m to verify frequency
    std::vector<double> crossings;
    for (size_t i = 1; i < traj.value().size(); ++i) {
        const double d0 = traj.value()[i - 1].submerged_depth_m - 0.18;
        const double d1 = traj.value()[i].submerged_depth_m - 0.18;
        if (d0 <= 0.0 && d1 > 0.0) {
            crossings.push_back(traj.value()[i].time_s);
        }
    }
    REQUIRE(crossings.size() >= 3);
    const double measured_period = crossings[2] - crossings[1];
    const double measured_freq = 1.0 / measured_period;
    CHECK(std::abs(measured_freq - expected_freq_hz) < 0.03);
}

TEST_CASE("INT-01-002 ChemoThermalConnector CSTR multi-steady states and ignition hysteresis") {
    CstrReactorSpec spec;
    ChemoThermalConnector cstr(spec, 2000.0, 300.0);

    // 1. Verify exact 3 steady-state Van Heerden roots: lower (~301 K), unstable saddle (~349 K), upper (~397 K)
    auto roots = cstr.find_steady_state_temps_k(285.0, 440.0, 2000);
    REQUIRE(roots.size() == 3);
    CHECK(roots[0] < 305.0);
    CHECK(roots[1] > 335.0);
    CHECK(roots[1] < 365.0);
    CHECK(roots[2] > 385.0);

    // 2. Starting cool (300 K) converges to lower stable branch (~301 K)
    REQUIRE(cstr.advance(40.0, 0.25).is_ok());
    CHECK(cstr.state().temp_k < 306.0);

    // 3. Raise coolant to 370 K (past ignition saddle fold) -> triggers exothermic thermal runaway ignition
    cstr.set_coolant_temp(370.0);
    REQUIRE(cstr.advance(100.0, 0.25).is_ok());
    CHECK(cstr.state().temp_k > 410.0);

    // 4. Lower coolant back to 300 K -> hysteresis keeps reactor on UPPER ignited steady state (~397 K)!
    cstr.set_coolant_temp(300.0);
    REQUIRE(cstr.advance(80.0, 0.25).is_ok());
    CHECK(std::abs(cstr.state().temp_k - roots[2]) < 2.0);
}
