#pragma once

#include "shunya/phy/symplectic_integrator.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include <vector>

namespace shunya::phy {

struct EnergySample {
    size_t step{0};
    double time_s{0.0};
    double kinetic_j{0.0};
    double potential_j{0.0};
    double total_j{0.0};
    double relative_drift{0.0};
};

/**
 * @brief Tracks kinetic, potential, and total mechanical energy drift across symplectic trajectories.
 */
class EnergyAuditor {
public:
    [[nodiscard]] static double compute_kinetic_energy(std::span<const DynamicalBodyState> bodies) noexcept {
        double ke = 0.0;
        for (const auto& b : bodies) {
            ke += 0.5 * b.mass * b.velocity.length_squared();
        }
        return ke;
    }

    void record(size_t step, double time_s, double kinetic_j, double potential_j) {
        const double total = kinetic_j + potential_j;
        if (!has_baseline_) {
            baseline_energy_j_ = total;
            has_baseline_ = true;
        }
        const double denom = std::max(1e-12, std::abs(baseline_energy_j_));
        const double drift = std::abs(total - baseline_energy_j_) / denom;
        max_rel_drift_ = std::max(max_rel_drift_, drift);
        samples_.push_back({step, time_s, kinetic_j, potential_j, total, drift});
    }

    [[nodiscard]] double baseline_energy_j() const noexcept { return baseline_energy_j_; }
    [[nodiscard]] double max_relative_drift() const noexcept { return max_rel_drift_; }
    [[nodiscard]] const std::vector<EnergySample>& samples() const noexcept { return samples_; }
    void reset() noexcept {
        has_baseline_ = false;
        baseline_energy_j_ = 0.0;
        max_rel_drift_ = 0.0;
        samples_.clear();
    }

private:
    bool has_baseline_{false};
    double baseline_energy_j_{0.0};
    double max_rel_drift_{0.0};
    std::vector<EnergySample> samples_;
};

} // namespace shunya::phy
