#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/ecad3d/component_designer_workspace.hpp"
#include "shunya/ecad3d/component_physics_qualification.hpp"
#include "shunya/ecad3d/symbol_association_engine.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace shunya::ecad3d {

/**
 * @brief Unified electronic component manifest representation.
 * (Roadmap task PH4-06.04.001)
 */
struct ComponentManifest {
    std::string schema_identifier{"shunyacomp"};
    std::string schema_version{"1.0.0"};
    std::string component_guid;

    // Component Metadata
    std::string name;
    eng::ComponentKind kind{eng::ComponentKind::Resistor};
    std::string manufacturer;
    std::string part_number;
    std::string package_type;
    std::string description;

    // Specifications & SPICE
    ComponentElectricalSpecs specs;
    std::string spice_model_equation;

    // Materials, 3D terminals & geometry summary
    std::vector<std::string> material_dependencies;
    std::vector<PhysicalTerminal3D> terminals;
    size_t vertex_count{0};
    size_t triangle_count{0};

    // Coupled 2D schematic symbol & pin mapping
    SchematicSymbol2D symbol_2d;
    std::vector<PinTerminalMappingEntry> pin_mappings;

    // Spatial mounting orientation
    SpatialMountingOrientation mounting_orientation;

    // Qualification certificate
    bool is_qualified{false};
    std::string certification_id;
};

/**
 * @brief Serializer and schema validator for `.shunyacomp.json` bundles.
 */
class ComponentManifestSerializer {
public:
    static constexpr std::string_view SCHEMA_IDENTIFIER = "shunyacomp";
    static constexpr std::string_view CURRENT_VERSION = "1.0.0";

    /**
     * @brief Exports a component manifest to a standard JSON string.
     */
    [[nodiscard]] static std::string to_json(const ComponentManifest& manifest, bool pretty = true);

    /**
     * @brief Deserializes a component manifest from JSON.
     */
    [[nodiscard]] static core::base::Result<ComponentManifest> from_json(std::string_view json_str);

    /**
     * @brief Validates JSON content against the `.shunyacomp.json` schema.
     */
    [[nodiscard]] static core::base::Result<void> validate_schema(std::string_view json_str);

    /**
     * @brief Synthesizes a manifest from completed workspace and qualification results.
     */
    [[nodiscard]] static ComponentManifest create_manifest(
        const ComponentAuthoringData& authoring_data,
        const SchematicSymbol2D& symbol_2d,
        const PinTerminalMapper& mapper,
        const ComponentQualificationInspector::QualificationCertificate& cert
    );
};

} // namespace shunya::ecad3d
