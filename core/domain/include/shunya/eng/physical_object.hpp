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
    ExtrudedProfile2D,
    Mesh3D
};

[[nodiscard]] constexpr std::string_view object_shape_kind_to_string(ObjectShapeKind k) noexcept {
    switch (k) {
    case ObjectShapeKind::Box:               return "Box";
    case ObjectShapeKind::Sphere:            return "Sphere";
    case ObjectShapeKind::Cylinder:          return "Cylinder";
    case ObjectShapeKind::Cone:              return "Cone";
    case ObjectShapeKind::ExtrudedProfile2D: return "ExtrudedProfile2D";
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
    double cross_section_area_m2{0.0};
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
};

struct ObjectRegion {
    std::string region_id{"whole"};
    std::string name{"Primary Body"};
    core::base::Uuid material_id;
    double volume_fraction{1.0};
    double thickness_m{0.0};
    bool is_conductive_path{true};
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

    bool remove_object(const core::base::Uuid& id);
    [[nodiscard]] const PhysicalObject* get_object(const core::base::Uuid& id) const noexcept;
    [[nodiscard]] PhysicalObject* get_object_mut(const core::base::Uuid& id) noexcept;
    [[nodiscard]] const PhysicalObject* find_by_name(std::string_view name) const noexcept;
    [[nodiscard]] const std::vector<PhysicalObject>& objects() const noexcept { return objects_; }
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
