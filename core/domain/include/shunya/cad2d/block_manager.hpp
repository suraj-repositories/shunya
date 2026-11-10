#pragma once

#include "shunya/cad2d/bezier_spline.hpp"
#include "shunya/cad2d/construction_layer.hpp"
#include "shunya/cad2d/curve_2d.hpp"

#include <memory>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace shunya::cad2d {

/**
 * @brief 2D affine transformation matrix:
 *   x' = a*x + b*y + tx
 *   y' = c*x + d*y + ty
 */
struct Transform2D {
    double a{1.0};
    double b{0.0};
    double c{0.0};
    double d{1.0};
    double tx{0.0};
    double ty{0.0};

    [[nodiscard]] static constexpr Transform2D identity() noexcept { return {}; }

    [[nodiscard]] static constexpr Transform2D translation(double dx, double dy) noexcept {
        return {1.0, 0.0, 0.0, 1.0, dx, dy};
    }

    [[nodiscard]] static Transform2D rotation(double angle_rad) noexcept;

    [[nodiscard]] static constexpr Transform2D scale(double sx, double sy) noexcept {
        return {sx, 0.0, 0.0, sy, 0.0, 0.0};
    }

    /**
     * @brief Composes T(tx, ty) * R(rotation_rad) * S(sx, sy).
     */
    [[nodiscard]] static Transform2D compose(
        double tx,
        double ty,
        double rotation_rad,
        double sx,
        double sy
    ) noexcept;

    [[nodiscard]] constexpr Transform2D operator*(const Transform2D& rhs) const noexcept {
        return {
            a * rhs.a + b * rhs.c,
            a * rhs.b + b * rhs.d,
            c * rhs.a + d * rhs.c,
            c * rhs.b + d * rhs.d,
            a * rhs.tx + b * rhs.ty + tx,
            c * rhs.tx + d * rhs.ty + ty
        };
    }

    [[nodiscard]] constexpr Point2D apply(const Point2D& pt) const noexcept {
        return {a * pt.x + b * pt.y + tx, c * pt.x + d * pt.y + ty};
    }

    [[nodiscard]] constexpr double determinant() const noexcept {
        return a * d - b * c;
    }
};

/**
 * @brief Applies an affine Transform2D to a Curve2D and returns a newly allocated transformed curve.
 */
[[nodiscard]] std::shared_ptr<Curve2D> transform_curve(const Curve2D& curve, const Transform2D& xf);

struct BlockItem {
    std::shared_ptr<Curve2D> curve;
    std::string layer{"0"};
    bool is_construction{false};
};

struct BlockReference {
    std::string id;
    std::string block_name;
    Point2D insertion_point{0.0, 0.0};
    double rotation_rad{0.0};
    double scale_x{1.0};
    double scale_y{1.0};
    std::string layer{"0"};

    [[nodiscard]] Transform2D to_transform() const noexcept;
};

struct BlockDefinition {
    std::string name;
    Point2D base_point{0.0, 0.0};
    std::vector<BlockItem> items;
    std::vector<BlockReference> nested_refs;
};

struct ResolvedBlockEntity {
    std::shared_ptr<Curve2D> curve;
    std::string layer{"0"};
    bool is_construction{false};
};

/**
 * @brief Manages reusable 2D CAD blocks, compound grouping, hierarchical instancing, and explosion.
 */
class BlockManager {
public:
    bool create_block(BlockDefinition def);
    bool update_block(const BlockDefinition& def);
    bool remove_block(std::string_view name);

    [[nodiscard]] const BlockDefinition* find_block(std::string_view name) const noexcept;
    [[nodiscard]] std::vector<const BlockDefinition*> blocks() const;
    [[nodiscard]] size_t block_count() const noexcept { return definitions_.size(); }

    std::string place_instance(
        std::string block_name,
        Point2D insertion_point,
        double rotation_rad = 0.0,
        double scale_x = 1.0,
        double scale_y = 1.0,
        std::string layer = "0"
    );

    bool remove_instance(std::string_view instance_id);
    [[nodiscard]] const BlockReference* find_instance(std::string_view instance_id) const noexcept;
    [[nodiscard]] const std::vector<BlockReference>& instances() const noexcept { return instances_; }
    [[nodiscard]] size_t instance_count() const noexcept { return instances_.size(); }

    [[nodiscard]] std::vector<ResolvedBlockEntity> resolve_instance_entities(std::string_view instance_id) const;
    [[nodiscard]] std::vector<ResolvedBlockEntity> resolve_all_instances() const;
    [[nodiscard]] BBox2D compute_instance_bbox(std::string_view instance_id) const;

    /**
     * @brief Explodes an instance into world-space entities inside doc and removes the instance.
     */
    std::vector<std::string> explode_instance(std::string_view instance_id, SketchDocument2D& doc);

    /**
     * @brief Creates a BlockDefinition from selected doc entities, removes originals, and places an instance.
     */
    std::string group_entities(
        SketchDocument2D& doc,
        const std::vector<std::string>& entity_ids,
        std::string group_name,
        Point2D base_point = {0.0, 0.0}
    );

private:
    bool references_block_recursive(
        std::string_view root_block,
        std::string_view target_block,
        std::unordered_set<std::string>& visited
    ) const;

    void resolve_recursive(
        std::string_view block_name,
        const Transform2D& parent_xf,
        std::string_view fallback_layer,
        std::vector<std::string>& call_stack,
        std::vector<ResolvedBlockEntity>& out
    ) const;

    uint64_t next_instance_id_{1};
    std::vector<BlockDefinition> definitions_;
    std::vector<BlockReference> instances_;
};

} // namespace shunya::cad2d
