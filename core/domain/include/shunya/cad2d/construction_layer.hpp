#pragma once

#include "shunya/cad2d/curve_2d.hpp"
#include "shunya/cad2d/polygon_boolean.hpp"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace shunya::cad2d {

struct SketchEntity {
    std::string id;
    std::shared_ptr<Curve2D> curve;
    bool is_construction{false};
    std::string layer{"0"};
    bool visible{true};
};

/**
 * @brief Document managing 2D sketch geometry entities and separating Construction Geometry from Solid Profiles.
 */
class SketchDocument2D {
public:
    std::string add_entity(
        std::shared_ptr<Curve2D> curve,
        bool is_construction = false,
        std::string layer = "0"
    );

    bool remove_entity(std::string_view id);
    bool set_construction(std::string_view id, bool is_construction);
    void clear();

    [[nodiscard]] const SketchEntity* find_entity(std::string_view id) const noexcept;
    [[nodiscard]] const std::vector<SketchEntity>& entities() const noexcept { return entities_; }
    [[nodiscard]] std::vector<const SketchEntity*> solid_entities() const;
    [[nodiscard]] std::vector<const SketchEntity*> construction_entities() const;

    /**
     * @brief Extracts closed planar profile loops exclusively from non-construction entities for 3D extrusion.
     */
    [[nodiscard]] Paths2D extract_solid_profile_loops(
        size_t curve_tessellation_segments = 32,
        double endpoint_tol = 1e-6
    ) const;

private:
    uint64_t next_id_{1};
    std::vector<SketchEntity> entities_;
};

} // namespace shunya::cad2d
