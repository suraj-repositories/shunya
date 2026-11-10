#pragma once

#include "shunya/core/base/math.hpp"
#include "shunya/pcb/keepout_manager.hpp"

#include <string>
#include <vector>

namespace shunya::pcb {

struct MountingHole {
    core::base::math::Vector2<double> center_mm;
    double drill_diameter_mm{3.2};
    double keepout_annular_margin_mm{0.5};
};

/**
 * @brief Planar PCB contour manager with internal milled cutouts, mounting holes, and keep-out clearance checks.
 */
class BoardOutline {
public:
    void set_outer_contour(std::vector<core::base::math::Vector2<double>> poly_mm);
    static BoardOutline make_rectangle(double width_mm, double height_mm);

    void add_cutout_polygon(std::vector<core::base::math::Vector2<double>> cutout_mm);
    void add_mounting_hole(MountingHole hole);
    void add_keepout_zone(KeepoutZone zone);

    [[nodiscard]] const std::vector<core::base::math::Vector2<double>>& outer_contour() const noexcept {
        return outer_contour_;
    }
    [[nodiscard]] const std::vector<MountingHole>& mounting_holes() const noexcept {
        return holes_;
    }

    /**
     * @brief Returns true if point lies strictly inside outer board contour and outside all cutouts/holes.
     */
    [[nodiscard]] bool contains_copper_region(const core::base::math::Vector2<double>& pt_mm) const noexcept;

    /**
     * @brief Minimum distance from point to board edge, cutout edge, or mounting hole edge (mm).
     */
    [[nodiscard]] double min_edge_clearance_mm(const core::base::math::Vector2<double>& pt_mm) const noexcept;

    [[nodiscard]] bool is_component_placement_allowed(const core::base::math::Vector2<double>& pt_mm,
                                                      double radius_mm,
                                                      double edge_clearance_mm = 0.5) const noexcept;

    [[nodiscard]] bool is_trace_point_allowed(const core::base::math::Vector2<double>& pt_mm,
                                              double trace_half_width_mm,
                                              double edge_clearance_mm = 0.3) const noexcept;

private:
    std::vector<core::base::math::Vector2<double>> outer_contour_;
    std::vector<std::vector<core::base::math::Vector2<double>>> cutouts_;
    std::vector<MountingHole> holes_;
    std::vector<KeepoutZone> keepouts_;
};

} // namespace shunya::pcb
