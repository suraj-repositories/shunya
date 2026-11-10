#include "shunya/cad2d/construction_layer.hpp"

#include <algorithm>
#include <cstdio>

namespace shunya::cad2d {

std::string SketchDocument2D::add_entity(
    std::shared_ptr<Curve2D> curve,
    bool is_construction,
    std::string layer
) {
    if (!curve) return "";
    char buf[32];
    std::snprintf(buf, sizeof(buf), "sk_%llu", static_cast<unsigned long long>(next_id_++));
    std::string id(buf);
    entities_.push_back(SketchEntity{id, std::move(curve), is_construction, std::move(layer), true});
    return id;
}

bool SketchDocument2D::remove_entity(std::string_view id) {
    auto it = std::find_if(entities_.begin(), entities_.end(), [&](const SketchEntity& e) {
        return e.id == id;
    });
    if (it == entities_.end()) return false;
    entities_.erase(it);
    return true;
}

bool SketchDocument2D::set_construction(std::string_view id, bool is_construction) {
    for (auto& e : entities_) {
        if (e.id == id) {
            e.is_construction = is_construction;
            return true;
        }
    }
    return false;
}

void SketchDocument2D::clear() {
    entities_.clear();
}

const SketchEntity* SketchDocument2D::find_entity(std::string_view id) const noexcept {
    for (const auto& e : entities_) {
        if (e.id == id) return &e;
    }
    return nullptr;
}

std::vector<const SketchEntity*> SketchDocument2D::solid_entities() const {
    std::vector<const SketchEntity*> out;
    for (const auto& e : entities_) {
        if (e.visible && !e.is_construction && e.curve) {
            out.push_back(&e);
        }
    }
    return out;
}

std::vector<const SketchEntity*> SketchDocument2D::construction_entities() const {
    std::vector<const SketchEntity*> out;
    for (const auto& e : entities_) {
        if (e.visible && e.is_construction && e.curve) {
            out.push_back(&e);
        }
    }
    return out;
}

Paths2D SketchDocument2D::extract_solid_profile_loops(
    size_t curve_tessellation_segments,
    double endpoint_tol
) const {
    Paths2D loops;
    std::vector<std::vector<Point2D>> open_chains;

    for (const auto* ent : solid_entities()) {
        const Curve2D& curve = *ent->curve;
        if (curve.is_closed()) {
            Path2D poly = curve.sample_polyline(
                (curve.type() == CurveType2D::Polyline) ? 1 : std::max<size_t>(16, curve_tessellation_segments)
            );
            if (poly.size() >= 2 && poly.front().approx_equals(poly.back(), endpoint_tol)) {
                poly.pop_back();
            }
            if (poly.size() >= 3) {
                loops.push_back(std::move(poly));
            }
        } else {
            auto pts = curve.sample_polyline(
                (curve.type() == CurveType2D::LineSegment || curve.type() == CurveType2D::Polyline)
                ? 1 : std::max<size_t>(8, curve_tessellation_segments / 2)
            );
            if (pts.size() >= 2) {
                open_chains.push_back(std::move(pts));
            }
        }
    }

    // Stitch open chains head-to-tail into closed loops
    std::vector<bool> used(open_chains.size(), false);
    for (size_t i = 0; i < open_chains.size(); ++i) {
        if (used[i]) continue;
        used[i] = true;
        Path2D loop = open_chains[i];

        bool extended = true;
        while (extended) {
            extended = false;
            if (loop.front().approx_equals(loop.back(), endpoint_tol)) {
                loop.pop_back();
                if (loop.size() >= 3) {
                    loops.push_back(std::move(loop));
                }
                break;
            }

            const Point2D& tail = loop.back();
            for (size_t j = 0; j < open_chains.size(); ++j) {
                if (used[j]) continue;
                const auto& cand = open_chains[j];
                if (cand.front().approx_equals(tail, endpoint_tol)) {
                    used[j] = true;
                    loop.insert(loop.end(), cand.begin() + 1, cand.end());
                    extended = true;
                    break;
                } else if (cand.back().approx_equals(tail, endpoint_tol)) {
                    used[j] = true;
                    for (auto it = cand.rbegin() + 1; it != cand.rend(); ++it) {
                        loop.push_back(*it);
                    }
                    extended = true;
                    break;
                }
            }
        }
    }

    return loops;
}

} // namespace shunya::cad2d
