#pragma once

#include "shunya/core/base/result.hpp"

#include <vector>

namespace shunya::integ {

struct CstrReactorSpec {
    double residence_time_tau_s{10.0};
    double feed_conc_mol_m3{2000.0};
    double feed_temp_k{300.0};
    double pre_exponential_a_per_s{5.0e11};
    double activation_energy_j_mol{85000.0};
    double heat_of_reaction_exothermic_j_mol{300000.0}; // -Delta H > 0
    double rho_cp_j_m3_k{4.0e6};
    double ua_per_volume_w_m3_k{2.0e5};
    double coolant_temp_k{300.0};
};

struct CstrState {
    double time_s{0.0};
    double conc_mol_m3{2000.0};
    double temp_k{300.0};
    double rate_constant_per_s{0.0};
};

/**
 * @brief Bidirectional chemo-thermal Continuous Stirred-Tank Reactor (CSTR) connector capturing multi-steady states & hysteresis.
 */
class ChemoThermalConnector {
public:
    static constexpr double kGasConstantR = 8.31446261815324;

    explicit ChemoThermalConnector(CstrReactorSpec spec = {}, double init_conc = 2000.0, double init_temp_k = 300.0);

    [[nodiscard]] double arrhenius_k(double temp_k) const noexcept;
    [[nodiscard]] double steady_conc_at_temp(double temp_k) const noexcept;
    [[nodiscard]] double heat_generation_w_m3(double temp_k) const noexcept;
    [[nodiscard]] double heat_removal_w_m3(double temp_k) const noexcept;

    /**
     * @brief Finds all steady-state operating temperatures [K] on [T_min, T_max] where Q_gen(T) == Q_rem(T).
     */
    [[nodiscard]] std::vector<double> find_steady_state_temps_k(
        double t_min_k = 280.0,
        double t_max_k = 500.0,
        int samples = 2000) const;

    void set_coolant_temp(double t_cool_k) noexcept { spec_.coolant_temp_k = t_cool_k; }

    [[nodiscard]] core::base::Result<CstrState> step(double dt_s);
    [[nodiscard]] core::base::Result<void> advance(double total_time_s, double dt_s);

    [[nodiscard]] const CstrState& state() const noexcept { return state_; }

private:
    CstrReactorSpec spec_;
    CstrState state_;
};

} // namespace shunya::integ
