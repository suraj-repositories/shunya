#include "shunya/phy/symplectic_integrator.hpp"
#include "shunya/phy/energy_auditor.hpp"

#include <array>
#include <cmath>

namespace shunya::phy {

namespace {

// 4th-order Yoshida symplectic composition coefficients
constexpr double kCbrt2 = 1.25992104989487316477;
constexpr double kW1 = 1.0 / (2.0 - kCbrt2);
constexpr double kW0 = -kCbrt2 / (2.0 - kCbrt2);
constexpr std::array<double, 4> kYoshidaC{
    0.5 * kW1,
    0.5 * (kW0 + kW1),
    0.5 * (kW0 + kW1),
    0.5 * kW1
};
constexpr std::array<double, 4> kYoshidaD{
    kW1,
    kW0,
    kW1,
    0.0
};

} // namespace

core::base::Result<void> SymplecticIntegrator::step(
    std::vector<DynamicalBodyState>& bodies,
    double t_s,
    double dt_s,
    const ForceCallback& force_fn,
    SymplecticScheme scheme) {
    using V3 = core::base::math::Vector3<double>;
    if (bodies.empty() || dt_s == 0.0 || !force_fn) {
        return core::base::Result<void>::err(
            core::base::Error{50, core::base::ErrorDomain::Validation, core::base::Severity::Error,
                              "SymplecticIntegrator::step requires bodies, non-zero dt, and force callback", ""});
    }

    const size_t n = bodies.size();
    std::vector<V3> pos(n);
    std::vector<V3> forces(n);

    if (scheme == SymplecticScheme::VelocityVerlet) {
        for (size_t i = 0; i < n; ++i) pos[i] = bodies[i].position;
        force_fn(t_s, pos, forces);

        for (size_t i = 0; i < n; ++i) {
            const double inv_m = 1.0 / bodies[i].mass;
            const V3 a0 = forces[i] * inv_m;
            bodies[i].velocity += a0 * (0.5 * dt_s);
            bodies[i].position += bodies[i].velocity * dt_s;
            pos[i] = bodies[i].position;
        }
        force_fn(t_s + dt_s, pos, forces);
        for (size_t i = 0; i < n; ++i) {
            const double inv_m = 1.0 / bodies[i].mass;
            const V3 a1 = forces[i] * inv_m;
            bodies[i].velocity += a1 * (0.5 * dt_s);
        }
        return core::base::Result<void>::ok();
    }

    if (scheme == SymplecticScheme::Leapfrog) {
        for (size_t i = 0; i < n; ++i) pos[i] = bodies[i].position;
        force_fn(t_s, pos, forces);
        for (size_t i = 0; i < n; ++i) {
            const double inv_m = 1.0 / bodies[i].mass;
            bodies[i].velocity += (forces[i] * inv_m) * (0.5 * dt_s);
            bodies[i].position += bodies[i].velocity * dt_s;
            pos[i] = bodies[i].position;
        }
        force_fn(t_s + dt_s, pos, forces);
        for (size_t i = 0; i < n; ++i) {
            const double inv_m = 1.0 / bodies[i].mass;
            bodies[i].velocity += (forces[i] * inv_m) * (0.5 * dt_s);
        }
        return core::base::Result<void>::ok();
    }

    // 4th-order Yoshida symplectic composition
    double t_acc = t_s;
    for (int s = 0; s < 4; ++s) {
        const double c = kYoshidaC[static_cast<size_t>(s)];
        const double d = kYoshidaD[static_cast<size_t>(s)];
        for (size_t i = 0; i < n; ++i) {
            bodies[i].position += bodies[i].velocity * (c * dt_s);
            pos[i] = bodies[i].position;
        }
        t_acc += c * dt_s;
        if (d != 0.0) {
            force_fn(t_acc, pos, forces);
            for (size_t i = 0; i < n; ++i) {
                const double inv_m = 1.0 / bodies[i].mass;
                bodies[i].velocity += (forces[i] * inv_m) * (d * dt_s);
            }
        }
    }
    return core::base::Result<void>::ok();
}

core::base::Result<void> SymplecticIntegrator::integrate(
    std::vector<DynamicalBodyState>& bodies,
    size_t num_steps,
    double dt_s,
    const ForceCallback& force_fn,
    SymplecticScheme scheme,
    EnergyAuditor* auditor,
    const PotentialCallback& potential_fn,
    size_t audit_stride) {
    if (auditor) {
        const double ke = EnergyAuditor::compute_kinetic_energy(bodies);
        const double pe = potential_fn ? potential_fn(0.0, bodies) : 0.0;
        auditor->record(0, 0.0, ke, pe);
    }

    const size_t stride = std::max<size_t>(1, audit_stride);
    for (size_t s = 0; s < num_steps; ++s) {
        const double t = static_cast<double>(s) * dt_s;
        auto res = step(bodies, t, dt_s, force_fn, scheme);
        if (!res.is_ok()) return res;

        if (auditor && ((s + 1) % stride == 0 || s + 1 == num_steps)) {
            const double t_next = static_cast<double>(s + 1) * dt_s;
            const double ke = EnergyAuditor::compute_kinetic_energy(bodies);
            const double pe = potential_fn ? potential_fn(t_next, bodies) : 0.0;
            auditor->record(s + 1, t_next, ke, pe);
        }
    }
    return core::base::Result<void>::ok();
}

} // namespace shunya::phy
