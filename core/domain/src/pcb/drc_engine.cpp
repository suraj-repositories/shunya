#include "shunya/pcb/drc_engine.hpp"

#include "shunya/pcb/trace_router.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::pcb {

namespace {

double point_to_segment_dist(const core::base::math::Vector2<double>& p,
                             const core::base::math::Vector2<double>& a,
                             const core::base::math::Vector2<double>& b,
                             core::base::math::Vector2<double>& closest_out) noexcept {
    const auto ab = b - a;
    const double l2 = ab.dot(ab);
    double u = (l2 > 1e-12) ? ((p - a).dot(ab) / l2) : 0.0;
    u = std::clamp(u, 0.0, 1.0);
    closest_out = a + ab * u;
    return (p - closest_out).length();
}

} // namespace

DrcReport DrcEngine::check_board(
    const DrcRules& rules,
    const std::vector<PcbTrackSegment>& tracks,
    const std::vector<DrcPad>& pads,
    const ViaManager& via_mgr,
    const std::vector<std::string>& required_nets) {
    DrcReport report;

    // 1. Track minimum width checks
    for (const auto& t : tracks) {
        if (t.width_mm < rules.min_track_width_mm - 1e-6) {
            const auto mid = (t.start_mm + t.end_mm) * 0.5;
            report.violations.push_back({
                DrcViolationKind::TrackWidthViolation,
                t.net_name,
                mid,
                t.width_mm,
                rules.min_track_width_mm,
                "Track width " + std::to_string(t.width_mm) + "mm below minimum"
            });
        }
    }

    // 2. Pairwise track-to-track clearances
    for (size_t i = 0; i < tracks.size(); ++i) {
        for (size_t j = i + 1; j < tracks.size(); ++j) {
            if (tracks[i].layer_index != tracks[j].layer_index ||
                tracks[i].net_name == tracks[j].net_name) {
                continue;
            }
            const double clr = InteractiveTraceRouter::edge_clearance_mm(tracks[i], tracks[j]);
            if (clr < rules.min_clearance_mm - 1e-6) {
                const auto mid = (tracks[i].start_mm + tracks[j].start_mm) * 0.5;
                report.violations.push_back({
                    DrcViolationKind::ClearanceViolation,
                    tracks[i].net_name + "/" + tracks[j].net_name,
                    mid,
                    clr,
                    rules.min_clearance_mm,
                    "Track clearance violation (" + std::to_string(clr) + "mm)"
                });
            }
        }
    }

    // 3. Track-to-pad clearances
    for (const auto& t : tracks) {
        for (const auto& p : pads) {
            if (t.layer_index != p.layer_index || t.net_name == p.net_name) continue;
            core::base::math::Vector2<double> closest;
            const double dist = point_to_segment_dist(p.center_mm, t.start_mm, t.end_mm, closest);
            const double pad_half = 0.5 * std::max(p.width_mm, p.height_mm);
            const double clr = dist - 0.5 * t.width_mm - pad_half;
            if (clr < rules.min_clearance_mm - 1e-6) {
                report.violations.push_back({
                    DrcViolationKind::ClearanceViolation,
                    t.net_name + "/" + p.ref_des + "." + p.pin,
                    closest,
                    clr,
                    rules.min_clearance_mm,
                    "Track-to-pad clearance violation"
                });
            }
        }
    }

    // 4. Pad-to-pad clearances
    for (size_t i = 0; i < pads.size(); ++i) {
        for (size_t j = i + 1; j < pads.size(); ++j) {
            if (pads[i].layer_index != pads[j].layer_index ||
                pads[i].net_name == pads[j].net_name) {
                continue;
            }
            const double dx = std::max(0.0, std::abs(pads[i].center_mm.x - pads[j].center_mm.x) -
                                                0.5 * (pads[i].width_mm + pads[j].width_mm));
            const double dy = std::max(0.0, std::abs(pads[i].center_mm.y - pads[j].center_mm.y) -
                                                0.5 * (pads[i].height_mm + pads[j].height_mm));
            const double clr = std::hypot(dx, dy);
            if (clr < rules.min_clearance_mm - 1e-6) {
                report.violations.push_back({
                    DrcViolationKind::ClearanceViolation,
                    pads[i].ref_des + "." + pads[i].pin + "/" + pads[j].ref_des + "." + pads[j].pin,
                    (pads[i].center_mm + pads[j].center_mm) * 0.5,
                    clr,
                    rules.min_clearance_mm,
                    "Pad-to-pad clearance violation"
                });
            }
        }
    }

    // 5. Via drill & annular ring checks
    for (const auto& v : via_mgr.vias()) {
        if (v.drill_mm < rules.min_via_drill_mm - 1e-6) {
            report.violations.push_back({
                DrcViolationKind::ViaDrillViolation,
                v.net_name,
                v.center_mm,
                v.drill_mm,
                rules.min_via_drill_mm,
                "Via drill diameter below minimum"
            });
        }
        if (v.annular_ring_mm() < rules.min_annular_ring_mm - 1e-6) {
            report.violations.push_back({
                DrcViolationKind::AnnularRingViolation,
                v.net_name,
                v.center_mm,
                v.annular_ring_mm(),
                rules.min_annular_ring_mm,
                "Via annular ring below minimum"
            });
        }
    }

    // 6. Unconnected net checks
    for (const auto& net : required_nets) {
        if (!via_mgr.verify_net_continuity(net, tracks)) {
            report.violations.push_back({
                DrcViolationKind::UnconnectedNet,
                net,
                {0.0, 0.0},
                0.0,
                1.0,
                "Unconnected net / open airwire on '" + net + "'"
            });
        }
    }

    return report;
}

} // namespace shunya::pcb
