#pragma once

#include <shunya/core/base/error.hpp>
#include <shunya/core/base/result.hpp>
#include <shunya/core/base/uuid.hpp>
#include <shunya/core/domain/material.hpp>

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace shunya::core::domain {

// MAT-06: Material Assignment
struct MaterialAssignment {
    base::Uuid target_id;             // Entity, body, or element instance UUID
    std::string subdomain{"default"};  // Subdomain, region, boundary or layer
    base::Uuid material_id;           // Material UUID
    int priority{0};                  // Higher priority overrides lower priority

    bool operator==(const MaterialAssignment& other) const noexcept {
        return target_id == other.target_id &&
               subdomain == other.subdomain &&
               material_id == other.material_id &&
               priority == other.priority;
    }
};

class MaterialRegistry {
public:
    MaterialRegistry();

    // Material Catalog
    base::Result<void> register_material(Material mat);
    [[nodiscard]] bool has_material(const base::Uuid& id) const noexcept;
    [[nodiscard]] bool has_material(std::string_view name) const noexcept;
    [[nodiscard]] const Material* get_material(const base::Uuid& id) const noexcept;
    [[nodiscard]] const Material* get_material(std::string_view name) const noexcept;
    base::Result<void> unregister_material(const base::Uuid& id);
    [[nodiscard]] size_t material_count() const noexcept { return materials_.size(); }
    [[nodiscard]] const std::unordered_map<base::Uuid, Material>& materials() const noexcept { return materials_; }

    // Seed preset engineering materials
    void register_standard_materials();

    // MAT-06: Material Assignments
    base::Result<void> assign_material(base::Uuid target_id,
                                       base::Uuid material_id,
                                       std::string subdomain = "default",
                                       int priority = 0);
    base::Result<void> unassign_material(base::Uuid target_id, std::string_view subdomain = "default");
    [[nodiscard]] const MaterialAssignment* get_assignment(base::Uuid target_id, std::string_view subdomain = "default") const noexcept;
    [[nodiscard]] const Material* get_material_for_target(base::Uuid target_id, std::string_view subdomain = "default") const noexcept;
    [[nodiscard]] std::vector<MaterialAssignment> get_assignments_for_target(base::Uuid target_id) const;
    [[nodiscard]] const std::vector<MaterialAssignment>& all_assignments() const noexcept { return assignments_; }
    void clear_assignments_for_target(base::Uuid target_id);

    // MAT-08: Validation
    [[nodiscard]] base::ValidationReport validate_registry() const;

    // JSON Serialization
    [[nodiscard]] std::string to_json() const;
    static base::Result<MaterialRegistry> from_json(std::string_view json_str);

private:
    std::unordered_map<base::Uuid, Material> materials_;
    std::vector<MaterialAssignment> assignments_;
};

} // namespace shunya::core::domain
