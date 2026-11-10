#include "tests/test_framework.hpp"

#include "shunya/env/climatic_chamber.hpp"
#include "shunya/env/wind_tunnel_fixture.hpp"
#include "shunya/phy/thermal_solver_3d.hpp"

#include <cmath>

using namespace shunya::env;
using namespace shunya::phy;

TEST_CASE("ENV-02-001 Hermetic climatic chamber MIL-STD-810 thermal shock & vacuum physics") {
    ClimaticChamberModel chamber(298.15, 101325.0, 25.0);
    const double h_atm = chamber.effective_h_w_m2_k();

    // MIL-STD-810 rapid thermal shock: -40 C (233.15 K) soak -> +125 C (398.15 K) soak
    REQUIRE(chamber.add_profile_segment({120.0, 233.15, 101325.0, 5.0}).is_ok());
    REQUIRE(chamber.add_profile_segment({180.0, 398.15, 101325.0, 5.0}).is_ok());

    ChamberDeviceUnderTest pcb;
    pcb.temperature_k = 298.15;
    pcb.thermal_mass_j_k = 20.0;
    pcb.surface_area_m2 = 0.025;
    pcb.emissivity = 0.90;

    auto samples = chamber.run_profile(1.0, pcb);
    REQUIRE(samples.is_ok());
    REQUIRE(samples.value().size() == 300);

    // At end of cold segment (index 119), PCB cooled well below 260 K
    CHECK(samples.value()[119].chamber_temp_k < 235.0);
    CHECK(samples.value()[119].dut_temp_k < 260.0);

    // At end of hot soak (index 299), PCB heated above 360 K
    CHECK(samples.value().back().chamber_temp_k > 395.0);
    CHECK(samples.value().back().dut_temp_k > 360.0);

    // High vacuum check down to 1e-4 Pa suppresses convective h almost completely
    ClimaticChamberModel vac_chamber(398.15, 1e-4, 25.0);
    CHECK(vac_chamber.effective_h_w_m2_k() < 1e-3 * h_atm);

    // Reject out-of-range temperature (< -40 C) or pressure (< 1e-4 Pa)
    CHECK_FALSE(chamber.set_setpoint(200.0, 101325.0).is_ok());
    CHECK_FALSE(chamber.set_setpoint(298.15, 1e-6).is_ok());

    // Couple to 3D ThermalSolver boundary
    ThermalGridSpec3D grid;
    grid.nx = 5; grid.ny = 3; grid.nz = 3;
    grid.initial_temp_k = 298.15;
    ThermalSolver3D t_solver(grid);
    vac_chamber.apply_boundary_to_solver(t_solver);
    REQUIRE(t_solver.advance(2.0, 0.5).is_ok());
    CHECK(t_solver.temperature(0, 1, 1) > 397.0);
}

TEST_CASE("ENV-02-001 Wind tunnel fixture NACA 0012 lift/drag symmetry and NASA polar reference") {
    WindTunnelConfig cfg;
    cfg.nx = 40;
    cfg.ny = 20;
    cfg.dx_m = 0.05;
    cfg.inlet_velocity_m_s = 4.0;
    cfg.fluid_density_kg_m3 = 1.225;
    cfg.kinematic_viscosity_m2_s = 0.02;

    // 1. Symmetric NACA 0012 at 0 deg AoA -> near-zero lift, positive drag
    WindTunnelFixture wt0(cfg);
    wt0.place_naca0012_airfoil(0.85, 0.50, 0.70, 0.0);
    auto f0 = wt0.advance_and_measure(25, 0.008);
    REQUIRE(f0.is_ok());
    CHECK(f0.value().drag_force_n > 0.0);
    CHECK(std::abs(f0.value().lift_force_n) < 0.25 * std::max(1e-6, std::abs(f0.value().drag_force_n)) + 0.05);
    CHECK(f0.value().max_divergence < 1e-3);

    // 2. Positive angle of attack (+8 deg) generates positive lift; -8 deg generates negative lift
    WindTunnelFixture wt_pos(cfg);
    wt_pos.place_naca0012_airfoil(0.85, 0.50, 0.70, 8.0);
    auto f_pos = wt_pos.advance_and_measure(25, 0.008);
    REQUIRE(f_pos.is_ok());

    WindTunnelFixture wt_neg(cfg);
    wt_neg.place_naca0012_airfoil(0.85, 0.50, 0.70, -8.0);
    auto f_neg = wt_neg.advance_and_measure(25, 0.008);
    REQUIRE(f_neg.is_ok());

    CHECK(f_pos.value().lift_force_n > 0.0);
    CHECK(f_neg.value().lift_force_n < 0.0);
    CHECK(f_pos.value().lift_force_n > f_neg.value().lift_force_n);

    // 3. NASA NACA 0012 handbook polar curve checks: CL(0) = 0, CL(8 deg) ~ 0.84, stall drop past 15 deg
    CHECK(std::abs(WindTunnelFixture::naca0012_reference_cl(0.0)) < 1e-9);
    const double cl_8 = WindTunnelFixture::naca0012_reference_cl(8.0);
    CHECK(cl_8 > 0.75);
    CHECK(cl_8 < 0.95);
    CHECK(WindTunnelFixture::naca0012_reference_cl(22.0) < WindTunnelFixture::naca0012_reference_cl(14.0));
    CHECK(WindTunnelFixture::naca0012_reference_cd(10.0) > WindTunnelFixture::naca0012_reference_cd(0.0));
}
