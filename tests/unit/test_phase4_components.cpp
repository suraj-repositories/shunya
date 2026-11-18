#include "tests/test_framework.hpp"

#include "shunya/ecad3d/component_manifest.hpp"
#include "shunya/lib30/passive_components.hpp"
#include "shunya/lib30/diode_components.hpp"
#include "shunya/lib30/transistor_components.hpp"
#include "shunya/lib30/ic_actuator_components.hpp"
#include "shunya/lib30/electromechanical_components.hpp"
#include "shunya/lib30/power_sources_components.hpp"

using namespace shunya::ecad3d;
using namespace shunya::lib30;

// =============================================================================
// PH4-13.01.002: Phase 4 3D Electronic Components & Dual 2D Symbols Verification
// =============================================================================

TEST_CASE("PH4-13.01.002-01: Passive Components 3D Manifests & 2D Symbol Bindings") {
    // 1. Resistor
    auto res = PassiveComponentsFactory::create_resistor(4700.0, 1.0, 0.5);
    CHECK(res.name.find("Resistor") != std::string::npos);
    CHECK(res.terminals.size() == 2u);
    CHECK(res.symbol_2d.pins.size() == 2u);
    CHECK(res.pin_mappings.size() == 2u);
    CHECK(res.specs.nominal_primary_value == 4700.0);
    CHECK(res.is_qualified);

    // 2. Ceramic Capacitor
    auto cap = PassiveComponentsFactory::create_ceramic_capacitor(10.0e-6, 50.0);
    CHECK(cap.terminals.size() == 2u);
    CHECK(cap.specs.nominal_primary_value == 10.0e-6);

    // 3. Electrolytic Capacitor (polarized)
    auto elec = PassiveComponentsFactory::create_electrolytic_capacitor(100.0e-6, 35.0);
    CHECK(elec.terminals.size() == 2u);
    CHECK(elec.pin_mappings.size() == 2u);
}

TEST_CASE("PH4-13.01.002-02: Diode & Transistor Multi-Region Terminals") {
    // 1. Rectifier Diode (Anode & Cathode)
    auto diode = DiodeComponentsFactory::create_pn_diode(0.70, 1000.0, 1.0);
    CHECK(diode.terminals.size() == 2u);
    CHECK(diode.symbol_2d.pins.size() == 2u);
    CHECK(diode.specs.max_voltage_v >= 100.0);

    // 2. LED
    auto led = DiodeComponentsFactory::create_led("Red", 630.0, 2.0, 0.020);
    CHECK(led.terminals.size() == 2u);

    // 3. NPN Bipolar Junction Transistor (Collector, Base, Emitter)
    auto npn = TransistorComponentsFactory::create_npn_bjt(200.0, 0.5, 40.0);
    CHECK(npn.terminals.size() == 3u);
    CHECK(npn.symbol_2d.pins.size() == 3u);
    CHECK(npn.pin_mappings.size() == 3u);

    // 4. N-Channel Power MOSFET (Drain, Gate, Source)
    auto mosfet = TransistorComponentsFactory::create_n_mosfet(2.5, 0.028, 30.0, 60.0);
    CHECK(mosfet.terminals.size() == 3u);
    CHECK(mosfet.pin_mappings.size() == 3u);
}

TEST_CASE("PH4-13.01.002-03: IC, Actuators & Electromechanical Components") {
    // 1. Operational Amplifier LM741
    auto opamp = ActuatorsTransducersFactory::create_opamp();
    CHECK(opamp.terminals.size() >= 5u);
    CHECK(opamp.symbol_2d.pins.size() >= 5u);

    // 2. Piezoelectric Buzzer
    auto buzzer = ActuatorsTransducersFactory::create_piezo_buzzer();
    CHECK(buzzer.terminals.size() == 2u);

    // 3. SPST Toggle Switch
    auto switch_spst = ElectromechanicalComponentsFactory::create_toggle_switch();
    CHECK(switch_spst.terminals.size() == 2u);

    // 4. DC Motor
    auto motor = ActuatorsTransducersFactory::create_dc_motor();
    CHECK(motor.terminals.size() == 2u);
}

TEST_CASE("PH4-13.01.002-04: Power Sources & Component Manifest Serialization") {
    // 1. Lithium-Ion Battery Cell
    auto cell = PowerSourcesProtectionFactory::create_battery_cell(3.7, 2.6);
    CHECK(cell.terminals.size() == 2u);
    CHECK(cell.specs.nominal_primary_value == 3.7);

    // 2. Component Manifest Serialization Round-trip
    std::string json = ComponentManifestSerializer::to_json(cell, true);
    CHECK(!json.empty());
    CHECK(json.find("shunyacomp") != std::string::npos);

    auto restored = ComponentManifestSerializer::from_json(json);
    REQUIRE(restored.is_ok());
    CHECK(restored.value().name == cell.name);
    CHECK(restored.value().specs.nominal_primary_value == cell.specs.nominal_primary_value);
    CHECK(restored.value().specs.max_voltage_v == cell.specs.max_voltage_v);
}
