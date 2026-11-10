#include "shunya/pcb/via_manager.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace shunya::pcb {

namespace {

core::base::Error make_via_error(std::string msg) {
    return core::base::Error{
        .code = 5110,
        .domain = core::base::ErrorDomain::Validation,
        .severity = core::base::Severity::Error,
        .message = std::move(msg),
        .details = ""
    };
}

} // namespace

core::base::Result<void> ViaManager::add_via(PcbVia via, double min_annular_ring_mm) {
    if (via.drill_mm <= 0.0 || via.pad_diameter_mm <= via.drill_mm) {
        return make_via_error("Via pad diameter must exceed drill diameter");
    }
    if (via.annular_ring_mm() < min_annular_ring_mm - 1e-6) {
        return make_via_error("Via annular ring violates minimum threshold");
    }
    if (via.start_layer > via.end_layer) {
        std::swap(via.start_layer, via.end_layer);
    }
    if (via.start_layer < 0 || via.end_layer >= total_layers_) {
        return make_via_error("Via layer span out of stackup bounds");
    }
    if (via.type == ViaType::Blind && via.start_layer != 0 && via.end_layer != total_layers_ - 1) {
        return make_via_error("Blind via must terminate on an outer surface copper layer");
    }
    if (via.type == ViaType::Buried && (via.start_layer == 0 || via.end_layer == total_layers_ - 1)) {
        return make_via_error("Buried via must remain strictly on internal layers");
    }
    vias_.push_back(std::move(via));
    return {};
}

bool ViaManager::verify_net_continuity(std::string_view net_name,
                                       const std::vector<PcbTrackSegment>& tracks) const {
    std::vector<const PcbTrackSegment*> net_tracks;
    for (const auto& t : tracks) {
        if (t.net_name == net_name) net_tracks.push_back(&t);
    }
    if (net_tracks.size() <= 1) return true;

    std::vector<const PcbVia*> net_vias;
    for (const auto& v : vias_) {
        if (v.net_name == net_name) net_vias.push_back(&v);
    }

    const size_t nt = net_tracks.size();
    const size_t nv = net_vias.size();
    std::vector<size_t> parent(nt + nv);
    std::iota(parent.begin(), parent.end(), 0);
    auto find = [&](size_t x) {
        while (parent[x] != x) {
            parent[x] = parent[parent[x]];
            x = parent[x];
        }
        return x;
    };
    auto unite = [&](size_t a, size_t b) {
        a = find(a);
        b = find(b);
        if (a != b) parent[b] = a;
    };

    auto pt_on_track = [](const core::base::math::Vector2<double>& p, const PcbTrackSegment& seg,
                          double tol = 0.05) {
        const auto ab = seg.end_mm - seg.start_mm;
        const double l2 = ab.dot(ab);
        double u = (l2 > 1e-12) ? ((p - seg.start_mm).dot(ab) / l2) : 0.0;
        u = std::clamp(u, 0.0, 1.0);
        const auto proj = seg.start_mm + ab * u;
        return (p - proj).length() <= (0.5 * seg.width_mm + tol);
    };

    // Track-to-track same-layer intersection/endpoint touches
    for (size_t i = 0; i < nt; ++i) {
        for (size_t j = i + 1; j < nt; ++j) {
            if (net_tracks[i]->layer_index != net_tracks[j]->layer_index) continue;
            if (pt_on_track(net_tracks[i]->start_mm, *net_tracks[j]) ||
                pt_on_track(net_tracks[i]->end_mm, *net_tracks[j]) ||
                pt_on_track(net_tracks[j]->start_mm, *net_tracks[i]) ||
                pt_on_track(net_tracks[j]->end_mm, *net_tracks[i])) {
                unite(i, j);
            }
        }
    }

    // Track-to-via connections bridging copper layers
    for (size_t vi = 0; vi < nv; ++vi) {
        const auto& v = *net_vias[vi];
        const double r = 0.5 * v.pad_diameter_mm;
        for (size_t ti = 0; ti < nt; ++ti) {
            if (!v.spans_layer(net_tracks[ti]->layer_index)) continue;
            if (pt_on_track(v.center_mm, *net_tracks[ti], r)) {
                unite(ti, nt + vi);
            }
        }
    }

    const size_t root0 = find(0);
    for (size_t ti = 1; ti < nt; ++ti) {
        if (find(ti) != root0) return false;
    }
    return true;
}

} // namespace shunya::pcb
