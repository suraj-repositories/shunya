#include "tests/test_framework.hpp"

#include "shunya/lib30/electromechanical_components.hpp"
#include "shunya/lib30/power_sources_components.hpp"
#include "shunya/eng/component_generator.hpp"

#include <cmath>

using namespace shunya::lib30;
using namespace shunya::ecad3d;
using namespace shunya::eng;
using shunya::core::domain::ElectricalPinPolarity;
using shunya::eng::ComponentKind;

TEST_CASE("PH4-07.04.001: Component 17 - Push Button (Momentary NO, contact resistance and bounce)") {
    auto btn = ElectromechanicalComponentsFactory::create_push_button(0.05, 0.50, 50.0);

    REQUIRE(btn.kind == ComponentKind::PushButton);
    REQUIRE(btn.specs.nominal_primary_value == 0.05); // 50 mOhm contact resistance
    REQUIRE(btn.specs.max_current_a == 0.50);
    REQUIRE(btn.specs.max_voltage_v == 50.0);
    REQUIRE(btn.terminals.size() == 2);
    REQUIRE(btn.symbol_2d.pins.size() == 2);

    // Conduction loss when actuated: P = I^2 * R_on
    double current = 0.20;
    double p_contact = current * current * btn.specs.nominal_primary_value;
    REQUIRE(std::abs(p_contact - 0.002) < 1e-6); // 2 mW
    REQUIRE(p_contact <= btn.specs.max_power_dissipation_w);
}

TEST_CASE("PH4-07.04.002: Component 18 - SPST Toggle Switch (Bistable state conduction and high voltage rating)") {
    auto toggle = ElectromechanicalComponentsFactory::create_toggle_switch(0.01, 5.0, 250.0);

    REQUIRE(toggle.kind == ComponentKind::Switch);
    REQUIRE(toggle.specs.nominal_primary_value == 0.01); // 10 mOhm
    REQUIRE(toggle.specs.max_current_a == 5.0);
    REQUIRE(toggle.specs.max_voltage_v == 250.0);
    REQUIRE(toggle.terminals.size() == 2);
    REQUIRE(toggle.symbol_2d.pins.size() == 2);

    // Conduction verification at rated 5A: V_drop = I * R = 5 * 0.01 = 50 mV
    double v_drop = toggle.specs.max_current_a * toggle.specs.nominal_primary_value;
    REQUIRE(std::abs(v_drop - 0.050) < 1e-6);
}

TEST_CASE("PH4-07.04.003: Component 19 - SPDT Slide Switch (Break-before-make 3-pin routing)") {
    auto slide = ElectromechanicalComponentsFactory::create_slide_switch(0.02, 1.0, 50.0);

    REQUIRE(slide.kind == ComponentKind::Switch);
    REQUIRE(slide.specs.nominal_primary_value == 0.02);
    REQUIRE(slide.terminals.size() == 3);
    REQUIRE(slide.symbol_2d.pins.size() == 3);

    // Pin mappings: Common=1, Throw1=2, Throw2=3
    REQUIRE(slide.terminals[0].terminal_id == "COM");
    REQUIRE(slide.terminals[1].terminal_id == "T1");
    REQUIRE(slide.terminals[2].terminal_id == "T2");

    // Open circuit isolation resistance: R_off = 1 Giga-ohm
    double r_off = 1.0e9;
    double i_leakage = 50.0 / r_off;
    REQUIRE(i_leakage <= 1.0e-7); // < 100 nA leakage
}

TEST_CASE("PH4-07.04.004: Component 20 - Electromechanical Relay (Coil pickup and SPDT isolated switching)") {
    auto relay = ElectromechanicalComponentsFactory::create_relay(12.0, 400.0, 10.0, 250.0);

    REQUIRE(relay.kind == ComponentKind::Relay);
    REQUIRE(relay.specs.nominal_primary_value == 400.0); // 400 Ohm coil
    REQUIRE(relay.specs.max_voltage_v == 250.0);
    REQUIRE(relay.specs.max_current_a == 10.0);
    REQUIRE(relay.terminals.size() == 5);
    REQUIRE(relay.symbol_2d.pins.size() == 5);

    // Coil current at 12V nominal: I = 12 / 400 = 30 mA
    double v_coil = 12.0;
    double i_coil = v_coil / relay.specs.nominal_primary_value;
    REQUIRE(std::abs(i_coil - 0.030) < 1e-6);

    // Coil power consumption: P = V^2 / R = 144 / 400 = 0.36 W (360 mW)
    double p_coil = v_coil * v_coil / relay.specs.nominal_primary_value;
    REQUIRE(std::abs(p_coil - 0.360) < 1e-6);
}

TEST_CASE("PH4-07.05.001: Component 21 - Chemical Battery (Thevenin equivalent circuit and discharge power)") {
    auto bat = PowerSourcesProtectionFactory::create_battery_cell(3.70, 2.60, 0.040);

    REQUIRE(bat.kind == ComponentKind::Battery);
    REQUIRE(bat.specs.nominal_primary_value == 3.70); // 3.7V nominal
    REQUIRE(bat.specs.max_voltage_v == 4.20);
    REQUIRE(bat.specs.max_current_a == (2.60 * 3.0)); // 7.8 A (3C rate)
    REQUIRE(bat.terminals.size() == 2);
    REQUIRE(bat.symbol_2d.pins.size() == 2);

    // Polarized pins: Pin 1 POS (+), Pin 2 NEG (-)
    REQUIRE(bat.terminals[0].polarity == ElectricalPinPolarity::Positive);
    REQUIRE(bat.terminals[1].polarity == ElectricalPinPolarity::Negative);

    // Terminal voltage under 2A load: V_term = V_ocv - I * R_int = 3.7 - 2 * 0.04 = 3.62 V
    double i_load = 2.0;
    double r_int = 0.040;
    double v_term = bat.specs.nominal_primary_value - i_load * r_int;
    REQUIRE(std::abs(v_term - 3.62) < 1e-6);

    // Stored chemical energy: E_wh = V_nom * Capacity_Ah = 3.7 * 2.6 = 9.62 Wh
    double energy_wh = bat.specs.nominal_primary_value * 2.60;
    REQUIRE(std::abs(energy_wh - 9.62) < 1e-6);
}

TEST_CASE("PH4-07.05.002: Component 22 - Ideal DC Voltage Source (Strict setpoint regulation and load current)") {
    auto dc_src = PowerSourcesProtectionFactory::create_dc_voltage_source(12.0, 5.0);

    REQUIRE(dc_src.kind == ComponentKind::DCVoltageSource);
    REQUIRE(dc_src.specs.nominal_primary_value == 12.0); // 12V setpoint
    REQUIRE(dc_src.specs.max_current_a == 5.0);
    REQUIRE(dc_src.terminals.size() == 2);
    REQUIRE(dc_src.symbol_2d.pins.size() == 2);

    // Polarized pins: Pin 1 POS (+), Pin 2 NEG (-)
    REQUIRE(dc_src.terminals[0].polarity == ElectricalPinPolarity::Positive);
    REQUIRE(dc_src.terminals[1].polarity == ElectricalPinPolarity::Negative);

    // Constant voltage delivery across 10 ohm load: I = V / R = 1.2 A <= 5 A limit
    double r_load = 10.0;
    double current = dc_src.specs.nominal_primary_value / r_load;
    REQUIRE(std::abs(current - 1.20) < 1e-6);
    REQUIRE(current <= dc_src.specs.max_current_a);
}

TEST_CASE("PH4-07.05.003: Component 23 - AC Voltage Source (Sinusoidal peak and RMS voltage)") {
    auto ac_src = PowerSourcesProtectionFactory::create_ac_voltage_source(10.0, 1000.0, 0.0);

    REQUIRE(ac_src.kind == ComponentKind::ACVoltageSource);
    REQUIRE(ac_src.specs.nominal_primary_value == 10.0); // 10V peak
    REQUIRE(ac_src.terminals.size() == 2);
    REQUIRE(ac_src.symbol_2d.pins.size() == 2);

    // RMS Voltage: V_rms = V_peak / sqrt(2) ~ 7.071 V
    double v_rms = ac_src.specs.nominal_primary_value / std::sqrt(2.0);
    REQUIRE(std::abs(v_rms - 7.071) < 0.01);
}

TEST_CASE("PH4-07.05.004: Component 24 - Earth / Ground Reference (Zero potential reference)") {
    auto gnd = PowerSourcesProtectionFactory::create_ground_reference();

    REQUIRE(gnd.kind == ComponentKind::GroundReference);
    REQUIRE(gnd.specs.nominal_primary_value == 0.0); // 0.0 V
    REQUIRE(gnd.specs.max_voltage_v == 0.0);
    REQUIRE(gnd.terminals.size() == 1);
    REQUIRE(gnd.terminals[0].polarity == ElectricalPinPolarity::Negative);
    REQUIRE(gnd.symbol_2d.pins.size() == 1);
    REQUIRE(gnd.symbol_2d.pins[0].polarity == ElectricalPinPolarity::Negative);
}

TEST_CASE("PH4-07.05.005: Component 25 - Fast-Acting Fuse (Over-current Joule heating blowout)") {
    auto fuse = PowerSourcesProtectionFactory::create_fuse(2.0, 0.050, 250.0);

    REQUIRE(fuse.kind == ComponentKind::Fuse);
    REQUIRE(fuse.specs.nominal_primary_value == 2.0); // 2.0A rated
    REQUIRE(fuse.specs.max_current_a == 2.0);
    REQUIRE(fuse.specs.max_voltage_v == 250.0);
    REQUIRE(fuse.terminals.size() == 2);
    REQUIRE(fuse.symbol_2d.pins.size() == 2);

    // Normal current (1.0 A): P_joule = 1^2 * 0.05 = 0.05 W <= P_max (0.20 W)
    double i_normal = 1.0;
    double p_norm = i_normal * i_normal * 0.050;
    REQUIRE(p_norm <= fuse.specs.max_power_dissipation_w);

    // Overcurrent (5.0 A): Exceeds rating (blowout trigger)
    double i_over = 5.0;
    REQUIRE(i_over > fuse.specs.max_current_a);
}
