#include "tests/test_framework.hpp"

#include "shunya/chem/battery_cell_model.hpp"
#include "shunya/chem/electrochemical_cell.hpp"
#include "shunya/phy/thermal_solver_3d.hpp"

#include <cmath>

using namespace shunya::chem;
using namespace shunya::phy;

TEST_CASE("CHEM-03-002 Electrochemical Daniell cell E0 and Nernst equation") {
    auto e_cu = ElectrochemicalCellModel::lookup_e0_volts("Cu2+/Cu");
    auto e_zn = ElectrochemicalCellModel::lookup_e0_volts("Zn2+/Zn");
    REQUIRE(e_cu.is_ok());
    REQUIRE(e_zn.is_ok());

    auto e_std = ElectrochemicalCellModel::evaluate_nernst_cell_v(
        e_cu.value(), e_zn.value(), 2, 1.0, 298.15);
    REQUIRE(e_std.is_ok());
    // Standard Daniell cell E0 = +0.3419 - (-0.7618) = 1.1037 V
    CHECK(std::abs(e_std.value() - 1.1037) < 1e-3);

    // Non-standard concentration: high Zn2+ (Q = 100) lowers cell voltage by ~0.05916 V at z=2
    auto e_high_q = ElectrochemicalCellModel::evaluate_nernst_cell_v(
        e_cu.value(), e_zn.value(), 2, 100.0, 298.15);
    REQUIRE(e_high_q.is_ok());
    CHECK(e_high_q.value() < e_std.value());
    CHECK(std::abs((e_std.value() - e_high_q.value()) - 0.05916) < 0.002);

    // At equilibrium Q = K, E_cell -> 0 V (choose smaller E0 pair to avoid double exp saturation)
    const double e0_small = 0.20;
    const double k_eq = ElectrochemicalCellModel::equilibrium_constant_k(e0_small, 2, 298.15);
    auto e_at_eq = ElectrochemicalCellModel::evaluate_nernst_cell_v(e0_small, 0.0, 2, k_eq, 298.15);
    REQUIRE(e_at_eq.is_ok());
    CHECK(std::abs(e_at_eq.value()) < 1e-6);

    // Unknown half-cell triggers validation error
    auto bad = ElectrochemicalCellModel::lookup_e0_volts("Unobtainium+/U");
    CHECK_FALSE(bad.is_ok());
}

TEST_CASE("CHEM-03-002 Faraday electrolysis mass deposition") {
    // Deposit Copper (M = 0.063546 kg/mol, z = 2) at 10 A for 964.853 s (~0.1 mol e- => 0.05 mol Cu = 3.1773 g)
    const double mass_kg = ElectrochemicalCellModel::faraday_mass_deposited_kg(
        10.0, 964.8533212, 0.063546, 2);
    CHECK(std::abs(mass_kg - 0.0031773) < 1e-6);
}

TEST_CASE("CHEM-03-002 Li-ion 18650 0.5C, 1C, and 2C discharge curves and thermal rise") {
    auto run_curve = [](double c_rate) {
        BatteryCellParams params;
        BatteryCellRuntime cell(params, 1.0);
        return cell.simulate_cc_discharge(c_rate, 2.8, 2.0);
    };

    auto curve_05c = run_curve(0.5);
    auto curve_1c  = run_curve(1.0);
    auto curve_2c  = run_curve(2.0);
    REQUIRE(curve_05c.is_ok());
    REQUIRE(curve_1c.is_ok());
    REQUIRE(curve_2c.is_ok());

    REQUIRE(!curve_05c.value().empty());
    REQUIRE(!curve_1c.value().empty());
    REQUIRE(!curve_2c.value().empty());

    // Initial voltage near 4.2 V minus I*R0 drop; higher C-rate causes larger initial IR sag
    CHECK(curve_05c.value().front().terminal_voltage_v > curve_1c.value().front().terminal_voltage_v);
    CHECK(curve_1c.value().front().terminal_voltage_v > curve_2c.value().front().terminal_voltage_v);

    // Duration decreases monotonically with C-rate (1C < ~3600s, 2C < ~1800s)
    const double t_05c = curve_05c.value().back().time_s;
    const double t_1c  = curve_1c.value().back().time_s;
    const double t_2c  = curve_2c.value().back().time_s;
    CHECK(t_05c > t_1c);
    CHECK(t_1c > t_2c);

    // Higher C-rate generates greater Joule self-heating peak temperature above 298.15 K ambient
    CHECK(curve_2c.value().back().temperature_k > curve_1c.value().back().temperature_k);
    CHECK(curve_1c.value().back().temperature_k > 299.0);
    CHECK(curve_2c.value().back().temperature_k > 302.0);

    // Discharge plateau around ~3.6-3.8 V near 50% time of 0.5C run
    const auto& mid = curve_05c.value()[curve_05c.value().size() / 2];
    CHECK(mid.terminal_voltage_v > 3.45);
    CHECK(mid.terminal_voltage_v < 3.90);
}

TEST_CASE("CHEM-03-002 Li-ion external short-circuit runaway heating and 3D ThermalSolver coupling") {
    BatteryCellParams params;
    params.enable_runaway_heating = true;
    params.cooling_h_area_w_k = 0.05; // poorly cooled pack
    BatteryCellRuntime cell(params, 1.0);

    // 5 milliohm hard short circuit over 45 seconds
    for (int i = 0; i < 45; ++i) {
        auto step = cell.step_resistive_load(0.005, 1.0);
        REQUIRE(step.is_ok());
    }

    // Should cross 393.15 K threshold and exhibit strong thermal runaway above 450 K
    CHECK(cell.temperature_k() > 450.0);
    CHECK(cell.last_heat_watts() > 150.0);

    // Inject heat into center node of a 3D thermal grid and step diffusion
    ThermalGridSpec3D grid_spec;
    grid_spec.nx = 5;
    grid_spec.ny = 3;
    grid_spec.nz = 3;
    grid_spec.lx_m = 0.25;
    grid_spec.ly_m = 0.15;
    grid_spec.lz_m = 0.15;
    grid_spec.initial_temp_k = 298.15;

    ThermalSolver3D solver(grid_spec);
    const double cell_vol = (grid_spec.lx_m / 5.0) * (grid_spec.ly_m / 3.0) * (grid_spec.lz_m / 3.0);
    cell.inject_heat_to_thermal_cell(solver, 2, 1, 1, cell_vol);

    auto adv = solver.advance(2.0, 0.5);
    REQUIRE(adv.is_ok());
    CHECK(solver.temperature(2, 1, 1) > 298.15);
    CHECK(solver.temperature(1, 1, 1) > 298.15); // diffused to neighbor
}
