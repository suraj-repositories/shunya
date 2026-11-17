#pragma once

#include "shunya/cad3d/solid_mesh.hpp"
#include "shunya/core/base/error.hpp"
#include "shunya/core/base/result.hpp"
#include "shunya/core/base/uuid.hpp"
#include "shunya/core/domain/circuit.hpp"
#include "shunya/core/domain/element_definition.hpp"
#include "shunya/eng/component_generator.hpp"
#include "shunya/mat/material_mixing_lab.hpp"
#include "shunya/matdb/element_database.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace shunya::ecad3d {

/**
 * @brief Authoring workflow step indices (1 to 10) in the 3D Component Designer.
 */
enum class AuthoringStep : uint8_t {
    Step1_SelectBaseMaterials = 1,      // Select chemical elements / engineered materials
    Step2_ValidateMaterialProps = 2,    // Verify conductivities, densities, thermal props
    Step3_Construct3DGeometry = 3,      // Build/import solid 3D mesh geometry
    Step4_DefineMeshRegions = 4,        // Partition mesh into semantic regions (leads, body, etc.)
    Step5_AssignRegionMaterials = 5,    // Bind distinct materials to mesh regions
    Step6_ConfigureElectricalSpecs = 6, // Configure ratings (V, I, P, thermal, tolerances)
    Step7_DefinePhysicalTerminals = 7,  // Set 3D contact terminals, snap anchors, polarity
    Step8_DefineSpiceModel = 8,         // Define SPICE behavioral subcircuit / equations
    Step9_SetComponentMetadata = 9,     // Manufacturer, part number, package type
    Step10_RegisterCatalog = 10         // Final validation and catalog registration
};

/**
 * @brief Material binding for a partitioned sub-mesh region.
 */
struct MeshRegionMaterial {
    std::string region_id;              // e.g., "leads", "housing", "core", "die"
    std::string material_name;          // e.g., "Copper", "Alumina", "Silicon"
    double electrical_conductivity{0.0};// S/m
    double thermal_conductivity{0.0};   // W/(m·K)
    double mass_density{0.0};           // kg/m^3
    double relative_permittivity{1.0};
    double relative_permeability{1.0};
    bool is_conductor{false};           // sigma > 1e5 S/m
    std::vector<uint32_t> triangle_indices; // triangles belonging to this region
};

/**
 * @brief Contact terminal on 3D mesh surface with snap anchor.
 */
struct PhysicalTerminal3D {
    std::string terminal_id;            // e.g., "Pin_1", "Anode", "Cathode"
    int pin_number{1};
    core::domain::ElectricalPinPolarity polarity{core::domain::ElectricalPinPolarity::PassiveBiDirectional};
    cad3d::Point3D contact_point{0.0, 0.0, 0.0};
    cad3d::Vec3d normal{0.0, 0.0, 1.0};
    std::string bound_region_id{"leads"};

    PhysicalTerminal3D() = default;
    PhysicalTerminal3D(
        std::string id,
        int num,
        core::domain::ElectricalPinPolarity pol = core::domain::ElectricalPinPolarity::PassiveBiDirectional,
        cad3d::Point3D pt = {0.0, 0.0, 0.0},
        cad3d::Vec3d n = {0.0, 0.0, 1.0},
        std::string reg = "leads"
    ) : terminal_id(std::move(id)), pin_number(num), polarity(pol),
        contact_point(pt), normal(n), bound_region_id(std::move(reg)) {}
};

/**
 * @brief Electrical specification and ratings configured in Step 6.
 */
struct ComponentElectricalSpecs {
    double nominal_primary_value{0.0};  // Resistance (Ohm), Capacitance (F), Inductance (H)
    double tolerance_percent{5.0};      // e.g., +/- 5%
    double max_voltage_v{50.0};         // Maximum operating voltage
    double max_current_a{1.0};          // Maximum continuous current
    double max_power_dissipation_w{0.5};// Maximum power rating
    double max_operating_temp_k{398.15};// Maximum temp (e.g. 125 C)
    double thermal_resistance_j_a{80.0};// R_theta_JA (K/W)
};

/**
 * @brief SPICE behavioral model definition configured in Step 8.
 */
struct ComponentSpiceModel {
    std::string model_type{"analytical"}; // "subcircuit", "primitive", "behavioral"
    std::string subcircuit_name;
    std::string netlist_template;       // e.g., "R1 %1 %2 %VALUE temp=%TEMP"
    std::unordered_map<std::string, double> parameters;
};

/**
 * @brief Metadata configured in Step 9.
 */
struct ComponentMetadata {
    std::string part_number;            // e.g., "CRCW0805100RFKEA"
    std::string manufacturer;          // e.g., "Vishay Dale"
    std::string package_type;          // e.g., "0805", "DIP-8", "TO-220", "Axial"
    std::string description;
    std::vector<std::string> tags;
};

/**
 * @brief Complete authoring document model for an electronic component in the 3D designer.
 */
struct ComponentAuthoringData {
    core::base::Uuid id{core::base::Uuid::generate_random()};
    std::string name{"New_Component"};
    eng::ComponentKind kind{eng::ComponentKind::Resistor};

    // Step 1-2: Selected base materials & elements
    std::vector<std::string> selected_material_names;

    // Step 3: 3D Geometry
    cad3d::SolidMesh3D mesh_geometry;

    // Step 4-5: Regions & Material assignments
    std::unordered_map<std::string, MeshRegionMaterial> regions;

    // Step 6: Electrical & Thermal Specs
    ComponentElectricalSpecs specs;

    // Step 7: Physical 3D Contact Terminals
    std::vector<PhysicalTerminal3D> terminals;

    // Step 8: SPICE Model
    ComponentSpiceModel spice_model;

    // Step 9: Metadata
    ComponentMetadata metadata;

    // State tracking
    AuthoringStep current_step{AuthoringStep::Step1_SelectBaseMaterials};
    bool is_registered{false};
};

/**
 * @brief Workflow coordinator and execution engine for 3D Electronic Component Authoring.
 */
class ComponentDesignerWorkspace {
public:
    ComponentDesignerWorkspace() = default;

    // Lifecycle
    void start_new_component(std::string name, eng::ComponentKind kind);
    [[nodiscard]] const ComponentAuthoringData& data() const noexcept { return data_; }
    [[nodiscard]] ComponentAuthoringData& data() noexcept { return data_; }
    [[nodiscard]] AuthoringStep current_step() const noexcept { return data_.current_step; }

    // Step 1-2: Material Selection & Validation
    core::base::Result<void> step1_select_materials(
        const std::vector<std::string>& material_names,
        const matdb::ElementDatabase* elem_db = nullptr,
        const mat::MaterialMixingLab* mix_lab = nullptr
    );

    // Step 3-5: 3D Geometry & Region Material Assignment
    core::base::Result<void> step3_set_geometry(cad3d::SolidMesh3D mesh);
    core::base::Result<void> step4_define_region(std::string region_id, std::vector<uint32_t> triangles);
    core::base::Result<void> step5_assign_region_material(
        std::string region_id,
        std::string material_name,
        double conductivity,
        double density,
        double thermal_cond = 50.0
    );

    // Step 6: Electrical & Thermal Specifications
    core::base::Result<void> step6_configure_specs(const ComponentElectricalSpecs& specs);

    // Step 7: Contact Terminals & Snap Anchors
    core::base::Result<void> step7_add_terminal(const PhysicalTerminal3D& terminal);
    core::base::Result<void> step7_clear_terminals();

    // Step 8: SPICE Behavioral Model
    core::base::Result<void> step8_configure_spice(const ComponentSpiceModel& model);

    // Step 9: Metadata
    core::base::Result<void> step9_set_metadata(const ComponentMetadata& metadata);

    // Step 10: Validation & Registration
    struct QualificationReport {
        bool is_valid{false};
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
        double total_mass_kg{0.0};
        cad3d::Point3D center_of_mass{0.0, 0.0, 0.0};
    };

    [[nodiscard]] QualificationReport step10_qualify_component() const;
    core::base::Result<void> step10_register_to_catalog();

private:
    ComponentAuthoringData data_;
};

} // namespace shunya::ecad3d
