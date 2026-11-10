#include "shunya/pcb/copper_pour.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::pcb {

namespace {

inline constexpr double kPi = 3.14159265358979323846;

double polygon_area(const std::vector<core::base::math::Vector2<double>>& poly) noexcept {
    if (poly.size() < 3) return 0.0;
    double a = 0.0;
    for (size_t i = 0; i < poly.size(); ++i) {
        const auto& p0 = poly[i];
        const auto& p1 = poly[(i + 1) % poly.size()];
        a += (p0.x * p1.y - p1.x * p0.y);
    }
    return std::abs(a) * 0.5;
}

std::vector<core::base::math::Vector2<double>> octagon_clearance_void(
    const core::base::math::Vector2<double>& center,
    double radius_mm) {
    std::vector<core::base::math::Vector2<double>> pts;
    pts.reserve(8);
    // 8-vertex octagon has interior angles 135 deg (>= 60 deg -> zero acid traps)
    for (int k = 0; k < 8; ++k) {
        const double ang = static_cast<double>(k) * (kPi / 4.0) + (kPi / 8.0);
        pts.push_back({center.x + radius_mm * std::cos(ang),
                       center.y + radius_mm * std::sin(ang)});
    }
    return pts;
}

std::vector<core::base::math::Vector2<double>> capsule_clearance_void(
    const core::base::math::Vector2<double>& a,
    const core::base::math::Vector2<double>& b,
    double half_ext) {
    auto ab = b - a;
    if (ab.length() < 1e-6) return octagon_clearance_void(a, half_ext);
    ab = ab.normalized();
    const core::base::math::Vector2<double> n{-ab.y, ab.x};
    // Chamfered rectangle clearance around track (minimum corner = 90/135 deg -> zero acute acid traps)
    const double c = half_ext * 0.41421356;
    return {
        a - ab * half_ext + n * c,
        a - ab * c + n * half_ext,
        b + ab * c + n * half_ext,
        b + ab * half_ext + n * c,
        b + ab * half_ext - n * c,
        b + ab * c - n * half_ext,
        a - ab * c - n * half_ext,
        a - ab * half_ext - n * c
    };
}

} // namespace

ThermalReliefPad CopperPourEngine::build_thermal_relief(
    std::string net_name,
    const core::base::math::Vector2<double>& center_mm,
    double pad_radius_mm,
    double isolation_gap_mm,
    double spoke_width_mm) {
    ThermalReliefPad tr;
    tr.net_name = std::move(net_name);
    tr.center_mm = center_mm;
    tr.pad_radius_mm = pad_radius_mm;
    tr.isolation_gap_mm = isolation_gap_mm;

    const double r_in = std::max(0.05, pad_radius_mm * 0.5);
    const double r_out = pad_radius_mm + isolation_gap_mm + 0.15;
    tr.spokes[0] = {{center_mm.x + r_in, center_mm.y}, {center_mm.x + r_out, center_mm.y}, spoke_width_mm};
    tr.spokes[1] = {{center_mm.x - r_in, center_mm.y}, {center_mm.x - r_out, center_mm.y}, spoke_width_mm};
    tr.spokes[2] = {{center_mm.x, center_mm.y + r_in}, {center_mm.x, center_mm.y + r_out}, spoke_width_mm};
    tr.spokes[3] = {{center_mm.x, center_mm.y - r_in}, {center_mm.x, center_mm.y - r_out}, spoke_width_mm};
    return tr;
}

bool CopperPourEngine::polygon_has_acute_acid_trap(
    const std::vector<core::base::math::Vector2<double>>& poly,
    double min_angle_deg) noexcept {
    if (poly.size() < 3) return false;
    for (size_t i = 0; i < poly.size(); ++i) {
        const auto& prev = poly[(i + poly.size() - 1) % poly.size()];
        const auto& cur = poly[i];
        const auto& next = poly[(i + 1) % poly.size()];
        auto v1 = prev - cur;
        auto v2 = next - cur;
        if (v1.length() < 1e-9 || v2.length() < 1e-9) continue;
        v1 = v1.normalized();
        v2 = v2.normalized();
        const double angle = std::acos(std::clamp(v1.dot(v2), -1.0, 1.0)) * (180.0 / kPi);
        if (angle < min_angle_deg) return true;
    }
    return false;
}

CopperPourResult CopperPourEngine::generate_pour(
    const CopperPourZone& zone,
    const BoardOutline& board,
    const std::vector<PcbTrackSegment>& tracks,
    const std::vector<PourPadObstacle>& pads,
    const std::vector<PcbVia>& vias) {
    CopperPourResult res;
    res.outer_boundary_mm = board.outer_contour();
    double area = polygon_area(res.outer_boundary_mm);
    if (polygon_has_acute_acid_trap(res.outer_boundary_mm)) {
        res.has_acute_acid_traps = true;
    }

    // Generate antipad/track clearance voids for foreign nets
    for (const auto& t : tracks) {
        if (t.layer_index != zone.layer_index || t.net_name == zone.net_name) continue;
        auto void_poly = capsule_clearance_void(t.start_mm, t.end_mm, 0.5 * t.width_mm + zone.clearance_mm);
        area -= polygon_area(void_poly);
        if (polygon_has_acute_acid_trap(void_poly)) res.has_acute_acid_traps = true;
        res.clearance_voids_mm.push_back(std::move(void_poly));
    }

    for (const auto& v : vias) {
        if (!v.spans_layer(zone.layer_index) || v.net_name == zone.net_name) continue;
        auto void_poly = octagon_clearance_void(v.center_mm, 0.5 * v.pad_diameter_mm + zone.clearance_mm);
        area -= polygon_area(void_poly);
        if (polygon_has_acute_acid_trap(void_poly)) res.has_acute_acid_traps = true;
        res.clearance_voids_mm.push_back(std::move(void_poly));
    }

    for (const auto& p : pads) {
        if (p.net_name == zone.net_name) {
            res.thermal_relief_pads.push_back(
                build_thermal_relief(zone.net_name, p.center_mm, p.radius_mm,
                                     zone.clearance_mm, zone.spoke_width_mm));
        } else {
            auto void_poly = octagon_clearance_void(p.center_mm, p.radius_mm + zone.clearance_mm);
            area -= polygon_area(void_poly);
            if (polygon_has_acute_acid_trap(void_poly)) res.has_acute_acid_traps = true;
            res.clearance_voids_mm.push_back(std::move(void_poly));
        }
    }

    res.net_copper_area_mm2 = std::max(0.0, area);
    return res;
}

} // namespace shunya::pcb
