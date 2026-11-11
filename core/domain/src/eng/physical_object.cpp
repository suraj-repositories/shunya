#include "shunya/eng/physical_object.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <regex>
#include <sstream>

namespace shunya::eng {

namespace {

constexpr double kPi = 3.14159265358979323846;

core::base::Error make_obj_error(int32_t code, std::string msg, std::string details = {}) {
    return core::base::Error{
        .code = code,
        .domain = core::base::ErrorDomain::Validation,
        .severity = core::base::Severity::Error,
        .message = std::move(msg),
        .details = std::move(details)
    };
}

std::string escape_json(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else out += c;
    }
    return out;
}

std::string json_str(std::string_view json, std::string_view key) {
    std::regex re("\"" + std::string(key) + "\"\\s*:\\s*\"([^\"]*)\"");
    std::cmatch m;
    if (std::regex_search(json.data(), json.data() + json.size(), m, re)) {
        return m[1].str();
    }
    return {};
}

double json_num(std::string_view json, std::string_view key, double fallback) {
    std::regex re("\"" + std::string(key) + "\"\\s*:\\s*([-+0-9eE\\.]+)");
    std::cmatch m;
    if (std::regex_search(json.data(), json.data() + json.size(), m, re)) {
        try { return std::stod(m[1].str()); } catch (...) { return fallback; }
    }
    return fallback;
}

bool json_bool(std::string_view json, std::string_view key, bool fallback) {
    std::regex re("\"" + std::string(key) + "\"\\s*:\\s*(true|false)");
    std::cmatch m;
    if (std::regex_search(json.data(), json.data() + json.size(), m, re)) {
        return m[1].str() == "true";
    }
    return fallback;
}

} // namespace

core::base::Result<ObjectShapeKind> object_shape_kind_from_string(std::string_view s) {
    if (s == "Box") return ObjectShapeKind::Box;
    if (s == "Sphere") return ObjectShapeKind::Sphere;
    if (s == "Cylinder") return ObjectShapeKind::Cylinder;
    if (s == "Cone") return ObjectShapeKind::Cone;
    if (s == "ExtrudedProfile2D") return ObjectShapeKind::ExtrudedProfile2D;
    if (s == "Mesh3D") return ObjectShapeKind::Mesh3D;
    return make_obj_error(4301, "Unknown ObjectShapeKind: " + std::string(s));
}

core::base::Result<void> ObjectGeometrySpec::validate() const {
    if (scale.x <= 0.0 || scale.y <= 0.0 || scale.z <= 0.0) {
        return make_obj_error(4302, "Object scale factors must be strictly positive");
    }
    switch (kind) {
    case ObjectShapeKind::Box:
        if (dim_x_m <= 0.0 || dim_y_m <= 0.0 || dim_z_m <= 0.0) {
            return make_obj_error(4303, "Box dimensions must be strictly positive");
        }
        break;
    case ObjectShapeKind::Sphere:
        if (radius_m <= 0.0) return make_obj_error(4304, "Sphere radius must be strictly positive");
        break;
    case ObjectShapeKind::Cylinder:
        if (radius_m <= 0.0 || dim_y_m <= 0.0) {
            return make_obj_error(4305, "Cylinder radius and length must be strictly positive");
        }
        break;
    case ObjectShapeKind::Cone:
        if (radius_m <= 0.0 || top_radius_m < 0.0 || dim_y_m <= 0.0) {
            return make_obj_error(4306, "Cone radii and height must be valid and positive");
        }
        break;
    case ObjectShapeKind::ExtrudedProfile2D:
        if (cross_section_area_m2 <= 0.0 || dim_z_m <= 0.0) {
            return make_obj_error(4307, "Extruded 2D profile area and depth must be strictly positive");
        }
        break;
    case ObjectShapeKind::Mesh3D:
        if (!mesh.has_value() || mesh->vertices.empty() || mesh->triangles.empty()) {
            return make_obj_error(4308, "Mesh3D object must have valid vertices and triangles");
        }
        break;
    }
    return core::base::Result<void>::ok();
}

double ObjectGeometrySpec::compute_volume_m3() const noexcept {
    const double s_vol = std::abs(scale.x * scale.y * scale.z);
    switch (kind) {
    case ObjectShapeKind::Box:
        return dim_x_m * dim_y_m * dim_z_m * s_vol;
    case ObjectShapeKind::Sphere:
        return (4.0 / 3.0) * kPi * radius_m * radius_m * radius_m * s_vol;
    case ObjectShapeKind::Cylinder:
        return kPi * radius_m * radius_m * dim_y_m * s_vol;
    case ObjectShapeKind::Cone:
        return (1.0 / 3.0) * kPi * dim_y_m *
               (radius_m * radius_m + radius_m * top_radius_m + top_radius_m * top_radius_m) * s_vol;
    case ObjectShapeKind::ExtrudedProfile2D:
        return cross_section_area_m2 * dim_z_m * s_vol;
    case ObjectShapeKind::Mesh3D:
        return mesh ? (mesh->compute_volume() * s_vol) : 0.0;
    }
    return 0.0;
}

double ObjectGeometrySpec::compute_surface_area_m2() const noexcept {
    switch (kind) {
    case ObjectShapeKind::Box: {
        const double x = dim_x_m * scale.x;
        const double y = dim_y_m * scale.y;
        const double z = dim_z_m * scale.z;
        return 2.0 * (x * y + y * z + x * z);
    }
    case ObjectShapeKind::Sphere: {
        const double r = radius_m * ((scale.x + scale.y + scale.z) / 3.0);
        return 4.0 * kPi * r * r;
    }
    case ObjectShapeKind::Cylinder: {
        const double r = radius_m * 0.5 * (scale.x + scale.z);
        const double h = dim_y_m * scale.y;
        return 2.0 * kPi * r * (r + h);
    }
    case ObjectShapeKind::Cone: {
        const double r1 = radius_m * 0.5 * (scale.x + scale.z);
        const double r2 = top_radius_m * 0.5 * (scale.x + scale.z);
        const double h = dim_y_m * scale.y;
        const double slant = std::hypot(r1 - r2, h);
        return kPi * (r1 * r1 + r2 * r2 + (r1 + r2) * slant);
    }
    case ObjectShapeKind::ExtrudedProfile2D: {
        const double r_eq = std::sqrt(std::max(1e-12, cross_section_area_m2) / kPi);
        return 2.0 * cross_section_area_m2 + 2.0 * kPi * r_eq * dim_z_m;
    }
    case ObjectShapeKind::Mesh3D:
        return mesh ? mesh->compute_surface_area() : 0.0;
    }
    return 0.0;
}

double ObjectGeometrySpec::compute_characteristic_length_m() const noexcept {
    switch (kind) {
    case ObjectShapeKind::Box:
        return std::max({dim_x_m * scale.x, dim_y_m * scale.y, dim_z_m * scale.z});
    case ObjectShapeKind::Sphere:
        return 2.0 * radius_m * scale.x;
    case ObjectShapeKind::Cylinder:
    case ObjectShapeKind::Cone:
        return dim_y_m * scale.y;
    case ObjectShapeKind::ExtrudedProfile2D:
        return dim_z_m * scale.z;
    case ObjectShapeKind::Mesh3D:
        if (mesh) {
            auto bb = mesh->compute_bbox().size();
            return std::max({bb.x * scale.x, bb.y * scale.y, bb.z * scale.z});
        }
        return 0.1;
    }
    return 0.1;
}

double ObjectGeometrySpec::compute_cross_section_area_m2() const noexcept {
    switch (kind) {
    case ObjectShapeKind::Box: {
        const double x = dim_x_m * scale.x;
        const double y = dim_y_m * scale.y;
        const double z = dim_z_m * scale.z;
        const double max_d = std::max({x, y, z});
        if (max_d == x) return y * z;
        if (max_d == y) return x * z;
        return x * y;
    }
    case ObjectShapeKind::Sphere:
        return kPi * radius_m * radius_m * scale.x * scale.z;
    case ObjectShapeKind::Cylinder:
        return kPi * radius_m * radius_m * scale.x * scale.z;
    case ObjectShapeKind::Cone: {
        const double r_avg = 0.5 * (radius_m + top_radius_m);
        return kPi * r_avg * r_avg * scale.x * scale.z;
    }
    case ObjectShapeKind::ExtrudedProfile2D:
        return cross_section_area_m2 * scale.x * scale.y;
    case ObjectShapeKind::Mesh3D: {
        const double l = std::max(1e-9, compute_characteristic_length_m());
        return compute_volume_m3() / l;
    }
    }
    return 1e-4;
}

cad3d::SolidMesh3D ObjectGeometrySpec::generate_solid_mesh() const {
    if (kind == ObjectShapeKind::Mesh3D && mesh.has_value()) {
        return *mesh;
    }

    cad3d::SolidMesh3D out;
    if (kind == ObjectShapeKind::Box || kind == ObjectShapeKind::ExtrudedProfile2D) {
        const double w = (kind == ObjectShapeKind::ExtrudedProfile2D)
            ? std::sqrt(std::max(1e-12, cross_section_area_m2)) : dim_x_m;
        const double h = (kind == ObjectShapeKind::ExtrudedProfile2D) ? w : dim_y_m;
        const double d = dim_z_m;
        const double hx = 0.5 * w * scale.x;
        const double hy = 0.5 * h * scale.y;
        const double hz = 0.5 * d * scale.z;

        out.vertices = {
            {-hx, -hy, -hz}, { hx, -hy, -hz}, { hx,  hy, -hz}, {-hx,  hy, -hz},
            {-hx, -hy,  hz}, { hx, -hy,  hz}, { hx,  hy,  hz}, {-hx,  hy,  hz}
        };
        out.triangles = {
            {0, 2, 1}, {0, 3, 2}, // -Z
            {4, 5, 6}, {4, 6, 7}, // +Z
            {0, 1, 5}, {0, 5, 4}, // -Y
            {3, 7, 6}, {3, 6, 2}, // +Y
            {0, 4, 7}, {0, 7, 3}, // -X
            {1, 2, 6}, {1, 6, 5}  // +X
        };
        return out;
    }

    if (kind == ObjectShapeKind::Cylinder || kind == ObjectShapeKind::Cone) {
        const int segs = 24;
        const double hy = 0.5 * dim_y_m * scale.y;
        const double r_bot = radius_m * 0.5 * (scale.x + scale.z);
        const double r_top = (kind == ObjectShapeKind::Cylinder)
            ? r_bot : top_radius_m * 0.5 * (scale.x + scale.z);

        const uint32_t bot_center = out.add_vertex({0.0, -hy, 0.0});
        const uint32_t top_center = out.add_vertex({0.0,  hy, 0.0});

        std::vector<uint32_t> bot_ring(segs), top_ring(segs);
        for (int i = 0; i < segs; ++i) {
            const double ang = 2.0 * kPi * static_cast<double>(i) / static_cast<double>(segs);
            const double ca = std::cos(ang), sa = std::sin(ang);
            bot_ring[i] = out.add_vertex({r_bot * ca, -hy, r_bot * sa});
            top_ring[i] = out.add_vertex({r_top * ca,  hy, r_top * sa});
        }
        for (int i = 0; i < segs; ++i) {
            const int nxt = (i + 1) % segs;
            out.add_triangle(bot_center, bot_ring[i], bot_ring[nxt]);
            if (r_top > 1e-9) {
                out.add_triangle(top_center, top_ring[nxt], top_ring[i]);
                out.add_triangle(bot_ring[i], top_ring[i], top_ring[nxt]);
                out.add_triangle(bot_ring[i], top_ring[nxt], bot_ring[nxt]);
            } else {
                out.add_triangle(bot_ring[i], top_center, bot_ring[nxt]);
            }
        }
        out.ensure_outward_winding();
        return out;
    }

    // Sphere UV mesh
    const int stacks = 12;
    const int slices = 24;
    const uint32_t south = out.add_vertex({0.0, -radius_m * scale.y, 0.0});
    const uint32_t north = out.add_vertex({0.0,  radius_m * scale.y, 0.0});
    std::vector<std::vector<uint32_t>> rings(stacks - 1, std::vector<uint32_t>(slices));
    for (int st = 1; st < stacks; ++st) {
        const double phi = -0.5 * kPi + kPi * static_cast<double>(st) / static_cast<double>(stacks);
        const double y = radius_m * std::sin(phi) * scale.y;
        const double r = radius_m * std::cos(phi);
        for (int sl = 0; sl < slices; ++sl) {
            const double th = 2.0 * kPi * static_cast<double>(sl) / static_cast<double>(slices);
            rings[st - 1][sl] = out.add_vertex({r * std::cos(th) * scale.x, y, r * std::sin(th) * scale.z});
        }
    }
    for (int sl = 0; sl < slices; ++sl) {
        const int nxt = (sl + 1) % slices;
        out.add_triangle(south, rings[0][nxt], rings[0][sl]);
        out.add_triangle(north, rings[stacks - 2][sl], rings[stacks - 2][nxt]);
    }
    for (int st = 0; st + 1 < stacks - 1; ++st) {
        for (int sl = 0; sl < slices; ++sl) {
            const int nxt = (sl + 1) % slices;
            out.add_triangle(rings[st][sl], rings[st][nxt], rings[st + 1][nxt]);
            out.add_triangle(rings[st][sl], rings[st + 1][nxt], rings[st + 1][sl]);
        }
    }
    out.ensure_outward_winding();
    return out;
}

std::string PhysicalObject::to_json() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"id\": \"" << id.to_string() << "\",\n";
    ss << "  \"name\": \"" << escape_json(name) << "\",\n";
    ss << "  \"category\": \"" << escape_json(category) << "\",\n";
    ss << "  \"shape\": \"" << object_shape_kind_to_string(geometry.kind) << "\",\n";
    ss << "  \"dim_x_m\": " << std::setprecision(10) << geometry.dim_x_m << ",\n";
    ss << "  \"dim_y_m\": " << std::setprecision(10) << geometry.dim_y_m << ",\n";
    ss << "  \"dim_z_m\": " << std::setprecision(10) << geometry.dim_z_m << ",\n";
    ss << "  \"radius_m\": " << std::setprecision(10) << geometry.radius_m << ",\n";
    ss << "  \"top_radius_m\": " << std::setprecision(10) << geometry.top_radius_m << ",\n";
    ss << "  \"cross_section_area_m2\": " << std::setprecision(10) << geometry.cross_section_area_m2 << ",\n";
    ss << "  \"pos_x\": " << geometry.position.x << ", \"pos_y\": " << geometry.position.y << ", \"pos_z\": " << geometry.position.z << ",\n";
    ss << "  \"rot_x\": " << geometry.rotation_deg.x << ", \"rot_y\": " << geometry.rotation_deg.y << ", \"rot_z\": " << geometry.rotation_deg.z << ",\n";
    ss << "  \"scale_x\": " << geometry.scale.x << ", \"scale_y\": " << geometry.scale.y << ", \"scale_z\": " << geometry.scale.z << ",\n";
    ss << "  \"operating_temp_k\": " << operating_temp_k << ",\n";
    ss << "  \"operating_pressure_pa\": " << operating_pressure_pa << ",\n";
    ss << "  \"regions\": [\n";
    for (size_t i = 0; i < regions.size(); ++i) {
        const auto& r = regions[i];
        ss << "    {\"region_id\": \"" << escape_json(r.region_id)
           << "\", \"name\": \"" << escape_json(r.name)
           << "\", \"material_id\": \"" << r.material_id.to_string()
           << "\", \"volume_fraction\": " << r.volume_fraction
           << ", \"thickness_m\": " << r.thickness_m
           << ", \"is_conductive_path\": " << (r.is_conductive_path ? "true" : "false") << "}";
        if (i + 1 < regions.size()) ss << ",";
        ss << "\n";
    }
    ss << "  ]\n";
    ss << "}\n";
    return ss.str();
}

core::base::Result<PhysicalObject> PhysicalObject::from_json(std::string_view src) {
    if (src.empty()) return make_obj_error(4310, "Empty PhysicalObject JSON");
    PhysicalObject obj;
    if (auto id_opt = core::base::Uuid::from_string(json_str(src, "id"))) {
        obj.id = *id_opt;
    }
    std::string nm = json_str(src, "name");
    if (!nm.empty()) obj.name = std::move(nm);
    std::string cat = json_str(src, "category");
    if (!cat.empty()) obj.category = std::move(cat);
    if (auto k = object_shape_kind_from_string(json_str(src, "shape")); k.is_ok()) {
        obj.geometry.kind = k.value();
    }
    obj.geometry.dim_x_m = json_num(src, "dim_x_m", 0.1);
    obj.geometry.dim_y_m = json_num(src, "dim_y_m", 0.1);
    obj.geometry.dim_z_m = json_num(src, "dim_z_m", 0.1);
    obj.geometry.radius_m = json_num(src, "radius_m", 0.05);
    obj.geometry.top_radius_m = json_num(src, "top_radius_m", 0.0);
    obj.geometry.cross_section_area_m2 = json_num(src, "cross_section_area_m2", 0.0);
    obj.geometry.position = {json_num(src, "pos_x", 0.0), json_num(src, "pos_y", 0.0), json_num(src, "pos_z", 0.0)};
    obj.geometry.rotation_deg = {json_num(src, "rot_x", 0.0), json_num(src, "rot_y", 0.0), json_num(src, "rot_z", 0.0)};
    obj.geometry.scale = {json_num(src, "scale_x", 1.0), json_num(src, "scale_y", 1.0), json_num(src, "scale_z", 1.0)};
    obj.operating_temp_k = json_num(src, "operating_temp_k", 298.15);
    obj.operating_pressure_pa = json_num(src, "operating_pressure_pa", 101325.0);

    const size_t rpos = src.find("\"regions\"");
    if (rpos != std::string_view::npos) {
        std::string_view sub = src.substr(rpos);
        std::regex item_re(R"(\{[^\{\}]*\})");
        auto begin = std::regex_iterator(sub.begin(), sub.end(), item_re);
        auto end = std::regex_iterator<std::string_view::const_iterator>();
        for (auto it = begin; it != end; ++it) {
            std::string item = it->str();
            ObjectRegion reg;
            reg.region_id = json_str(item, "region_id");
            reg.name = json_str(item, "name");
            if (auto mid = core::base::Uuid::from_string(json_str(item, "material_id"))) {
                reg.material_id = *mid;
            }
            reg.volume_fraction = json_num(item, "volume_fraction", 1.0);
            reg.thickness_m = json_num(item, "thickness_m", 0.0);
            reg.is_conductive_path = json_bool(item, "is_conductive_path", true);
            obj.regions.push_back(std::move(reg));
        }
    }
    return obj;
}

core::base::Result<ObjectComputedProperties> PhysicalObjectManager::evaluate_properties(
    const PhysicalObject& obj,
    const core::domain::MaterialRegistry& reg) {
    auto g_val = obj.geometry.validate();
    if (g_val.is_err()) return g_val.error();

    if (obj.regions.empty()) {
        return make_obj_error(4311, "PhysicalObject must have at least one material region");
    }

    double vf_sum = 0.0;
    for (const auto& r : obj.regions) {
        if (r.volume_fraction <= 0.0) {
            return make_obj_error(4312, "Region volume_fraction must be strictly positive");
        }
        if (!reg.has_material(r.material_id)) {
            return make_obj_error(4313, "Object region '" + r.name + "' references unknown material UUID");
        }
        vf_sum += r.volume_fraction;
    }
    if (std::abs(vf_sum - 1.0) > 1e-3) {
        return make_obj_error(4314, "Object region volume fractions must sum to 1.0");
    }

    ObjectComputedProperties p;
    p.volume_m3 = obj.geometry.compute_volume_m3();
    p.surface_area_m2 = obj.geometry.compute_surface_area_m2();

    double rho_eff = 0.0;
    double mass_cp_sum = 0.0;
    double k_eff = 0.0;
    double sigma_eff = 0.0;
    double e_eff = 0.0;
    double cte_eff = 0.0;

    for (const auto& r : obj.regions) {
        const auto* mat = reg.get_material(r.material_id);
        const double vf = r.volume_fraction / vf_sum;
        const double rho_0 = std::max(1e-6, mat->density());
        const double alpha = mat->thermal().thermal_expansion_coeff.value_or(0.0);
        const double dT = obj.operating_temp_k - 293.15;
        const double rho_t = rho_0 / std::max(0.5, 1.0 + 3.0 * alpha * dT);

        rho_eff += vf * rho_t;
        const double r_mass = vf * p.volume_m3 * rho_t;
        const double cp = mat->thermal().specific_heat_capacity.value_or(500.0);
        mass_cp_sum += r_mass * cp;
        k_eff += vf * mat->thermal().thermal_conductivity.value_or(1.0);

        double sig = mat->electrical().electrical_conductivity.value_or(0.0);
        if (sig <= 0.0 && mat->electrical().electrical_resistivity.has_value() &&
            *mat->electrical().electrical_resistivity > 0.0) {
            sig = 1.0 / (*mat->electrical().electrical_resistivity);
        }
        if (r.is_conductive_path && sig > 1.0) {
            p.has_conductive_path = true;
            sigma_eff += vf * sig;
        }
        e_eff += vf * mat->mechanical().youngs_modulus_pa.value_or(0.0);
        cte_eff += vf * alpha;

        if (mat->thermal().melting_point_k.has_value() &&
            obj.operating_temp_k >= *mat->thermal().melting_point_k) {
            p.is_solid_at_operating_temp = false;
            p.warnings.push_back("Region '" + r.name + "' (" + mat->name() +
                                 ") exceeds melting point at operating temperature.");
        }
    }

    p.effective_density_kg_m3 = rho_eff;
    p.mass_kg = p.volume_m3 * rho_eff;
    p.thermal_mass_j_k = mass_cp_sum;
    p.effective_specific_heat_j_kgk = (p.mass_kg > 1e-18) ? (mass_cp_sum / p.mass_kg) : 0.0;
    p.effective_thermal_conductivity_w_mk = k_eff;
    p.effective_electrical_conductivity_s_m = sigma_eff;
    p.effective_electrical_resistivity_ohm_m = (sigma_eff > 1e-18) ? (1.0 / sigma_eff) : 1e12;
    p.effective_young_modulus_pa = e_eff;
    p.effective_thermal_expansion_1_k = cte_eff;

    const double len = std::max(1e-9, obj.geometry.compute_characteristic_length_m());
    const double area = std::max(1e-12, obj.geometry.compute_cross_section_area_m2());
    p.axial_electrical_resistance_ohm = (sigma_eff > 1e-18) ? (len / (sigma_eff * area)) : 1e12;
    p.thermal_resistance_k_w = (k_eff > 1e-9) ? (len / (k_eff * area)) : 1e9;
    p.valid = true;
    return p;
}

core::base::Result<core::base::Uuid> PhysicalObjectManager::add_object(
    PhysicalObject obj,
    const core::domain::MaterialRegistry& reg) {
    if (get_object(obj.id) != nullptr) {
        return make_obj_error(4315, "Duplicate PhysicalObject UUID in workspace");
    }
    auto eval = evaluate_properties(obj, reg);
    if (eval.is_err()) return eval.error();
    obj.computed = std::move(eval.value());
    const core::base::Uuid id = obj.id;
    objects_.push_back(std::move(obj));
    return id;
}

core::base::Result<void> PhysicalObjectManager::update_object(
    PhysicalObject obj,
    const core::domain::MaterialRegistry& reg) {
    auto eval = evaluate_properties(obj, reg);
    if (eval.is_err()) return eval.error();
    obj.computed = std::move(eval.value());
    for (auto& existing : objects_) {
        if (existing.id == obj.id) {
            existing = std::move(obj);
            return core::base::Result<void>::ok();
        }
    }
    return make_obj_error(4315, "Object not found for update");
}

bool PhysicalObjectManager::remove_object(const core::base::Uuid& id) {
    auto it = std::remove_if(objects_.begin(), objects_.end(), [&](const PhysicalObject& o) {
        return o.id == id;
    });
    if (it == objects_.end()) return false;
    objects_.erase(it, objects_.end());
    return true;
}

const PhysicalObject* PhysicalObjectManager::get_object(const core::base::Uuid& id) const noexcept {
    for (const auto& o : objects_) {
        if (o.id == id) return &o;
    }
    return nullptr;
}

PhysicalObject* PhysicalObjectManager::get_object_mut(const core::base::Uuid& id) noexcept {
    for (auto& o : objects_) {
        if (o.id == id) return &o;
    }
    return nullptr;
}

const PhysicalObject* PhysicalObjectManager::find_by_name(std::string_view name) const noexcept {
    for (const auto& o : objects_) {
        if (o.name == name) return &o;
    }
    return nullptr;
}

void PhysicalObjectManager::recalculate_for_material(
    const core::base::Uuid& material_id,
    const core::domain::MaterialRegistry& reg) {
    for (auto& obj : objects_) {
        bool uses = false;
        for (const auto& r : obj.regions) {
            if (r.material_id == material_id) {
                uses = true;
                break;
            }
        }
        if (uses) {
            auto eval = evaluate_properties(obj, reg);
            if (eval.is_ok()) {
                obj.computed = std::move(eval.value());
            } else {
                obj.computed.valid = false;
                obj.computed.warnings.push_back(eval.error().message);
            }
        }
    }
}

void PhysicalObjectManager::recalculate_all(const core::domain::MaterialRegistry& reg) {
    for (auto& obj : objects_) {
        auto eval = evaluate_properties(obj, reg);
        if (eval.is_ok()) {
            obj.computed = std::move(eval.value());
        } else {
            obj.computed.valid = false;
        }
    }
}

} // namespace shunya::eng
