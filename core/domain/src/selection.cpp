#include "shunya/core/domain/selection.hpp"

#include <cmath>
#include <numbers>
#include <algorithm>

namespace shunya::core::domain {

void SelectionSet::clear() noexcept {
    selected_ids_.clear();
    selected_set_.clear();
    primary_id_ = std::nullopt;
}

void SelectionSet::select(const base::Uuid& id, SelectionMode mode) {
    switch (mode) {
        case SelectionMode::Replace:
            clear();
            selected_ids_.push_back(id);
            selected_set_.insert(id);
            primary_id_ = id;
            break;

        case SelectionMode::Add:
            if (selected_set_.insert(id).second) {
                selected_ids_.push_back(id);
            }
            primary_id_ = id;
            break;

        case SelectionMode::Remove: {
            auto it = selected_set_.find(id);
            if (it != selected_set_.end()) {
                selected_set_.erase(it);
                selected_ids_.erase(
                    std::remove(selected_ids_.begin(), selected_ids_.end(), id),
                    selected_ids_.end()
                );
                if (primary_id_.has_value() && primary_id_.value() == id) {
                    primary_id_ = selected_ids_.empty() ? std::nullopt : std::optional{selected_ids_.back()};
                }
            }
            break;
        }

        case SelectionMode::Toggle:
            if (contains(id)) {
                select(id, SelectionMode::Remove);
            } else {
                select(id, SelectionMode::Add);
            }
            break;
    }
}

void SelectionSet::select(const std::vector<base::Uuid>& ids, SelectionMode mode) {
    if (mode == SelectionMode::Replace) {
        clear();
        for (const auto& id : ids) {
            select(id, SelectionMode::Add);
        }
    } else {
        for (const auto& id : ids) {
            select(id, mode);
        }
    }
}

SelectionSet SelectionSet::filter(const std::function<bool(const base::Uuid&)>& predicate) const {
    SelectionSet subset(name_ + "_filtered");
    for (const auto& id : selected_ids_) {
        if (predicate(id)) {
            subset.select(id, SelectionMode::Add);
        }
    }
    return subset;
}

void SelectionManager::select(const base::Uuid& id, SelectionMode mode) {
    active_selection_.select(id, mode);
    notify_changed();
}

void SelectionManager::select(const std::vector<base::Uuid>& ids, SelectionMode mode) {
    active_selection_.select(ids, mode);
    notify_changed();
}

void SelectionManager::clear_selection() {
    if (!active_selection_.empty()) {
        active_selection_.clear();
        notify_changed();
    }
}

void SelectionManager::save_selection_set(std::string name) {
    SelectionSet saved(name);
    for (const auto& id : active_selection_.items()) {
        saved.select(id, SelectionMode::Add);
    }
    saved_sets_[std::move(name)] = std::move(saved);
}

bool SelectionManager::has_selection_set(std::string_view name) const noexcept {
    return saved_sets_.find(std::string(name)) != saved_sets_.end();
}

std::optional<SelectionSet> SelectionManager::get_selection_set(std::string_view name) const {
    auto it = saved_sets_.find(std::string(name));
    if (it != saved_sets_.end()) {
        return it->second;
    }
    return std::nullopt;
}

void SelectionManager::remove_selection_set(std::string_view name) {
    saved_sets_.erase(std::string(name));
}

std::vector<std::string> SelectionManager::list_selection_sets() const {
    std::vector<std::string> names;
    names.reserve(saved_sets_.size());
    for (const auto& [name, _] : saved_sets_) {
        names.push_back(name);
    }
    return names;
}

base::math::Vec3 SelectionManager::snap_position(const base::math::Vec3& pos) const noexcept {
    if (!snap_settings_.grid_snap_enabled || snap_settings_.grid_size < 1e-9) {
        return pos;
    }

    double g = snap_settings_.grid_size;
    return base::math::Vec3{
        std::round(pos.x / g) * g,
        std::round(pos.y / g) * g,
        std::round(pos.z / g) * g
    };
}

double SelectionManager::snap_rotation_angle(double angle_radians) const noexcept {
    if (!snap_settings_.rotation_snap_enabled || snap_settings_.rotation_step_degrees < 1e-9) {
        return angle_radians;
    }

    constexpr double deg2rad = std::numbers::pi / 180.0;
    double step_rad = snap_settings_.rotation_step_degrees * deg2rad;
    return std::round(angle_radians / step_rad) * step_rad;
}

base::Result<std::vector<base::Uuid>> SelectionManager::duplicate_selection(
    SceneGraph& scene,
    const base::math::Vec3& translation_offset
) {
    if (active_selection_.empty()) {
        return std::vector<base::Uuid>{};
    }

    std::vector<base::Uuid> new_ids;
    new_ids.reserve(active_selection_.size());

    for (const auto& orig_id : active_selection_.items()) {
        auto orig = scene.get_entity(orig_id);
        if (!orig) continue;

        auto new_ent_res = scene.create_entity(orig->name() + " (Copy)", orig->parent_id());
        if (new_ent_res.is_err()) {
            return new_ent_res.error();
        }

        auto new_ent = new_ent_res.value();
        auto new_xform = orig->local_transform();
        new_xform.position = new_xform.position + translation_offset;
        new_ent->set_local_transform(new_xform);
        new_ent->set_visible(orig->is_visible());
        new_ent->set_active(orig->is_active());

        for (const auto& tag : orig->tags()) {
            new_ent->add_tag(tag);
        }
        for (const auto& [k, v] : orig->domain_attachments()) {
            new_ent->set_attachment(k, v);
        }

        new_ids.push_back(new_ent->id());
    }

    // Update active selection to newly duplicated items
    select(new_ids, SelectionMode::Replace);
    return new_ids;
}

base::Result<std::size_t> SelectionManager::delete_selection(
    SceneGraph& scene,
    bool recursive
) {
    if (active_selection_.empty()) {
        return 0;
    }

    std::size_t deleted_count = 0;
    std::vector<base::Uuid> to_delete = active_selection_.items();

    for (const auto& id : to_delete) {
        if (scene.has_entity(id)) {
            auto res = scene.remove_entity(id, recursive);
            if (res.is_ok()) {
                ++deleted_count;
            }
        }
    }

    clear_selection();
    return deleted_count;
}

void SelectionManager::notify_changed() {
    if (on_selection_changed_) {
        on_selection_changed_(active_selection_);
    }
}

} // namespace shunya::core::domain
