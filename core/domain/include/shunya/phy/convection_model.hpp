#pragma once

#include "shunya/phy/nusselt_correlations.hpp"

#include <cstdint>

namespace shunya::phy {

enum class ConvectionGeometry : uint8_t {
    VerticalFlatPlateNatural,
    HorizontalCylinderNatural,
    CylinderCrossFlowForced
};

struct ConvectionEvaluation {
    double h_w_m2_k{0.0};
    double nusselt{0.0};
    double rayleigh_or_reynolds{0.0};
    double heat_flux_w{0.0}; // Positive when leaving hot surface into ambient
};

/**
 * @brief Surface convective heat transfer evaluator using dimensionless Nusselt correlations.
 */
class ConvectionBoundaryModel {
public:
    [[nodiscard]] static ConvectionEvaluation evaluate(
        ConvectionGeometry geometry,
        double surface_temp_k,
        double ambient_temp_k,
        double char_length_m,
        double surface_area_m2,
        const FluidPropertiesAirWater& fluid,
        double free_stream_velocity_m_s = 0.0) noexcept;
};

} // namespace shunya::phy
