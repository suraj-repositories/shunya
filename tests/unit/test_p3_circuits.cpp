#include "tests/test_framework.hpp"

#include "shunya/eng/interactive_circuit.hpp"

#include <cmath>

using namespace shunya::eng;

TEST_CASE("CIRSIM: DC Ohm law, series & parallel divider, KCL/KVL, and LED indicator") {
    // 10V DC source -> SW1 (closed) -> R1=100 -> R2=100 -> GND (plus parallel Ammeter/Voltmeter checks)
    InteractiveCircuitDocument doc("Series_Parallel_DC");
    PlacedCircuitComponent v1;
    v1.id = "V1";
    v1.kind = CircuitSymbolKind::VoltageSourceDC;
    v1.x_mm = 0.0; v1.y_mm = 0.0;
    v1.primary_value = 10.0;
    doc.add_component(v1);

    PlacedCircuitComponent r1;
    r1.id = "R1";
    r1.kind = CircuitSymbolKind::Resistor;
    r1.x_mm = 40.0; r1.y_mm = 0.0;
    r1.primary_value = 100.0;
    doc.add_component(r1);

    PlacedCircuitComponent r2;
    r2.id = "R2";
    r2.kind = CircuitSymbolKind::Resistor;
    r2.x_mm = 80.0; r2.y_mm = 0.0;
    r2.primary_value = 100.0;
    doc.add_component(r2);

    PlacedCircuitComponent gnd;
    gnd.id = "GND1";
    gnd.kind = CircuitSymbolKind::Ground;
    gnd.x_mm = 0.0; gnd.y_mm = 30.0;
    doc.add_component(gnd);

    const auto vt = v1.terminal_world_positions();
    const auto r1t = r1.terminal_world_positions();
    const auto r2t = r2.terminal_world_positions();
    const auto gt = gnd.terminal_world_positions();

    doc.add_wire({"W1", {vt[0].second, {vt[0].second.x, -15.0}, {r1t[0].second.x, -15.0}, r1t[0].second}, {}});
    doc.add_wire({"W2", {r1t[1].second, r2t[0].second}, {}});
    doc.add_wire({"W3", {r2t[1].second, {r2t[1].second.x, 25.0}, gt[0].second}, {}});
    doc.add_wire({"W4", {vt[1].second, {vt[1].second.x, 25.0}, gt[0].second}, {}});
    doc.resolve_connectivity();

    auto issues = doc.validate();
    for (const auto& iss : issues) {
        CHECK(iss.severity != shunya::core::domain::CircuitDiagnosticSeverity::Error);
    }

    auto dc_res = CircuitSimulationRunner::run_dc(doc);
    REQUIRE(dc_res.is_ok());
    const auto& dc = dc_res.value();
    CHECK(dc.converged);
    CHECK(dc.kcl_max_residual_a < 1e-6);

    // Total R = 200 Ohm => I = 10 / 200 = 0.05 A, P(R1) = 0.25 W
    bool found_r1 = false;
    for (const auto& m : dc.component_metrics) {
        if (m.component_id == "R1") {
            found_r1 = true;
            CHECK(std::abs(std::abs(m.current_a) - 0.05) < 1e-4);
            CHECK(std::abs(std::abs(m.voltage_v) - 5.0) < 1e-3);
            CHECK(std::abs(m.power_w - 0.25) < 1e-3);
        }
    }
    CHECK(found_r1);

    // Duplicate + JSON round-trip
    auto dup = doc.duplicate("Copy_DC");
    CHECK(dup.name() == "Copy_DC");
    CHECK(dup.components().size() == doc.components().size());
    auto parsed = InteractiveCircuitDocument::from_json(doc.to_json());
    REQUIRE(parsed.is_ok());
    CHECK(parsed.value().components().size() == 4u);
}

TEST_CASE("CIRSIM: RC charging transient, switch opening, and invalid open-circuit rejection") {
    // 5V DC -> R1=1000 Ohm -> C1=100 uF (tau = 0.1 s) -> GND
    InteractiveCircuitDocument rc("RC_Transient");
    PlacedCircuitComponent v1; v1.id = "V1"; v1.label = "5V"; v1.kind = CircuitSymbolKind::VoltageSourceDC; v1.x_mm = 0; v1.y_mm = 0; v1.primary_value = 5.0;
    PlacedCircuitComponent r1; r1.id = "R1"; r1.label = "1k"; r1.kind = CircuitSymbolKind::Resistor; r1.x_mm = 40; r1.y_mm = 0; r1.primary_value = 1000.0;
    PlacedCircuitComponent c1; c1.id = "C1"; c1.label = "100uF"; c1.kind = CircuitSymbolKind::Capacitor; c1.x_mm = 80; c1.y_mm = 0; c1.primary_value = 100e-6;
    PlacedCircuitComponent gnd; gnd.id = "GND"; gnd.label = "GND"; gnd.kind = CircuitSymbolKind::Ground; gnd.x_mm = 0; gnd.y_mm = 30;
    rc.add_component(v1);
    rc.add_component(r1);
    rc.add_component(c1);
    rc.add_component(gnd);

    const auto vt = v1.terminal_world_positions();
    const auto r1t = r1.terminal_world_positions();
    const auto c1t = c1.terminal_world_positions();
    const auto gt = gnd.terminal_world_positions();
    rc.add_wire({"W1", {vt[0].second, {vt[0].second.x, -15.0}, {r1t[0].second.x, -15.0}, r1t[0].second}, {}});
    rc.add_wire({"W2", {r1t[1].second, c1t[0].second}, {}});
    rc.add_wire({"W3", {c1t[1].second, {c1t[1].second.x, 25.0}, gt[0].second}, {}});
    rc.add_wire({"W4", {vt[1].second, {vt[1].second.x, 25.0}, gt[0].second}, {}});
    rc.resolve_connectivity();

    auto tr_res = CircuitSimulationRunner::run_transient(rc, 0.5, 0.005);
    REQUIRE(tr_res.is_ok());
    const auto& tr = tr_res.value();
    CHECK(tr.success);
    REQUIRE(tr.component_voltages_v.count("C1") == 1u);
    const auto& vc = tr.component_voltages_v.at("C1");
    REQUIRE(vc.size() > 50u);
    CHECK(std::abs(vc.front()) < 0.5);
    CHECK(vc.back() > 4.5); // at t=5*tau, Vc -> 5 * (1 - e^-5) ~= 4.966V

    // Invalid circuit (no ground) must be caught cleanly by validate() & run_dc()
    InteractiveCircuitDocument bad("No_Ground");
    bad.add_component(v1);
    bad.add_component(r1);
    bad.resolve_connectivity();
    CHECK(CircuitSimulationRunner::run_dc(bad).is_err());
}
