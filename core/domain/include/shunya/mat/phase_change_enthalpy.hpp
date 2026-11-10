#pragma once

#include "shunya/mat/mushy_zone.hpp"

namespace shunya::mat {

/**
 * @brief Enthalpy-method mushy-zone phase-change model and analytical 1D Neumann Stefan solver.
 */
class PhaseChangeEnthalpyModel {
public:
    explicit PhaseChangeEnthalpyModel(LatentPhaseParameters params) : params_(params) {}

    [[nodiscard]] const LatentPhaseParameters& parameters() const noexcept { return params_; }

    [[nodiscard]] double liquid_fraction(double temp_k) const noexcept;
    [[nodiscard]] double d_liquid_fraction_dT(double temp_k) const noexcept;
    [[nodiscard]] double apparent_cp_j_kg_k(double temp_k) const noexcept;
    [[nodiscard]] double specific_enthalpy_j_kg(double temp_k, double ref_temp_k = 273.15) const noexcept;

    /**
     * @brief Analytical 1D Neumann solidification front thickness s(t) [m] for wall temperature < solidus.
     */
    [[nodiscard]] double neumann_solidification_depth_m(double wall_temp_k, double time_s) const noexcept;

    [[nodiscard]] static PhaseChangeEnthalpyModel water_ice();
    [[nodiscard]] static PhaseChangeEnthalpyModel aluminum_melting();

private:
    LatentPhaseParameters params_;
};

} // namespace shunya::mat
