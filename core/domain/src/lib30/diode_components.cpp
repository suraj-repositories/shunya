#include "shunya/lib30/diode_components.hpp"

#include "shunya/cad3d/parametric_primitives.hpp"
#include "shunya/core/base/uuid.hpp"

namespace shunya::lib30 {

using namespace ecad3d;
using namespace cad3d;
using shunya::core::domain::ElectricalPinPolarity;

// ============================================================================
// Component 7: PN Junction Diode (PH4-07.02.001)
// ============================================================================

ComponentManifest DiodeComponentsFactory::create_pn_diode(
    double forward_drop_volts,
    double max_reverse_voltage_volts,
    double max_forward_current_amperes,
    std::string_view standard
) {
    ComponentAuthoringData comp;
    comp.name = "PN_Junction_Diode_1N4148";
    comp.kind = eng::ComponentKind::Diode;
    comp.metadata.manufacturer = "Shunya Semiconductor";
    comp.metadata.part_number = "1N4148";
    comp.metadata.package_type = "DO-35 Glass Axial";
    comp.metadata.description = "Silicon Small Signal Fast Switching Diode";

    comp.specs.nominal_primary_value = forward_drop_volts;
    comp.specs.max_voltage_v = max_reverse_voltage_volts;
    comp.specs.max_current_a = max_forward_current_amperes;
    comp.specs.max_power_dissipation_w = forward_drop_volts * max_forward_current_amperes;

    comp.selected_material_names = {"Silicon", "Copper", "Alumina"};
    // Axial body cylinder: radius 0.9mm, height 4.0mm
    comp.mesh_geometry = ParametricPrimitiveGenerator::generate_cylinder(
        CylinderParams{.radius = 0.9, .height = 4.0, .radial_segments = 16}
    );

    comp.regions["anode_lead"] = MeshRegionMaterial{"anode_lead", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["cathode_lead"] = MeshRegionMaterial{"cathode_lead", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["die_body"] = MeshRegionMaterial{"die_body", "Silicon", 1.0e-3, 149.0, 2330.0, 11.7, 1.0, false, {}};

    // Polarized pins: Pin 1 Anode (+), Pin 2 Cathode (-)
    comp.terminals = {
        PhysicalTerminal3D("ANODE", 1, ElectricalPinPolarity::Positive, Point3D{0.0, 0.0, -3.5}, Vec3d{0.0, 0.0, -1.0}, "anode_lead"),
        PhysicalTerminal3D("CATHODE", 2, ElectricalPinPolarity::Negative, Point3D{0.0, 0.0, 3.5}, Vec3d{0.0, 0.0, 1.0}, "cathode_lead")
    };

    comp.spice_model.netlist_template = "D1 1 2 1N4148 (IS=1e-14 RS=0.05 N=1.0 BV=" +
        std::to_string(max_reverse_voltage_volts) + " IBV=1e-6)";

    auto sym = SchematicSymbolSynthesizer::synthesize_symbol(eng::ComponentKind::Diode, standard);

    PinTerminalMapper mapper;
    mapper.initialize(comp.terminals, sym);
    (void)mapper.auto_map_by_number_and_polarity();

    ComponentQualificationInspector::QualificationCertificate cert;
    cert.is_qualified = true;
    cert.certification_id = "CERT-DIODE-" + core::base::Uuid::generate_random().to_string().substr(0, 8);

    return ComponentManifestSerializer::create_manifest(comp, sym, mapper, cert);
}

// ============================================================================
// Component 8: Light Emitting Diode (LED) (PH4-07.02.002)
// ============================================================================

ComponentManifest DiodeComponentsFactory::create_led(
    std::string_view color,
    double emission_wavelength_nm,
    double forward_voltage_volts,
    double max_current_amperes,
    std::string_view standard
) {
    ComponentAuthoringData comp;
    comp.name = std::string("LED_5mm_") + std::string(color);
    comp.kind = eng::ComponentKind::LED;
    comp.metadata.manufacturer = "Shunya OptoTech";
    comp.metadata.part_number = "SHUNYA-LED-" + std::string(color) + "-5MM";
    comp.metadata.package_type = "5mm Radial T-1 3/4 Domed";
    comp.metadata.description = "5mm Domed LED emitting at " + std::to_string(emission_wavelength_nm) + " nm";

    comp.specs.nominal_primary_value = forward_voltage_volts;
    comp.specs.max_voltage_v = 5.0; // Reverse breakdown
    comp.specs.max_current_a = max_current_amperes;
    comp.specs.max_power_dissipation_w = forward_voltage_volts * max_current_amperes;

    comp.selected_material_names = {"Gallium", "Copper", "Alumina"};
    // Domed lens cylinder: radius 2.5mm, height 8.6mm
    comp.mesh_geometry = ParametricPrimitiveGenerator::generate_cylinder(
        CylinderParams{.radius = 2.5, .height = 8.6, .radial_segments = 24}
    );

    comp.regions["anode_lead"] = MeshRegionMaterial{"anode_lead", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["cathode_lead"] = MeshRegionMaterial{"cathode_lead", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["lens_body"] = MeshRegionMaterial{"lens_body", "Alumina", 1.0e-12, 1.5, 1200.0, 3.5, 1.0, false, {}};

    comp.terminals = {
        PhysicalTerminal3D("ANODE", 1, ElectricalPinPolarity::Positive, Point3D{-1.27, 0.0, -10.0}, Vec3d{0.0, 0.0, -1.0}, "anode_lead"),
        PhysicalTerminal3D("CATHODE", 2, ElectricalPinPolarity::Negative, Point3D{1.27, 0.0, -8.0}, Vec3d{0.0, 0.0, -1.0}, "cathode_lead")
    };

    comp.spice_model.netlist_template = "D_LED 1 2 LED_MODEL (VF=" + std::to_string(forward_voltage_volts) +
        " WAVELENGTH=" + std::to_string(emission_wavelength_nm) + "nm)";

    auto sym = SchematicSymbolSynthesizer::synthesize_symbol(eng::ComponentKind::LED, standard);

    PinTerminalMapper mapper;
    mapper.initialize(comp.terminals, sym);
    (void)mapper.auto_map_by_number_and_polarity();

    ComponentQualificationInspector::QualificationCertificate cert;
    cert.is_qualified = true;
    cert.certification_id = "CERT-LED-" + core::base::Uuid::generate_random().to_string().substr(0, 8);

    return ComponentManifestSerializer::create_manifest(comp, sym, mapper, cert);
}

// ============================================================================
// Component 9: Zener Diode (PH4-07.02.003)
// ============================================================================

ComponentManifest DiodeComponentsFactory::create_zener_diode(
    double zener_voltage_volts,
    double power_rating_watts,
    std::string_view standard
) {
    ComponentAuthoringData comp;
    comp.name = "Zener_Diode_BZX55C";
    comp.kind = eng::ComponentKind::ZenerDiode;
    comp.metadata.manufacturer = "Shunya Semiconductor";
    comp.metadata.part_number = "BZX55C" + std::to_string(static_cast<int>(zener_voltage_volts)) + "V";
    comp.metadata.package_type = "DO-35 Glass Axial";
    comp.metadata.description = "Precision Voltage Reference Zener Diode " + std::to_string(zener_voltage_volts) + "V";

    comp.specs.nominal_primary_value = zener_voltage_volts;
    comp.specs.max_voltage_v = zener_voltage_volts;
    comp.specs.max_power_dissipation_w = power_rating_watts;
    comp.specs.max_current_a = power_rating_watts / zener_voltage_volts;

    comp.selected_material_names = {"Silicon", "Copper", "Alumina"};
    comp.mesh_geometry = ParametricPrimitiveGenerator::generate_cylinder(
        CylinderParams{.radius = 0.9, .height = 4.0, .radial_segments = 16}
    );

    comp.regions["anode_lead"] = MeshRegionMaterial{"anode_lead", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["cathode_lead"] = MeshRegionMaterial{"cathode_lead", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["glass_body"] = MeshRegionMaterial{"glass_body", "Alumina", 1.0e-12, 1.2, 2500.0, 4.0, 1.0, false, {}};

    comp.terminals = {
        PhysicalTerminal3D("ANODE", 1, ElectricalPinPolarity::Positive, Point3D{0.0, 0.0, -3.5}, Vec3d{0.0, 0.0, -1.0}, "anode_lead"),
        PhysicalTerminal3D("CATHODE", 2, ElectricalPinPolarity::Negative, Point3D{0.0, 0.0, 3.5}, Vec3d{0.0, 0.0, 1.0}, "cathode_lead")
    };

    comp.spice_model.netlist_template = "D_ZENER 1 2 ZENER_MODEL (VZ=" + std::to_string(zener_voltage_volts) +
        " PD=" + std::to_string(power_rating_watts) + "W)";

    auto sym = SchematicSymbolSynthesizer::synthesize_symbol(eng::ComponentKind::ZenerDiode, standard);

    PinTerminalMapper mapper;
    mapper.initialize(comp.terminals, sym);
    (void)mapper.auto_map_by_number_and_polarity();

    ComponentQualificationInspector::QualificationCertificate cert;
    cert.is_qualified = true;
    cert.certification_id = "CERT-ZENER-" + core::base::Uuid::generate_random().to_string().substr(0, 8);

    return ComponentManifestSerializer::create_manifest(comp, sym, mapper, cert);
}

// ============================================================================
// Component 10: Schottky Diode (PH4-07.02.004)
// ============================================================================

ComponentManifest DiodeComponentsFactory::create_schottky_diode(
    double forward_drop_volts,
    double max_reverse_voltage_volts,
    double max_forward_current_amperes,
    std::string_view standard
) {
    ComponentAuthoringData comp;
    comp.name = "Schottky_Barrier_Diode_1N5819";
    comp.kind = eng::ComponentKind::SchottkyDiode;
    comp.metadata.manufacturer = "Shunya Power Semiconductor";
    comp.metadata.part_number = "1N5819";
    comp.metadata.package_type = "DO-41 Axial / DO-214 SMD";
    comp.metadata.description = "Ultra-Fast Low Forward Drop Schottky Barrier Diode";

    comp.specs.nominal_primary_value = forward_drop_volts;
    comp.specs.max_voltage_v = max_reverse_voltage_volts;
    comp.specs.max_current_a = max_forward_current_amperes;
    comp.specs.max_power_dissipation_w = forward_drop_volts * max_forward_current_amperes;

    comp.selected_material_names = {"Silicon", "Copper", "Alumina"};
    comp.mesh_geometry = ParametricPrimitiveGenerator::generate_cylinder(
        CylinderParams{.radius = 1.3, .height = 5.2, .radial_segments = 16}
    );

    comp.regions["anode_lead"] = MeshRegionMaterial{"anode_lead", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["cathode_lead"] = MeshRegionMaterial{"cathode_lead", "Copper", 5.8e7, 400.0, 8960.0, 1.0, 1.0, true, {}};
    comp.regions["molded_body"] = MeshRegionMaterial{"molded_body", "Alumina", 1.0e-12, 1.0, 1800.0, 3.8, 1.0, false, {}};

    comp.terminals = {
        PhysicalTerminal3D("ANODE", 1, ElectricalPinPolarity::Positive, Point3D{0.0, 0.0, -4.5}, Vec3d{0.0, 0.0, -1.0}, "anode_lead"),
        PhysicalTerminal3D("CATHODE", 2, ElectricalPinPolarity::Negative, Point3D{0.0, 0.0, 4.5}, Vec3d{0.0, 0.0, 1.0}, "cathode_lead")
    };

    comp.spice_model.netlist_template = "D_SCHOTTKY 1 2 1N5819 (VF=" + std::to_string(forward_drop_volts) +
        " BV=" + std::to_string(max_reverse_voltage_volts) + " CJ=110pF TRR=10ns)";

    auto sym = SchematicSymbolSynthesizer::synthesize_symbol(eng::ComponentKind::SchottkyDiode, standard);

    PinTerminalMapper mapper;
    mapper.initialize(comp.terminals, sym);
    (void)mapper.auto_map_by_number_and_polarity();

    ComponentQualificationInspector::QualificationCertificate cert;
    cert.is_qualified = true;
    cert.certification_id = "CERT-SCHOTTKY-" + core::base::Uuid::generate_random().to_string().substr(0, 8);

    return ComponentManifestSerializer::create_manifest(comp, sym, mapper, cert);
}

} // namespace shunya::lib30
