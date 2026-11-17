#include "shunya/cad2d/group_manager.hpp"

#include <algorithm>

namespace shunya::cad2d {

std::string GroupManager::create_group(
    const SketchDocument2D& doc,
    const std::vector<std::string>& entity_ids,
    const std::vector<std::string>& nested_group_ids,
    std::string name,
    std::string group_layer
) {
    if (entity_ids.empty() && nested_group_ids.empty()) return {};

    Group2D grp;
    grp.id = "grp_" + std::to_string(next_group_id_++);
    grp.name = name.empty() ? grp.id : std::move(name);
    grp.layer = group_layer.empty() ? "0" : std::move(group_layer);

    BBox2D bbox;

    // Collect entities
    for (const auto& id : entity_ids) {
        if (const auto* ent = doc.find_entity(id)) {
            grp.entity_items.push_back(GroupEntityItem{id, ent->layer});
            entity_to_group_map_[id] = grp.id;
            if (ent->curve) {
                bbox.expand(ent->curve->bounding_box());
            }
        }
    }

    // Collect nested groups
    for (const auto& gid : nested_group_ids) {
        auto it = groups_.find(gid);
        if (it != groups_.end()) {
            it->second.parent_group_id = grp.id;
            grp.child_group_ids.push_back(gid);
            bbox.expand(compute_group_bbox(doc, gid));
        }
    }

    if (grp.entity_items.empty() && grp.child_group_ids.empty()) {
        return {};
    }

    grp.pivot = bbox.is_valid() ? bbox.center() : Point2D{0.0, 0.0};
    const std::string id = grp.id;
    groups_[id] = std::move(grp);
    return id;
}

bool GroupManager::ungroup(SketchDocument2D& doc, std::string_view group_id) {
    auto it = groups_.find(std::string(group_id));
    if (it == groups_.end()) return false;

    const Group2D grp = std::move(it->second);
    groups_.erase(it);

    // Detach entity mappings, ensure layers are intact
    for (const auto& item : grp.entity_items) {
        entity_to_group_map_.erase(item.entity_id);
        for (auto& ent : doc.mutable_entities()) {
            if (ent.id == item.entity_id) {
                ent.layer = item.original_layer;
                break;
            }
        }
    }

    // Detach child groups
    for (const auto& c_gid : grp.child_group_ids) {
        auto c_it = groups_.find(c_gid);
        if (c_it != groups_.end()) {
            c_it->second.parent_group_id = grp.parent_group_id;
        }
    }

    // If parent group existed, replace this group with children in parent
    if (!grp.parent_group_id.empty()) {
        auto p_it = groups_.find(grp.parent_group_id);
        if (p_it != groups_.end()) {
            auto& siblings = p_it->second.child_group_ids;
            siblings.erase(std::remove(siblings.begin(), siblings.end(), grp.id), siblings.end());
            for (const auto& c_gid : grp.child_group_ids) {
                siblings.push_back(c_gid);
            }
            for (const auto& item : grp.entity_items) {
                p_it->second.entity_items.push_back(item);
                entity_to_group_map_[item.entity_id] = grp.parent_group_id;
            }
        }
    }

    return true;
}

bool GroupManager::ungroup_recursive(SketchDocument2D& doc, std::string_view group_id) {
    auto it = groups_.find(std::string(group_id));
    if (it == groups_.end()) return false;

    std::vector<std::string> child_groups = it->second.child_group_ids;
    for (const auto& c_gid : child_groups) {
        ungroup_recursive(doc, c_gid);
    }
    return ungroup(doc, group_id);
}

bool GroupManager::transform_group(SketchDocument2D& doc, std::string_view group_id, const Transform2D& xf) {
    auto it = groups_.find(std::string(group_id));
    if (it == groups_.end()) return false;

    Group2D& grp = it->second;
    grp.local_transform = xf * grp.local_transform;
    grp.pivot = xf.apply(grp.pivot);

    // Transform all direct entities in document
    for (const auto& item : grp.entity_items) {
        for (auto& ent : doc.mutable_entities()) {
            if (ent.id == item.entity_id && ent.curve) {
                ent.curve = transform_curve(*ent.curve, xf);
                break;
            }
        }
    }

    // Recursively transform child groups
    for (const auto& c_gid : grp.child_group_ids) {
        transform_group(doc, c_gid, xf);
    }

    return true;
}

const Group2D* GroupManager::find_group(std::string_view group_id) const noexcept {
    auto it = groups_.find(std::string(group_id));
    return (it != groups_.end()) ? &it->second : nullptr;
}

std::vector<const Group2D*> GroupManager::all_groups() const {
    std::vector<const Group2D*> out;
    out.reserve(groups_.size());
    for (const auto& [_, g] : groups_) {
        out.push_back(&g);
    }
    return out;
}

std::string GroupManager::find_parent_group_of_entity(std::string_view entity_id) const {
    auto it = entity_to_group_map_.find(std::string(entity_id));
    return (it != entity_to_group_map_.end()) ? it->second : std::string{};
}

std::string GroupManager::find_parent_group_of_group(std::string_view child_group_id) const {
    const auto* grp = find_group(child_group_id);
    return grp ? grp->parent_group_id : std::string{};
}

std::vector<std::string> GroupManager::collect_all_entity_ids(std::string_view group_id) const {
    std::vector<std::string> res;
    const auto* grp = find_group(group_id);
    if (!grp) return res;

    for (const auto& item : grp->entity_items) {
        res.push_back(item.entity_id);
    }
    for (const auto& c_gid : grp->child_group_ids) {
        const auto sub = collect_all_entity_ids(c_gid);
        res.insert(res.end(), sub.begin(), sub.end());
    }
    return res;
}

BBox2D GroupManager::compute_group_bbox(const SketchDocument2D& doc, std::string_view group_id) const {
    BBox2D bbox;
    const auto* grp = find_group(group_id);
    if (!grp) return bbox;

    for (const auto& item : grp->entity_items) {
        if (const auto* ent = doc.find_entity(item.entity_id)) {
            if (ent->curve) {
                bbox.expand(ent->curve->bounding_box());
            }
        }
    }
    for (const auto& c_gid : grp->child_group_ids) {
        bbox.expand(compute_group_bbox(doc, c_gid));
    }
    return bbox;
}

bool GroupManager::set_visible(std::string_view group_id, bool visible, SketchDocument2D* doc) {
    auto it = groups_.find(std::string(group_id));
    if (it == groups_.end()) return false;

    it->second.visible = visible;
    if (doc) {
        const auto all_ids = collect_all_entity_ids(group_id);
        for (const auto& id : all_ids) {
            for (auto& ent : doc->mutable_entities()) {
                if (ent.id == id) {
                    ent.visible = visible;
                    break;
                }
            }
        }
    }
    return true;
}

bool GroupManager::set_locked(std::string_view group_id, bool locked) {
    auto it = groups_.find(std::string(group_id));
    if (it == groups_.end()) return false;
    it->second.locked = locked;
    return true;
}

void GroupManager::clear() noexcept {
    groups_.clear();
    entity_to_group_map_.clear();
}

} // namespace shunya::cad2d
