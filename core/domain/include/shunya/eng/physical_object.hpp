#pragma once

#include "shunya/cad3d/solid_mesh.hpp"
#include "shunya/core/base/error.hpp"
#include "shunya/core/base/math.hpp"
#include "shunya/core/base/result.hpp"
#include "shunya/core/base/uuid.hpp"
#include "shunya/core/domain/material_assignment.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace shunya::eng {

enum class ObjectShapeKind : uint8_t {
    Box,
    Sphere,
    Cylinder,
    Cone,
    Torus,
    Capsule,
    Plane,
    Pipe,
    ExtrudedProfile2D,
    RevolvedProfile,
    SweptPath,
    LoftedSurface,
    Mesh3D
};

[[nodiscard]] constexpr std::string_view object_shape_kind_to_string(ObjectShapeKind k) noexcept {
    switch (k) {
    case ObjectShapeKind::Box:               return "Box";
    case ObjectShapeKind::Sphere:            return "Sphere";
    case ObjectShapeKind::Cylinder:          return "Cylinder";
    case ObjectShapeKind::Cone:              return "Cone";
    case ObjectShapeKind::Torus:             return "Torus";
    case ObjectShapeKind::Capsule:           return "Capsule";
    case ObjectShapeKind::Plane:             return "Plane";
    case ObjectShapeKind::Pipe:              return "Pipe";
    case ObjectShapeKind::ExtrudedProfile2D: return "ExtrudedProfile2D";
    case ObjectShapeKind::RevolvedProfile:   return "RevolvedProfile";
    case ObjectShapeKind::SweptPath:         return "SweptPath";
    case ObjectShapeKind::LoftedSurface:     return "LoftedSurface";
    case ObjectShapeKind::Mesh3D:            return "Mesh3D";
    }
    return "Box";
}

[[nodiscard]] core::base::Result<ObjectShapeKind> object_shape_kind_from_string(std::string_view s);

struct ObjectGeometrySpec {
    ObjectShapeKind kind{ObjectShapeKind::Box};
    double dim_x_m{0.1};
    double dim_y_m{0.1};
    double dim_z_m{0.1};
    double radius_m{0.05};
    double top_radius_m{0.0};
    double wall_thickness_m{0.005};  // For Pipe, Hollow bodies, Shells
    double minor_radius_m{0.01};      // For Torus minor tube radius
    int helical_turns{10};           // For SweptPath / Helix coils
    double helical_pitch_m{0.005};    // For SweptPath / Helix pitch
    double cross_section_area_m2{0.0};
    bool is_hollow{false};            // Shell / hollow body flag
    std::optional<cad3d::SolidMesh3D> mesh;

    core::base::math::Vector3<double> position{0.0, 0.0, 0.0};
    core::base::math::Vector3<double> rotation_deg{0.0, 0.0, 0.0};
    core::base::math::Vector3<double> scale{1.0, 1.0, 1.0};

    [[nodiscard]] core::base::Result<void> validate() const;
    [[nodiscard]] double compute_volume_m3() const noexcept;
    [[nodiscard]] double compute_surface_area_m2() const noexcept;
    [[nodiscard]] double compute_characteristic_length_m() const noexcept;
    [[nodiscard]] double compute_cross_section_area_m2() const noexcept;
    [[nodiscard]] cad3d::SolidMesh3D generate_solid_mesh() const;

    // Parametric primitive builders
    static ObjectGeometrySpec make_box(double x, double y, double z);
    static ObjectGeometrySpec make_cylinder(double radius, double length);
    static ObjectGeometrySpec make_sphere(double radius);
    static ObjectGeometrySpec make_cone(double bottom_radius, double top_radius, double height);
    static ObjectGeometrySpec make_pipe(double outer_radius, double wall_thickness, double length);
    static ObjectGeometrySpec make_torus(double major_radius, double minor_radius);
    static ObjectGeometrySpec make_capsule(double radius, double total_length);
    static ObjectGeometrySpec make_helical_coil(double wire_radius, double coil_radius, double coil_height, int turns = 10);
    static ObjectGeometrySpec make_revolved(cad3d::SolidMesh3D m);
    static ObjectGeometrySpec make_extruded(cad3d::SolidMesh3D m);
    static ObjectGeometrySpec make_mesh(cad3d::SolidMesh3D m);

    // Boolean and Shell operations
    [[nodiscard]] static cad3d::SolidMesh3D boolean_union(const cad3d::SolidMesh3D& a, const cad3d::SolidMesh3D& b);
    [[nodiscard]] static cad3d::SolidMesh3D boolean_difference(const cad3d::SolidMesh3D& a, const cad3d::SolidMesh3D& b);
    [[nodiscard]] static cad3d::SolidMesh3D boolean_intersection(const cad3d::SolidMesh3D& a, const cad3d::SolidMesh3D& b);
    [[nodiscard]] static cad3d::SolidMesh3D shell_solid_mesh(const cad3d::SolidMesh3D& solid, double thickness_m);
};

// Functional Role for Object Sub-Body Regions
enum class RegionFunctionalRole : uint8_t {
    Structural,
    ConductivePath,
    ThermalConductor,
    DielectricSpacer,
    ActiveEmitter
};

[[nodiscard]] constexpr std::string_view region_functional_role_to_string(RegionFunctionalRole r) noexcept {
    switch (r) {
    case RegionFunctionalRole::Structural:        return "Structural";
    case RegionFunctionalRole::ConductivePath:   return "ConductivePath";
    case RegionFunctionalRole::ThermalConductor:  return "ThermalConductor";
    case RegionFunctionalRole::DielectricSpacer:  return "DielectricSpacer";
    case RegionFunctionalRole::ActiveEmitter:     return "ActiveEmitter";
    }
    return "Structural";
}

[[nodiscard]] inline core::base::Result<RegionFunctionalRole> region_functional_role_from_string(std::string_view s) {
    if (s == "Structural") return RegionFunctionalRole::Structural;
    if (s == "ConductivePath" || s == "Conductive") return RegionFunctionalRole::ConductivePath;
    if (s == "ThermalConductor" || s == "Thermal") return RegionFunctionalRole::ThermalConductor;
    if (s == "DielectricSpacer" || s == "Dielectric" || s == "Insulator") return RegionFunctionalRole::DielectricSpacer;
    if (s == "ActiveEmitter" || s == "Emitter") return RegionFunctionalRole::ActiveEmitter;
    return core::base::Error{.code = 4315, .domain = core::base::ErrorDomain::Validation, .severity = core::base::Severity::Error, .message = "Unknown region functional role: " + std::string(s), .details = ""};
}

struct ObjectRegion {
    std::string region_id{"whole"};
    std::string name{"Primary Body"};
    core::base::Uuid material_id;
    double volume_fraction{1.0};
    double thickness_m{0.0};
    bool is_conductive_path{true};
    std::string body_ref;
    RegionFunctionalRole role{RegionFunctionalRole::Structural};
    double operating_temp_k{293.15};
    std::string physical_state{"Solid"};
    std::string color_hex;
    bool is_visible{true};
    bool is_isolated{false};
    bool is_locked{false};

    ObjectRegion() = default;
    ObjectRegion(std::string id, std::string nm, core::base::Uuid mat, double vf = 1.0, double thick = 0.0, bool cond = true,
                 std::string bref = "", RegionFunctionalRole r = RegionFunctionalRole::Structural,
                 double temp = 293.15, std::string state = "Solid", std::string col = "")
        : region_id(std::move(id)), name(std::move(nm)), material_id(std::move(mat)),
          volume_fraction(vf), thickness_m(thick), is_conductive_path(cond),
          body_ref(std::move(bref)), role(r), operating_temp_k(temp), physical_state(std::move(state)),
          color_hex(std::move(col)) {}
};

struct ObjectComputedProperties {
    double volume_m3{0.0};
    double surface_area_m2{0.0};
    double effective_density_kg_m3{0.0};
    double mass_kg{0.0};
    double thermal_mass_j_k{0.0};
    double effective_specific_heat_j_kgk{0.0};
    double effective_thermal_conductivity_w_mk{0.0};
    double effective_electrical_conductivity_s_m{0.0};
    double effective_electrical_resistivity_ohm_m{0.0};
    double axial_electrical_resistance_ohm{0.0};
    double thermal_resistance_k_w{0.0};
    double effective_young_modulus_pa{0.0};
    double effective_thermal_expansion_1_k{0.0};
    // Center of Mass (relative to object geometric center)
    core::base::math::Vector3<double> center_of_mass_m{0.0, 0.0, 0.0};
    // Mass Moment of Inertia Tensor around Center of Mass [kg * m^2]
    double inertia_ixx{0.0};
    double inertia_iyy{0.0};
    double inertia_izz{0.0};
    double inertia_ixy{0.0};
    double inertia_iyz{0.0};
    double inertia_izx{0.0};
    bool has_conductive_path{false};
    bool is_solid_at_operating_temp{true};
    bool valid{false};
    std::vector<std::string> warnings;
};

struct PhysicalObject {
    core::base::Uuid id{core::base::Uuid::generate_random()};
    std::string name{"Object"};
    std::string category{"Custom"};
    ObjectGeometrySpec geometry;
    std::vector<ObjectRegion> regions;
    double operating_temp_k{298.15};
    double operating_pressure_pa{101325.0};
    ObjectComputedProperties computed;

    [[nodiscard]] std::string to_json() const;
    [[nodiscard]] static core::base::Result<PhysicalObject> from_json(std::string_view json_str);
};

/**
 * @brief Manages material-backed 2D/3D physical objects and keeps computed mass, density,
 *        and physical parameters synchronized with MaterialRegistry.
 */
class PhysicalObjectManager {
public:
    PhysicalObjectManager() = default;

    [[nodiscard]] static core::base::Result<ObjectComputedProperties> evaluate_properties(
        const PhysicalObject& obj,
        const core::domain::MaterialRegistry& reg);

    [[nodiscard]] core::base::Result<core::base::Uuid> add_object(
        PhysicalObject obj,
        const core::domain::MaterialRegistry& reg);

    [[nodiscard]] core::base::Result<void> update_object(
        PhysicalObject obj,
        const core::domain::MaterialRegistry& reg);

    [[nodiscard]] std::string generate_unique_name(std::string_view base_name) const;

    [[nodiscard]] core::base::Result<void> rename_object(
        const core::base::Uuid& id,
        std::string_view new_name,
        const core::domain::MaterialRegistry& reg);

    [[nodiscard]] core::base::Result<core::base::Uuid> duplicate_object(
        const core::base::Uuid& id,
        const core::domain::MaterialRegistry& reg);

    bool remove_object(const core::base::Uuid& id);
    [[nodiscard]] const PhysicalObject* get_object(const core::base::Uuid& id) const noexcept;
    [[nodiscard]] PhysicalObject* get_object_mut(const core::base::Uuid& id) noexcept;
    [[nodiscard]] const PhysicalObject* find_by_name(std::string_view name) const noexcept;
    [[nodiscard]] const std::vector<PhysicalObject>& objects() const noexcept { return objects_; }
    [[nodiscard]] std::vector<PhysicalObject>& objects_mut() noexcept { return objects_; }
    [[nodiscard]] size_t count() const noexcept { return objects_.size(); }
    void clear() noexcept { objects_.clear(); }

    // Reactive update when a material definition or mixture changes
    void recalculate_for_material(const core::base::Uuid& material_id,
                                  const core::domain::MaterialRegistry& reg);
    void recalculate_all(const core::domain::MaterialRegistry& reg);

private:
    std::vector<PhysicalObject> objects_;
};

} // namespace shunya::eng
