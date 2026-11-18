#include "shunya/eng/physical_object.hpp"
#include "shunya/cad3d/csg_boolean.hpp"

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
    if (s == "Torus") return ObjectShapeKind::Torus;
    if (s == "Capsule") return ObjectShapeKind::Capsule;
    if (s == "Plane") return ObjectShapeKind::Plane;
    if (s == "Pipe") return ObjectShapeKind::Pipe;
    if (s == "ExtrudedProfile2D") return ObjectShapeKind::ExtrudedProfile2D;
    if (s == "RevolvedProfile") return ObjectShapeKind::RevolvedProfile;
    if (s == "SweptPath") return ObjectShapeKind::SweptPath;
    if (s == "LoftedSurface") return ObjectShapeKind::LoftedSurface;
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
    case ObjectShapeKind::Torus:
        if (radius_m <= 0.0 || minor_radius_m <= 0.0) {
            return make_obj_error(4309, "Torus major and minor radii must be strictly positive");
        }
        break;
    case ObjectShapeKind::Capsule:
        if (radius_m <= 0.0 || dim_y_m <= 0.0) {
            return make_obj_error(4310, "Capsule radius and length must be strictly positive");
        }
        break;
    case ObjectShapeKind::Plane:
        if (dim_x_m <= 0.0 || dim_z_m <= 0.0) {
            return make_obj_error(4311, "Plane dimensions must be strictly positive");
        }
        break;
    case ObjectShapeKind::Pipe:
        if (radius_m <= 0.0 || wall_thickness_m <= 0.0 || wall_thickness_m >= radius_m || dim_y_m <= 0.0) {
            return make_obj_error(4312, "Pipe dimensions and wall thickness must be valid and positive");
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
    case ObjectShapeKind::RevolvedProfile:
    case ObjectShapeKind::SweptPath:
    case ObjectShapeKind::LoftedSurface:
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
    case ObjectShapeKind::Pipe: {
        const double r_in = std::max(0.0, radius_m - wall_thickness_m);
        return kPi * (radius_m * radius_m - r_in * r_in) * dim_y_m * s_vol;
    }
    case ObjectShapeKind::Torus: {
        const double r_tube = (minor_radius_m > 0.0) ? minor_radius_m : (0.25 * radius_m);
        return 2.0 * kPi * kPi * radius_m * r_tube * r_tube * s_vol;
    }
    case ObjectShapeKind::Capsule: {
        const double cyl_h = std::max(0.0, dim_y_m - 2.0 * radius_m);
        return (kPi * radius_m * radius_m * cyl_h + (4.0 / 3.0) * kPi * radius_m * radius_m * radius_m) * s_vol;
    }
    case ObjectShapeKind::Plane:
        return dim_x_m * (dim_y_m > 0.0 ? dim_y_m : 0.001) * dim_z_m * s_vol;
    case ObjectShapeKind::ExtrudedProfile2D:
        return cross_section_area_m2 * dim_z_m * s_vol;
    case ObjectShapeKind::SweptPath: {
        const double turns = helical_turns > 0 ? helical_turns : 10;
        const double coil_r = 0.5 * dim_x_m;
        const double coil_h = dim_y_m;
        const double wire_r = radius_m > 0.0 ? radius_m : 0.0005;
        const double coil_len = std::sqrt(std::pow(2.0 * kPi * coil_r * turns, 2.0) + std::pow(coil_h, 2.0));
        return kPi * wire_r * wire_r * coil_len * s_vol;
    }
    case ObjectShapeKind::RevolvedProfile:
    case ObjectShapeKind::LoftedSurface:
    case ObjectShapeKind::Mesh3D:
        return mesh ? (mesh->compute_volume() * s_vol) : (dim_x_m * dim_y_m * dim_z_m * s_vol);
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
    case ObjectShapeKind::Pipe: {
        const double R = radius_m * 0.5 * (scale.x + scale.z);
        const double r = std::max(0.0, R - wall_thickness_m * 0.5 * (scale.x + scale.z));
        const double h = dim_y_m * scale.y;
        return 2.0 * kPi * (R + r) * h + 2.0 * kPi * std::max(0.0, R * R - r * r);
    }
    case ObjectShapeKind::Torus: {
        const double R = radius_m * 0.5 * (scale.x + scale.z);
        const double r = ((minor_radius_m > 0.0) ? minor_radius_m : 0.25 * radius_m) * scale.y;
        return 4.0 * kPi * kPi * R * r;
    }
    case ObjectShapeKind::Capsule: {
        const double R = radius_m * 0.5 * (scale.x + scale.z);
        const double cyl_h = std::max(0.0, dim_y_m * scale.y - 2.0 * R);
        return 2.0 * kPi * R * cyl_h + 4.0 * kPi * R * R;
    }
    case ObjectShapeKind::Plane: {
        const double x = dim_x_m * scale.x;
        const double y = (dim_y_m > 0.0 ? dim_y_m : 0.001) * scale.y;
        const double z = dim_z_m * scale.z;
        return 2.0 * (x * y + y * z + x * z);
    }
    case ObjectShapeKind::ExtrudedProfile2D: {
        const double r_eq = std::sqrt(std::max(1e-12, cross_section_area_m2) / kPi);
        return 2.0 * cross_section_area_m2 + 2.0 * kPi * r_eq * dim_z_m;
    }
    case ObjectShapeKind::SweptPath: {
        const double turns = helical_turns > 0 ? helical_turns : 10;
        const double coil_r = 0.5 * dim_x_m * 0.5 * (scale.x + scale.z);
        const double coil_h = dim_y_m * scale.y;
        const double wire_r = (radius_m > 0.0 ? radius_m : 0.0005) * 0.5 * (scale.x + scale.z);
        const double coil_len = std::sqrt(std::pow(2.0 * kPi * coil_r * turns, 2.0) + std::pow(coil_h, 2.0));
        return 2.0 * kPi * wire_r * coil_len + 2.0 * kPi * wire_r * wire_r;
    }
    case ObjectShapeKind::RevolvedProfile:
    case ObjectShapeKind::LoftedSurface:
    case ObjectShapeKind::Mesh3D:
        return mesh ? mesh->compute_surface_area() : (2.0 * (dim_x_m * dim_y_m + dim_y_m * dim_z_m + dim_x_m * dim_z_m));
    }
    return 0.0;
}

double ObjectGeometrySpec::compute_characteristic_length_m() const noexcept {
    switch (kind) {
    case ObjectShapeKind::Box:
    case ObjectShapeKind::Plane:
        return std::max({dim_x_m * scale.x, dim_y_m * scale.y, dim_z_m * scale.z});
    case ObjectShapeKind::Sphere:
        return 2.0 * radius_m * scale.x;
    case ObjectShapeKind::Cylinder:
    case ObjectShapeKind::Cone:
    case ObjectShapeKind::Pipe:
    case ObjectShapeKind::Capsule:
    case ObjectShapeKind::SweptPath:
        return dim_y_m * scale.y;
    case ObjectShapeKind::Torus:
        return 2.0 * (radius_m + minor_radius_m) * scale.x;
    case ObjectShapeKind::ExtrudedProfile2D:
        return dim_z_m * scale.z;
    case ObjectShapeKind::RevolvedProfile:
    case ObjectShapeKind::LoftedSurface:
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
    case ObjectShapeKind::Box:
    case ObjectShapeKind::Plane: {
        const double x = dim_x_m * scale.x;
        const double y = dim_y_m * scale.y;
        const double z = dim_z_m * scale.z;
        const double max_d = std::max({x, y, z});
        if (max_d == x) return y * z;
        if (max_d == y) return x * z;
        return x * y;
    }
    case ObjectShapeKind::Sphere:
    case ObjectShapeKind::Capsule:
        return kPi * radius_m * radius_m * scale.x * scale.z;
    case ObjectShapeKind::Cylinder:
        return kPi * radius_m * radius_m * scale.x * scale.z;
    case ObjectShapeKind::Pipe: {
        const double R = radius_m * 0.5 * (scale.x + scale.z);
        const double r = std::max(0.0, R - wall_thickness_m * 0.5 * (scale.x + scale.z));
        return kPi * (R * R - r * r);
    }
    case ObjectShapeKind::Torus: {
        const double r = (minor_radius_m > 0.0 ? minor_radius_m : 0.25 * radius_m) * scale.y;
        return 2.0 * kPi * r * r;
    }
    case ObjectShapeKind::Cone: {
        const double r_avg = 0.5 * (radius_m + top_radius_m);
        return kPi * r_avg * r_avg * scale.x * scale.z;
    }
    case ObjectShapeKind::ExtrudedProfile2D:
        return cross_section_area_m2 * scale.x * scale.y;
    case ObjectShapeKind::SweptPath: {
        const double r_wire = (radius_m > 0.0 ? radius_m : 0.0005) * 0.5 * (scale.x + scale.z);
        return kPi * r_wire * r_wire;
    }
    case ObjectShapeKind::RevolvedProfile:
    case ObjectShapeKind::LoftedSurface:
    case ObjectShapeKind::Mesh3D: {
        const double l = std::max(1e-9, compute_characteristic_length_m());
        return compute_volume_m3() / l;
    }
    }
    return 1e-4;
}

cad3d::SolidMesh3D ObjectGeometrySpec::generate_solid_mesh() const {
    if (mesh.has_value() && (kind == ObjectShapeKind::Mesh3D ||
                             kind == ObjectShapeKind::RevolvedProfile ||
                             kind == ObjectShapeKind::LoftedSurface)) {
        return *mesh;
    }

    cad3d::SolidMesh3D out;
    if (kind == ObjectShapeKind::Box || kind == ObjectShapeKind::ExtrudedProfile2D || kind == ObjectShapeKind::Plane) {
        const double w = (kind == ObjectShapeKind::ExtrudedProfile2D)
            ? std::sqrt(std::max(1e-12, cross_section_area_m2)) : dim_x_m;
        const double h = (kind == ObjectShapeKind::ExtrudedProfile2D) ? w :
                         (kind == ObjectShapeKind::Plane && dim_y_m <= 0.0 ? 0.001 : dim_y_m);
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

    if (kind == ObjectShapeKind::Pipe) {
        const int segs = 24;
        const double hy = 0.5 * dim_y_m * scale.y;
        const double R = radius_m * 0.5 * (scale.x + scale.z);
        const double r = std::max(0.001 * R, R - wall_thickness_m * 0.5 * (scale.x + scale.z));

        std::vector<uint32_t> out_bot(segs), out_top(segs), in_bot(segs), in_top(segs);
        for (int i = 0; i < segs; ++i) {
            const double ang = 2.0 * kPi * static_cast<double>(i) / static_cast<double>(segs);
            const double ca = std::cos(ang), sa = std::sin(ang);
            out_bot[i] = out.add_vertex({R * ca, -hy, R * sa});
            out_top[i] = out.add_vertex({R * ca,  hy, R * sa});
            in_bot[i]  = out.add_vertex({r * ca, -hy, r * sa});
            in_top[i]  = out.add_vertex({r * ca,  hy, r * sa});
        }
        for (int i = 0; i < segs; ++i) {
            const int nxt = (i + 1) % segs;
            out.add_triangle(out_bot[i], out_top[i], out_top[nxt]);
            out.add_triangle(out_bot[i], out_top[nxt], out_bot[nxt]);

            out.add_triangle(in_bot[i], in_top[nxt], in_top[i]);
            out.add_triangle(in_bot[i], in_bot[nxt], in_top[nxt]);

            out.add_triangle(out_top[i], in_top[i], in_top[nxt]);
            out.add_triangle(out_top[i], in_top[nxt], out_top[nxt]);

            out.add_triangle(out_bot[i], in_bot[nxt], in_bot[i]);
            out.add_triangle(out_bot[i], out_bot[nxt], in_bot[nxt]);
        }
        out.ensure_outward_winding();
        return out;
    }

    if (kind == ObjectShapeKind::Torus) {
        const int major_segs = 24;
        const int minor_segs = 12;
        const double R = radius_m * 0.5 * (scale.x + scale.z);
        const double r = ((minor_radius_m > 0.0) ? minor_radius_m : 0.25 * radius_m) * scale.y;

        std::vector<std::vector<uint32_t>> grid(major_segs, std::vector<uint32_t>(minor_segs));
        for (int i = 0; i < major_segs; ++i) {
            const double u = 2.0 * kPi * static_cast<double>(i) / static_cast<double>(major_segs);
            const double cu = std::cos(u), su = std::sin(u);
            for (int j = 0; j < minor_segs; ++j) {
                const double v = 2.0 * kPi * static_cast<double>(j) / static_cast<double>(minor_segs);
                const double cv = std::cos(v), sv = std::sin(v);
                const double px = (R + r * cv) * cu;
                const double py = r * sv;
                const double pz = (R + r * cv) * su;
                grid[i][j] = out.add_vertex({px, py, pz});
            }
        }
        for (int i = 0; i < major_segs; ++i) {
            const int inxt = (i + 1) % major_segs;
            for (int j = 0; j < minor_segs; ++j) {
                const int jnxt = (j + 1) % minor_segs;
                out.add_triangle(grid[i][j], grid[inxt][j], grid[inxt][jnxt]);
                out.add_triangle(grid[i][j], grid[inxt][jnxt], grid[i][jnxt]);
            }
        }
        out.ensure_outward_winding();
        return out;
    }

    if (kind == ObjectShapeKind::Capsule) {
        const int segs = 20;
        const int cap_stacks = 8;
        const double R = radius_m * 0.5 * (scale.x + scale.z);
        const double total_h = dim_y_m * scale.y;
        const double cyl_h = std::max(0.0, total_h - 2.0 * R);
        const double hy = 0.5 * cyl_h;

        const uint32_t north = out.add_vertex({0.0, hy + R, 0.0});
        const uint32_t south = out.add_vertex({0.0, -hy - R, 0.0});

        std::vector<std::vector<uint32_t>> top_cap(cap_stacks, std::vector<uint32_t>(segs));
        std::vector<std::vector<uint32_t>> bot_cap(cap_stacks, std::vector<uint32_t>(segs));

        for (int st = 1; st < cap_stacks; ++st) {
            const double phi = (kPi * 0.5) * static_cast<double>(st) / static_cast<double>(cap_stacks);
            const double r_st = R * std::sin(phi);
            const double y_top = hy + R * std::cos(phi);
            const double y_bot = -hy - R * std::cos(phi);
            for (int s = 0; s < segs; ++s) {
                const double th = 2.0 * kPi * static_cast<double>(s) / static_cast<double>(segs);
                top_cap[st][s] = out.add_vertex({r_st * std::cos(th), y_top, r_st * std::sin(th)});
                bot_cap[st][s] = out.add_vertex({r_st * std::cos(th), y_bot, r_st * std::sin(th)});
            }
        }
        for (int s = 0; s < segs; ++s) {
            const int nxt = (s + 1) % segs;
            out.add_triangle(north, top_cap[1][s], top_cap[1][nxt]);
            out.add_triangle(south, bot_cap[1][nxt], bot_cap[1][s]);
        }
        for (int st = 1; st + 1 < cap_stacks; ++st) {
            for (int s = 0; s < segs; ++s) {
                const int nxt = (s + 1) % segs;
                out.add_triangle(top_cap[st][s], top_cap[st + 1][s], top_cap[st + 1][nxt]);
                out.add_triangle(top_cap[st][s], top_cap[st + 1][nxt], top_cap[st][nxt]);

                out.add_triangle(bot_cap[st][s], bot_cap[st][nxt], bot_cap[st + 1][nxt]);
                out.add_triangle(bot_cap[st][s], bot_cap[st + 1][nxt], bot_cap[st + 1][s]);
            }
        }
        const auto& rim_top = top_cap[cap_stacks - 1];
        const auto& rim_bot = bot_cap[cap_stacks - 1];
        for (int s = 0; s < segs; ++s) {
            const int nxt = (s + 1) % segs;
            out.add_triangle(rim_top[s], rim_bot[s], rim_bot[nxt]);
            out.add_triangle(rim_top[s], rim_bot[nxt], rim_top[nxt]);
        }
        out.ensure_outward_winding();
        return out;
    }

    if (kind == ObjectShapeKind::SweptPath) {
        const int total_samples = 100;
        const int wire_segs = 8;
        const double turns = helical_turns > 0 ? helical_turns : 10;
        const double R = 0.5 * dim_x_m * 0.5 * (scale.x + scale.z);
        const double H = dim_y_m * scale.y;
        const double r_wire = (radius_m > 0.0 ? radius_m : 0.0005) * 0.5 * (scale.x + scale.z);

        std::vector<std::vector<uint32_t>> rings(total_samples, std::vector<uint32_t>(wire_segs));
        for (int i = 0; i < total_samples; ++i) {
            const double t = static_cast<double>(i) / static_cast<double>(total_samples - 1);
            const double theta = 2.0 * kPi * turns * t;
            const double cx = R * std::cos(theta);
            const double cy = (t - 0.5) * H;
            const double cz = R * std::sin(theta);

            const double tx = -2.0 * kPi * turns * R * std::sin(theta);
            const double ty = H;
            const double tz =  2.0 * kPi * turns * R * std::cos(theta);
            core::base::math::Vector3<double> tan{tx, ty, tz};
            tan = tan.normalized();

            core::base::math::Vector3<double> norm{-std::cos(theta), 0.0, -std::sin(theta)};
            core::base::math::Vector3<double> binorm = tan.cross(norm).normalized();
            norm = binorm.cross(tan).normalized();

            for (int j = 0; j < wire_segs; ++j) {
                const double phi = 2.0 * kPi * static_cast<double>(j) / static_cast<double>(wire_segs);
                const auto pt = core::base::math::Vector3<double>{cx, cy, cz} +
                                norm * (r_wire * std::cos(phi)) +
                                binorm * (r_wire * std::sin(phi));
                rings[i][j] = out.add_vertex(pt);
            }
        }
        for (int i = 0; i + 1 < total_samples; ++i) {
            for (int j = 0; j < wire_segs; ++j) {
                const int nxt = (j + 1) % wire_segs;
                out.add_triangle(rings[i][j], rings[i + 1][j], rings[i + 1][nxt]);
                out.add_triangle(rings[i][j], rings[i + 1][nxt], rings[i][nxt]);
            }
        }
        out.ensure_outward_winding();
        return out;
    }

    if (kind == ObjectShapeKind::RevolvedProfile) {
        // Revolve an incandescent bulb envelope profile around Y axis
        const int slices = 32;
        const double max_r = radius_m * scale.x;
        const double h = dim_y_m * scale.y;
        std::vector<std::pair<double, double>> prof = {
            { -0.50 * h, 0.40 * max_r },
            { -0.30 * h, 0.40 * max_r },
            { -0.15 * h, 0.65 * max_r },
            {  0.05 * h, 0.95 * max_r },
            {  0.25 * h, 1.00 * max_r },
            {  0.40 * h, 0.85 * max_r }
        };
        const uint32_t apex = out.add_vertex({0.0, 0.50 * h, 0.0});
        const uint32_t base_center = out.add_vertex({0.0, -0.50 * h, 0.0});
        const size_t n_rings = prof.size();
        std::vector<std::vector<uint32_t>> rings(n_rings, std::vector<uint32_t>(slices));
        for (size_t r = 0; r < n_rings; ++r) {
            const double y_val = prof[r].first;
            const double r_val = prof[r].second;
            for (int s = 0; s < slices; ++s) {
                const double th = 2.0 * kPi * static_cast<double>(s) / static_cast<double>(slices);
                rings[r][s] = out.add_vertex({r_val * std::cos(th), y_val, r_val * std::sin(th)});
            }
        }
        for (int s = 0; s < slices; ++s) {
            const int nxt = (s + 1) % slices;
            out.add_triangle(base_center, rings[0][nxt], rings[0][s]);
        }
        for (size_t r = 0; r + 1 < n_rings; ++r) {
            for (int s = 0; s < slices; ++s) {
                const int nxt = (s + 1) % slices;
                out.add_triangle(rings[r][s], rings[r + 1][s], rings[r + 1][nxt]);
                out.add_triangle(rings[r][s], rings[r + 1][nxt], rings[r][nxt]);
            }
        }
        for (int s = 0; s < slices; ++s) {
            const int nxt = (s + 1) % slices;
            out.add_triangle(rings[n_rings - 1][s], apex, rings[n_rings - 1][nxt]);
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

// Parametric builder methods
ObjectGeometrySpec ObjectGeometrySpec::make_box(double x, double y, double z) {
    ObjectGeometrySpec spec;
    spec.kind = ObjectShapeKind::Box;
    spec.dim_x_m = x;
    spec.dim_y_m = y;
    spec.dim_z_m = z;
    return spec;
}

ObjectGeometrySpec ObjectGeometrySpec::make_cylinder(double radius, double length) {
    ObjectGeometrySpec spec;
    spec.kind = ObjectShapeKind::Cylinder;
    spec.radius_m = radius;
    spec.dim_y_m = length;
    return spec;
}

ObjectGeometrySpec ObjectGeometrySpec::make_sphere(double radius) {
    ObjectGeometrySpec spec;
    spec.kind = ObjectShapeKind::Sphere;
    spec.radius_m = radius;
    return spec;
}

ObjectGeometrySpec ObjectGeometrySpec::make_cone(double bottom_radius, double top_radius, double height) {
    ObjectGeometrySpec spec;
    spec.kind = ObjectShapeKind::Cone;
    spec.radius_m = bottom_radius;
    spec.top_radius_m = top_radius;
    spec.dim_y_m = height;
    return spec;
}

ObjectGeometrySpec ObjectGeometrySpec::make_pipe(double outer_radius, double wall_thickness, double length) {
    ObjectGeometrySpec spec;
    spec.kind = ObjectShapeKind::Pipe;
    spec.radius_m = outer_radius;
    spec.wall_thickness_m = wall_thickness;
    spec.dim_y_m = length;
    return spec;
}

ObjectGeometrySpec ObjectGeometrySpec::make_torus(double major_radius, double minor_radius) {
    ObjectGeometrySpec spec;
    spec.kind = ObjectShapeKind::Torus;
    spec.radius_m = major_radius;
    spec.minor_radius_m = minor_radius;
    return spec;
}

ObjectGeometrySpec ObjectGeometrySpec::make_capsule(double radius, double total_length) {
    ObjectGeometrySpec spec;
    spec.kind = ObjectShapeKind::Capsule;
    spec.radius_m = radius;
    spec.dim_y_m = total_length;
    return spec;
}

ObjectGeometrySpec ObjectGeometrySpec::make_helical_coil(double wire_radius, double coil_radius, double coil_height, int turns) {
    ObjectGeometrySpec spec;
    spec.kind = ObjectShapeKind::SweptPath;
    spec.radius_m = wire_radius;
    spec.dim_x_m = 2.0 * coil_radius;
    spec.dim_y_m = coil_height;
    spec.helical_turns = turns;
    return spec;
}

ObjectGeometrySpec ObjectGeometrySpec::make_revolved(cad3d::SolidMesh3D m) {
    ObjectGeometrySpec spec;
    spec.kind = ObjectShapeKind::RevolvedProfile;
    spec.mesh = std::move(m);
    return spec;
}

ObjectGeometrySpec ObjectGeometrySpec::make_extruded(cad3d::SolidMesh3D m) {
    ObjectGeometrySpec spec;
    spec.kind = ObjectShapeKind::ExtrudedProfile2D;
    spec.mesh = std::move(m);
    return spec;
}

ObjectGeometrySpec ObjectGeometrySpec::make_mesh(cad3d::SolidMesh3D m) {
    ObjectGeometrySpec spec;
    spec.kind = ObjectShapeKind::Mesh3D;
    spec.mesh = std::move(m);
    return spec;
}

cad3d::SolidMesh3D ObjectGeometrySpec::boolean_union(const cad3d::SolidMesh3D& a, const cad3d::SolidMesh3D& b) {
    return cad3d::CsgBooleanEngine::boolean_union(a, b);
}

cad3d::SolidMesh3D ObjectGeometrySpec::boolean_difference(const cad3d::SolidMesh3D& a, const cad3d::SolidMesh3D& b) {
    return cad3d::CsgBooleanEngine::boolean_difference(a, b);
}

cad3d::SolidMesh3D ObjectGeometrySpec::boolean_intersection(const cad3d::SolidMesh3D& a, const cad3d::SolidMesh3D& b) {
    return cad3d::CsgBooleanEngine::boolean_intersection(a, b);
}

cad3d::SolidMesh3D ObjectGeometrySpec::shell_solid_mesh(const cad3d::SolidMesh3D& solid, double thickness_m) {
    auto bbox = solid.compute_bbox();
    auto center = (bbox.min + bbox.max) * 0.5;
    auto size = bbox.size();
    if (size.x <= 2.0 * thickness_m || size.y <= 2.0 * thickness_m || size.z <= 2.0 * thickness_m) {
        return solid;
    }
    double sx = (size.x - 2.0 * thickness_m) / size.x;
    double sy = (size.y - 2.0 * thickness_m) / size.y;
    double sz = (size.z - 2.0 * thickness_m) / size.z;

    cad3d::SolidMesh3D inner = solid;
    for (auto& v : inner.vertices) {
        v = center + core::base::math::Vector3<double>{(v.x - center.x) * sx, (v.y - center.y) * sy, (v.z - center.z) * sz};
    }
    return cad3d::CsgBooleanEngine::boolean_difference(solid, inner);
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
    ss << "  \"wall_thickness_m\": " << std::setprecision(10) << geometry.wall_thickness_m << ",\n";
    ss << "  \"minor_radius_m\": " << std::setprecision(10) << geometry.minor_radius_m << ",\n";
    ss << "  \"helical_turns\": " << geometry.helical_turns << ",\n";
    ss << "  \"is_hollow\": " << (geometry.is_hollow ? "true" : "false") << ",\n";
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
    obj.geometry.wall_thickness_m = json_num(src, "wall_thickness_m", 0.005);
    obj.geometry.minor_radius_m = json_num(src, "minor_radius_m", 0.01);
    obj.geometry.helical_turns = static_cast<int>(json_num(src, "helical_turns", 10.0));
    obj.geometry.is_hollow = json_bool(src, "is_hollow", false);
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

    p.mass_kg = p.volume_m3 * rho_eff;
    p.effective_density_kg_m3 = (p.volume_m3 > 1e-18) ? (p.mass_kg / p.volume_m3) : rho_eff;
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

    // Center of Mass and Mass Moment of Inertia Tensor around Center of Mass
    const double M = p.mass_kg;
    p.center_of_mass_m = {0.0, 0.0, 0.0};
    p.inertia_ixx = 0.0;
    p.inertia_iyy = 0.0;
    p.inertia_izz = 0.0;
    p.inertia_ixy = 0.0;
    p.inertia_iyz = 0.0;
    p.inertia_izx = 0.0;

    switch (obj.geometry.kind) {
    case ObjectShapeKind::Box: {
        const double w = obj.geometry.dim_x_m * obj.geometry.scale.x;
        const double h = obj.geometry.dim_y_m * obj.geometry.scale.y;
        const double d = obj.geometry.dim_z_m * obj.geometry.scale.z;
        p.inertia_ixx = (1.0 / 12.0) * M * (h * h + d * d);
        p.inertia_iyy = (1.0 / 12.0) * M * (w * w + d * d);
        p.inertia_izz = (1.0 / 12.0) * M * (w * w + h * h);
        break;
    }
    case ObjectShapeKind::Sphere: {
        const double r = obj.geometry.radius_m * ((obj.geometry.scale.x + obj.geometry.scale.y + obj.geometry.scale.z) / 3.0);
        const double I = (2.0 / 5.0) * M * r * r;
        p.inertia_ixx = I;
        p.inertia_iyy = I;
        p.inertia_izz = I;
        break;
    }
    case ObjectShapeKind::Cylinder: {
        const double r = obj.geometry.radius_m * 0.5 * (obj.geometry.scale.x + obj.geometry.scale.z);
        const double h = obj.geometry.dim_y_m * obj.geometry.scale.y;
        p.inertia_iyy = 0.5 * M * r * r;
        p.inertia_ixx = (1.0 / 12.0) * M * (3.0 * r * r + h * h);
        p.inertia_izz = p.inertia_ixx;
        break;
    }
    case ObjectShapeKind::Cone: {
        const double r = obj.geometry.radius_m * 0.5 * (obj.geometry.scale.x + obj.geometry.scale.z);
        const double h = obj.geometry.dim_y_m * obj.geometry.scale.y;
        p.center_of_mass_m = {0.0, -0.25 * h, 0.0};
        p.inertia_iyy = (3.0 / 10.0) * M * r * r;
        p.inertia_ixx = (3.0 / 20.0) * M * r * r + (3.0 / 80.0) * M * h * h;
        p.inertia_izz = p.inertia_ixx;
        break;
    }
    case ObjectShapeKind::Pipe: {
        const double r_out = obj.geometry.radius_m * 0.5 * (obj.geometry.scale.x + obj.geometry.scale.z);
        const double r_in = std::max(0.0, r_out - obj.geometry.wall_thickness_m);
        const double h = obj.geometry.dim_y_m * obj.geometry.scale.y;
        p.inertia_iyy = 0.5 * M * (r_out * r_out + r_in * r_in);
        p.inertia_ixx = (1.0 / 12.0) * M * (3.0 * (r_out * r_out + r_in * r_in) + h * h);
        p.inertia_izz = p.inertia_ixx;
        break;
    }
    case ObjectShapeKind::Torus: {
        const double R = obj.geometry.radius_m;
        const double r = obj.geometry.minor_radius_m;
        p.inertia_iyy = M * (R * R + 0.75 * r * r);
        p.inertia_ixx = M * (0.5 * R * R + 0.625 * r * r);
        p.inertia_izz = p.inertia_ixx;
        break;
    }
    case ObjectShapeKind::Capsule: {
        const double r = obj.geometry.radius_m * 0.5 * (obj.geometry.scale.x + obj.geometry.scale.z);
        const double h = obj.geometry.dim_y_m * obj.geometry.scale.y;
        p.inertia_iyy = 0.5 * M * r * r;
        p.inertia_ixx = (1.0 / 12.0) * M * (3.0 * r * r + h * h);
        p.inertia_izz = p.inertia_ixx;
        break;
    }
    case ObjectShapeKind::Plane: {
        const double w = obj.geometry.dim_x_m * obj.geometry.scale.x;
        const double d = obj.geometry.dim_z_m * obj.geometry.scale.z;
        p.inertia_iyy = (1.0 / 12.0) * M * (w * w + d * d);
        p.inertia_ixx = (1.0 / 12.0) * M * (d * d);
        p.inertia_izz = (1.0 / 12.0) * M * (w * w);
        break;
    }
    case ObjectShapeKind::RevolvedProfile:
    case ObjectShapeKind::SweptPath:
    case ObjectShapeKind::LoftedSurface:
    case ObjectShapeKind::ExtrudedProfile2D:
    case ObjectShapeKind::Mesh3D: {
        const double char_l = std::max(1e-6, obj.geometry.compute_characteristic_length_m());
        const double I = (1.0 / 6.0) * M * char_l * char_l;
        p.inertia_ixx = I;
        p.inertia_iyy = I;
        p.inertia_izz = I;
        break;
    }
    }

    p.valid = true;
    return p;
}

std::string PhysicalObjectManager::generate_unique_name(std::string_view base_name) const {
    std::string candidate = base_name.empty() ? "Object" : std::string(base_name);
    if (!find_by_name(candidate)) {
        return candidate;
    }
    // Extract base prefix without existing numeric suffix if any
    std::string prefix = candidate;
    size_t last_underscore = candidate.rfind('_');
    int start_num = 1;
    if (last_underscore != std::string::npos && last_underscore + 1 < candidate.size()) {
        bool all_digits = true;
        for (size_t i = last_underscore + 1; i < candidate.size(); ++i) {
            if (!std::isdigit(static_cast<unsigned char>(candidate[i]))) {
                all_digits = false;
                break;
            }
        }
        if (all_digits) {
            prefix = candidate.substr(0, last_underscore);
            try {
                start_num = std::stoi(candidate.substr(last_underscore + 1)) + 1;
            } catch (...) {
                start_num = 1;
            }
        }
    }
    for (int num = start_num; ; ++num) {
        std::string test_name = prefix + "_" + std::to_string(num);
        if (!find_by_name(test_name)) {
            return test_name;
        }
    }
}

core::base::Result<void> PhysicalObjectManager::rename_object(
    const core::base::Uuid& id,
    std::string_view new_name,
    const core::domain::MaterialRegistry& /*reg*/) {
    if (new_name.empty()) {
        return make_obj_error(4316, "Object name cannot be empty");
    }
    auto* existing = get_object_mut(id);
    if (!existing) {
        return make_obj_error(4317, "Object not found for rename");
    }
    if (existing->name == new_name) {
        return core::base::Result<void>::ok();
    }
    const auto* collision = find_by_name(new_name);
    if (collision != nullptr && collision->id != id) {
        return make_obj_error(4318, "An object named '" + std::string(new_name) + "' already exists");
    }
    existing->name = std::string(new_name);
    return core::base::Result<void>::ok();
}

core::base::Result<core::base::Uuid> PhysicalObjectManager::duplicate_object(
    const core::base::Uuid& id,
    const core::domain::MaterialRegistry& reg) {
    const auto* orig = get_object(id);
    if (!orig) {
        return make_obj_error(4319, "Source object not found for duplication");
    }
    PhysicalObject copy = *orig;
    copy.id = core::base::Uuid::generate_random();
    copy.name = generate_unique_name(orig->name);
    copy.geometry.position.x += 0.02;
    copy.geometry.position.z += 0.02;
    return add_object(std::move(copy), reg);
}

core::base::Result<core::base::Uuid> PhysicalObjectManager::add_object(
    PhysicalObject obj,
    const core::domain::MaterialRegistry& reg) {
    if (get_object(obj.id) != nullptr) {
        return make_obj_error(4315, "Duplicate PhysicalObject UUID in workspace");
    }
    if (obj.name.empty()) {
        obj.name = "Object";
    }
    if (find_by_name(obj.name) != nullptr) {
        obj.name = generate_unique_name(obj.name);
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
