#include "tests/test_framework.hpp"
#include "shunya/core/domain/thermal.hpp"

#include <cmath>
#include <vector>

using namespace shunya::core::domain::thermal;
using namespace shunya::core::base::math;

// =============================================================================
// THM-01: Thermal State & Heat Capacity
// =============================================================================

TEST_CASE("THM-01: Thermal State, Units, and Thermal Mass") {
    ThermalState state;
    state.set_celsius(25.0);
    CHECK(approx_equal(state.temperature, 298.15));
    CHECK(approx_equal(state.temperature_celsius(), 25.0));

    ThermalMaterial alu;
    alu.name = "Aluminum";
    alu.density = 2700.0;
    alu.specific_heat = 900.0;
    alu.thermal_conductivity = 205.0;

    // Volumetric heat capacity: rho * cp = 2700 * 900 = 2,430,000 J / (m^3 * K)
    CHECK(approx_equal(alu.volumetric_heat_capacity(), 2430000.0));

    ThermalNode node;
    node.volume = 0.002; // 2 liters
    node.material = alu;
    node.state = state;

    // Mass = 0.002 * 2700 = 5.4 kg
    // Thermal mass C_th = 5.4 * 900 = 4860 J / K
    CHECK(approx_equal(node.thermal_mass(), 4860.0));
    // Internal energy = 4860 * 298.15 = 1,449,009 J
    CHECK(approx_equal(node.internal_energy(), 4860.0 * 298.15));
}

// =============================================================================
// THM-02: Conduction Models & Boundary Conditions
// =============================================================================

TEST_CASE("THM-02: 1D Steady Conduction and Fixed Temperatures") {
    // 1D slab: L = 1.0 m, Area = 1.0 m^2, k = 100 W/(m*K), T_left = 373.15 K, T_right = 273.15 K
    auto net = ThermalBenchmarkFactory::create_1d_slab_conduction(1.0, 1.0, 100.0, 373.15, 273.15, 5);

    auto res = net.solve_steady_state();
    REQUIRE(res.is_ok());

    // Midpoint temperature should be exactly halfway: 323.15 K (50 °C)
    const auto& mid_node = net.get_node(2);
    CHECK(approx_equal(mid_node.state.temperature, 323.15, 0.01));

    // Linear temperature profile: node 1 should be 348.15, node 3 should be 298.15
    CHECK(approx_equal(net.get_node(1).state.temperature, 348.15, 0.01));
    CHECK(approx_equal(net.get_node(3).state.temperature, 298.15, 0.01));
}

// =============================================================================
// THM-03: Convection Boundary Models
// =============================================================================

TEST_CASE("THM-03: Convective Heat Transfer and Newton Cooling") {
    ThermalNetwork net;

    // Heated plate with heat source Q = 500 W cooled by convection h = 25 W/(m^2*K), Area = 1 m^2, T_amb = 293.15 K
    ThermalNode plate;
    plate.volume = 1e-3;
    plate.material.density = 2700.0;
    plate.material.specific_heat = 900.0;
    plate.state.heat_source = 500.0;

    plate.boundary.type = BoundaryType::Convection;
    plate.boundary.surface_area = 1.0;
    plate.boundary.convection_h = 25.0;
    plate.boundary.ambient_temperature = 293.15;

    net.add_node(plate);

    auto res = net.solve_steady_state();
    REQUIRE(res.is_ok());

    // Q = h * A * (T - T_amb) -> Delta T = Q / (h * A) = 500 / 25 = 20 K
    // Expected T = 293.15 + 20.0 = 313.15 K
    CHECK(approx_equal(net.get_node(0).state.temperature, 313.15, 0.01));
}

// =============================================================================
// THM-04: Stefan-Boltzmann Radiation Model
// =============================================================================

TEST_CASE("THM-04: Radiation Model and Stefan-Boltzmann Non-linear Cooling") {
    // Body in radiative equilibrium with space: Q_absorbed = 100 W, eps = 1.0, Area = 1 m^2, T_amb = 3 K
    ThermalNetwork net;

    ThermalNode radiator;
    radiator.volume = 1e-3;
    radiator.state.heat_source = 100.0;
    radiator.boundary.type = BoundaryType::Radiation;
    radiator.boundary.surface_area = 1.0;
    radiator.boundary.emissivity = 1.0;
    radiator.boundary.ambient_temperature = 3.0; // near absolute zero

    net.add_node(radiator);

    ThermalSimulationConfig cfg;
    cfg.max_iterations = 100;
    cfg.convergence_tol = 1e-5;
    auto res = net.solve_steady_state(cfg);
    REQUIRE(res.is_ok());

    // In steady state: Q = eps * sigma * A * (T^4 - T_amb^4) ~ eps * sigma * A * T^4
    // T = (100 / (1.0 * 5.67037e-8))^0.25 = (1.76355e9)^0.25 ~ 204.9 K
    const double expected_t = std::pow(100.0 / (1.0 * STEFAN_BOLTZMANN * 1.0), 0.25);
    CHECK(approx_equal(net.get_node(0).state.temperature, expected_t, 0.5));
}

// =============================================================================
// THM-05: Thermal Contacts & Interface Resistance
// =============================================================================

TEST_CASE("THM-05: Composite Wall with Thermal Contact Resistance") {
    // Layer 1: k1 = 10, L1 = 0.1 -> R1 = L1 / (k1 * A) = 0.1 / 10 = 0.01 K/W
    // Contact: R_c = 0.02 K/W
    // Layer 2: k2 = 5, L2 = 0.1 -> R2 = 0.1 / 5 = 0.02 K/W
    // Total R = 0.01 + 0.02 + 0.02 = 0.05 K/W
    // Delta T = 100 K (T1 = 400 K, T2 = 300 K) -> Q = 100 / 0.05 = 2000 W
    auto net = ThermalBenchmarkFactory::create_composite_wall_benchmark(
        10.0, 0.1, 0.02, 5.0, 0.1, 1.0, 400.0, 300.0);

    auto res = net.solve_steady_state();
    REQUIRE(res.is_ok());

    // Temperature drop across layer 1: Delta T1 = Q * R1 = 2000 * 0.01 = 20 K -> T_interface1 = 380 K
    // Temperature drop across contact: Delta T_c = Q * R_c = 2000 * 0.02 = 40 K -> T_interface2 = 340 K
    // Temperature drop across layer 2: Delta T2 = Q * R2 = 2000 * 0.02 = 40 K -> T2 = 300 K
    CHECK(approx_equal(net.get_node(1).state.temperature, 380.0, 0.1));
    CHECK(approx_equal(net.get_node(2).state.temperature, 340.0, 0.1));
}

// =============================================================================
// THM-06: Transient and Steady-State Analysis
// =============================================================================

TEST_CASE("THM-06: Transient Heat Integration and Energy Balance") {
    ThermalNetwork net;

    // A block with C_th = 1000 J/K heated by 100 W for 10 seconds -> Delta T = (100 * 10) / 1000 = 1.0 K
    ThermalNode block;
    block.volume = 1e-3;
    block.material.density = 1000.0;
    block.material.specific_heat = 1000.0; // C_th = 1000 J/K
    block.state.temperature = 300.0;
    block.state.heat_source = 100.0; // 100 W
    net.add_node(block);

    const double initial_energy = net.total_internal_energy();

    const double dt = 0.1;
    for (int step = 0; step < 100; ++step) { // 10.0 seconds total
        auto res = net.step_transient(dt);
        REQUIRE(res.is_ok());
    }

    CHECK(approx_equal(net.get_node(0).state.temperature, 301.0, 0.01));

    // Total energy gained = 100 W * 10 s = 1000 J
    const double final_energy = net.total_internal_energy();
    CHECK(approx_equal(final_energy - initial_energy, 1000.0, 0.1));
}

// =============================================================================
// THM-07: Temperature-Dependent Material & Phase Change
// =============================================================================

TEST_CASE("THM-07: Temperature-Dependent Conductivity and Latent Heat") {
    ThermalMaterial mat;
    mat.k_func = [](double temp_k) {
        // Linear temperature dependence: k(T) = 50 + 0.1 * (T - 300)
        return 50.0 + 0.1 * (temp_k - 300.0);
    };

    CHECK(approx_equal(mat.get_k(300.0), 50.0));
    CHECK(approx_equal(mat.get_k(400.0), 60.0));

    // Phase Change Material: ice/water melting between 273.15 and 274.15 K
    PhaseChangeMaterial pcm;
    pcm.solidus_temperature = 273.15;
    pcm.liquidus_temperature = 274.15;
    pcm.latent_heat = 333000.0;
    pcm.base_specific_heat = 2000.0;

    // Far from phase change
    CHECK(approx_equal(pcm.effective_specific_heat(260.0), 2000.0));
    CHECK(approx_equal(pcm.effective_specific_heat(300.0), 2000.0));

    // In the 1 K melting window: apparent cp = 2000 + 333000 / 1.0 = 335000 J/(kg*K)
    CHECK(approx_equal(pcm.effective_specific_heat(273.65), 335000.0));
}

// =============================================================================
// THM-08: Thermal Visualization & Observability Probes
// =============================================================================

TEST_CASE("THM-08: Thermal Probe Sampling and Time Series") {
    ThermalProbe probe(0, "HeaterProbe");
    CHECK(probe.node_id() == 0u);
    CHECK(probe.label() == "HeaterProbe");

    probe.record(0.0, 300.0);
    probe.record(1.0, 310.0, 50.0);
    probe.record(2.0, 320.0, 40.0);

    CHECK(probe.samples().size() == 3u);
    auto latest = probe.latest_sample();
    REQUIRE(latest.has_value());
    CHECK(latest->timestamp == 2.0);
    CHECK(latest->temperature_k == 320.0);
    CHECK(approx_equal(latest->temperature_c, 320.0 - 273.15));
    CHECK(latest->heat_flow == 40.0);
}

// =============================================================================
// THM-09: Validation Benchmark Suite
// =============================================================================

TEST_CASE("THM-09: Analytical Validation of Lumped Capacitance Cooling") {
    // Lumped capacitance benchmark:
    // mass = 1 kg, cp = 1000 J/(kg*K) -> C_th = 1000 J/K
    // Area = 0.1 m^2, h = 10 W/(m^2*K) -> G_conv = 1.0 W/K
    // Thermal time constant: tau = C_th / G_conv = 1000 s
    // Initial T = 373.15 K, T_amb = 293.15 K
    // T(t) = T_amb + (T0 - T_amb) * exp(-t / tau)
    // At t = tau (1000 s): Delta T = (373.15 - 293.15) * exp(-1) = 80 * 0.367879 = 29.43 K
    // Expected T(1000) = 293.15 + 29.43 = 322.58 K
    auto net = ThermalBenchmarkFactory::create_lumped_cooling_benchmark(
        1.0, 1000.0, 0.1, 10.0, 373.15, 293.15);

    const double dt = 5.0; // 5 second time step
    const int steps = 200; // 200 * 5 = 1000 seconds = 1 tau

    for (int i = 0; i < steps; ++i) {
        auto res = net.step_transient(dt);
        REQUIRE(res.is_ok());
    }

    const double sim_t = net.get_node(0).state.temperature;
    const double expected_t = 293.15 + (373.15 - 293.15) * std::exp(-1.0);

    // Backward Euler has truncation error O(dt/tau) ~ 5/1000 = 0.5%
    CHECK(approx_equal(sim_t, expected_t, 0.5));
}
