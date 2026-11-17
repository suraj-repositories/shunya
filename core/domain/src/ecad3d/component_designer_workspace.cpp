#include "shunya/ecad3d/component_designer_workspace.hpp"

#include <cmath>
#include <unordered_set>

namespace shunya::ecad3d {

namespace {

inline core::base::Result<void> make_fail(std::string msg) {
    core::base::Error err;
    err.message = std::move(msg);
    err.domain = core::base::ErrorDomain::Validation;
    return core::base::Result<void>(std::move(err));
}

inline core::base::Result<void> make_ok() {
    return core::base::Result<void>();
}

} // namespace

void ComponentDesignerWorkspace::start_new_component(std::string name, eng::ComponentKind kind) {
    data_ = ComponentAuthoringData{};
    data_.name = std::move(name);
    data_.kind = kind;
    data_.current_step = AuthoringStep::Step1_SelectBaseMaterials;
}

core::base::Result<void> ComponentDesignerWorkspace::step1_select_materials(
    const std::vector<std::string>& material_names,
    const matdb::ElementDatabase* elem_db,
    const mat::MaterialMixingLab* mix_lab
) {
    (void)mix_lab;
    if (material_names.empty()) {
        return make_fail("Material selection cannot be empty");
    }

    // Verify materials exist if registries are provided
    for (const auto& mat_name : material_names) {
        bool found = false;
        if (elem_db && elem_db->find_by_name(mat_name) != nullptr) {
            found = true;
        }
        if (!found && elem_db && elem_db->find_by_symbol(mat_name) != nullptr) {
            found = true;
        }
        // If neither registry provided or custom material name
        if (elem_db == nullptr && mix_lab == nullptr) {
            found = true;
        }

        if (!found) {
            return make_fail("Material '" + mat_name + "' not found in element database");
        }
    }

    data_.selected_material_names = material_names;
    data_.current_step = AuthoringStep::Step2_ValidateMaterialProps;
    return make_ok();
}

core::base::Result<void> ComponentDesignerWorkspace::step3_set_geometry(cad3d::SolidMesh3D mesh) {
    if (mesh.vertices.empty() || mesh.triangles.empty()) {
        return make_fail("3D mesh geometry cannot be empty");
    }

    data_.mesh_geometry = std::move(mesh);
    data_.current_step = AuthoringStep::Step4_DefineMeshRegions;
    return make_ok();
}

core::base::Result<void> ComponentDesignerWorkspace::step4_define_region(
    std::string region_id,
    std::vector<uint32_t> triangles
) {
    if (region_id.empty()) {
        return make_fail("Region ID cannot be empty");
    }
    if (triangles.empty()) {
        return make_fail("Region must contain at least one triangle");
    }

    // Verify triangle indices are valid for current mesh
    const size_t total_tris = data_.mesh_geometry.triangles.size();
    for (uint32_t tri_idx : triangles) {
        if (tri_idx >= total_tris) {
            return make_fail("Triangle index out of bounds for current 3D mesh");
        }
    }

    MeshRegionMaterial reg;
    reg.region_id = region_id;
    reg.triangle_indices = std::move(triangles);
    data_.regions[region_id] = std::move(reg);

    data_.current_step = AuthoringStep::Step5_AssignRegionMaterials;
    return make_ok();
}

core::base::Result<void> ComponentDesignerWorkspace::step5_assign_region_material(
    std::string region_id,
    std::string material_name,
    double conductivity,
    double density,
    double thermal_cond
) {
    auto it = data_.regions.find(region_id);
    if (it == data_.regions.end()) {
        return make_fail("Region '" + region_id + "' not defined");
    }

    it->second.material_name = std::move(material_name);
    it->second.electrical_conductivity = conductivity;
    it->second.mass_density = density;
    it->second.thermal_conductivity = thermal_cond;
    it->second.is_conductor = (conductivity >= 1e5);

    data_.current_step = AuthoringStep::Step6_ConfigureElectricalSpecs;
    return make_ok();
}

core::base::Result<void> ComponentDesignerWorkspace::step6_configure_specs(const ComponentElectricalSpecs& specs) {
    if (specs.max_voltage_v <= 0.0 || specs.max_current_a <= 0.0) {
        return make_fail("Max voltage and current must be positive");
    }

    data_.specs = specs;
    data_.current_step = AuthoringStep::Step7_DefinePhysicalTerminals;
    return make_ok();
}

core::base::Result<void> ComponentDesignerWorkspace::step7_add_terminal(const PhysicalTerminal3D& terminal) {
    if (terminal.terminal_id.empty()) {
        return make_fail("Terminal ID cannot be empty");
    }

    // Check if terminal references valid conductive region
    auto it = data_.regions.find(terminal.bound_region_id);
    if (it != data_.regions.end() && !it->second.is_conductor) {
        return make_fail("Terminal must bind to a conductive material region (sigma >= 1e5 S/m)");
    }

    data_.terminals.push_back(terminal);
    data_.current_step = AuthoringStep::Step8_DefineSpiceModel;
    return make_ok();
}

core::base::Result<void> ComponentDesignerWorkspace::step7_clear_terminals() {
    data_.terminals.clear();
    return make_ok();
}

core::base::Result<void> ComponentDesignerWorkspace::step8_configure_spice(const ComponentSpiceModel& model) {
    data_.spice_model = model;
    data_.current_step = AuthoringStep::Step9_SetComponentMetadata;
    return make_ok();
}

core::base::Result<void> ComponentDesignerWorkspace::step9_set_metadata(const ComponentMetadata& metadata) {
    data_.metadata = metadata;
    data_.current_step = AuthoringStep::Step10_RegisterCatalog;
    return make_ok();
}

ComponentDesignerWorkspace::QualificationReport ComponentDesignerWorkspace::step10_qualify_component() const {
    QualificationReport rep;
    rep.is_valid = true;

    // Check geometry
    if (data_.mesh_geometry.vertices.empty() || data_.mesh_geometry.triangles.empty()) {
        rep.errors.push_back("Component has no 3D geometry");
        rep.is_valid = false;
    }

    // Check terminals (must have at least 2 terminals for two-terminal components)
    if (data_.terminals.size() < 2) {
        rep.errors.push_back("Electronic component must define at least 2 physical contact terminals");
        rep.is_valid = false;
    }

    // Check terminal names uniqueness
    std::unordered_set<std::string> term_names;
    for (const auto& t : data_.terminals) {
        if (!term_names.insert(t.terminal_id).second) {
            rep.errors.push_back("Duplicate terminal ID: " + t.terminal_id);
            rep.is_valid = false;
        }
    }

    // Check regions (must have at least one conductive region and one housing/insulating region)
    bool has_conductor = false;
    for (const auto& [_, reg] : data_.regions) {
        if (reg.is_conductor) {
            has_conductor = true;
            break;
        }
    }
    if (!has_conductor) {
        rep.errors.push_back("Component lacks conductive terminal material assignment");
        rep.is_valid = false;
    }

    // Calculate total mass
    double total_mass = 0.0;
    cad3d::Point3D com{0.0, 0.0, 0.0};
    double total_vol = data_.mesh_geometry.compute_volume();

    // Default density if not partitioned
    double avg_density = 2500.0; // kg/m3 default ceramic/resin
    if (!data_.regions.empty()) {
        double density_sum = 0.0;
        for (const auto& [_, reg] : data_.regions) {
            if (reg.mass_density > 0.0) {
                density_sum += reg.mass_density;
            }
        }
        avg_density = density_sum / static_cast<double>(data_.regions.size());
    }

    total_mass = total_vol * avg_density * 1e-9; // mm^3 to m^3 conversion factor: (1e-3)^3 = 1e-9
    rep.total_mass_kg = total_mass;

    for (const auto& v : data_.mesh_geometry.vertices) {
        com = com + v;
    }
    if (!data_.mesh_geometry.vertices.empty()) {
        com = com * (1.0 / static_cast<double>(data_.mesh_geometry.vertices.size()));
    }
    rep.center_of_mass = com;

    return rep;
}

core::base::Result<void> ComponentDesignerWorkspace::step10_register_to_catalog() {
    auto qual = step10_qualify_component();
    if (!qual.is_valid) {
        std::string err_str = "Component qualification failed: ";
        for (const auto& e : qual.errors) {
            err_str += "[" + e + "] ";
        }
        return make_fail(err_str);
    }

    data_.is_registered = true;
    return make_ok();
}

} // namespace shunya::ecad3d
