#include <shunya/core/domain/material_assignment.hpp>

#include <algorithm>
#include <regex>
#include <sstream>

namespace shunya::core::domain {

namespace {

base::Error make_error(std::string msg, int32_t code = 1) {
    return base::Error{
        .code = code,
        .domain = base::ErrorDomain::Materials,
        .severity = base::Severity::Error,
        .message = std::move(msg),
        .details = ""
    };
}

} // namespace

MaterialRegistry::MaterialRegistry() = default;

base::Result<void> MaterialRegistry::register_material(Material mat) {
    if (mat.id().is_nil()) {
        return make_error("Cannot register material with nil UUID");
    }
    if (mat.name().empty()) {
        return make_error("Cannot register material with empty name");
    }
    materials_[mat.id()] = std::move(mat);
    return {};
}

bool MaterialRegistry::has_material(const base::Uuid& id) const noexcept {
    return materials_.find(id) != materials_.end();
}

bool MaterialRegistry::has_material(std::string_view name) const noexcept {
    for (const auto& pair : materials_) {
        if (pair.second.name() == name) return true;
    }
    return false;
}

const Material* MaterialRegistry::get_material(const base::Uuid& id) const noexcept {
    auto it = materials_.find(id);
    if (it != materials_.end()) return &it->second;
    return nullptr;
}

const Material* MaterialRegistry::get_material(std::string_view name) const noexcept {
    for (const auto& pair : materials_) {
        if (pair.second.name() == name) return &pair.second;
    }
    return nullptr;
}

base::Result<void> MaterialRegistry::unregister_material(const base::Uuid& id) {
    auto it = materials_.find(id);
    if (it == materials_.end()) {
        return make_error("Material not found in registry: " + id.to_string());
    }
    materials_.erase(it);

    // Remove dangling assignments
    assignments_.erase(
        std::remove_if(assignments_.begin(), assignments_.end(), [&](const MaterialAssignment& a) {
            return a.material_id == id;
        }),
        assignments_.end());

    return {};
}

void MaterialRegistry::register_standard_materials() {
    register_material(Material::structural_steel());
    register_material(Material::aluminium_6061());
    register_material(Material::copper());
    register_material(Material::silicon());
    register_material(Material::air_standard());
    register_material(Material::water_liquid());
}

base::Result<void> MaterialRegistry::assign_material(base::Uuid target_id,
                                                   base::Uuid material_id,
                                                   std::string subdomain,
                                                   int priority) {
    if (target_id.is_nil()) {
        return make_error("Target entity/element ID cannot be nil for material assignment");
    }
    if (!has_material(material_id)) {
        return make_error("Cannot assign unregistered material: " + material_id.to_string());
    }

    // Check if assignment exists for target_id + subdomain
    for (auto& a : assignments_) {
        if (a.target_id == target_id && a.subdomain == subdomain) {
            if (priority >= a.priority) {
                a.material_id = material_id;
                a.priority = priority;
                return {};
            } else {
                return make_error("Existing assignment for subdomain '" + subdomain +
                                  "' has higher priority (" + std::to_string(a.priority) +
                                  " > " + std::to_string(priority) + ")");
            }
        }
    }

    assignments_.push_back({target_id, std::move(subdomain), material_id, priority});
    return {};
}

base::Result<void> MaterialRegistry::unassign_material(base::Uuid target_id, std::string_view subdomain) {
    auto it = std::find_if(assignments_.begin(), assignments_.end(), [&](const MaterialAssignment& a) {
        return a.target_id == target_id && a.subdomain == subdomain;
    });

    if (it == assignments_.end()) {
        return make_error("No material assignment found for target " + target_id.to_string() + " subdomain: " + std::string(subdomain));
    }

    assignments_.erase(it);
    return {};
}

const MaterialAssignment* MaterialRegistry::get_assignment(base::Uuid target_id, std::string_view subdomain) const noexcept {
    const MaterialAssignment* best = nullptr;
    for (const auto& a : assignments_) {
        if (a.target_id == target_id && a.subdomain == subdomain) {
            if (!best || a.priority > best->priority) {
                best = &a;
            }
        }
    }
    return best;
}

const Material* MaterialRegistry::get_material_for_target(base::Uuid target_id, std::string_view subdomain) const noexcept {
    const auto* assign = get_assignment(target_id, subdomain);
    if (!assign && subdomain != "default") {
        // Fall back to default subdomain
        assign = get_assignment(target_id, "default");
    }
    if (assign) {
        return get_material(assign->material_id);
    }
    return nullptr;
}

std::vector<MaterialAssignment> MaterialRegistry::get_assignments_for_target(base::Uuid target_id) const {
    std::vector<MaterialAssignment> res;
    for (const auto& a : assignments_) {
        if (a.target_id == target_id) {
            res.push_back(a);
        }
    }
    return res;
}

void MaterialRegistry::clear_assignments_for_target(base::Uuid target_id) {
    assignments_.erase(
        std::remove_if(assignments_.begin(), assignments_.end(), [&](const MaterialAssignment& a) {
            return a.target_id == target_id;
        }),
        assignments_.end());
}

base::ValidationReport MaterialRegistry::validate_registry() const {
    base::ValidationReport report;

    // Validate each registered material
    for (const auto& pair : materials_) {
        auto mat_report = pair.second.validate();
        report.merge(mat_report);
    }

    // Validate assignments
    for (const auto& a : assignments_) {
        if (a.target_id.is_nil()) {
            report.add_error("MAT_ASSIGN_NIL_TARGET", "Material assignment has nil target ID");
        }
        if (!has_material(a.material_id)) {
            report.add_error("MAT_ASSIGN_MISSING_MATERIAL",
                             "Material assignment references nonexistent material: " + a.material_id.to_string());
        }
    }

    return report;
}

std::string MaterialRegistry::to_json() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"materials\": [\n";
    size_t m_idx = 0;
    for (const auto& pair : materials_) {
        ss << pair.second.to_json();
        if (++m_idx < materials_.size()) ss << ",";
        ss << "\n";
    }
    ss << "  ],\n";
    ss << "  \"assignments\": [\n";
    for (size_t i = 0; i < assignments_.size(); ++i) {
        const auto& a = assignments_[i];
        ss << "    {\n";
        ss << "      \"target_id\": \"" << a.target_id.to_string() << "\",\n";
        ss << "      \"subdomain\": \"" << a.subdomain << "\",\n";
        ss << "      \"material_id\": \"" << a.material_id.to_string() << "\",\n";
        ss << "      \"priority\": " << a.priority << "\n";
        ss << "    }";
        if (i + 1 < assignments_.size()) ss << ",";
        ss << "\n";
    }
    ss << "  ]\n";
    ss << "}";
    return ss.str();
}

base::Result<MaterialRegistry> MaterialRegistry::from_json(std::string_view json_str) {
    std::string s(json_str);
    MaterialRegistry reg;

    // Parse materials array
    size_t mat_arr = s.find("\"materials\"");
    if (mat_arr != std::string::npos) {
        size_t start = s.find('[', mat_arr);
        if (start != std::string::npos) {
            size_t depth = 0;
            size_t obj_start = std::string::npos;
            for (size_t i = start; i < s.size(); ++i) {
                if (s[i] == '{') {
                    if (depth == 0) obj_start = i;
                    depth++;
                } else if (s[i] == '}') {
                    depth--;
                    if (depth == 0 && obj_start != std::string::npos) {
                        std::string mat_json = s.substr(obj_start, i - obj_start + 1);
                        auto m_res = Material::from_json(mat_json);
                        if (m_res.is_ok()) {
                            reg.register_material(m_res.value());
                        }
                        obj_start = std::string::npos;
                    }
                } else if (s[i] == ']' && depth == 0) {
                    break;
                }
            }
        }
    }

    // Parse assignments array
    size_t assign_arr = s.find("\"assignments\"");
    if (assign_arr != std::string::npos) {
        std::regex assign_re(R"re(\{\s*"target_id"\s*:\s*"([^"]+)"\s*,\s*"subdomain"\s*:\s*"([^"]*)"\s*,\s*"material_id"\s*:\s*"([^"]+)"\s*,\s*"priority"\s*:\s*([-+]?[0-9]+)\s*\})re");
        auto it = std::sregex_iterator(s.begin() + assign_arr, s.end(), assign_re);
        auto end = std::sregex_iterator();
        for (; it != end; ++it) {
            auto tid_res = base::Uuid::from_string((*it)[1].str());
            std::string sub = (*it)[2].str();
            auto mid_res = base::Uuid::from_string((*it)[3].str());
            int prio = std::stoi((*it)[4].str());

            if (tid_res.has_value() && mid_res.has_value()) {
                reg.assign_material(*tid_res, *mid_res, std::move(sub), prio);
            }
        }
    }

    return reg;
}

} // namespace shunya::core::domain
