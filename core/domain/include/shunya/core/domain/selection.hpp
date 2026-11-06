#pragma once

#include "shunya/core/base/uuid.hpp"
#include "shunya/core/base/math.hpp"
#include "shunya/core/base/result.hpp"
#include "shunya/core/domain/scene_graph.hpp"

#include <string>
#include <string_view>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <optional>
#include <functional>

namespace shunya::core::domain {

enum class SelectionMode {
    Replace, // Clear existing and select target
    Add,     // Union: add target to selection
    Remove,  // Difference: remove target from selection
    Toggle   // Invert selection state for target
};

struct SnapSettings {
    bool grid_snap_enabled = false;
    double grid_size = 1.0;          // in meters/units
    bool rotation_snap_enabled = false;
    double rotation_step_degrees = 15.0; // in degrees
};

class SelectionSet {
public:
    explicit SelectionSet(std::string name = "") : name_(std::move(name)) {}

    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    void set_name(std::string name) { name_ = std::move(name); }

    [[nodiscard]] bool empty() const noexcept { return selected_ids_.empty(); }
    [[nodiscard]] std::size_t size() const noexcept { return selected_ids_.size(); }
    [[nodiscard]] bool contains(const base::Uuid& id) const noexcept {
        return selected_set_.find(id) != selected_set_.end();
    }

    [[nodiscard]] const std::vector<base::Uuid>& items() const noexcept { return selected_ids_; }
    [[nodiscard]] std::optional<base::Uuid> primary() const noexcept { return primary_id_; }

    void clear() noexcept;
    void select(const base::Uuid& id, SelectionMode mode = SelectionMode::Replace);
    void select(const std::vector<base::Uuid>& ids, SelectionMode mode = SelectionMode::Replace);

    // Filter selection matching predicate
    [[nodiscard]] SelectionSet filter(const std::function<bool(const base::Uuid&)>& predicate) const;

private:
    std::string name_;
    std::vector<base::Uuid> selected_ids_;
    std::unordered_set<base::Uuid> selected_set_;
    std::optional<base::Uuid> primary_id_;
};

class SelectionManager {
public:
    using SelectionChangedCallback = std::function<void(const SelectionSet&)>;

    SelectionManager() = default;

    [[nodiscard]] const SelectionSet& current_selection() const noexcept { return active_selection_; }
    SelectionSet& current_selection() noexcept { return active_selection_; }

    void select(const base::Uuid& id, SelectionMode mode = SelectionMode::Replace);
    void select(const std::vector<base::Uuid>& ids, SelectionMode mode = SelectionMode::Replace);
    void clear_selection();

    // Named selection sets
    void save_selection_set(std::string name);
    [[nodiscard]] bool has_selection_set(std::string_view name) const noexcept;
    [[nodiscard]] std::optional<SelectionSet> get_selection_set(std::string_view name) const;
    void remove_selection_set(std::string_view name);
    [[nodiscard]] std::vector<std::string> list_selection_sets() const;

    // Snapping helpers
    [[nodiscard]] const SnapSettings& snap_settings() const noexcept { return snap_settings_; }
    SnapSettings& snap_settings() noexcept { return snap_settings_; }

    [[nodiscard]] base::math::Vec3 snap_position(const base::math::Vec3& pos) const noexcept;
    [[nodiscard]] double snap_rotation_angle(double angle_radians) const noexcept;

    // Editing operations on current selection
    [[nodiscard]] base::Result<std::vector<base::Uuid>> duplicate_selection(
        SceneGraph& scene,
        const base::math::Vec3& translation_offset = {1.0, 0.0, 0.0}
    );

    [[nodiscard]] base::Result<std::size_t> delete_selection(
        SceneGraph& scene,
        bool recursive = true
    );

    void set_on_selection_changed(SelectionChangedCallback cb) {
        on_selection_changed_ = std::move(cb);
    }

private:
    void notify_changed();

    SelectionSet active_selection_;
    std::unordered_map<std::string, SelectionSet> saved_sets_;
    SnapSettings snap_settings_;
    SelectionChangedCallback on_selection_changed_;
};

} // namespace shunya::core::domain
