#include "shunya/cad2d/block_manager.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::cad2d {

Transform2D Transform2D::rotation(double angle_rad) noexcept {
    const double c_val = std::cos(angle_rad);
    const double s_val = std::sin(angle_rad);
    return {c_val, -s_val, s_val, c_val, 0.0, 0.0};
}

Transform2D Transform2D::compose(
    double tx,
    double ty,
    double rotation_rad,
    double sx,
    double sy
) noexcept {
    const double c_val = std::cos(rotation_rad);
    const double s_val = std::sin(rotation_rad);
    return {
        c_val * sx,
        -s_val * sy,
        s_val * sx,
        c_val * sy,
        tx,
        ty
    };
}

std::shared_ptr<Curve2D> transform_curve(const Curve2D& curve, const Transform2D& xf) {
    switch (curve.type()) {
        case CurveType2D::LineSegment: {
            const auto& seg = static_cast<const LineSegment2D&>(curve);
            return std::make_shared<LineSegment2D>(xf.apply(seg.p0), xf.apply(seg.p1));
        }
        case CurveType2D::Polyline: {
            const auto& poly = static_cast<const Polyline2D&>(curve);
            std::vector<Point2D> pts;
            pts.reserve(poly.points.size());
            for (const auto& p : poly.points) {
                pts.push_back(xf.apply(p));
            }
            return std::make_shared<Polyline2D>(std::move(pts), poly.closed);
        }
        case CurveType2D::BezierCubic: {
            const auto& bez = static_cast<const BezierCubic2D&>(curve);
            return std::make_shared<BezierCubic2D>(
                xf.apply(bez.p0),
                xf.apply(bez.p1),
                xf.apply(bez.p2),
                xf.apply(bez.p3)
            );
        }
        case CurveType2D::Circle: {
            const auto& circ = static_cast<const Circle2D&>(curve);
            const Point2D c_new = xf.apply(circ.center);
            const Vec2d ex{xf.a, xf.c};
            const Vec2d ey{xf.b, xf.d};
            const double sx = ex.length();
            const double sy = ey.length();
            if (std::abs(ex.dot(ey)) <= 1e-9 * sx * sy) {
                if (std::abs(sx - sy) <= 1e-9 * std::max(1.0, sx)) {
                    return std::make_shared<Circle2D>(c_new, circ.radius * sx);
                }
                const double rot = std::atan2(ex.y, ex.x);
                return std::make_shared<Ellipse2D>(c_new, circ.radius * sx, circ.radius * sy, rot);
            }
            auto pts = circ.sample_polyline(64);
            for (auto& p : pts) p = xf.apply(p);
            return std::make_shared<Polyline2D>(std::move(pts), true);
        }
        case CurveType2D::Arc: {
            const auto& arc = static_cast<const Arc2D&>(curve);
            const Vec2d ex{xf.a, xf.c};
            const Vec2d ey{xf.b, xf.d};
            const double sx = ex.length();
            const double sy = ey.length();
            if (std::abs(ex.dot(ey)) <= 1e-9 * sx * sy &&
                std::abs(sx - sy) <= 1e-9 * std::max(1.0, sx)) {
                const Point2D c_new = xf.apply(arc.center);
                const Point2D p_start_local{
                    arc.center.x + arc.radius * std::cos(arc.start_angle),
                    arc.center.y + arc.radius * std::sin(arc.start_angle)
                };
                const Point2D p_start_world = xf.apply(p_start_local);
                const double start_world = std::atan2(p_start_world.y - c_new.y, p_start_world.x - c_new.x);
                const double sweep_world = (xf.determinant() < 0.0) ? -arc.sweep_angle : arc.sweep_angle;
                return std::make_shared<Arc2D>(c_new, arc.radius * sx, start_world, sweep_world);
            }
            auto pts = arc.sample_polyline(48);
            for (auto& p : pts) p = xf.apply(p);
            return std::make_shared<Polyline2D>(std::move(pts), false);
        }
        case CurveType2D::Ellipse: {
            const auto& ell = static_cast<const Ellipse2D&>(curve);
            const Vec2d ex{xf.a, xf.c};
            const Vec2d ey{xf.b, xf.d};
            const double sx = ex.length();
            const double sy = ey.length();
            if (std::abs(ex.dot(ey)) <= 1e-9 * sx * sy &&
                std::abs(sx - sy) <= 1e-9 * std::max(1.0, sx)) {
                const Point2D c_new = xf.apply(ell.center);
                const double rot_xf = std::atan2(ex.y, ex.x);
                const double rot_new = (xf.determinant() < 0.0) ? (rot_xf - ell.rotation) : (rot_xf + ell.rotation);
                return std::make_shared<Ellipse2D>(c_new, ell.rx * sx, ell.ry * sx, rot_new);
            }
            auto pts = ell.sample_polyline(64);
            for (auto& p : pts) p = xf.apply(p);
            return std::make_shared<Polyline2D>(std::move(pts), true);
        }
    }
    return curve.clone();
}

Transform2D BlockReference::to_transform() const noexcept {
    return Transform2D::compose(
        insertion_point.x,
        insertion_point.y,
        rotation_rad,
        scale_x,
        scale_y
    );
}

bool BlockManager::create_block(BlockDefinition def) {
    if (def.name.empty() || find_block(def.name) != nullptr) {
        return false;
    }
    // Reject self-referencing definition at creation
    for (const auto& nr : def.nested_refs) {
        if (nr.block_name == def.name) {
            return false;
        }
    }
    definitions_.push_back(std::move(def));
    return true;
}

bool BlockManager::update_block(const BlockDefinition& def) {
    for (auto& d : definitions_) {
        if (d.name == def.name) {
            // Check for cycles with nested_refs
            for (const auto& nr : def.nested_refs) {
                if (nr.block_name == def.name) return false;
                std::unordered_set<std::string> visited;
                if (references_block_recursive(nr.block_name, def.name, visited)) {
                    return false;
                }
            }
            d = def;
            return true;
        }
    }
    return false;
}

bool BlockManager::remove_block(std::string_view name) {
    const auto it = std::find_if(definitions_.begin(), definitions_.end(), [&](const BlockDefinition& d) {
        return d.name == name;
    });
    if (it == definitions_.end()) return false;
    definitions_.erase(it);
    instances_.erase(
        std::remove_if(instances_.begin(), instances_.end(), [&](const BlockReference& r) {
            return r.block_name == name;
        }),
        instances_.end()
    );
    return true;
}

const BlockDefinition* BlockManager::find_block(std::string_view name) const noexcept {
    for (const auto& d : definitions_) {
        if (d.name == name) return &d;
    }
    return nullptr;
}

std::vector<const BlockDefinition*> BlockManager::blocks() const {
    std::vector<const BlockDefinition*> out;
    out.reserve(definitions_.size());
    for (const auto& d : definitions_) out.push_back(&d);
    return out;
}

std::string BlockManager::place_instance(
    std::string block_name,
    Point2D insertion_point,
    double rotation_rad,
    double scale_x,
    double scale_y,
    std::string layer
) {
    if (find_block(block_name) == nullptr) {
        return {};
    }
    BlockReference ref;
    ref.id = "blk_" + std::to_string(next_instance_id_++);
    ref.block_name = std::move(block_name);
    ref.insertion_point = insertion_point;
    ref.rotation_rad = rotation_rad;
    ref.scale_x = scale_x;
    ref.scale_y = scale_y;
    ref.layer = layer.empty() ? std::string("0") : std::move(layer);
    instances_.push_back(ref);
    return ref.id;
}

bool BlockManager::remove_instance(std::string_view instance_id) {
    const auto it = std::find_if(instances_.begin(), instances_.end(), [&](const BlockReference& r) {
        return r.id == instance_id;
    });
    if (it == instances_.end()) return false;
    instances_.erase(it);
    return true;
}

const BlockReference* BlockManager::find_instance(std::string_view instance_id) const noexcept {
    for (const auto& r : instances_) {
        if (r.id == instance_id) return &r;
    }
    return nullptr;
}

bool BlockManager::references_block_recursive(
    std::string_view root_block,
    std::string_view target_block,
    std::unordered_set<std::string>& visited
) const {
    if (root_block == target_block) return true;
    const std::string key(root_block);
    if (!visited.insert(key).second) return false;
    const BlockDefinition* def = find_block(root_block);
    if (!def) return false;
    for (const auto& nr : def->nested_refs) {
        if (references_block_recursive(nr.block_name, target_block, visited)) {
            return true;
        }
    }
    return false;
}

void BlockManager::resolve_recursive(
    std::string_view block_name,
    const Transform2D& parent_xf,
    std::string_view fallback_layer,
    std::vector<std::string>& call_stack,
    std::vector<ResolvedBlockEntity>& out
) const {
    const BlockDefinition* def = find_block(block_name);
    if (!def) return;
    if (std::find(call_stack.begin(), call_stack.end(), def->name) != call_stack.end()) {
        return; // Cycle guard
    }
    call_stack.push_back(def->name);

    const Transform2D base_shift = Transform2D::translation(-def->base_point.x, -def->base_point.y);
    const Transform2D total_xf = parent_xf * base_shift;

    for (const auto& item : def->items) {
        if (!item.curve) continue;
        ResolvedBlockEntity resolved;
        resolved.curve = transform_curve(*item.curve, total_xf);
        resolved.layer = (item.layer == "0" && !fallback_layer.empty()) ? std::string(fallback_layer) : item.layer;
        resolved.is_construction = item.is_construction;
        out.push_back(std::move(resolved));
    }

    for (const auto& nested : def->nested_refs) {
        const Transform2D nested_xf = total_xf * nested.to_transform();
        const std::string_view child_layer =
            (nested.layer != "0" && !nested.layer.empty()) ? std::string_view(nested.layer) : fallback_layer;
        resolve_recursive(nested.block_name, nested_xf, child_layer, call_stack, out);
    }

    call_stack.pop_back();
}

std::vector<ResolvedBlockEntity> BlockManager::resolve_instance_entities(std::string_view instance_id) const {
    std::vector<ResolvedBlockEntity> out;
    const BlockReference* ref = find_instance(instance_id);
    if (!ref) return out;
    std::vector<std::string> stack;
    resolve_recursive(ref->block_name, ref->to_transform(), ref->layer, stack, out);
    return out;
}

std::vector<ResolvedBlockEntity> BlockManager::resolve_all_instances() const {
    std::vector<ResolvedBlockEntity> out;
    for (const auto& ref : instances_) {
        std::vector<std::string> stack;
        resolve_recursive(ref.block_name, ref.to_transform(), ref.layer, stack, out);
    }
    return out;
}

BBox2D BlockManager::compute_instance_bbox(std::string_view instance_id) const {
    BBox2D bbox;
    const auto resolved = resolve_instance_entities(instance_id);
    for (const auto& item : resolved) {
        if (item.curve) {
            bbox.expand(item.curve->bounding_box());
        }
    }
    return bbox;
}

std::vector<std::string> BlockManager::explode_instance(std::string_view instance_id, SketchDocument2D& doc) {
    std::vector<std::string> created_ids;
    const auto resolved = resolve_instance_entities(instance_id);
    if (resolved.empty() && !find_instance(instance_id)) {
        return created_ids;
    }
    created_ids.reserve(resolved.size());
    for (const auto& r : resolved) {
        if (r.curve) {
            created_ids.push_back(doc.add_entity(r.curve, r.is_construction, r.layer));
        }
    }
    remove_instance(instance_id);
    return created_ids;
}

std::string BlockManager::group_entities(
    SketchDocument2D& doc,
    const std::vector<std::string>& entity_ids,
    std::string group_name,
    Point2D base_point
) {
    if (group_name.empty() || find_block(group_name) != nullptr || entity_ids.empty()) {
        return {};
    }
    BlockDefinition def;
    def.name = group_name;
    def.base_point = base_point;

    std::string primary_layer = "0";
    for (const auto& id : entity_ids) {
        const SketchEntity* e = doc.find_entity(id);
        if (!e || !e->curve) continue;
        if (primary_layer == "0" && !e->layer.empty()) {
            primary_layer = e->layer;
        }
        def.items.push_back(BlockItem{
            std::shared_ptr<Curve2D>(e->curve->clone()),
            e->layer,
            e->is_construction
        });
    }
    if (def.items.empty()) {
        return {};
    }

    if (!create_block(std::move(def))) {
        return {};
    }
    for (const auto& id : entity_ids) {
        doc.remove_entity(id);
    }
    return place_instance(group_name, base_point, 0.0, 1.0, 1.0, primary_layer);
}

} // namespace shunya::cad2d
