#pragma once

#include "shunya/cad2d/block_manager.hpp"
#include "shunya/cad2d/construction_layer.hpp"
#include "shunya/cad2d/curve_2d.hpp"

#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace shunya::cad2d {

struct GroupEntityItem {
    std::string entity_id;
    std::string original_layer;
};

struct Group2D {
    std::string id;
    std::string name;
    std::string layer{"0"};
    std::vector<GroupEntityItem> entity_items;
    std::vector<std::string> child_group_ids;
    std::string parent_group_id;
    Transform2D local_transform{Transform2D::identity()};
    Point2D pivot{0.0, 0.0};
    bool visible{true};
    bool locked{false};
};

/**
 * @brief High-level Group and Ungroup manager for 2D CAD entities.
 *
 * Preserves relative child coordinates, affine transforms, hierarchy,
 * and individual layer assignments during grouping, nested grouping,
 * group transforms, and ungrouping.
 */
class GroupManager {
public:
    GroupManager() = default;

    /**
     * @brief Creates a group from the specified entities and optional nested groups.
     * Original layer assignments and relative transforms are preserved.
     */
    std::string create_group(
        const SketchDocument2D& doc,
        const std::vector<std::string>& entity_ids,
        const std::vector<std::string>& nested_group_ids = {},
        std::string name = "",
        std::string group_layer = "0"
    );

    /**
     * @brief Dissolves the specified group. Child entities remain in doc with their original
     * layer assignments and current world-space positions. Direct child groups are unparented.
     */
    bool ungroup(SketchDocument2D& doc, std::string_view group_id);

    /**
     * @brief Recursively dissolves the group and all its nested child groups.
     */
    bool ungroup_recursive(SketchDocument2D& doc, std::string_view group_id);

    /**
     * @brief Applies an affine transformation to a group, updating all child entities
     * and nested groups while preserving relative offsets.
     */
    bool transform_group(SketchDocument2D& doc, std::string_view group_id, const Transform2D& xf);

    [[nodiscard]] const Group2D* find_group(std::string_view group_id) const noexcept;
    [[nodiscard]] size_t group_count() const noexcept { return groups_.size(); }
    [[nodiscard]] std::vector<const Group2D*> all_groups() const;

    [[nodiscard]] std::string find_parent_group_of_entity(std::string_view entity_id) const;
    [[nodiscard]] std::string find_parent_group_of_group(std::string_view child_group_id) const;

    [[nodiscard]] std::vector<std::string> collect_all_entity_ids(std::string_view group_id) const;
    [[nodiscard]] BBox2D compute_group_bbox(const SketchDocument2D& doc, std::string_view group_id) const;

    bool set_visible(std::string_view group_id, bool visible, SketchDocument2D* doc = nullptr);
    bool set_locked(std::string_view group_id, bool locked);

    void clear() noexcept;

private:
    uint64_t next_group_id_{1};
    std::unordered_map<std::string, Group2D> groups_;
    std::unordered_map<std::string, std::string> entity_to_group_map_;
};

} // namespace shunya::cad2d
