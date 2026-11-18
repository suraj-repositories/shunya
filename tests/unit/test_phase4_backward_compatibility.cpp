#include "tests/test_framework.hpp"

#include "shunya/eng/engineering_pipeline.hpp"

#include <filesystem>

using namespace shunya::core;
using namespace shunya::core::domain;
using namespace shunya::eng;

// =============================================================================
// PH4-13.02.004: Backward Compatibility for Legacy Phase 3 Projects (.shunya3.json)
// =============================================================================

TEST_CASE("PH4-13.02.004: Load Legacy Phase 3 Project (.shunya3.json) & Verify Execution Without Data Loss") {
    // 1. Locate and load legacy project file
    const auto legacy_path = std::filesystem::path(SHUNYA_SOURCE_DIR) / "tests" / "data" / "legacy_v3_demo.shunya3.json";
    REQUIRE(std::filesystem::exists(legacy_path));

    EngineeringWorkspaceState ws;
    auto load_res = ws.load_from_file(legacy_path);
    REQUIRE(load_res.is_ok());

    // 2. Verify all legacy domain entities exist
    CHECK(ws.material_registry().material_count() >= 5);
    CHECK(ws.object_manager().count() >= 2);
    CHECK(ws.component_library().count() >= 1);
    CHECK(!ws.circuits().empty());

    // Verify key legacy materials exist
    CHECK(ws.material_registry().has_material("PP/PE Porous Separator"));

    // 3. Verify active circuit document and run DC simulation
    const auto& circuit_doc = ws.active_circuit_doc();
    CHECK(!circuit_doc.components().empty());

    auto dc_res = CircuitSimulationRunner::run_dc(circuit_doc);
    REQUIRE(dc_res.is_ok());
    CHECK(dc_res.value().converged);

    // 4. Run transient simulation on legacy circuit
    auto tr_res = CircuitSimulationRunner::run_transient(
        circuit_doc, 0.05, 0.001,
        &ws.component_library(), ws.environment().temperature_k);
    REQUIRE(tr_res.is_ok());
    CHECK(tr_res.value().success);
    CHECK(!tr_res.value().time_s.empty());
    CHECK(!tr_res.value().node_voltages.empty());
}
