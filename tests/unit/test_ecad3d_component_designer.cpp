#include "tests/test_framework.hpp"

#include "shunya/cad3d/parametric_primitives.hpp"
#include "shunya/ecad3d/component_designer_workspace.hpp"
#include "shunya/ecad3d/symbol_association_engine.hpp"

#include <cmath>

using namespace shunya::ecad3d;
using namespace shunya::cad3d;
using namespace shunya::eng;
using shunya::core::domain::ElectricalPinPolarity;

TEST_CASE("PH4-06.01.001: Dedicated 3D Electronic Component Designer workspace layout & architecture") {
    ComponentDesignerWorkspace workspace;
    workspace.start_new_component("Precision_Resistor_1k", ComponentKind::Resistor);

    REQUIRE(workspace.data().name == "Precision_Resistor_1k");
    REQUIRE(workspace.data().kind == ComponentKind::Resistor);
    REQUIRE(workspace.current_step() == AuthoringStep::Step1_SelectBaseMaterials);
    REQUIRE_FALSE(workspace.data().is_registered);
}

TEST_CASE("PH4-06.01.002: Step 1-2 workflow: select active chemical elements or engineered materials") {
    ComponentDesignerWorkspace workspace;
    workspace.start_new_component("SMD_Resistor_0805", ComponentKind::Resistor);

    // Empty selection fails
    auto fail_res = workspace.step1_select_materials({});
    REQUIRE(fail_res.is_err());

    // Valid material selections: Alumina ceramic, Nichrome film, Copper terminations
    std::vector<std::string> materials = {"Copper", "Alumina", "Nichrome"};
    auto ok_res = workspace.step1_select_materials(materials);
    REQUIRE(ok_res.is_ok());
    REQUIRE(workspace.data().selected_material_names.size() == 3);
    REQUIRE(workspace.current_step() == AuthoringStep::Step2_ValidateMaterialProps);
}

TEST_CASE("PH4-06.01.003: Step 3-5 workflow: construct 3D geometry and assign distinct materials to individual mesh regions") {
    ComponentDesignerWorkspace workspace;
    workspace.start_new_component("Power_Resistor", ComponentKind::Resistor);
    workspace.step1_select_materials({"Copper", "Alumina"});

    // Step 3: Geometry creation (Cube with 12 triangles)
    CubeParams cp;
    cp.width = 4.0; cp.height = 2.0; cp.depth = 1.0;
    auto mesh = ParametricPrimitiveGenerator::generate_cube(cp);
    mesh.weld_vertices();
    REQUIRE(mesh.triangles.size() == 12);

    auto geo_res = workspace.step3_set_geometry(mesh);
    REQUIRE(geo_res.is_ok());
    REQUIRE(workspace.current_step() == AuthoringStep::Step4_DefineMeshRegions);

    // Step 4: Partition into regions (leads: tris 0-3, body: tris 4-11)
    auto reg_leads = workspace.step4_define_region("leads", {0, 1, 2, 3});
    auto reg_body = workspace.step4_define_region("housing", {4, 5, 6, 7, 8, 9, 10, 11});
    REQUIRE(reg_leads.is_ok());
    REQUIRE(reg_body.is_ok());

    // Step 5: Assign materials (leads = Copper, housing = Alumina)
    auto mat_leads = workspace.step5_assign_region_material("leads", "Copper", 5.8e7, 8960.0, 400.0);
    auto mat_body = workspace.step5_assign_region_material("housing", "Alumina", 1e-12, 3950.0, 30.0);
    REQUIRE(mat_leads.is_ok());
    REQUIRE(mat_body.is_ok());

    REQUIRE(workspace.data().regions["leads"].is_conductor);
    REQUIRE_FALSE(workspace.data().regions["housing"].is_conductor);
    REQUIRE(workspace.current_step() == AuthoringStep::Step6_ConfigureElectricalSpecs);
}

TEST_CASE("PH4-06.01.004: Step 6 workflow: configure nominal electrical specifications, ratings, thermal limits, tolerances") {
    ComponentDesignerWorkspace workspace;
    workspace.start_new_component("Precision_Resistor_100R", ComponentKind::Resistor);

    ComponentElectricalSpecs specs;
    specs.nominal_primary_value = 100.0; // 100 Ohm
    specs.tolerance_percent = 1.0;       // 1%
    specs.max_voltage_v = 150.0;
    specs.max_current_a = 0.5;
    specs.max_power_dissipation_w = 0.25;// 1/4 Watt
    specs.max_operating_temp_k = 428.15; // 155 C
    specs.thermal_resistance_j_a = 60.0; // 60 K/W

    auto res = workspace.step6_configure_specs(specs);
    REQUIRE(res.is_ok());
    REQUIRE(workspace.data().specs.nominal_primary_value == 100.0);
    REQUIRE(workspace.data().specs.tolerance_percent == 1.0);
    REQUIRE(workspace.current_step() == AuthoringStep::Step7_DefinePhysicalTerminals);
}

TEST_CASE("PH4-06.01.005: Step 7 workflow: define physical 3D contact terminals, snap anchors, polarity") {
    ComponentDesignerWorkspace workspace;
    workspace.start_new_component("Axial_Resistor", ComponentKind::Resistor);

    // Setup geometry and regions
    CubeParams cp;
    auto mesh = ParametricPrimitiveGenerator::generate_cube(cp);
    workspace.step3_set_geometry(mesh);
    workspace.step4_define_region("leads", {0, 1});
    workspace.step5_assign_region_material("leads", "Copper", 5.8e7, 8960.0);

    // Terminal 1 & 2
    PhysicalTerminal3D t1("Pin_1", 1, ElectricalPinPolarity::PassiveBiDirectional, {-1.0, 0.0, 0.0}, {-1.0, 0.0, 0.0}, "leads");
    PhysicalTerminal3D t2("Pin_2", 2, ElectricalPinPolarity::PassiveBiDirectional, {1.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, "leads");

    auto r1 = workspace.step7_add_terminal(t1);
    auto r2 = workspace.step7_add_terminal(t2);
    REQUIRE(r1.is_ok());
    REQUIRE(r2.is_ok());
    REQUIRE(workspace.data().terminals.size() == 2);
    REQUIRE(workspace.current_step() == AuthoringStep::Step8_DefineSpiceModel);
}

TEST_CASE("PH4-06.01.006: Step 8-10 workflow: define SPICE behavioral equations, metadata, and save component to catalog") {
    ComponentDesignerWorkspace workspace;
    workspace.start_new_component("Vishay_CRCW0805", ComponentKind::Resistor);

    // Step 1-7 configuration
    CubeParams cp;
    cp.width = 2.0; cp.height = 1.25; cp.depth = 0.5;
    auto mesh = ParametricPrimitiveGenerator::generate_cube(cp);
    mesh.weld_vertices();
    workspace.step3_set_geometry(mesh);
    workspace.step4_define_region("leads", {0, 1, 2, 3});
    workspace.step4_define_region("body", {4, 5, 6, 7, 8, 9, 10, 11});
    workspace.step5_assign_region_material("leads", "Copper", 5.8e7, 8960.0);
    workspace.step5_assign_region_material("body", "Alumina", 1e-12, 3950.0);

    ComponentElectricalSpecs specs;
    specs.nominal_primary_value = 1000.0;
    specs.max_voltage_v = 150.0;
    specs.max_current_a = 0.1;
    workspace.step6_configure_specs(specs);

    PhysicalTerminal3D t1("Pin_1", 1, ElectricalPinPolarity::PassiveBiDirectional, {-1.0, 0.0, 0.0}, {-1.0, 0.0, 0.0}, "leads");
    PhysicalTerminal3D t2("Pin_2", 2, ElectricalPinPolarity::PassiveBiDirectional, {1.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, "leads");
    workspace.step7_add_terminal(t1);
    workspace.step7_add_terminal(t2);

    // Step 8: SPICE Model
    ComponentSpiceModel spice;
    spice.model_type = "analytical";
    spice.netlist_template = "R1 %1 %2 1000.0 tc1=100e-6";
    spice.parameters["resistance"] = 1000.0;
    auto sp_res = workspace.step8_configure_spice(spice);
    REQUIRE(sp_res.is_ok());

    // Step 9: Metadata
    ComponentMetadata meta;
    meta.part_number = "CRCW08051K00FKEA";
    meta.manufacturer = "Vishay Dale";
    meta.package_type = "0805";
    meta.description = "Thick Film Chip Resistor 1k 1% 0805 SMD";
    auto meta_res = workspace.step9_set_metadata(meta);
    REQUIRE(meta_res.is_ok());

    // Step 10: Qualification and Catalog Registration
    auto qual = workspace.step10_qualify_component();
    REQUIRE(qual.is_valid);
    REQUIRE(qual.errors.empty());
    REQUIRE(qual.total_mass_kg > 0.0);

    auto reg_res = workspace.step10_register_to_catalog();
    REQUIRE(reg_res.is_ok());
    REQUIRE(workspace.data().is_registered);
}

TEST_CASE("PH4-06.02.001: Core architectural invariant: no 3D model registered without verified coupled 2D circuit symbol") {
    ComponentAuthoringData comp;
    CubeParams cp;
    comp.mesh_geometry = ParametricPrimitiveGenerator::generate_cube(cp);
    comp.terminals.push_back(PhysicalTerminal3D("Pin_1", 1, ElectricalPinPolarity::PassiveBiDirectional, {-1.0, 0.0, 0.0}, {-1.0, 0.0, 0.0}, "leads"));
    comp.terminals.push_back(PhysicalTerminal3D("Pin_2", 2, ElectricalPinPolarity::PassiveBiDirectional, {1.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, "leads"));

    // 1. Without coupled symbol -> REJECTED
    SchematicSymbol2D empty_sym;
    PinTerminalMapper unmapped;
    auto rej_res = ComponentSymbolCouplingEngine::verify_coupling(comp, empty_sym, unmapped);
    REQUIRE_FALSE(rej_res.is_coupled);
    REQUIRE(!rej_res.rejection_reasons.empty());

    // 2. With valid IEEE symbol and verified pin mapper -> APPROVED
    auto sym = SchematicSymbolSynthesizer::synthesize_symbol(ComponentKind::Resistor, "IEEE");
    PinTerminalMapper mapper;
    mapper.initialize(comp.terminals, sym);
    mapper.auto_map_by_number_and_polarity();

    auto ok_res = ComponentSymbolCouplingEngine::verify_coupling(comp, sym, mapper);
    REQUIRE(ok_res.is_coupled);
    REQUIRE(ok_res.rejection_reasons.empty());
}

TEST_CASE("PH4-06.02.002: Automated 2D symbol generator synthesizing standard IEEE/IEC schematic symbols") {
    // 1. IEEE Resistor (zigzag, 2 pins)
    auto ieee_res = SchematicSymbolSynthesizer::synthesize_symbol(ComponentKind::Resistor, "IEEE");
    REQUIRE(ieee_res.standard == "IEEE");
    REQUIRE(ieee_res.primitive_shape == "zigzag");
    REQUIRE(ieee_res.pins.size() == 2);

    // 2. IEC Resistor (rectangle, 2 pins)
    auto iec_res = SchematicSymbolSynthesizer::synthesize_symbol(ComponentKind::Resistor, "IEC");
    REQUIRE(iec_res.standard == "IEC");
    REQUIRE(iec_res.primitive_shape == "rectangle");
    REQUIRE(iec_res.pins.size() == 2);

    // 3. Capacitor (parallel plates, 2 pins with polarity)
    auto cap = SchematicSymbolSynthesizer::synthesize_symbol(ComponentKind::Capacitor, "IEEE");
    REQUIRE(cap.primitive_shape == "parallel_plates");
    REQUIRE(cap.pins.size() == 2);
    REQUIRE(cap.pins[0].polarity == ElectricalPinPolarity::Positive);
    REQUIRE(cap.pins[1].polarity == ElectricalPinPolarity::Negative);

    // 4. Battery (cells, 2 pins)
    auto bat = SchematicSymbolSynthesizer::synthesize_symbol(ComponentKind::Battery, "IEEE");
    REQUIRE(bat.primitive_shape == "battery_cells");
    REQUIRE(bat.pins.size() == 2);

    // 5. Generic IC / Connector (8 pins)
    auto ic8 = SchematicSymbolSynthesizer::synthesize_symbol(ComponentKind::Connector, "IEEE", 8);
    REQUIRE(ic8.primitive_shape == "ic_box");
    REQUIRE(ic8.pins.size() == 8);
}

TEST_CASE("PH4-06.02.003: Interactive pin-to-terminal mapper: link each 3D contact point directly to corresponding 2D symbol pins") {
    // Setup 3D terminals
    std::vector<PhysicalTerminal3D> terminals = {
        PhysicalTerminal3D("Term_A", 1, ElectricalPinPolarity::Positive, {-5.0, 0.0, 0.0}, {-1.0, 0.0, 0.0}, "lead_a"),
        PhysicalTerminal3D("Term_K", 2, ElectricalPinPolarity::Negative, {5.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, "lead_k")
    };

    auto sym = SchematicSymbolSynthesizer::synthesize_symbol(ComponentKind::Capacitor, "IEEE");

    PinTerminalMapper mapper;
    mapper.initialize(terminals, sym);
    REQUIRE_FALSE(mapper.is_complete());

    // Automated mapping by pin number & polarity
    auto map_res = mapper.auto_map_by_number_and_polarity();
    REQUIRE(map_res.is_ok());
    REQUIRE(mapper.is_complete());

    // Bidirectional lookup
    auto pin_a = mapper.get_symbol_pin_for_terminal("Term_A");
    auto pin_k = mapper.get_symbol_pin_for_terminal("Term_K");
    REQUIRE(pin_a.has_value());
    REQUIRE(pin_k.has_value());
    REQUIRE(*pin_a == "1");
    REQUIRE(*pin_k == "2");

    auto term_1 = mapper.get_terminal_for_symbol_pin("1");
    auto term_2 = mapper.get_terminal_for_symbol_pin("2");
    REQUIRE(term_1.has_value());
    REQUIRE(term_2.has_value());
    REQUIRE(*term_1 == "Term_A");
    REQUIRE(*term_2 == "Term_K");

    // Spatial mounting orientation configuration
    SpatialMountingOrientation mount;
    mount.mounting_normal = {0.0, 0.0, 1.0};
    mount.primary_alignment_axis = {1.0, 0.0, 0.0};
    mount.default_rotation_deg = 90.0;
    mapper.set_mounting_orientation(mount);

    REQUIRE(mapper.mounting_orientation().default_rotation_deg == 90.0);
    REQUIRE(mapper.mounting_orientation().mounting_normal.z == 1.0);
}
