#pragma once

#include "shunya/cad2d/curve_2d.hpp"
#include "shunya/cad2d/fillet_chamfer.hpp"
#include "shunya/cad2d/construction_layer.hpp"
#include "shunya/core/base/result.hpp"

#include <memory>
#include <string>
#include <vector>

namespace shunya::cad2d {

enum class BlendOperation {
    Fillet,
    Chamfer
};

/**
 * @brief Parameters for an associative 2D fillet or chamfer feature.
 */
struct BlendParameters {
    BlendOperation op{BlendOperation::Fillet};
    double radius{1.0};           // Used when op == Fillet
    double chamfer_dist1{1.0};    // Used when op == Chamfer
    double chamfer_dist2{1.0};    // Used when op == Chamfer
};

/**
 * @brief Associative tracking record linking blend arcs/bevels to source entities.
 */
class AssociativeBlendFeature {
public:
    std::string id;
    BlendParameters parameters;
    std::string entity_a_id;
    std::string entity_b_id;
    std::string blend_entity_id;  // ID of the created Arc2D or Bevel LineSegment2D

    /**
     * @brief Re-evaluates intersection, re-trims legs, and updates the blend entity in doc.
     */
    bool update_associative(SketchDocument2D& doc);
};

/**
 * @brief Interactive 2D Fillet & Chamfer Tool supporting both polyline vertex rounding
 * and associative intersection trimming between line segments.
 */
class AssociativeFilletChamferTool {
public:
    BlendParameters current_params;

    explicit AssociativeFilletChamferTool(BlendParameters params = {}) : current_params(params) {}

    /**
     * @brief Applies a fillet or chamfer to vertex `vertex_idx` of a Polyline2D in-place.
     */
    [[nodiscard]] static core::base::Result<Polyline2D> blend_polyline_vertex(
        const Polyline2D& poly,
        size_t vertex_idx,
        const BlendParameters& params,
        size_t arc_segments = 8
    );

    /**
     * @brief Applies fillet or chamfer between two intersecting LineSegment2D entities in doc.
     * Trims both lines and creates the associative blend entity.
     */
    core::base::Result<AssociativeBlendFeature> apply_between_lines(
        SketchDocument2D& doc,
        const std::string& line_a_id,
        const std::string& line_b_id,
        const BlendParameters& params
    );

    /**
     * @brief Updates all registered associative blend features when underlying lines are modified.
     */
    size_t update_all_features(SketchDocument2D& doc);

    [[nodiscard]] const std::vector<AssociativeBlendFeature>& features() const noexcept { return features_; }
    [[nodiscard]] std::vector<AssociativeBlendFeature>& mutable_features() noexcept { return features_; }

private:
    uint64_t next_id_{1};
    std::vector<AssociativeBlendFeature> features_;
};

} // namespace shunya::cad2d
