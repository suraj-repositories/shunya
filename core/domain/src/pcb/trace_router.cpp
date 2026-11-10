#include "shunya/pcb/trace_router.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::pcb {

namespace {

double dot2(const core::base::math::Vector2<double>& a,
            const core::base::math::Vector2<double>& b) noexcept {
    return a.x * b.x + a.y * b.y;
}

std::pair<core::base::math::Vector2<double>, core::base::math::Vector2<double>>
closest_points_segments(const core::base::math::Vector2<double>& p1,
                        const core::base::math::Vector2<double>& q1,
                        const core::base::math::Vector2<double>& p2,
                        const core::base::math::Vector2<double>& q2) noexcept {
    const auto d1 = q1 - p1;
    const auto d2 = q2 - p2;
    const auto r = p1 - p2;
    const double a = dot2(d1, d1);
    const double e = dot2(d2, d2);
    const double f = dot2(d2, r);
    double s = 0.0, t = 0.0;

    if (a <= 1e-12 && e <= 1e-12) {
        return {p1, p2};
    } else if (a <= 1e-12) {
        t = std::clamp(f / e, 0.0, 1.0);
    } else {
        const double c = dot2(d1, r);
        if (e <= 1e-12) {
            s = std::clamp(-c / a, 0.0, 1.0);
        } else {
            const double b = dot2(d1, d2);
            const double denom = a * e - b * b;
            s = (std::abs(denom) > 1e-12) ? std::clamp((b * f - c * e) / denom, 0.0, 1.0) : 0.0;
            t = (b * s + f) / e;
            if (t < 0.0) {
                t = 0.0;
                s = std::clamp(-c / a, 0.0, 1.0);
            } else if (t > 1.0) {
                t = 1.0;
                s = std::clamp((b - c) / a, 0.0, 1.0);
            }
        }
    }
    return {p1 + d1 * s, p2 + d2 * t};
}

} // namespace

std::vector<PcbTrackSegment> InteractiveTraceRouter::route_octilinear_pair(
    std::string net_name,
    int layer_index,
    double width_mm,
    const core::base::math::Vector2<double>& start_mm,
    const core::base::math::Vector2<double>& end_mm,
    bool miter_first) {
    std::vector<PcbTrackSegment> out;
    const double dx = end_mm.x - start_mm.x;
    const double dy = end_mm.y - start_mm.y;
    const double adx = std::abs(dx);
    const double ady = std::abs(dy);

    if (adx < 1e-6 && ady < 1e-6) return out;
    if (adx < 1e-6 || ady < 1e-6 || std::abs(adx - ady) < 1e-6) {
        out.push_back({std::move(net_name), layer_index, width_mm, start_mm, end_mm, false});
        return out;
    }

    const double sx = (dx >= 0.0) ? 1.0 : -1.0;
    const double sy = (dy >= 0.0) ? 1.0 : -1.0;
    const double diag = std::min(adx, ady);

    core::base::math::Vector2<double> elbow;
    if (miter_first) {
        elbow = {start_mm.x + sx * diag, start_mm.y + sy * diag};
    } else {
        if (adx > ady) {
            elbow = {start_mm.x + sx * (adx - diag), start_mm.y};
        } else {
            elbow = {start_mm.x, start_mm.y + sy * (ady - diag)};
        }
    }

    out.push_back({net_name, layer_index, width_mm, start_mm, elbow, false});
    out.push_back({std::move(net_name), layer_index, width_mm, elbow, end_mm, false});
    return out;
}

bool InteractiveTraceRouter::is_45_deg_aligned(const PcbTrackSegment& seg, double tol) noexcept {
    const double adx = std::abs(seg.end_mm.x - seg.start_mm.x);
    const double ady = std::abs(seg.end_mm.y - seg.start_mm.y);
    return (adx <= tol) || (ady <= tol) || (std::abs(adx - ady) <= tol);
}

double InteractiveTraceRouter::edge_clearance_mm(const PcbTrackSegment& a,
                                                   const PcbTrackSegment& b) noexcept {
    const auto [ca, cb] = closest_points_segments(a.start_mm, a.end_mm, b.start_mm, b.end_mm);
    return (ca - cb).length() - 0.5 * (a.width_mm + b.width_mm);
}

std::vector<PcbTrackSegment> PushShoveEngine::shove_parallel_tracks(
    const PcbTrackSegment& intruder,
    std::vector<PcbTrackSegment> neighbors,
    const PushShoveOptions& opts) {
    const auto intr_mid = (intruder.start_mm + intruder.end_mm) * 0.5;
    auto axis = intruder.end_mm - intruder.start_mm;
    if (axis.length() < 1e-9) axis = {1.0, 0.0};
    axis = axis.normalized();
    const core::base::math::Vector2<double> default_normal{-axis.y, axis.x};

    for (int iter = 0; iter < opts.max_iterations; ++iter) {
        bool any_pushed = false;

        // Push neighbors away from intruder
        for (auto& nb : neighbors) {
            if (nb.locked || nb.layer_index != intruder.layer_index || nb.net_name == intruder.net_name) {
                continue;
            }
            const double clr = InteractiveTraceRouter::edge_clearance_mm(intruder, nb);
            if (clr < opts.min_clearance_mm - 1e-6) {
                const double push = (opts.min_clearance_mm - clr) + 1e-4;
                const auto [ci, cn] = closest_points_segments(
                    intruder.start_mm, intruder.end_mm, nb.start_mm, nb.end_mm);
                auto dir = cn - ci;
                if (dir.length() < 1e-9) {
                    const auto nb_mid = (nb.start_mm + nb.end_mm) * 0.5;
                    dir = ((nb_mid - intr_mid).dot(default_normal) >= 0.0)
                              ? default_normal
                              : (default_normal * -1.0);
                } else {
                    dir = dir.normalized();
                }
                nb.start_mm = nb.start_mm + dir * push;
                nb.end_mm = nb.end_mm + dir * push;
                any_pushed = true;
            }
        }

        // Propagate cascade shove across neighboring parallel traces outward from intruder
        for (size_t i = 0; i < neighbors.size(); ++i) {
            for (size_t j = i + 1; j < neighbors.size(); ++j) {
                auto& a = neighbors[i];
                auto& b = neighbors[j];
                if (a.layer_index != b.layer_index || a.net_name == b.net_name) continue;
                const double clr = InteractiveTraceRouter::edge_clearance_mm(a, b);
                if (clr < opts.min_clearance_mm - 1e-6) {
                    const double push = (opts.min_clearance_mm - clr) + 1e-4;
                    const auto da = (((a.start_mm + a.end_mm) * 0.5) - intr_mid).length();
                    const auto db = (((b.start_mm + b.end_mm) * 0.5) - intr_mid).length();
                    // Push whichever neighbor is further from the intruder (unless locked)
                    PcbTrackSegment* mover = (!b.locked && (db >= da || a.locked)) ? &b : (!a.locked ? &a : nullptr);
                    PcbTrackSegment* anchor = (mover == &b) ? &a : &b;
                    if (!mover) continue;

                    const auto [ca, cm] = closest_points_segments(
                        anchor->start_mm, anchor->end_mm, mover->start_mm, mover->end_mm);
                    auto dir = cm - ca;
                    if (dir.length() < 1e-9) {
                        dir = default_normal;
                    } else {
                        dir = dir.normalized();
                    }
                    mover->start_mm = mover->start_mm + dir * push;
                    mover->end_mm = mover->end_mm + dir * push;
                    any_pushed = true;
                }
            }
        }
        if (!any_pushed) break;
    }

    return neighbors;
}

} // namespace shunya::pcb
