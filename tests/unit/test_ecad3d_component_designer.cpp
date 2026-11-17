#include "tests/test_framework.hpp"

#include "shunya/cad3d/parametric_primitives.hpp"
#include "shunya/ecad3d/component_designer_workspace.hpp"
#include "shunya/ecad3d/component_manifest.hpp"
#include "shunya/ecad3d/component_physics_qualification.hpp"
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

TEST_CASE("PH4-06.02.004: Enforce pin numbering and naming parity between 3D model geometry and 2D schematic symbols") {
    ComponentAuthoringData comp;
    comp.name = "Test_Diode";
    comp.terminals = {
        PhysicalTerminal3D("T1", 1, ElectricalPinPolarity::Anode, {-2.0, 0.0, 0.0}, {-1.0, 0.0, 0.0}),
        PhysicalTerminal3D("T2", 2, ElectricalPinPolarity::Cathode, {2.0, 0.0, 0.0}, {1.0, 0.0, 0.0})
    };

    SchematicSymbol2D sym;
    sym.pins = {
        SchematicSymbolPin("P1", 1, "A", ElectricalPinPolarity::Anode),
        SchematicSymbolPin("P2", 2, "K", ElectricalPinPolarity::Cathode)
    };

    // 1. Valid parity
    auto report1 = ComponentSymbolCouplingEngine::verify_pin_parity(comp, sym);
    REQUIRE(report1.is_valid);
    REQUIRE(report1.parity_violations.empty());

    // 2. Count mismatch
    sym.pins.push_back(SchematicSymbolPin("P3", 3, "NC", ElectricalPinPolarity::PassiveBiDirectional));
    auto report2 = ComponentSymbolCouplingEngine::verify_pin_parity(comp, sym);
    REQUIRE_FALSE(report2.is_valid);

    // 3. Duplicate pin numbers in 3D
    sym.pins.pop_back();
    comp.terminals[1].pin_number = 1; // Duplicate pin 1
    auto report3 = ComponentSymbolCouplingEngine::verify_pin_parity(comp, sym);
    REQUIRE_FALSE(report3.is_valid);

    // 4. Polarity conflict
    comp.terminals[1].pin_number = 2;
    comp.terminals[1].polarity = ElectricalPinPolarity::Anode; // Conflict: Cathode vs Anode on pin 2
    auto report4 = ComponentSymbolCouplingEngine::verify_pin_parity(comp, sym);
    REQUIRE_FALSE(report4.is_valid);
}

TEST_CASE("PH4-06.02.005: Spatial orientation mapping: define component default orientation, rotation axes, and mounting planes") {
    SpatialMountingOrientation mount;
    mount.mounting_plane = MountingPlane::XY_TopSurface;
    mount.seating_height_mm = 0.25; // 0.25mm solder paste standoff
    mount.default_rotation_deg = 90.0;
    mount.center_offset = Point3D{0.0, 0.0, 0.0};

    REQUIRE(mount.is_valid());

    // Local terminal at (10, 0, 0)
    Point3D term_pos{10.0, 0.0, 0.0};

    // With 90 deg rotation, (10, 0) transforms to (0, 10), and Z gets +0.25 standoff
    auto pcb_p = mount.transform_terminal_to_pcb(term_pos);
    REQUIRE(std::abs(pcb_p.x) < 1e-4);
    REQUIRE(std::abs(pcb_p.y - 10.0) < 1e-4);
    REQUIRE(std::abs(pcb_p.z - 0.25) < 1e-4);

    // Bottom mounting mirrors X
    mount.mounting_plane = MountingPlane::XY_BottomSurface;
    auto pcb_bot = mount.transform_terminal_to_pcb(term_pos);
    REQUIRE(std::abs(pcb_bot.x) < 1e-4);
    REQUIRE(std::abs(pcb_bot.y - 10.0) < 1e-4);
    REQUIRE(std::abs(pcb_bot.z - (-0.25)) < 1e-4);

    // Footprint bounds computation
    std::vector<PhysicalTerminal3D> terms = {
        PhysicalTerminal3D("T1", 1, ElectricalPinPolarity::PassiveBiDirectional, {-5.0, -2.0, 0.0}),
        PhysicalTerminal3D("T2", 2, ElectricalPinPolarity::PassiveBiDirectional, {5.0, 2.0, 0.0})
    };
    mount.mounting_plane = MountingPlane::XY_TopSurface;
    mount.default_rotation_deg = 0.0;
    auto bounds = mount.compute_mounting_footprint_bounds(terms, 0.5);
    REQUIRE(bounds.first.x == -5.5);
    REQUIRE(bounds.second.x == 5.5);
    REQUIRE(bounds.first.y == -2.5);
    REQUIRE(bounds.second.y == 2.5);
}

TEST_CASE("PH4-06.02.006: Bidirectional selection highlighting: selecting a pin in 3D highlights 2D symbol pin and vice-versa") {
    SelectionHighlightCoordinator coordinator;

    std::vector<PhysicalTerminal3D> terminals = {
        PhysicalTerminal3D("TERM_1", 1, ElectricalPinPolarity::Positive, {-5.0, 0.0, 0.0}),
        PhysicalTerminal3D("TERM_2", 2, ElectricalPinPolarity::Negative, {5.0, 0.0, 0.0})
    };
    auto sym = SchematicSymbolSynthesizer::synthesize_symbol(ComponentKind::Resistor);

    PinTerminalMapper mapper;
    mapper.initialize(terminals, sym);
    REQUIRE(mapper.auto_map_by_number_and_polarity().is_ok());

    SelectionHighlightEvent captured_event;
    coordinator.set_listener([&](const SelectionHighlightEvent& ev) {
        captured_event = ev;
    });

    // 1. Select from 3D viewport
    coordinator.select_from_3d("TERM_1", mapper);
    REQUIRE(coordinator.is_terminal_highlighted("TERM_1"));
    REQUIRE(coordinator.is_pin_highlighted("1"));
    REQUIRE(captured_event.source == SelectionSource::Viewport3D);
    REQUIRE(captured_event.pin_number == 1);

    // 2. Select from 2D schematic editor
    coordinator.select_from_2d("2", mapper);
    REQUIRE(coordinator.is_pin_highlighted("2"));
    REQUIRE(coordinator.is_terminal_highlighted("TERM_2"));
    REQUIRE(captured_event.source == SelectionSource::Schematic2D);
    REQUIRE(captured_event.pin_number == 2);

    // 3. Clear
    coordinator.clear_selection();
    REQUIRE_FALSE(coordinator.is_terminal_highlighted("TERM_1"));
    REQUIRE_FALSE(coordinator.is_pin_highlighted("2"));
}

TEST_CASE("PH4-06.03.001: Multi-region material assignment validation: terminal conductivity & body insulation") {
    ComponentAuthoringData comp;
    comp.name = "SMD_Inductor";

    // 1. Valid assignments: Copper leads, Ferrite body
    comp.regions["reg_lead1"] = MeshRegionMaterial{"reg_lead1", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["reg_lead2"] = MeshRegionMaterial{"reg_lead2", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["reg_body"] = MeshRegionMaterial{"reg_body", "Alumina", 1.0e-12, 30.0, 3950.0, 9.0, 1.0, false, {}};

    auto report1 = ComponentPhysicsEngine::validate_material_assignments(comp);
    REQUIRE(report1.is_valid);
    REQUIRE(report1.violations.empty());

    // 2. Invalid assignment: Terminal assigned Alumina ceramic (insulator < 1e5 S/m)
    comp.regions["reg_lead1"].material_name = "Alumina";
    auto report2 = ComponentPhysicsEngine::validate_material_assignments(comp);
    REQUIRE_FALSE(report2.is_valid);
    REQUIRE_FALSE(report2.violations.empty());

    // 3. Warning: Housing assigned conductive Copper
    comp.regions["reg_lead1"].material_name = "Copper";
    comp.regions["reg_body"].material_name = "Copper"; // body assigned Copper
    auto report3 = ComponentPhysicsEngine::validate_material_assignments(comp);
    REQUIRE_FALSE(report3.warnings.empty());
}

TEST_CASE("PH4-06.03.002: Automated electrical continuity checks through internal 3D geometries between terminals") {
    ComponentAuthoringData comp;
    comp.name = "Resistor_0603";
    comp.kind = ComponentKind::Resistor;
    comp.specs.nominal_primary_value = 100.0;
    comp.terminals = {
        PhysicalTerminal3D("T1", 1, ElectricalPinPolarity::PassiveBiDirectional, Point3D{-0.8, 0.0, 0.0}, Vec3d{-1.0, 0.0, 0.0}, "reg_t1"),
        PhysicalTerminal3D("T2", 2, ElectricalPinPolarity::PassiveBiDirectional, Point3D{0.8, 0.0, 0.0}, Vec3d{1.0, 0.0, 0.0}, "reg_t2")
    };
    comp.regions["reg_t1"] = MeshRegionMaterial{"reg_t1", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["reg_t2"] = MeshRegionMaterial{"reg_t2", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["reg_body"] = MeshRegionMaterial{"reg_body", "Alumina", 1.0e-12, 30.0, 3950.0, 9.0, 1.0, false, {}};

    // 1. Resistor continuity check
    auto cont_res = ComponentPhysicsEngine::check_electrical_continuity(comp, 0, 1);
    REQUIRE(cont_res.status == ContinuityReport::Status::Continuous);
    REQUIRE(cont_res.estimated_dc_conductance_siemens == 0.01); // 1 / 100 ohm

    // 2. Capacitor DC open-circuit check
    comp.kind = ComponentKind::Capacitor;
    auto cont_cap = ComponentPhysicsEngine::check_electrical_continuity(comp, 0, 1);
    REQUIRE(cont_cap.status == ContinuityReport::Status::OpenCircuit);
    REQUIRE(cont_cap.estimated_dc_conductance_siemens < 1e-9);

    // 3. Capacitor short-circuit breach (both terminals mapped to same region)
    comp.terminals[1].bound_region_id = "reg_t1";
    auto cont_short = ComponentPhysicsEngine::check_electrical_continuity(comp, 0, 1);
    REQUIRE(cont_short.status == ContinuityReport::Status::ShortCircuitIsolationBreach);
}

TEST_CASE("PH4-06.03.003: Calculate equivalent series resistance (ESR) based on geometric lead length, area, resistivity") {
    ComponentAuthoringData comp;
    comp.terminals = {
        PhysicalTerminal3D("T1", 1, ElectricalPinPolarity::PassiveBiDirectional, Point3D{-2.0, 0.0, 0.0}),
        PhysicalTerminal3D("T2", 2, ElectricalPinPolarity::PassiveBiDirectional, Point3D{2.0, 0.0, 0.0})
    };

    // Lead parameters: length 2mm, area 0.2 mm^2, copper rho = 1.68e-8
    std::vector<LeadGeometricSpec> leads = {
        LeadGeometricSpec{2.0, 0.2, 1.68e-8},
        LeadGeometricSpec{2.0, 0.2, 1.68e-8}
    };

    double internal_r = 0.050; // 50 milliohms internal element
    auto esr = ComponentPhysicsEngine::calculate_esr(comp, internal_r, leads);

    // R_lead = 2 * (1.68e-8 * 0.002 / 0.2e-6) = 2 * 0.000168 = 0.000336 ohms
    REQUIRE(std::abs(esr.terminal_leads_resistance_ohms - 0.000336) < 1e-6);
    REQUIRE(esr.internal_element_resistance_ohms == 0.050);
    REQUIRE(std::abs(esr.total_esr_ohms - 0.050336) < 1e-6);
    REQUIRE(esr.esr_at_100khz_ohms > esr.total_esr_ohms);
}

TEST_CASE("PH4-06.03.004: Calculate total component mass, center of gravity, and inertia tensor") {
    ComponentAuthoringData comp;
    comp.name = "Alumina_Resistor";
    // Box 4mm x 2mm x 1mm
    comp.mesh_geometry = ParametricPrimitiveGenerator::generate_cube(CubeParams{.width = 4.0, .height = 2.0, .depth = 1.0});
    comp.regions["reg_body"] = MeshRegionMaterial{"reg_body", "Alumina", 1e-12, 30.0, 3950.0, 9.0, 1.0, false, {}};

    auto mass_prop = ComponentPhysicsEngine::calculate_mass_properties(comp);

    // Volume = 4 * 2 * 1 mm^3 = 8e-9 m^3. Alumina density = 3950 kg/m^3.
    // Mass = 8e-9 * 3950 = 3.16e-5 kg = 0.0316 grams.
    REQUIRE(mass_prop.total_mass_kg > 3.0e-5);
    REQUIRE(mass_prop.total_mass_kg < 3.3e-5);

    // Center of gravity centered near origin (symmetric box)
    REQUIRE(std::abs(mass_prop.center_of_gravity_mm.x) < 1e-3);
    REQUIRE(std::abs(mass_prop.center_of_gravity_mm.y) < 1e-3);
    REQUIRE(std::abs(mass_prop.center_of_gravity_mm.z) < 1e-3);

    // Principal diagonal inertia components must be positive
    REQUIRE(mass_prop.inertia_tensor_kg_m2[0][0] > 0.0);
    REQUIRE(mass_prop.inertia_tensor_kg_m2[1][1] > 0.0);
    REQUIRE(mass_prop.inertia_tensor_kg_m2[2][2] > 0.0);
}

TEST_CASE("PH4-06.03.005: Thermal dissipation rating calculations based on 3D surface area and convective HTC") {
    ComponentAuthoringData comp;
    // Box 4mm x 2mm x 1mm -> Surface area = 2 * (4*2 + 2*1 + 4*1) = 28 mm^2
    comp.mesh_geometry = ParametricPrimitiveGenerator::generate_cube(CubeParams{.width = 4.0, .height = 2.0, .depth = 1.0});

    ThermalDissipationSpec spec;
    spec.ambient_temp_c = 25.0;
    spec.max_operating_temp_c = 125.0;
    spec.convective_htc_w_m2k = 15.0;

    auto thermal = ComponentPhysicsEngine::calculate_thermal_dissipation(comp, spec);

    REQUIRE(std::abs(thermal.total_surface_area_mm2 - 28.0) < 1e-3);
    REQUIRE(thermal.max_continuous_power_watts > 0.040); // > 40 mW
    REQUIRE(thermal.thermal_resistance_deg_c_per_watt > 0.0);
    REQUIRE(thermal.derated_power_at_70c_watts < thermal.max_continuous_power_watts);
}

TEST_CASE("PH4-06.03.006: Component Qualification Inspector: reject registration if isolation breached or requirements missing") {
    ComponentAuthoringData comp;
    comp.name = "Power_Resistor_10R";
    comp.kind = ComponentKind::Resistor;
    comp.mesh_geometry = ParametricPrimitiveGenerator::generate_cube(CubeParams{.width = 5.0, .height = 3.0, .depth = 2.0});
    comp.regions["r1"] = MeshRegionMaterial{"r1", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["r2"] = MeshRegionMaterial{"r2", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["r3"] = MeshRegionMaterial{"r3", "Alumina", 1e-12, 30.0, 3950.0, 9.0, 1.0, false, {}};

    comp.terminals = {
        PhysicalTerminal3D("T1", 1, ElectricalPinPolarity::PassiveBiDirectional, Point3D{-2.5, 0.0, 0.0}, Vec3d{-1.0, 0.0, 0.0}, "r1"),
        PhysicalTerminal3D("T2", 2, ElectricalPinPolarity::PassiveBiDirectional, Point3D{2.5, 0.0, 0.0}, Vec3d{1.0, 0.0, 0.0}, "r2")
    };
    comp.spice_model.netlist_template = "R1 1 2 10.0";

    auto sym = SchematicSymbolSynthesizer::synthesize_symbol(ComponentKind::Resistor);
    PinTerminalMapper mapper;
    mapper.initialize(comp.terminals, sym);
    REQUIRE(mapper.auto_map_by_number_and_polarity().is_ok());

    // 1. Qualified component
    auto cert1 = ComponentQualificationInspector::inspect_and_qualify(comp, sym, mapper);
    REQUIRE(cert1.is_qualified);
    REQUIRE_FALSE(cert1.certification_id.empty());
    REQUIRE(cert1.critical_failures.empty());
    REQUIRE(cert1.evaluated_max_power_w > 0.0);

    // 2. Disqualified: Missing SPICE equation
    comp.spice_model.netlist_template = "";
    auto cert2 = ComponentQualificationInspector::inspect_and_qualify(comp, sym, mapper);
    REQUIRE_FALSE(cert2.is_qualified);
    REQUIRE_FALSE(cert2.critical_failures.empty());
}

TEST_CASE("PH4-06.04.001: Unified component manifest schema (.shunyacomp.json) serialization and round-trip") {
    ComponentAuthoringData comp;
    comp.name = "Precision_Resistor_100R";
    comp.kind = ComponentKind::Resistor;
    comp.metadata.manufacturer = "Vishay / Shunya Precision";
    comp.metadata.part_number = "SHUNYA-RES-0603-100R";
    comp.metadata.package_type = "SMD 0603";
    comp.metadata.description = "Thick Film Chip Resistor 100 Ohm 0.1W 1%";
    comp.specs.nominal_primary_value = 100.0;
    comp.specs.max_power_dissipation_w = 0.1;
    comp.specs.tolerance_percent = 1.0;
    comp.spice_model.netlist_template = "R1 1 2 100.0";
    comp.selected_material_names = {"Copper", "Alumina", "Nichrome"};
    comp.mesh_geometry = ParametricPrimitiveGenerator::generate_cube(CubeParams{.width = 1.6, .height = 0.8, .depth = 0.45});
    comp.terminals = {
        PhysicalTerminal3D("T1", 1, ElectricalPinPolarity::PassiveBiDirectional, Point3D{-0.8, 0.0, 0.0}),
        PhysicalTerminal3D("T2", 2, ElectricalPinPolarity::PassiveBiDirectional, Point3D{0.8, 0.0, 0.0})
    };

    auto sym = SchematicSymbolSynthesizer::synthesize_symbol(ComponentKind::Resistor);
    PinTerminalMapper mapper;
    mapper.initialize(comp.terminals, sym);
    REQUIRE(mapper.auto_map_by_number_and_polarity().is_ok());

    ComponentQualificationInspector::QualificationCertificate cert;
    cert.is_qualified = true;
    cert.certification_id = "CERT-998877-QUALIFIED";

    // Create manifest
    auto manifest = ComponentManifestSerializer::create_manifest(comp, sym, mapper, cert);
    REQUIRE(manifest.schema_identifier == "shunyacomp");
    REQUIRE(manifest.name == "Precision_Resistor_100R");
    REQUIRE(manifest.is_qualified);

    // Serialize to JSON
    std::string json_str = ComponentManifestSerializer::to_json(manifest);
    REQUIRE_FALSE(json_str.empty());

    // Schema validation
    auto valid_res = ComponentManifestSerializer::validate_schema(json_str);
    REQUIRE(valid_res.is_ok());

    // Deserialization round-trip
    auto roundtrip_res = ComponentManifestSerializer::from_json(json_str);
    REQUIRE(roundtrip_res.is_ok());
    const auto& rt = roundtrip_res.value();
    REQUIRE(rt.name == "Precision_Resistor_100R");
    REQUIRE(rt.manufacturer == "Vishay / Shunya Precision");
    REQUIRE(rt.part_number == "SHUNYA-RES-0603-100R");
    REQUIRE(rt.specs.nominal_primary_value == 100.0);
    REQUIRE(rt.specs.max_power_dissipation_w == 0.1);
    REQUIRE(rt.is_qualified);
    REQUIRE(rt.certification_id == "CERT-998877-QUALIFIED");
}
