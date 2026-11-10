#pragma once

#include "shunya/cad2d/curve_2d.hpp"
#include "shunya/cad2d/construction_layer.hpp"
#include "shunya/cad2d/sketch_viewport_2d.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace shunya::cad2d {

enum class SnapType : uint32_t {
    None         = 0,
    Endpoint     = 1u << 0,
    Midpoint     = 1u << 1,
    Center       = 1u << 2,
    Quadrant     = 1u << 3,
    Intersection = 1u << 4,
    Tangent      = 1u << 5,
    All          = Endpoint | Midpoint | Center | Quadrant | Intersection | Tangent
};

[[nodiscard]] constexpr uint32_t operator|(SnapType a, SnapType b) noexcept {
    return static_cast<uint32_t>(a) | static_cast<uint32_t>(b);
}

[[nodiscard]] constexpr std::string_view snap_type_to_string(SnapType t) noexcept {
    switch (t) {
        case SnapType::Endpoint:     return "Endpoint";
        case SnapType::Midpoint:     return "Midpoint";
        case SnapType::Center:       return "Center";
        case SnapType::Quadrant:     return "Quadrant";
        case SnapType::Intersection: return "Intersection";
        case SnapType::Tangent:      return "Tangent";
        default: return "None";
    }
}

struct SnapCandidate {
    Point2D world_pos{};
    SnapType type{SnapType::None};
    std::string entity_id;
    double distance_px{0.0};
};

/**
 * @brief Spatial hash grid and analytical geometry engine for real-time 2D object snapping.
 */
class SnapTargetManager {
public:
    explicit SnapTargetManager(double hash_cell_size = 1.0);

    /**
     * @brief Rebuilds the 2D spatial hash grid index from all visible entities in a SketchDocument2D.
     */
    void rebuild_index(const SketchDocument2D& doc);

    /**
     * @brief Queries the best snap target near cursor_world within tolerance_px in viewport scale.
     */
    [[nodiscard]] std::optional<SnapCandidate> query_snap(
        const SketchDocument2D& doc,
        const SketchViewport2D& viewport,
        const Point2D& cursor_world,
        double tolerance_px = 10.0,
        uint32_t enabled_mask = static_cast<uint32_t>(SnapType::All),
        std::optional<Point2D> active_anchor = std::nullopt
    ) const;

    /**
     * @brief Computes all exact analytical intersections between two Curve2D primitives.
     */
    [[nodiscard]] static std::vector<Point2D> intersect_curves(
        const Curve2D& a,
        const Curve2D& b
    );

    /**
     * @brief Computes analytical tangent points on a circle/arc from an external anchor point.
     */
    [[nodiscard]] static std::vector<Point2D> find_tangent_points(
        const Point2D& anchor,
        const Curve2D& curve
    );

private:
    struct CellKey {
        int64_t ix{0};
        int64_t iy{0};
        bool operator==(const CellKey& o) const noexcept { return ix == o.ix && iy == o.iy; }
    };

    struct CellHash {
        size_t operator()(const CellKey& k) const noexcept {
            uint64_t h1 = static_cast<uint64_t>(k.ix) * 0x9e3779b97f4a7c15ULL;
            uint64_t h2 = static_cast<uint64_t>(k.iy) * 0xbf58476d1ce4e5b9ULL;
            return static_cast<size_t>(h1 ^ (h2 >> 19));
        }
    };

    double cell_size_{1.0};
    std::unordered_map<CellKey, std::vector<size_t>, CellHash> grid_;
    std::vector<size_t> large_entities_; // Entities spanning many cells cleanly indexed separately
};

} // namespace shunya::cad2d
