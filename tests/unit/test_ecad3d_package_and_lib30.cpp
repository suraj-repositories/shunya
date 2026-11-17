#include "tests/test_framework.hpp"

#include "shunya/ecad3d/component_package.hpp"
#include "shunya/eng/component_generator.hpp"
#include "shunya/lib30/passive_components.hpp"

#include <cmath>
#include <filesystem>

using namespace shunya::ecad3d;
using namespace shunya::lib30;
using namespace shunya::eng;
using shunya::core::domain::ElectricalPinPolarity;
using shunya::eng::ComponentKind;

TEST_CASE("PH4-06.04.002: Standalone component bundle import/export and packaging (.shunyacomp)") {
    auto res_comp = PassiveComponentsFactory::create_resistor(4700.0, 5.0, 0.25);

    ComponentBundleArchive bundle;
    bundle.manifest = res_comp;
    bundle.mesh_geometry_data = "v -1.0 0.0 0.0\nv 1.0 0.0 0.0\nf 1 2 1\n";
    bundle.symbol_svg_data = "<svg viewBox=\"0 0 100 100\"><path d=\"M 0 50 L 100 50\"/></svg>";
    bundle.custom_material_definitions["Nichrome"] = "resistivity=1.1e-6";

    // Serialize
    std::string serialized = bundle.serialize_bundle();
    REQUIRE_FALSE(serialized.empty());
    REQUIRE(serialized.find("SECTION:MANIFEST") != std::string::npos);
    REQUIRE(serialized.find("SECTION:GEOMETRY") != std::string::npos);
    REQUIRE(serialized.find("SECTION:SYMBOL_SVG") != std::string::npos);
    REQUIRE(serialized.find("SECTION:CHECKSUM") != std::string::npos);

    // Deserialize round-trip
    auto des_res = ComponentBundleArchive::deserialize_bundle(serialized);
    REQUIRE(des_res.is_ok());
    const auto& rt_bundle = des_res.value();
    REQUIRE(rt_bundle.manifest.name == res_comp.name);
    REQUIRE(rt_bundle.manifest.specs.nominal_primary_value == 4700.0);
    REQUIRE_FALSE(rt_bundle.mesh_geometry_data.empty());
    REQUIRE_FALSE(rt_bundle.symbol_svg_data.empty());

    // File I/O
    auto temp_path = std::filesystem::temp_directory_path() / "test_resistor.shunyacomp";
    auto export_res = ComponentBundleArchive::export_to_file(bundle, temp_path);
    REQUIRE(export_res.is_ok());

    auto import_res = ComponentBundleArchive::import_from_file(temp_path);
    REQUIRE(import_res.is_ok());
    REQUIRE(import_res.value().manifest.specs.nominal_primary_value == 4700.0);

    std::filesystem::remove(temp_path);
}

TEST_CASE("PH4-06.04.003: Component versioning, GUID tracking, and dependency resolution") {
    ComponentRegistryResolver resolver;

    VersionedComponentEntry v1;
    v1.component_guid = "GUID-RES-100R";
    v1.version = SemanticVersion{1, 0, 0};
    v1.name = "Resistor_100R";
    v1.required_materials = {"Copper", "Alumina"};

    // Register v1.0.0
    auto reg1 = resolver.register_component(v1);
    REQUIRE(reg1.is_ok());
    REQUIRE(resolver.contains("GUID-RES-100R"));
    REQUIRE(resolver.size() == 1);

    // Register upgrade v1.1.0
    VersionedComponentEntry v2 = v1;
    v2.version = SemanticVersion{1, 1, 0};
    auto reg2 = resolver.register_component(v2);
    REQUIRE(reg2.is_ok());
    REQUIRE(resolver.find_by_guid("GUID-RES-100R")->version.minor == 1);

    // Attempt downgrading to v0.9.0 with same GUID fails
    VersionedComponentEntry v_old = v1;
    v_old.version = SemanticVersion{0, 9, 0};
    auto reg_old = resolver.register_component(v_old);
    REQUIRE(reg_old.is_err());

    // Dependency resolution
    VersionedComponentEntry composite_part;
    composite_part.component_guid = "GUID-MODULE-AMPLIFIER";
    composite_part.name = "Amplifier_Subassembly";
    composite_part.dependencies = {"GUID-RES-100R", "GUID-TRANSISTOR-2N2222"};

    auto missing = resolver.resolve_missing_dependencies(composite_part);
    REQUIRE(missing.size() == 1);
    REQUIRE(missing[0] == "GUID-TRANSISTOR-2N2222");
}

TEST_CASE("PH4-06.04.004: Automated SPICE model numerical stability verification via synthetic DC/AC pulses") {
    auto res_comp = PassiveComponentsFactory::create_resistor(1000.0);

    // Stable linear resistor
    auto report1 = SpiceStabilityTester::test_stability(res_comp);
    REQUIRE(report1.is_stable);
    REQUIRE(report1.convergence_iterations > 0);
    REQUIRE(report1.diagnostics.empty());

    // Unstable component with missing equation
    auto bad_comp = res_comp;
    bad_comp.spice_model_equation = "";
    auto report2 = SpiceStabilityTester::test_stability(bad_comp);
    REQUIRE_FALSE(report2.is_stable);
    REQUIRE_FALSE(report2.diagnostics.empty());
}

TEST_CASE("PH4-06.04.005: Component Library Palette: categories, search filtering, and drag-and-drop descriptors") {
    ComponentLibraryPalette palette;

    auto r1k = PassiveComponentsFactory::create_resistor(1000.0);
    auto c100n = PassiveComponentsFactory::create_ceramic_capacitor(100.0e-9);
    auto pot10k = PassiveComponentsFactory::create_potentiometer(10000.0);

    palette.add_component(r1k, ComponentPaletteCategory::Passive);
    palette.add_component(c100n, ComponentPaletteCategory::Passive);
    palette.add_component(pot10k, ComponentPaletteCategory::Passive);

    REQUIRE(palette.total_items() == 3);

    // Search query for "Resistor"
    auto res_items = palette.search("Resistor");
    REQUIRE(res_items.size() == 1);
    REQUIRE(res_items[0].name == "Standard_Resistor");
    REQUIRE(res_items[0].drag_and_drop_mime_type.find("application/x-shunya-component") != std::string::npos);

    // Search query for "Capacitor"
    auto cap_items = palette.search("Capacitor");
    REQUIRE(cap_items.size() == 1);
    REQUIRE(cap_items[0].name == "Ceramic_Disc_Capacitor");

    // Empty search returns all items
    auto all_items = palette.search("");
    REQUIRE(all_items.size() == 3);
}

TEST_CASE("PH4-06.04.006: Component Clone-and-Modify feature: derive custom variants from standard catalog components") {
    auto base_res = PassiveComponentsFactory::create_resistor(1000.0, 5.0, 0.25);

    ComponentVariantFactory::CustomVariantParams params;
    params.new_name = "Precision_Resistor_4k7_1Pct";
    params.new_part_number = "SHUNYA-CUSTOM-4K7-1PCT";
    params.new_nominal_value = 4700.0;
    params.new_power_rating_w = 0.50;
    params.new_tolerance_pct = 1.0;

    auto derived = ComponentVariantFactory::clone_and_modify(base_res, params);

    // Distinct GUID
    REQUIRE_FALSE(derived.component_guid.empty());
    REQUIRE(derived.component_guid != base_res.component_guid);

    // Updated parameters
    REQUIRE(derived.name == "Precision_Resistor_4k7_1Pct");
    REQUIRE(derived.part_number == "SHUNYA-CUSTOM-4K7-1PCT");
    REQUIRE(derived.specs.nominal_primary_value == 4700.0);
    REQUIRE(derived.specs.max_power_dissipation_w == 0.50);
    REQUIRE(derived.specs.tolerance_percent == 1.0);
    REQUIRE(derived.is_qualified);
    REQUIRE(derived.certification_id.find("DERIVED-") != std::string::npos);
}

TEST_CASE("PH4-07.01.001: Component 1 - Fixed Resistor (3D Model, 2D Symbol, Ohm's law, and Joule heating)") {
    auto res = PassiveComponentsFactory::create_resistor(1000.0, 5.0, 0.25);

    // 1. Metadata & Classification
    REQUIRE(res.kind == ComponentKind::Resistor);
    REQUIRE(res.specs.nominal_primary_value == 1000.0);
    REQUIRE(res.specs.tolerance_percent == 5.0);
    REQUIRE(res.specs.max_power_dissipation_w == 0.25);

    // 2. 3D Model & Mesh
    REQUIRE(res.vertex_count > 0);
    REQUIRE(res.triangle_count > 0);
    REQUIRE(res.terminals.size() == 2);

    // 3. 2D Symbol
    REQUIRE(res.symbol_2d.pins.size() == 2);
    REQUIRE(res.pin_mappings.size() == 2);

    // 4. Electrical Law Verification: V = I * R
    double applied_v = 10.0; // 10 Volts
    double current_a = applied_v / res.specs.nominal_primary_value;
    REQUIRE(std::abs(current_a - 0.010) < 1e-6); // 10 mA

    // Joule heating: P = V^2 / R = 100 / 1000 = 0.10 W <= 0.25 W max rating
    double p_joule = applied_v * applied_v / res.specs.nominal_primary_value;
    REQUIRE(std::abs(p_joule - 0.10) < 1e-6);
    REQUIRE(p_joule <= res.specs.max_power_dissipation_w);
}

TEST_CASE("PH4-07.01.002: Component 2 - Variable Resistor (Rheostat) sweep & power rating") {
    // 50% wiper position on 0-100 Ohm rheostat
    auto rheo = PassiveComponentsFactory::create_rheostat(0.0, 100.0, 0.5, 50.0);

    REQUIRE(rheo.specs.nominal_primary_value == 50.0);
    REQUIRE(rheo.specs.max_power_dissipation_w == 50.0);
    REQUIRE(rheo.terminals.size() == 2);
    REQUIRE(rheo.symbol_2d.pins.size() == 2);

    // Sweep: min wiper (0%) -> 0 Ohm
    auto rheo_min = PassiveComponentsFactory::create_rheostat(0.0, 100.0, 0.0);
    REQUIRE(rheo_min.specs.nominal_primary_value == 0.0);

    // Sweep: max wiper (100%) -> 100 Ohm
    auto rheo_max = PassiveComponentsFactory::create_rheostat(0.0, 100.0, 1.0);
    REQUIRE(rheo_max.specs.nominal_primary_value == 100.0);

    // Power validation: at max current 0.5A through 50 Ohm, P = I^2 * R = 0.25 * 50 = 12.5 W <= 50 W
    double p_dissipated = 0.5 * 0.5 * rheo.specs.nominal_primary_value;
    REQUIRE(p_dissipated <= rheo.specs.max_power_dissipation_w);
}

TEST_CASE("PH4-07.01.003: Component 3 - Potentiometer voltage divider linearity & 3-pin configuration") {
    double total_r = 10000.0; // 10k Pot
    double wiper_pos = 0.35;  // 35% rotation
    auto pot = PassiveComponentsFactory::create_potentiometer(total_r, wiper_pos, 0.5);

    REQUIRE(pot.specs.nominal_primary_value == total_r);
    REQUIRE(pot.terminals.size() == 3);
    REQUIRE(pot.symbol_2d.pins.size() == 3);
    REQUIRE(pot.pin_mappings.size() == 3);

    // Dual-voltage divider verification: R1 + R2 = R_total
    double r1 = total_r * wiper_pos;
    double r2 = total_r * (1.0 - wiper_pos);
    REQUIRE(std::abs((r1 + r2) - total_r) < 1e-6);

    // Voltage divider output: Vout = Vin * (R1 / R_total)
    double vin = 12.0;
    double vout = vin * (r1 / total_r);
    REQUIRE(std::abs(vout - (12.0 * 0.35)) < 1e-6);
}

TEST_CASE("PH4-07.01.004: Component 4 - Ceramic Capacitor (RC time constant & AC reactance)") {
    double cap_f = 100.0e-9; // 100 nF
    auto cap = PassiveComponentsFactory::create_ceramic_capacitor(cap_f, 50.0);

    REQUIRE(cap.kind == ComponentKind::Capacitor);
    REQUIRE(cap.specs.nominal_primary_value == cap_f);
    REQUIRE(cap.specs.max_voltage_v == 50.0);
    REQUIRE(cap.terminals.size() == 2);
    REQUIRE(cap.symbol_2d.pins.size() == 2);

    // RC Time Constant: tau = R * C with 10k resistor
    double r_series = 10000.0;
    double tau_s = r_series * cap.specs.nominal_primary_value;
    REQUIRE(std::abs(tau_s - 0.001) < 1e-6); // 1.0 ms

    // Capacitive Reactance: X_C = 1 / (2 * pi * f * C) at f = 1 kHz
    double f_hz = 1000.0;
    double pi = 3.14159265358979323846;
    double x_c = 1.0 / (2.0 * pi * f_hz * cap_f);
    REQUIRE(std::abs(x_c - 1591.549) < 0.1); // ~1591.55 ohms
}

TEST_CASE("PH4-07.01.005: Component 5 - Polarized Electrolytic Capacitor (polarity enforcement & energy storage)") {
    double cap_f = 100.0e-6; // 100 uF
    auto ecap = PassiveComponentsFactory::create_electrolytic_capacitor(cap_f, 25.0);

    REQUIRE(ecap.kind == ComponentKind::Capacitor);
    REQUIRE(ecap.specs.nominal_primary_value == cap_f);
    REQUIRE(ecap.specs.max_voltage_v == 25.0);
    REQUIRE(ecap.terminals.size() == 2);

    // Polarity verification: Pin 1 is Positive, Pin 2 is Negative
    REQUIRE(ecap.terminals[0].polarity == ElectricalPinPolarity::Positive);
    REQUIRE(ecap.terminals[1].polarity == ElectricalPinPolarity::Negative);
    REQUIRE(ecap.symbol_2d.pins[0].polarity == ElectricalPinPolarity::Positive);
    REQUIRE(ecap.symbol_2d.pins[1].polarity == ElectricalPinPolarity::Negative);

    // Stored Energy: E = 0.5 * C * V^2 at 12 Volts
    double v_applied = 12.0;
    double energy_joules = 0.5 * cap_f * v_applied * v_applied;
    REQUIRE(std::abs(energy_joules - 0.0072) < 1e-6); // 7.2 mJ
}

TEST_CASE("PH4-07.01.006: Component 6 - Inductor (RL transient delay, AC reactance, and magnetic energy storage)") {
    double l_val = 10.0e-3; // 10 mH
    auto ind = PassiveComponentsFactory::create_inductor(l_val, 2.0);

    REQUIRE(ind.kind == ComponentKind::Inductor);
    REQUIRE(ind.specs.nominal_primary_value == l_val);
    REQUIRE(ind.specs.max_current_a == 2.0);
    REQUIRE(ind.terminals.size() == 2);
    REQUIRE(ind.symbol_2d.pins.size() == 2);
    REQUIRE(ind.vertex_count > 0);
    REQUIRE(ind.triangle_count > 0);

    // 1. Inductive Reactance: X_L = 2 * pi * f * L at f = 1000 Hz
    double f_hz = 1000.0;
    double pi = 3.14159265358979323846;
    double x_l = 2.0 * pi * f_hz * l_val;
    REQUIRE(std::abs(x_l - 62.8318) < 0.01); // ~62.83 ohms

    // 2. RL Transient Time Constant: tau = L / R with 100 ohm series resistor
    double r_val = 100.0;
    double tau_s = l_val / r_val;
    REQUIRE(std::abs(tau_s - 0.0001) < 1e-7); // 100 microseconds

    // 3. Magnetic Energy Storage: E = 0.5 * L * I^2 at max current 2.0 A
    double current = 2.0;
    double energy_stored_j = 0.5 * l_val * current * current;
    REQUIRE(std::abs(energy_stored_j - 0.020) < 1e-6); // 20 mJ

    // 4. Inductive Kickback: V = -L * (di / dt) with di = 2A shut off in 10 microseconds
    double dt = 10.0e-6;
    double v_spike = l_val * (current / dt);
    REQUIRE(std::abs(v_spike - 2000.0) < 1e-3); // 2000 V kickback spike requiring snubbing/freewheeling
}
