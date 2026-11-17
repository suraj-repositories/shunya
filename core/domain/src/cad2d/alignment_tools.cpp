#include "shunya/cad2d/alignment_tools.hpp"
#include "shunya/cad2d/bezier_spline.hpp"
#include "shunya/cad2d/advanced_curves.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::cad2d {

bool CurveTransformHelper::translate_curve(Curve2D& curve, const Vec2d& delta) {
    if (std::abs(delta.x) < 1e-14 && std::abs(delta.y) < 1e-14) {
        return true;
    }

    if (auto* line = dynamic_cast<LineSegment2D*>(&curve)) {
        line->p0 = line->p0 + delta;
        line->p1 = line->p1 + delta;
        return true;
    }
    if (auto* poly = dynamic_cast<Polyline2D*>(&curve)) {
        for (auto& pt : poly->points) {
            pt = pt + delta;
        }
        return true;
    }
    if (auto* circ = dynamic_cast<Circle2D*>(&curve)) {
        circ->center = circ->center + delta;
        return true;
    }
    if (auto* arc = dynamic_cast<Arc2D*>(&curve)) {
        arc->center = arc->center + delta;
        return true;
    }
    if (auto* ell = dynamic_cast<Ellipse2D*>(&curve)) {
        ell->center = ell->center + delta;
        return true;
    }
    if (auto* bez = dynamic_cast<BezierCubic2D*>(&curve)) {
        bez->p0 = bez->p0 + delta;
        bez->p1 = bez->p1 + delta;
        bez->p2 = bez->p2 + delta;
        bez->p3 = bez->p3 + delta;
        return true;
    }
    if (auto* cat = dynamic_cast<CatmullRomSpline2D*>(&curve)) {
        for (auto& pt : cat->control_points) {
            pt = pt + delta;
        }
        return true;
    }
    if (auto* bsp = dynamic_cast<BSpline2D*>(&curve)) {
        for (auto& pt : bsp->control_points) {
            pt = pt + delta;
        }
        return true;
    }
    if (auto* par = dynamic_cast<ParabolicArc2D*>(&curve)) {
        par->vertex = par->vertex + delta;
        return true;
    }

    return false;
}

core::base::Result<size_t> AlignmentEngine::align_entities(
    SketchDocument2D& doc,
    const std::vector<std::string>& entity_ids,
    AlignmentMode mode,
    const std::string& anchor_entity_id
) {
    std::vector<SketchEntity*> targets;
    targets.reserve(entity_ids.size());

    for (const auto& id : entity_ids) {
        auto* ent = const_cast<SketchEntity*>(doc.find_entity(id));
        if (ent && ent->curve) {
            targets.push_back(ent);
        }
    }

    if (targets.size() < 2) {
        return core::base::Result<size_t>::err({
            .code = 4401,
            .domain = core::base::ErrorDomain::Validation,
            .severity = core::base::Severity::Error,
            .message = "Alignment requires at least two valid entities",
            .details = ""
        });
    }

    BBox2D ref_box;
    if (!anchor_entity_id.empty()) {
        const auto* anchor_ent = doc.find_entity(anchor_entity_id);
        if (anchor_ent && anchor_ent->curve) {
            ref_box = anchor_ent->curve->bounding_box();
        }
    }
    if (!ref_box.is_valid()) {
        for (const auto* e : targets) {
            ref_box.expand(e->curve->bounding_box());
        }
    }

    size_t count = 0;
    for (auto* e : targets) {
        if (!anchor_entity_id.empty() && e->id == anchor_entity_id) {
            continue; // Anchor remains stationary
        }

        BBox2D box = e->curve->bounding_box();
        Vec2d delta{0.0, 0.0};

        switch (mode) {
            case AlignmentMode::AlignLeft:
                delta.x = ref_box.min.x - box.min.x;
                break;
            case AlignmentMode::AlignCenterHorizontal:
                delta.x = ref_box.center().x - box.center().x;
                break;
            case AlignmentMode::AlignRight:
                delta.x = ref_box.max.x - box.max.x;
                break;
            case AlignmentMode::AlignTop:
                delta.y = ref_box.max.y - box.max.y;
                break;
            case AlignmentMode::AlignMiddleVertical:
                delta.y = ref_box.center().y - box.center().y;
                break;
            case AlignmentMode::AlignBottom:
                delta.y = ref_box.min.y - box.min.y;
                break;
        }

        if (CurveTransformHelper::translate_curve(*e->curve, delta)) {
            ++count;
        }
    }

    return core::base::Result<size_t>::ok(count);
}

core::base::Result<size_t> AlignmentEngine::distribute_entities(
    SketchDocument2D& doc,
    const std::vector<std::string>& entity_ids,
    DistributionMode mode
) {
    struct Item {
        SketchEntity* entity{nullptr};
        BBox2D box;
    };

    std::vector<Item> items;
    items.reserve(entity_ids.size());

    for (const auto& id : entity_ids) {
        auto* ent = const_cast<SketchEntity*>(doc.find_entity(id));
        if (ent && ent->curve) {
            items.push_back({ent, ent->curve->bounding_box()});
        }
    }

    if (items.size() < 3) {
        return core::base::Result<size_t>::err({
            .code = 4402,
            .domain = core::base::ErrorDomain::Validation,
            .severity = core::base::Severity::Error,
            .message = "Distribution requires at least three valid entities",
            .details = ""
        });
    }

    size_t count = 0;
    if (mode == DistributionMode::DistributeHorizontally) {
        std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) {
            return a.box.min.x < b.box.min.x;
        });

        double total_w = 0.0;
        for (const auto& it : items) total_w += it.box.width();

        double span = items.back().box.max.x - items.front().box.min.x;
        double gap = (span - total_w) / static_cast<double>(items.size() - 1);

        double cur_x = items.front().box.min.x;
        for (size_t i = 0; i < items.size(); ++i) {
            double dx = cur_x - items[i].box.min.x;
            if (CurveTransformHelper::translate_curve(*items[i].entity->curve, Vec2d{dx, 0.0})) {
                ++count;
            }
            cur_x += items[i].box.width() + gap;
        }
    } else if (mode == DistributionMode::DistributeCentersHorizontally) {
        std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) {
            return a.box.center().x < b.box.center().x;
        });

        double first_cx = items.front().box.center().x;
        double last_cx = items.back().box.center().x;
        double step = (last_cx - first_cx) / static_cast<double>(items.size() - 1);

        for (size_t i = 0; i < items.size(); ++i) {
            double target_cx = first_cx + static_cast<double>(i) * step;
            double dx = target_cx - items[i].box.center().x;
            if (CurveTransformHelper::translate_curve(*items[i].entity->curve, Vec2d{dx, 0.0})) {
                ++count;
            }
        }
    } else if (mode == DistributionMode::DistributeVertically) {
        std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) {
            return a.box.min.y < b.box.min.y;
        });

        double total_h = 0.0;
        for (const auto& it : items) total_h += it.box.height();

        double span = items.back().box.max.y - items.front().box.min.y;
        double gap = (span - total_h) / static_cast<double>(items.size() - 1);

        double cur_y = items.front().box.min.y;
        for (size_t i = 0; i < items.size(); ++i) {
            double dy = cur_y - items[i].box.min.y;
            if (CurveTransformHelper::translate_curve(*items[i].entity->curve, Vec2d{0.0, dy})) {
                ++count;
            }
            cur_y += items[i].box.height() + gap;
        }
    } else if (mode == DistributionMode::DistributeCentersVertically) {
        std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) {
            return a.box.center().y < b.box.center().y;
        });

        double first_cy = items.front().box.center().y;
        double last_cy = items.back().box.center().y;
        double step = (last_cy - first_cy) / static_cast<double>(items.size() - 1);

        for (size_t i = 0; i < items.size(); ++i) {
            double target_cy = first_cy + static_cast<double>(i) * step;
            double dy = target_cy - items[i].box.center().y;
            if (CurveTransformHelper::translate_curve(*items[i].entity->curve, Vec2d{0.0, dy})) {
                ++count;
            }
        }
    }

    return core::base::Result<size_t>::ok(count);
}

} // namespace shunya::cad2d
