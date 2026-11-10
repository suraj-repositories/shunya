#include "shunya/phy/convection_model.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::phy {

ConvectionEvaluation ConvectionBoundaryModel::evaluate(
    ConvectionGeometry geometry,
    double surface_temp_k,
    double ambient_temp_k,
    double char_length_m,
    double surface_area_m2,
    const FluidPropertiesAirWater& fluid,
    double free_stream_velocity_m_s) noexcept {

    const double lc = std::max(1e-6, char_length_m);
    const double dT = surface_temp_k - ambient_temp_k;

    double dimless = 0.0;
    double nu = 1.0;

    if (geometry == ConvectionGeometry::VerticalFlatPlateNatural) {
        dimless = NusseltCorrelations::rayleigh(fluid.beta_per_k, dT, lc, fluid.nu_m2_s, fluid.prandtl);
        nu = NusseltCorrelations::churchill_chu_vertical_plate(dimless, fluid.prandtl);
    } else if (geometry == ConvectionGeometry::HorizontalCylinderNatural) {
        dimless = NusseltCorrelations::rayleigh(fluid.beta_per_k, dT, lc, fluid.nu_m2_s, fluid.prandtl);
        nu = NusseltCorrelations::churchill_chu_horizontal_cylinder(dimless, fluid.prandtl);
    } else {
        dimless = NusseltCorrelations::reynolds(free_stream_velocity_m_s, lc, fluid.nu_m2_s);
        nu = NusseltCorrelations::churchill_bernstein_cylinder(dimless, fluid.prandtl);
    }

    const double h = nu * fluid.k_w_m_k / lc;
    const double q = h * surface_area_m2 * dT;

    ConvectionEvaluation out;
    out.h_w_m2_k = h;
    out.nusselt = nu;
    out.rayleigh_or_reynolds = dimless;
    out.heat_flux_w = q;
    return out;
}

} // namespace shunya::phy
