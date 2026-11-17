#include "shunya/cad2d/clipboard_manager.hpp"

namespace shunya::cad2d {

void ClipboardManager2D::clear() noexcept {
    items_.clear();
    buffer_bbox_ = BBox2D{};
    paste_streak_ = 0;
}

size_t ClipboardManager2D::copy(const SketchDocument2D& doc, const std::vector<std::string>& entity_ids) {
    clear();
    for (const auto& id : entity_ids) {
        if (const auto* ent = doc.find_entity(id)) {
            if (ent->curve) {
                ClipboardItem item;
                item.curve = std::shared_ptr<Curve2D>(ent->curve->clone());
                item.original_layer = ent->layer;
                item.is_construction = ent->is_construction;
                buffer_bbox_.expand(item.curve->bounding_box());
                items_.push_back(std::move(item));
            }
        }
    }
    return items_.size();
}

size_t ClipboardManager2D::cut(SketchDocument2D& doc, const std::vector<std::string>& entity_ids) {
    size_t copied = copy(doc, entity_ids);
    for (const auto& id : entity_ids) {
        doc.remove_entity(id);
    }
    return copied;
}

std::vector<std::string> ClipboardManager2D::paste(SketchDocument2D& doc, Point2D base_offset) {
    if (items_.empty()) return {};

    ++paste_streak_;
    const double dx = base_offset.x * static_cast<double>(paste_streak_);
    const double dy = base_offset.y * static_cast<double>(paste_streak_);
    const auto xf = Transform2D::translation(dx, dy);

    std::vector<std::string> new_ids;
    new_ids.reserve(items_.size());

    for (const auto& item : items_) {
        if (!item.curve) continue;
        auto cloned_curve = std::shared_ptr<Curve2D>(item.curve->clone());
        auto transformed = transform_curve(*cloned_curve, xf);
        std::string new_id = doc.add_entity(transformed, item.is_construction, item.original_layer);
        new_ids.push_back(std::move(new_id));
    }

    return new_ids;
}

std::vector<std::string> ClipboardManager2D::paste_at(SketchDocument2D& doc, Point2D target_center) {
    if (items_.empty()) return {};

    const Point2D c = buffer_bbox_.is_valid() ? buffer_bbox_.center() : Point2D{0.0, 0.0};
    const double dx = target_center.x - c.x;
    const double dy = target_center.y - c.y;
    const auto xf = Transform2D::translation(dx, dy);

    std::vector<std::string> new_ids;
    new_ids.reserve(items_.size());

    for (const auto& item : items_) {
        if (!item.curve) continue;
        auto cloned_curve = std::shared_ptr<Curve2D>(item.curve->clone());
        auto transformed = transform_curve(*cloned_curve, xf);
        std::string new_id = doc.add_entity(transformed, item.is_construction, item.original_layer);
        new_ids.push_back(std::move(new_id));
    }

    return new_ids;
}

std::vector<std::string> ClipboardManager2D::duplicate(
    SketchDocument2D& doc,
    const std::vector<std::string>& entity_ids,
    Point2D offset
) {
    if (entity_ids.empty()) return {};

    const auto xf = Transform2D::translation(offset.x, offset.y);
    std::vector<std::string> new_ids;
    new_ids.reserve(entity_ids.size());

    for (const auto& id : entity_ids) {
        if (const auto* ent = doc.find_entity(id)) {
            if (ent->curve) {
                auto cloned_curve = std::shared_ptr<Curve2D>(ent->curve->clone());
                auto transformed = transform_curve(*cloned_curve, xf);
                std::string new_id = doc.add_entity(transformed, ent->is_construction, ent->layer);
                new_ids.push_back(std::move(new_id));
            }
        }
    }

    return new_ids;
}

} // namespace shunya::cad2d
