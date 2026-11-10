#pragma once

#include "shunya/core/base/math.hpp"
#include "shunya/core/base/result.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <vector>

namespace shunya::phy {

enum class SymplecticScheme : uint8_t {
    VelocityVerlet,
    Leapfrog,
    YoshidaRk4
};

struct DynamicalBodyState {
    core::base::math::Vector3<double> position{0.0, 0.0, 0.0};
    core::base::math::Vector3<double> velocity{0.0, 0.0, 0.0};
    double mass{1.0};
};

class EnergyAuditor;

/**
 * @brief Time-reversible symplectic Hamiltonian integrators preserving phase-space volume and total energy.
 */
class SymplecticIntegrator {
public:
    using ForceCallback = std::function<void(
        double t_s,
        std::span<const core::base::math::Vector3<double>> positions,
        std::span<core::base::math::Vector3<double>> out_forces)>;

    using PotentialCallback = std::function<double(
        double t_s,
        std::span<const DynamicalBodyState> bodies)>;

    [[nodiscard]] static core::base::Result<void> step(
        std::vector<DynamicalBodyState>& bodies,
        double t_s,
        double dt_s,
        const ForceCallback& force_fn,
        SymplecticScheme scheme = SymplecticScheme::VelocityVerlet);

    [[nodiscard]] static core::base::Result<void> integrate(
        std::vector<DynamicalBodyState>& bodies,
        size_t num_steps,
        double dt_s,
        const ForceCallback& force_fn,
        SymplecticScheme scheme = SymplecticScheme::YoshidaRk4,
        EnergyAuditor* auditor = nullptr,
        const PotentialCallback& potential_fn = nullptr,
        size_t audit_stride = 1);
};

} // namespace shunya::phy
