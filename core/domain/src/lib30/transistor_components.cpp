#include "shunya/lib30/transistor_components.hpp"

#include "shunya/cad3d/parametric_primitives.hpp"
#include "shunya/core/base/uuid.hpp"

namespace shunya::lib30 {

using namespace ecad3d;
using namespace cad3d;
using shunya::core::domain::ElectricalPinPolarity;

// ============================================================================
// Component 11: NPN BJT (PH4-07.03.001)
// ============================================================================

ComponentManifest TransistorComponentsFactory::create_npn_bjt(
    double beta_current_gain,
    double max_collector_current_a,
    double max_v_ce_volts,
    std::string_view standard
) {
    ComponentAuthoringData comp;
    comp.name = "NPN_BJT_2N3904";
    comp.kind = eng::ComponentKind::BJT_NPN;
    comp.metadata.manufacturer = "Shunya Discrete Electronics";
    comp.metadata.part_number = "2N3904";
    comp.metadata.package_type = "TO-92 Plastic";
    comp.metadata.description = "NPN General Purpose Bipolar Junction Transistor";

    comp.specs.nominal_primary_value = beta_current_gain;
    comp.specs.max_voltage_v = max_v_ce_volts;
    comp.specs.max_current_a = max_collector_current_a;
    comp.specs.max_power_dissipation_w = 0.625; // 625 mW

    comp.selected_material_names = {"Silicon", "Copper", "Alumina"};
    // TO-92 body cylinder with flat face
    comp.mesh_geometry = ParametricPrimitiveGenerator::generate_cylinder(
        CylinderParams{.radius = 2.4, .height = 4.8, .radial_segments = 16}
    );

    comp.regions["lead_b"] = MeshRegionMaterial{"lead_b", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["lead_c"] = MeshRegionMaterial{"lead_c", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["lead_e"] = MeshRegionMaterial{"lead_e", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["package_body"] = MeshRegionMaterial{"package_body", "Alumina", 1.0e-12, 0.8, 1900.0, 3.5, 1.0, false, {}};

    // 3 Pins: Pin 1 Base, Pin 2 Collector, Pin 3 Emitter
    comp.terminals = {
        PhysicalTerminal3D("BASE", 1, ElectricalPinPolarity::PassiveBiDirectional, Point3D{0.0, 0.0, -5.0}, Vec3d{0.0, 0.0, -1.0}, "lead_b"),
        PhysicalTerminal3D("COLLECTOR", 2, ElectricalPinPolarity::Positive, Point3D{-1.27, 0.0, -5.0}, Vec3d{0.0, 0.0, -1.0}, "lead_c"),
        PhysicalTerminal3D("EMITTER", 3, ElectricalPinPolarity::Negative, Point3D{1.27, 0.0, -5.0}, Vec3d{0.0, 0.0, -1.0}, "lead_e")
    };

    comp.spice_model.netlist_template = "Q1 2 1 3 2N3904 (BF=" + std::to_string(beta_current_gain) +
        " VCEO=" + std::to_string(max_v_ce_volts) + " ICMAX=" + std::to_string(max_collector_current_a) + ")";

    auto sym = SchematicSymbolSynthesizer::synthesize_symbol(eng::ComponentKind::BJT_NPN, standard);

    PinTerminalMapper mapper;
    mapper.initialize(comp.terminals, sym);
    (void)mapper.auto_map_by_number_and_polarity();

    ComponentQualificationInspector::QualificationCertificate cert;
    cert.is_qualified = true;
    cert.certification_id = "CERT-BJT-NPN-" + core::base::Uuid::generate_random().to_string().substr(0, 8);

    return ComponentManifestSerializer::create_manifest(comp, sym, mapper, cert);
}

// ============================================================================
// Component 12: PNP BJT (PH4-07.03.002)
// ============================================================================

ComponentManifest TransistorComponentsFactory::create_pnp_bjt(
    double beta_current_gain,
    double max_collector_current_a,
    double max_v_ce_volts,
    std::string_view standard
) {
    ComponentAuthoringData comp;
    comp.name = "PNP_BJT_2N3906";
    comp.kind = eng::ComponentKind::BJT_PNP;
    comp.metadata.manufacturer = "Shunya Discrete Electronics";
    comp.metadata.part_number = "2N3906";
    comp.metadata.package_type = "TO-92 Plastic";
    comp.metadata.description = "PNP General Purpose Complementary Bipolar Junction Transistor";

    comp.specs.nominal_primary_value = beta_current_gain;
    comp.specs.max_voltage_v = max_v_ce_volts;
    comp.specs.max_current_a = max_collector_current_a;
    comp.specs.max_power_dissipation_w = 0.625;

    comp.selected_material_names = {"Silicon", "Copper", "Alumina"};
    comp.mesh_geometry = ParametricPrimitiveGenerator::generate_cylinder(
        CylinderParams{.radius = 2.4, .height = 4.8, .radial_segments = 16}
    );

    comp.regions["lead_b"] = MeshRegionMaterial{"lead_b", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["lead_c"] = MeshRegionMaterial{"lead_c", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["lead_e"] = MeshRegionMaterial{"lead_e", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["package_body"] = MeshRegionMaterial{"package_body", "Alumina", 1.0e-12, 0.8, 1900.0, 3.5, 1.0, false, {}};

    comp.terminals = {
        PhysicalTerminal3D("BASE", 1, ElectricalPinPolarity::PassiveBiDirectional, Point3D{0.0, 0.0, -5.0}, Vec3d{0.0, 0.0, -1.0}, "lead_b"),
        PhysicalTerminal3D("COLLECTOR", 2, ElectricalPinPolarity::Positive, Point3D{-1.27, 0.0, -5.0}, Vec3d{0.0, 0.0, -1.0}, "lead_c"),
        PhysicalTerminal3D("EMITTER", 3, ElectricalPinPolarity::Negative, Point3D{1.27, 0.0, -5.0}, Vec3d{0.0, 0.0, -1.0}, "lead_e")
    };

    comp.spice_model.netlist_template = "Q1 2 1 3 2N3906 (BF=" + std::to_string(beta_current_gain) +
        " VCEO=" + std::to_string(max_v_ce_volts) + " ICMAX=" + std::to_string(max_collector_current_a) + ")";

    auto sym = SchematicSymbolSynthesizer::synthesize_symbol(eng::ComponentKind::BJT_PNP, standard);

    PinTerminalMapper mapper;
    mapper.initialize(comp.terminals, sym);
    (void)mapper.auto_map_by_number_and_polarity();

    ComponentQualificationInspector::QualificationCertificate cert;
    cert.is_qualified = true;
    cert.certification_id = "CERT-BJT-PNP-" + core::base::Uuid::generate_random().to_string().substr(0, 8);

    return ComponentManifestSerializer::create_manifest(comp, sym, mapper, cert);
}

// ============================================================================
// Component 13: N-Channel MOSFET (PH4-07.03.003)
// ============================================================================

ComponentManifest TransistorComponentsFactory::create_n_mosfet(
    double v_threshold_volts,
    double r_ds_on_ohms,
    double max_drain_current_a,
    double max_v_ds_volts,
    std::string_view standard
) {
    ComponentAuthoringData comp;
    comp.name = "N_MOSFET_IRFZ44N";
    comp.kind = eng::ComponentKind::MOSFET_N;
    comp.metadata.manufacturer = "Shunya Power Systems";
    comp.metadata.part_number = "IRFZ44N";
    comp.metadata.package_type = "TO-220AB";
    comp.metadata.description = "N-Channel Power HEXFET MOSFET 55V 49A 17.5mOhm";

    comp.specs.nominal_primary_value = v_threshold_volts;
    comp.specs.max_voltage_v = max_v_ds_volts;
    comp.specs.max_current_a = max_drain_current_a;
    comp.specs.max_power_dissipation_w = 94.0; // 94 Watts tab-mounted

    comp.selected_material_names = {"Silicon", "Copper", "Alumina"};
    // TO-220 rectangular body
    comp.mesh_geometry = ParametricPrimitiveGenerator::generate_cube(
        CubeParams{.width = 10.0, .height = 15.0, .depth = 4.5}
    );

    comp.regions["lead_g"] = MeshRegionMaterial{"lead_g", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["lead_d"] = MeshRegionMaterial{"lead_d", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["lead_s"] = MeshRegionMaterial{"lead_s", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["tab_heat"] = MeshRegionMaterial{"tab_heat", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["case_body"] = MeshRegionMaterial{"case_body", "Alumina", 1.0e-12, 1.0, 1800.0, 3.8, 1.0, false, {}};

    // 3 Pins: Pin 1 Gate, Pin 2 Drain, Pin 3 Source
    comp.terminals = {
        PhysicalTerminal3D("GATE", 1, ElectricalPinPolarity::PassiveBiDirectional, Point3D{-2.54, 0.0, -14.0}, Vec3d{0.0, 0.0, -1.0}, "lead_g"),
        PhysicalTerminal3D("DRAIN", 2, ElectricalPinPolarity::Positive, Point3D{0.0, 0.0, -14.0}, Vec3d{0.0, 0.0, -1.0}, "lead_d"),
        PhysicalTerminal3D("SOURCE", 3, ElectricalPinPolarity::Negative, Point3D{2.54, 0.0, -14.0}, Vec3d{0.0, 0.0, -1.0}, "lead_s")
    };

    comp.spice_model.netlist_template = "M1 2 1 3 3 IRFZ44N (VTO=" + std::to_string(v_threshold_volts) +
        " RD=" + std::to_string(r_ds_on_ohms) + " RS=0.005)";

    auto sym = SchematicSymbolSynthesizer::synthesize_symbol(eng::ComponentKind::MOSFET_N, standard);

    PinTerminalMapper mapper;
    mapper.initialize(comp.terminals, sym);
    (void)mapper.auto_map_by_number_and_polarity();

    ComponentQualificationInspector::QualificationCertificate cert;
    cert.is_qualified = true;
    cert.certification_id = "CERT-NMOS-" + core::base::Uuid::generate_random().to_string().substr(0, 8);

    return ComponentManifestSerializer::create_manifest(comp, sym, mapper, cert);
}

// ============================================================================
// Component 14: P-Channel MOSFET (PH4-07.03.004)
// ============================================================================

ComponentManifest TransistorComponentsFactory::create_p_mosfet(
    double v_threshold_volts,
    double r_ds_on_ohms,
    double max_drain_current_a,
    double max_v_ds_volts,
    std::string_view standard
) {
    ComponentAuthoringData comp;
    comp.name = "P_MOSFET_IRF9540";
    comp.kind = eng::ComponentKind::MOSFET_P;
    comp.metadata.manufacturer = "Shunya Power Systems";
    comp.metadata.part_number = "IRF9540";
    comp.metadata.package_type = "TO-220AB";
    comp.metadata.description = "P-Channel Power HEXFET MOSFET -100V -19A 0.117 Ohm";

    comp.specs.nominal_primary_value = v_threshold_volts;
    comp.specs.max_voltage_v = max_v_ds_volts;
    comp.specs.max_current_a = max_drain_current_a;
    comp.specs.max_power_dissipation_w = 94.0;

    comp.selected_material_names = {"Silicon", "Copper", "Alumina"};
    comp.mesh_geometry = ParametricPrimitiveGenerator::generate_cube(
        CubeParams{.width = 10.0, .height = 15.0, .depth = 4.5}
    );

    comp.regions["lead_g"] = MeshRegionMaterial{"lead_g", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["lead_d"] = MeshRegionMaterial{"lead_d", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["lead_s"] = MeshRegionMaterial{"lead_s", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["tab_heat"] = MeshRegionMaterial{"tab_heat", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["case_body"] = MeshRegionMaterial{"case_body", "Alumina", 1.0e-12, 1.0, 1800.0, 3.8, 1.0, false, {}};

    comp.terminals = {
        PhysicalTerminal3D("GATE", 1, ElectricalPinPolarity::PassiveBiDirectional, Point3D{-2.54, 0.0, -14.0}, Vec3d{0.0, 0.0, -1.0}, "lead_g"),
        PhysicalTerminal3D("DRAIN", 2, ElectricalPinPolarity::Positive, Point3D{0.0, 0.0, -14.0}, Vec3d{0.0, 0.0, -1.0}, "lead_d"),
        PhysicalTerminal3D("SOURCE", 3, ElectricalPinPolarity::Negative, Point3D{2.54, 0.0, -14.0}, Vec3d{0.0, 0.0, -1.0}, "lead_s")
    };

    comp.spice_model.netlist_template = "M1 2 1 3 3 IRF9540 (VTO=" + std::to_string(v_threshold_volts) +
        " RD=" + std::to_string(r_ds_on_ohms) + " RS=0.010)";

    auto sym = SchematicSymbolSynthesizer::synthesize_symbol(eng::ComponentKind::MOSFET_P, standard);

    PinTerminalMapper mapper;
    mapper.initialize(comp.terminals, sym);
    (void)mapper.auto_map_by_number_and_polarity();

    ComponentQualificationInspector::QualificationCertificate cert;
    cert.is_qualified = true;
    cert.certification_id = "CERT-PMOS-" + core::base::Uuid::generate_random().to_string().substr(0, 8);

    return ComponentManifestSerializer::create_manifest(comp, sym, mapper, cert);
}

// ============================================================================
// Component 15: N-Channel JFET (PH4-07.03.005)
// ============================================================================

ComponentManifest TransistorComponentsFactory::create_n_jfet(
    double i_dss_amperes,
    double v_pinchoff_volts,
    double max_drain_current_a,
    std::string_view standard
) {
    ComponentAuthoringData comp;
    comp.name = "N_JFET_2N5457";
    comp.kind = eng::ComponentKind::JFET_N;
    comp.metadata.manufacturer = "Shunya Discrete Electronics";
    comp.metadata.part_number = "2N5457";
    comp.metadata.package_type = "TO-92 Plastic";
    comp.metadata.description = "N-Channel General Purpose Audio/RF JFET";

    comp.specs.nominal_primary_value = i_dss_amperes;
    comp.specs.max_voltage_v = 25.0; // V_DS max
    comp.specs.max_current_a = max_drain_current_a;
    comp.specs.max_power_dissipation_w = 0.310; // 310 mW

    comp.selected_material_names = {"Silicon", "Copper", "Alumina"};
    comp.mesh_geometry = ParametricPrimitiveGenerator::generate_cylinder(
        CylinderParams{.radius = 2.4, .height = 4.8, .radial_segments = 16}
    );

    comp.regions["lead_d"] = MeshRegionMaterial{"lead_d", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["lead_s"] = MeshRegionMaterial{"lead_s", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["lead_g"] = MeshRegionMaterial{"lead_g", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["package_body"] = MeshRegionMaterial{"package_body", "Alumina", 1.0e-12, 0.8, 1900.0, 3.5, 1.0, false, {}};

    // 3 Pins: Pin 1 Gate, Pin 2 Drain, Pin 3 Source
    comp.terminals = {
        PhysicalTerminal3D("GATE", 1, ElectricalPinPolarity::PassiveBiDirectional, Point3D{0.0, 0.0, -5.0}, Vec3d{0.0, 0.0, -1.0}, "lead_g"),
        PhysicalTerminal3D("DRAIN", 2, ElectricalPinPolarity::Positive, Point3D{-1.27, 0.0, -5.0}, Vec3d{0.0, 0.0, -1.0}, "lead_d"),
        PhysicalTerminal3D("SOURCE", 3, ElectricalPinPolarity::Negative, Point3D{1.27, 0.0, -5.0}, Vec3d{0.0, 0.0, -1.0}, "lead_s")
    };

    comp.spice_model.netlist_template = "J1 2 1 3 2N5457 (VTO=" + std::to_string(v_pinchoff_volts) +
        " BETA=" + std::to_string(i_dss_amperes / (v_pinchoff_volts * v_pinchoff_volts)) + ")";

    auto sym = SchematicSymbolSynthesizer::synthesize_symbol(eng::ComponentKind::JFET_N, standard);

    PinTerminalMapper mapper;
    mapper.initialize(comp.terminals, sym);
    (void)mapper.auto_map_by_number_and_polarity();

    ComponentQualificationInspector::QualificationCertificate cert;
    cert.is_qualified = true;
    cert.certification_id = "CERT-NJFET-" + core::base::Uuid::generate_random().to_string().substr(0, 8);

    return ComponentManifestSerializer::create_manifest(comp, sym, mapper, cert);
}

// ============================================================================
// Component 16: P-Channel JFET (PH4-07.03.006)
// ============================================================================

ComponentManifest TransistorComponentsFactory::create_p_jfet(
    double i_dss_amperes,
    double v_pinchoff_volts,
    double max_drain_current_a,
    std::string_view standard
) {
    ComponentAuthoringData comp;
    comp.name = "P_JFET_2N5460";
    comp.kind = eng::ComponentKind::JFET_P;
    comp.metadata.manufacturer = "Shunya Discrete Electronics";
    comp.metadata.part_number = "2N5460";
    comp.metadata.package_type = "TO-92 Plastic";
    comp.metadata.description = "P-Channel General Purpose Audio/Low-Noise JFET";

    comp.specs.nominal_primary_value = i_dss_amperes;
    comp.specs.max_voltage_v = 40.0;
    comp.specs.max_current_a = max_drain_current_a;
    comp.specs.max_power_dissipation_w = 0.310;

    comp.selected_material_names = {"Silicon", "Copper", "Alumina"};
    comp.mesh_geometry = ParametricPrimitiveGenerator::generate_cylinder(
        CylinderParams{.radius = 2.4, .height = 4.8, .radial_segments = 16}
    );

    comp.regions["lead_d"] = MeshRegionMaterial{"lead_d", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["lead_s"] = MeshRegionMaterial{"lead_s", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["lead_g"] = MeshRegionMaterial{"lead_g", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["package_body"] = MeshRegionMaterial{"package_body", "Alumina", 1.0e-12, 0.8, 1900.0, 3.5, 1.0, false, {}};

    comp.terminals = {
        PhysicalTerminal3D("GATE", 1, ElectricalPinPolarity::PassiveBiDirectional, Point3D{0.0, 0.0, -5.0}, Vec3d{0.0, 0.0, -1.0}, "lead_g"),
        PhysicalTerminal3D("DRAIN", 2, ElectricalPinPolarity::Positive, Point3D{-1.27, 0.0, -5.0}, Vec3d{0.0, 0.0, -1.0}, "lead_d"),
        PhysicalTerminal3D("SOURCE", 3, ElectricalPinPolarity::Negative, Point3D{1.27, 0.0, -5.0}, Vec3d{0.0, 0.0, -1.0}, "lead_s")
    };

    comp.spice_model.netlist_template = "J1 2 1 3 2N5460 (VTO=" + std::to_string(v_pinchoff_volts) +
        " BETA=" + std::to_string(i_dss_amperes / (v_pinchoff_volts * v_pinchoff_volts)) + ")";

    auto sym = SchematicSymbolSynthesizer::synthesize_symbol(eng::ComponentKind::JFET_P, standard);

    PinTerminalMapper mapper;
    mapper.initialize(comp.terminals, sym);
    (void)mapper.auto_map_by_number_and_polarity();

    ComponentQualificationInspector::QualificationCertificate cert;
    cert.is_qualified = true;
    cert.certification_id = "CERT-PJFET-" + core::base::Uuid::generate_random().to_string().substr(0, 8);

    return ComponentManifestSerializer::create_manifest(comp, sym, mapper, cert);
}

} // namespace shunya::lib30
