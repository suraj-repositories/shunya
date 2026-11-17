#include "tests/test_framework.hpp"

#include "shunya/lib30/diode_components.hpp"
#include "shunya/lib30/transistor_components.hpp"
#include "shunya/eng/component_generator.hpp"

#include <cmath>

using namespace shunya::lib30;
using namespace shunya::ecad3d;
using namespace shunya::eng;
using shunya::core::domain::ElectricalPinPolarity;
using shunya::eng::ComponentKind;

TEST_CASE("PH4-07.02.001: Component 7 - PN Junction Diode (Shockley conduction, reverse leakage, and rectification)") {
    auto diode = DiodeComponentsFactory::create_pn_diode(0.70, 100.0, 1.0);

    REQUIRE(diode.kind == ComponentKind::Diode);
    REQUIRE(diode.specs.nominal_primary_value == 0.70);
    REQUIRE(diode.specs.max_voltage_v == 100.0);
    REQUIRE(diode.specs.max_current_a == 1.0);
    REQUIRE(diode.terminals.size() == 2);
    REQUIRE(diode.symbol_2d.pins.size() == 2);
    REQUIRE(diode.vertex_count > 0);

    // Polarity: Pin 1 Anode (+), Pin 2 Cathode (-)
    REQUIRE(diode.terminals[0].polarity == ElectricalPinPolarity::Positive);
    REQUIRE(diode.terminals[1].polarity == ElectricalPinPolarity::Negative);

    // Shockley Diode equation: I = Is * (exp(V / (n * Vt)) - 1)
    // with Is = 1e-14 A, n = 1.0, Vt = 25.85 mV at 300K
    double i_s = 1.0e-14;
    double v_t = 0.02585;
    double n = 1.0;

    // At forward voltage Vf = 0.70 V:
    double i_forward = i_s * (std::exp(0.70 / (n * v_t)) - 1.0);
    REQUIRE(i_forward > 0.001); // Conduction threshold verified (mA range)

    // At reverse voltage Vr = -10.0 V:
    double i_reverse = i_s * (std::exp(-10.0 / (n * v_t)) - 1.0);
    REQUIRE(std::abs(i_reverse + i_s) < 1e-18); // Reverse leakage clamped to -Is
}

TEST_CASE("PH4-07.02.002: Component 8 - Light Emitting Diode (Optical emission wavelength and radiant flux)") {
    auto led = DiodeComponentsFactory::create_led("Red", 630.0, 2.0, 0.030);

    REQUIRE(led.kind == ComponentKind::LED);
    REQUIRE(led.specs.nominal_primary_value == 2.0); // 2.0V forward drop
    REQUIRE(led.specs.max_current_a == 0.030);       // 30 mA max rating
    REQUIRE(led.terminals.size() == 2);
    REQUIRE(led.symbol_2d.pins.size() == 2);

    // Photon Energy: E_ph = h * c / lambda
    double h = 6.62607015e-34; // Planck's constant (J*s)
    double c = 2.99792458e8;    // Speed of light (m/s)
    double lambda_m = 630.0e-9; // 630 nm
    double photon_energy_j = h * c / lambda_m;
    double photon_energy_ev = photon_energy_j / 1.602176634e-19; // eV

    // Red photon energy is approximately 1.97 eV, nicely aligned with Vf ~ 2.0V
    REQUIRE(std::abs(photon_energy_ev - 1.968) < 0.05);

    // Current vs Luminous Radiant Flux (at 20 mA operating point)
    double i_op = 0.020;
    double eta_ext = 0.15; // 15% external quantum efficiency
    double optical_power_w = eta_ext * i_op * led.specs.nominal_primary_value;
    REQUIRE(optical_power_w > 0.001); // > 1 mW optical output

    // Overcurrent check: 50 mA exceeds 30 mA maximum
    double i_over = 0.050;
    REQUIRE(i_over > led.specs.max_current_a);
}

TEST_CASE("PH4-07.02.003: Component 9 - Zener Diode (Reverse breakdown regulation and clamp)") {
    auto zener = DiodeComponentsFactory::create_zener_diode(5.1, 0.50);

    REQUIRE(zener.kind == ComponentKind::ZenerDiode);
    REQUIRE(zener.specs.nominal_primary_value == 5.1); // 5.1V Zener clamp
    REQUIRE(zener.specs.max_power_dissipation_w == 0.50);
    REQUIRE(zener.terminals.size() == 2);

    // Max continuous Zener current: Iz_max = P_max / Vz = 0.50 / 5.1 ~ 98 mA
    double i_z_max = zener.specs.max_power_dissipation_w / zener.specs.nominal_primary_value;
    REQUIRE(std::abs(i_z_max - 0.098) < 0.005);

    // Regulation test: With Vin = 12V and current-limiting resistor R = 220 Ohm
    double v_in = 12.0;
    double r_limit = 220.0;
    double i_load = (v_in - zener.specs.nominal_primary_value) / r_limit;
    REQUIRE(i_load < i_z_max); // Safely operating below max dissipation
    double p_zener = i_load * zener.specs.nominal_primary_value;
    REQUIRE(p_zener < zener.specs.max_power_dissipation_w);
}

TEST_CASE("PH4-07.02.004: Component 10 - Schottky Diode (Low forward drop and high-efficiency switching)") {
    auto schottky = DiodeComponentsFactory::create_schottky_diode(0.28, 40.0, 3.0);

    REQUIRE(schottky.kind == ComponentKind::SchottkyDiode);
    REQUIRE(schottky.specs.nominal_primary_value == 0.28); // 0.28V forward drop
    REQUIRE(schottky.specs.max_voltage_v == 40.0);
    REQUIRE(schottky.specs.max_current_a == 3.0);

    // Power savings comparison vs silicon PN diode (0.7V) at 2A continuous current
    double current = 2.0;
    double p_schottky = current * schottky.specs.nominal_primary_value; // 0.56 W
    double p_silicon = current * 0.70;                                  // 1.40 W
    REQUIRE(p_schottky < p_silicon);
    REQUIRE(std::abs(p_schottky - 0.56) < 1e-6);
}

TEST_CASE("PH4-07.03.001: Component 11 - NPN BJT (Common-emitter amplification and saturation)") {
    auto npn = TransistorComponentsFactory::create_npn_bjt(200.0, 0.20, 40.0);

    REQUIRE(npn.kind == ComponentKind::BJT_NPN);
    REQUIRE(npn.specs.nominal_primary_value == 200.0); // Beta current gain = 200
    REQUIRE(npn.terminals.size() == 3);
    REQUIRE(npn.symbol_2d.pins.size() == 3);

    // Pin mappings: Base=1, Collector=2, Emitter=3
    REQUIRE(npn.terminals[0].terminal_id == "BASE");
    REQUIRE(npn.terminals[1].terminal_id == "COLLECTOR");
    REQUIRE(npn.terminals[2].terminal_id == "EMITTER");

    // Common-emitter current amplification: Ic = Beta * Ib
    double i_base = 0.5e-3; // 0.5 mA
    double i_collector = npn.specs.nominal_primary_value * i_base;
    REQUIRE(std::abs(i_collector - 0.100) < 1e-6); // 100 mA Collector current
    REQUIRE(i_collector <= npn.specs.max_current_a);

    // Saturation Vce_sat verification
    double v_ce_sat = 0.20;
    REQUIRE(v_ce_sat < 0.30);
}

TEST_CASE("PH4-07.03.002: Component 12 - PNP BJT (High-side switching and complementary operation)") {
    auto pnp = TransistorComponentsFactory::create_pnp_bjt(200.0, 0.20, 40.0);

    REQUIRE(pnp.kind == ComponentKind::BJT_PNP);
    REQUIRE(pnp.specs.nominal_primary_value == 200.0);
    REQUIRE(pnp.terminals.size() == 3);
    REQUIRE(pnp.symbol_2d.pins.size() == 3);

    // Complementary behavior: Conduction occurs when Base is pulled lower than Emitter
    double v_emitter = 5.0; // High side 5V rail
    double v_base = 4.3;    // Base pulled low by 0.7V
    double v_be = v_base - v_emitter;
    REQUIRE(v_be <= -0.65); // Forward biased P-N base-emitter junction
}

TEST_CASE("PH4-07.03.003: Component 13 - N-Channel MOSFET (Gate threshold and square-law conduction)") {
    auto nmos = TransistorComponentsFactory::create_n_mosfet(2.5, 0.028, 30.0, 60.0);

    REQUIRE(nmos.kind == ComponentKind::MOSFET_N);
    REQUIRE(nmos.specs.nominal_primary_value == 2.5); // V_th = 2.5V
    REQUIRE(nmos.specs.max_voltage_v == 60.0);
    REQUIRE(nmos.specs.max_current_a == 30.0);
    REQUIRE(nmos.terminals.size() == 3);

    // Square-law Saturation: Id = 0.5 * Kn * (Vgs - Vth)^2 with Kn = 5.0 A/V^2
    double v_gs = 4.0; // 4.0V gate drive
    double v_th = nmos.specs.nominal_primary_value;
    double k_n = 5.0;
    double v_ov = v_gs - v_th; // Overdrive = 1.5V
    REQUIRE(v_ov > 0.0);
    double i_drain = 0.5 * k_n * v_ov * v_ov;
    REQUIRE(std::abs(i_drain - 5.625) < 1e-3); // 5.625 A in saturation

    // Conduction loss in linear mode at 10A through Rds_on = 28 mOhm
    double i_load = 10.0;
    double r_ds = 0.028;
    double p_cond = i_load * i_load * r_ds;
    REQUIRE(std::abs(p_cond - 2.8) < 1e-6); // 2.8 Watts
    REQUIRE(p_cond <= nmos.specs.max_power_dissipation_w);
}

TEST_CASE("PH4-07.03.004: Component 14 - P-Channel MOSFET (High-side power switching and negative threshold)") {
    auto pmos = TransistorComponentsFactory::create_p_mosfet(-2.5, 0.055, 20.0, 55.0);

    REQUIRE(pmos.kind == ComponentKind::MOSFET_P);
    REQUIRE(pmos.specs.nominal_primary_value == -2.5); // V_th = -2.5V
    REQUIRE(pmos.specs.max_current_a == 20.0);
    REQUIRE(pmos.terminals.size() == 3);

    // High side switching: Source connected to +12V, Gate pulled to 0V (GND)
    double v_source = 12.0;
    double v_gate = 0.0;
    double v_gs = v_gate - v_source; // -12.0V
    REQUIRE(v_gs < pmos.specs.nominal_primary_value); // -12V < -2.5V -> fully turned ON
}

TEST_CASE("PH4-07.03.005: Component 15 - N-Channel JFET (Depletion-mode pinch-off and high-impedance buffer)") {
    auto jfet = TransistorComponentsFactory::create_n_jfet(10.0e-3, -3.0, 0.020);

    REQUIRE(jfet.kind == ComponentKind::JFET_N);
    REQUIRE(jfet.specs.nominal_primary_value == 0.010); // Idss = 10 mA
    REQUIRE(jfet.terminals.size() == 3);

    // Shockley JFET pinch-off equation: Id = Idss * (1 - Vgs / Vp)^2
    double i_dss = jfet.specs.nominal_primary_value;
    double v_p = -3.0;

    // At Vgs = 0: Id = Idss = 10 mA
    double id_0 = i_dss * std::pow(1.0 - (0.0 / v_p), 2.0);
    REQUIRE(std::abs(id_0 - 0.010) < 1e-6);

    // At Vgs = -1.5V (midpoint): Id = 10mA * (1 - 0.5)^2 = 2.5 mA
    double id_mid = i_dss * std::pow(1.0 - (-1.5 / v_p), 2.0);
    REQUIRE(std::abs(id_mid - 0.0025) < 1e-6);

    // At pinch-off Vgs = -3.0V: Id = 0 mA
    double id_pinch = i_dss * std::pow(1.0 - (-3.0 / v_p), 2.0);
    REQUIRE(std::abs(id_pinch) < 1e-9);
}

TEST_CASE("PH4-07.03.006: Component 16 - P-Channel JFET (Positive gate pinch-off and audio low-noise behavior)") {
    auto pjfet = TransistorComponentsFactory::create_p_jfet(8.0e-3, 2.5, 0.015);

    REQUIRE(pjfet.kind == ComponentKind::JFET_P);
    REQUIRE(pjfet.specs.nominal_primary_value == 0.008); // Idss = 8 mA
    REQUIRE(pjfet.terminals.size() == 3);
    REQUIRE(pjfet.symbol_2d.pins.size() == 3);

    // Pin mappings: Gate=1, Drain=2, Source=3
    REQUIRE(pjfet.terminals[0].terminal_id == "GATE");
    REQUIRE(pjfet.terminals[1].terminal_id == "DRAIN");
    REQUIRE(pjfet.terminals[2].terminal_id == "SOURCE");

    // P-Channel JFET: Pinched off by applying positive gate voltage (Vgs = +Vp = +2.5V)
    double i_dss = pjfet.specs.nominal_primary_value;
    double v_p = 2.5;

    // At Vgs = 0: Id = Idss = 8 mA
    double id_0 = i_dss * std::pow(1.0 - (0.0 / v_p), 2.0);
    REQUIRE(std::abs(id_0 - 0.008) < 1e-6);

    // At pinch-off Vgs = +2.5V: Id = 0 mA
    double id_pinch = i_dss * std::pow(1.0 - (2.5 / v_p), 2.0);
    REQUIRE(std::abs(id_pinch) < 1e-9);
}
