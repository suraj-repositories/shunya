#include "shunya/cad2d/selection_gizmo.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::cad2d {

namespace {

bool point_in_polygon(const Point2D& pt, const std::vector<Point2D>& poly) {
    if (poly.size() < 3) return false;
    bool inside = false;
    for (size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
        const double xi = poly[i].x, yi = poly[i].y;
        const double xj = poly[j].x, yj = poly[j].y;
        bool intersect = ((yi > pt.y) != (yj > pt.y)) &&
                         (pt.x < (xj - xi) * (pt.y - yi) / (yj - yi + 1e-12) + xi);
        if (intersect) inside = !inside;
    }
    return inside;
}

double dist_sq(const Point2D& a, const Point2D& b) noexcept {
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    return dx * dx + dy * dy;
}

} // namespace

void SelectionGizmo2D::clear_selection() noexcept {
    selected_ids_.clear();
    selected_set_.clear();
    current_bbox_ = BBox2D{};
}

void SelectionGizmo2D::add_to_selection(std::string_view entity_id) {
    if (entity_id.empty()) return;
    std::string id_str(entity_id);
    if (selected_set_.insert(id_str).second) {
        selected_ids_.push_back(std::move(id_str));
    }
}

void SelectionGizmo2D::remove_from_selection(std::string_view entity_id) {
    auto it = selected_set_.find(std::string(entity_id));
    if (it != selected_set_.end()) {
        selected_set_.erase(it);
        selected_ids_.erase(
            std::remove(selected_ids_.begin(), selected_ids_.end(), entity_id),
            selected_ids_.end()
        );
    }
}

void SelectionGizmo2D::toggle_selection(std::string_view entity_id) {
    if (is_selected(entity_id)) {
        remove_from_selection(entity_id);
    } else {
        add_to_selection(entity_id);
    }
}

void SelectionGizmo2D::set_selection(const std::vector<std::string>& entity_ids) {
    clear_selection();
    for (const auto& id : entity_ids) {
        add_to_selection(id);
    }
}

bool SelectionGizmo2D::is_selected(std::string_view entity_id) const noexcept {
    return selected_set_.find(std::string(entity_id)) != selected_set_.end();
}

std::string SelectionGizmo2D::hit_test_entity(
    const SketchDocument2D& doc,
    const Point2D& pt,
    double tolerance
) const {
    const double tol_sq = tolerance * tolerance;
    double best_dist_sq = tol_sq;
    std::string best_id;

    for (const auto& ent : doc.entities()) {
        if (!ent.visible || !ent.curve) continue;
        const BBox2D b = ent.curve->bounding_box();
        if (b.is_valid()) {
            if (pt.x < b.min.x - tolerance || pt.x > b.max.x + tolerance ||
                pt.y < b.min.y - tolerance || pt.y > b.max.y + tolerance) {
                continue;
            }
        }
        Point2D cp = ent.curve->closest_point(pt);
        double d2 = dist_sq(pt, cp);
        if (d2 <= best_dist_sq) {
            best_dist_sq = d2;
            best_id = ent.id;
        }
    }
    return best_id;
}

void SelectionGizmo2D::apply_selection_modifier(
    const std::vector<std::string>& matched_ids,
    SelectionModifier modifier
) {
    switch (modifier) {
        case SelectionModifier::Replace:
            set_selection(matched_ids);
            break;
        case SelectionModifier::Add:
            for (const auto& id : matched_ids) add_to_selection(id);
            break;
        case SelectionModifier::Remove:
            for (const auto& id : matched_ids) remove_from_selection(id);
            break;
        case SelectionModifier::ShiftToggle:
            for (const auto& id : matched_ids) toggle_selection(id);
            break;
    }
}

void SelectionGizmo2D::apply_click(
    const SketchDocument2D& doc,
    const Point2D& pt,
    SelectionModifier modifier,
    double tolerance
) {
    std::string hit_id = hit_test_entity(doc, pt, tolerance);
    if (hit_id.empty()) {
        if (modifier == SelectionModifier::Replace) {
            clear_selection();
        }
    } else {
        apply_selection_modifier({hit_id}, modifier);
    }
    update_bounds(doc);
}

std::vector<std::string> SelectionGizmo2D::select_marquee(
    const SketchDocument2D& doc,
    const Point2D& corner1,
    const Point2D& corner2,
    MarqueeMode mode,
    SelectionModifier modifier
) {
    BBox2D marquee_box;
    marquee_box.expand(corner1);
    marquee_box.expand(corner2);

    std::vector<std::string> matched;
    for (const auto& ent : doc.entities()) {
        if (!ent.visible || !ent.curve) continue;
        const BBox2D ent_box = ent.curve->bounding_box();
        if (!ent_box.is_valid()) continue;

        if (mode == MarqueeMode::Window) {
            // Must be strictly within marquee box
            if (marquee_box.contains(ent_box.min) && marquee_box.contains(ent_box.max)) {
                matched.push_back(ent.id);
            }
        } else {
            // Crossing mode: intersects or inside
            if (marquee_box.intersects(ent_box)) {
                matched.push_back(ent.id);
            }
        }
    }

    apply_selection_modifier(matched, modifier);
    update_bounds(doc);
    return matched;
}

std::vector<std::string> SelectionGizmo2D::select_lasso(
    const SketchDocument2D& doc,
    const std::vector<Point2D>& polygon,
    MarqueeMode mode,
    SelectionModifier modifier
) {
    if (polygon.size() < 3) return {};

    BBox2D poly_box;
    for (const auto& pt : polygon) poly_box.expand(pt);

    std::vector<std::string> matched;
    for (const auto& ent : doc.entities()) {
        if (!ent.visible || !ent.curve) continue;
        const BBox2D ent_box = ent.curve->bounding_box();
        if (!ent_box.is_valid() || !poly_box.intersects(ent_box)) continue;

        // Sample points along the curve
        const auto samples = ent.curve->sample_polyline(16);
        if (samples.empty()) continue;

        if (mode == MarqueeMode::Window) {
            bool all_inside = true;
            for (const auto& s : samples) {
                if (!point_in_polygon(s, polygon)) {
                    all_inside = false;
                    break;
                }
            }
            if (all_inside) matched.push_back(ent.id);
        } else {
            bool any_inside = false;
            for (const auto& s : samples) {
                if (point_in_polygon(s, polygon)) {
                    any_inside = true;
                    break;
                }
            }
            if (any_inside) matched.push_back(ent.id);
        }
    }

    apply_selection_modifier(matched, modifier);
    update_bounds(doc);
    return matched;
}

BBox2D SelectionGizmo2D::update_bounds(const SketchDocument2D& doc) {
    current_bbox_ = BBox2D{};
    for (const auto& id : selected_ids_) {
        if (const auto* ent = doc.find_entity(id)) {
            if (ent->curve) {
                current_bbox_.expand(ent->curve->bounding_box());
            }
        }
    }
    return current_bbox_;
}

Point2D SelectionGizmo2D::pivot_point() const noexcept {
    return current_bbox_.is_valid() ? current_bbox_.center() : Point2D{0.0, 0.0};
}

Point2D SelectionGizmo2D::rotate_handle_pos() const noexcept {
    if (!current_bbox_.is_valid()) return {0.0, 0.0};
    return {current_bbox_.center().x, current_bbox_.max.y + rotation_arm_length_};
}

Point2D SelectionGizmo2D::handle_pos(GizmoHandle handle) const noexcept {
    if (!current_bbox_.is_valid()) return {0.0, 0.0};
    const auto& b = current_bbox_;
    const Point2D c = b.center();

    switch (handle) {
        case GizmoHandle::ScaleTopLeft:     return {b.min.x, b.max.y};
        case GizmoHandle::ScaleTop:         return {c.x, b.max.y};
        case GizmoHandle::ScaleTopRight:    return {b.max.x, b.max.y};
        case GizmoHandle::ScaleRight:       return {b.max.x, c.y};
        case GizmoHandle::ScaleBottomRight: return {b.max.x, b.min.y};
        case GizmoHandle::ScaleBottom:      return {c.x, b.min.y};
        case GizmoHandle::ScaleBottomLeft:  return {b.min.x, b.min.y};
        case GizmoHandle::ScaleLeft:        return {b.min.x, c.y};
        case GizmoHandle::RotateHandle:     return rotate_handle_pos();
        case GizmoHandle::MoveBody:         return c;
        case GizmoHandle::None:             return {0.0, 0.0};
    }
    return {0.0, 0.0};
}

GizmoHandle SelectionGizmo2D::hit_test_gizmo(const Point2D& cursor, double handle_radius) const noexcept {
    if (!current_bbox_.is_valid() || selected_ids_.empty()) return GizmoHandle::None;

    const double min_dim = std::min(current_bbox_.width(), current_bbox_.height());
    const double eff_r = min_dim > 0.0 ? std::min(handle_radius, min_dim * 0.35) : handle_radius;
    const double r2 = eff_r * eff_r;

    // 1. Rotation knob
    if (dist_sq(cursor, rotate_handle_pos()) <= handle_radius * handle_radius) {
        return GizmoHandle::RotateHandle;
    }

    // 2. Corner scale handles
    const GizmoHandle corners[] = {
        GizmoHandle::ScaleTopLeft, GizmoHandle::ScaleTopRight,
        GizmoHandle::ScaleBottomRight, GizmoHandle::ScaleBottomLeft
    };
    for (auto h : corners) {
        if (dist_sq(cursor, handle_pos(h)) <= r2) return h;
    }

    // 3. Edge scale handles
    const GizmoHandle edges[] = {
        GizmoHandle::ScaleTop, GizmoHandle::ScaleRight,
        GizmoHandle::ScaleBottom, GizmoHandle::ScaleLeft
    };
    for (auto h : edges) {
        if (dist_sq(cursor, handle_pos(h)) <= r2) return h;
    }

    // 4. Move body (inside bounding box)
    if (current_bbox_.contains(cursor)) {
        return GizmoHandle::MoveBody;
    }

    return GizmoHandle::None;
}

bool SelectionGizmo2D::begin_transform(GizmoHandle handle, const Point2D& cursor) {
    if (handle == GizmoHandle::None || !current_bbox_.is_valid() || selected_ids_.empty()) {
        return false;
    }
    active_handle_ = handle;
    transform_start_cursor_ = cursor;
    initial_bbox_ = current_bbox_;
    transform_pivot_ = current_bbox_.center();
    return true;
}

Transform2D SelectionGizmo2D::update_transform(
    const Point2D& cursor,
    bool maintain_aspect,
    double angle_snap_rad
) {
    if (!is_transforming()) return Transform2D::identity();

    if (active_handle_ == GizmoHandle::MoveBody) {
        const double dx = cursor.x - transform_start_cursor_.x;
        const double dy = cursor.y - transform_start_cursor_.y;
        return Transform2D::translation(dx, dy);
    }

    if (active_handle_ == GizmoHandle::RotateHandle) {
        const Point2D v0 = {transform_start_cursor_.x - transform_pivot_.x, transform_start_cursor_.y - transform_pivot_.y};
        const Point2D v1 = {cursor.x - transform_pivot_.x, cursor.y - transform_pivot_.y};
        const double a0 = std::atan2(v0.y, v0.x);
        double a1 = std::atan2(v1.y, v1.x);
        double d_theta = a1 - a0;

        if (angle_snap_rad > 1e-4) {
            d_theta = std::round(d_theta / angle_snap_rad) * angle_snap_rad;
        }

        const auto t_to_pivot = Transform2D::translation(-transform_pivot_.x, -transform_pivot_.y);
        const auto rot = Transform2D::rotation(d_theta);
        const auto t_back = Transform2D::translation(transform_pivot_.x, transform_pivot_.y);
        return t_back * rot * t_to_pivot;
    }

    // Scale handles
    const double w0 = std::max(initial_bbox_.width(), 1e-4);
    const double h0 = std::max(initial_bbox_.height(), 1e-4);

    Point2D anchor = transform_pivot_;
    double sx = 1.0;
    double sy = 1.0;

    switch (active_handle_) {
        case GizmoHandle::ScaleTopRight:
            anchor = initial_bbox_.min;
            sx = (cursor.x - anchor.x) / w0;
            sy = (cursor.y - anchor.y) / h0;
            break;
        case GizmoHandle::ScaleTopLeft:
            anchor = {initial_bbox_.max.x, initial_bbox_.min.y};
            sx = (anchor.x - cursor.x) / w0;
            sy = (cursor.y - anchor.y) / h0;
            break;
        case GizmoHandle::ScaleBottomRight:
            anchor = {initial_bbox_.min.x, initial_bbox_.max.y};
            sx = (cursor.x - anchor.x) / w0;
            sy = (anchor.y - cursor.y) / h0;
            break;
        case GizmoHandle::ScaleBottomLeft:
            anchor = initial_bbox_.max;
            sx = (anchor.x - cursor.x) / w0;
            sy = (anchor.y - cursor.y) / h0;
            break;
        case GizmoHandle::ScaleRight:
            anchor = {initial_bbox_.min.x, transform_pivot_.y};
            sx = (cursor.x - anchor.x) / w0;
            sy = 1.0;
            break;
        case GizmoHandle::ScaleLeft:
            anchor = {initial_bbox_.max.x, transform_pivot_.y};
            sx = (anchor.x - cursor.x) / w0;
            sy = 1.0;
            break;
        case GizmoHandle::ScaleTop:
            anchor = {transform_pivot_.x, initial_bbox_.min.y};
            sx = 1.0;
            sy = (cursor.y - anchor.y) / h0;
            break;
        case GizmoHandle::ScaleBottom:
            anchor = {transform_pivot_.x, initial_bbox_.max.y};
            sx = 1.0;
            sy = (anchor.y - cursor.y) / h0;
            break;
        default:
            break;
    }

    if (maintain_aspect && active_handle_ != GizmoHandle::ScaleLeft && active_handle_ != GizmoHandle::ScaleRight &&
        active_handle_ != GizmoHandle::ScaleTop && active_handle_ != GizmoHandle::ScaleBottom) {
        double s = (std::abs(sx) + std::abs(sy)) * 0.5;
        if (sx < 0) s = -s;
        sx = s;
        sy = s;
    }

    const auto t_to_anchor = Transform2D::translation(-anchor.x, -anchor.y);
    const auto sc = Transform2D::scale(sx, sy);
    const auto t_back = Transform2D::translation(anchor.x, anchor.y);
    return t_back * sc * t_to_anchor;
}

bool SelectionGizmo2D::apply_transform(SketchDocument2D& doc, const Transform2D& xf) {
    if (selected_ids_.empty()) return false;
    for (const auto& id : selected_ids_) {
        for (auto& ent : doc.mutable_entities()) {
            if (ent.id == id && ent.curve) {
                ent.curve = transform_curve(*ent.curve, xf);
            }
        }
    }
    update_bounds(doc);
    return true;
}

void SelectionGizmo2D::end_transform() noexcept {
    active_handle_ = GizmoHandle::None;
}

} // namespace shunya::cad2d
