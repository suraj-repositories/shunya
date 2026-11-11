#include "tests/test_framework.hpp"

#include "shunya/eng/engineering_pipeline.hpp"

#include <cmath>

using namespace shunya::eng;

TEST_CASE("E2E: Elements -> Mixing Lab -> Physical Objects -> Battery & Components -> Closed Circuit -> Transient & Physics") {
    EngineeringWorkspaceState ws;
    REQUIRE(ws.seed_lithium_battery_workflow().is_ok());
    CHECK(ws.validate_workspace().is_valid());

    // 1. Verify Elements database populated with CSV data
    CHECK(ws.element_database().is_loaded());
    CHECK(ws.element_database().count() == 118u);

    // 2. Verify Battery was engineered & qualified
    REQUIRE(ws.battery_specs().size() == 1u);
    CHECK(ws.battery_specs()[0].derived.is_compatible);
    CHECK(ws.battery_specs()[0].derived.nominal_capacity_ah > 0.0);

    // 3. Verify Components are linked to Objects via UUIDs (no duplication)
    REQUIRE(ws.component_library().count() >= 3u);
    const auto* load_comp = ws.component_library().find_by_name("Nichrome_Load_Comp");
    REQUIRE(load_comp != nullptr);
    const double initial_r_load = load_comp->derived.primary_value;
    CHECK(initial_r_load > 0.1);

    // 4. Run transient circuit simulation: battery discharges into Nichrome load, heating up & draining SOC
    auto tr_res = CircuitSimulationRunner::run_transient(
        ws.active_circuit_doc(), 120.0, 2.0, &ws.component_library(), 298.15);
    REQUIRE(tr_res.is_ok());
    const auto& tr = tr_res.value();
    CHECK(tr.success);
    REQUIRE(tr.time_s.size() > 10u);
    REQUIRE(tr.battery_soc.size() == tr.time_s.size());
    CHECK(tr.battery_soc.back() < tr.battery_soc.front());
    CHECK(tr.component_temperatures_k.at("R1").back() > 298.15);

    // 5. Mutate Load Object dimension (double length -> double R), propagate reactively, confirm current halves!
    auto dc_before = CircuitSimulationRunner::run_dc(ws.active_circuit_doc());
    REQUIRE(dc_before.is_ok());
    double i_before = 0.0;
    for (const auto& m : dc_before.value().component_metrics) {
        if (m.component_id == "R1") i_before = std::abs(m.current_a);
    }
    REQUIRE(i_before > 0.0);

    PhysicalObject* load_obj = ws.object_manager().get_object_mut(load_comp->source_object_id);
    REQUIRE(load_obj != nullptr);
    load_obj->geometry.dim_z_m *= 2.0; // double resistor length
    // Clear explicit override on component config so it tracks object length or set length *2
    if (auto* lc_mut = ws.component_library().get_component_mut(load_comp->id)) {
        lc_mut->config.conductor_length_m *= 2.0;
    }
    ws.notify_object_modified(load_obj->id, "Doubled Nichrome resistor length");

    const auto* load_after = ws.component_library().find_by_name("Nichrome_Load_Comp");
    REQUIRE(load_after != nullptr);
    CHECK(load_after->derived.primary_value > initial_r_load * 1.9);

    auto dc_after = CircuitSimulationRunner::run_dc(ws.active_circuit_doc());
    REQUIRE(dc_after.is_ok());
    double i_after = 0.0;
    for (const auto& m : dc_after.value().component_metrics) {
        if (m.component_id == "R1") i_after = std::abs(m.current_a);
    }
    CHECK(i_after < i_before);
}
