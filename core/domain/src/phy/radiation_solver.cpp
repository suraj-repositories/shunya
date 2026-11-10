#include "shunya/phy/radiation_solver.hpp"
#include "shunya/core/domain/circuit.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::phy {

namespace {

constexpr double kPi = 3.14159265358979323846;

} // namespace

std::vector<std::vector<double>> ViewFactorRaymarcher::compute_view_factor_matrix(
    const std::vector<RadiativePatch3D>& patches,
    uint32_t samples_per_axis_i,
    uint32_t samples_per_axis_j) noexcept {

    const size_t n = patches.size();
    std::vector<std::vector<double>> F(n, std::vector<double>(n, 0.0));
    const uint32_t ns = std::max<uint32_t>(4, samples_per_axis_i); // spatial samples per axis on emitter
    const uint32_t nr = std::max<uint32_t>(8, samples_per_axis_j); // hemisphere stratified grid per axis

    for (size_t i = 0; i < n; ++i) {
        const auto& pi = patches[i];
        std::vector<uint64_t> hit_counts(n, 0);
        uint64_t total_rays = 0;

        for (uint32_t ui = 0; ui < ns; ++ui) {
            const double su = -1.0 + 2.0 * (ui + 0.5) / static_cast<double>(ns);
            for (uint32_t vi = 0; vi < ns; ++vi) {
                const double sv = -1.0 + 2.0 * (vi + 0.5) / static_cast<double>(ns);
                const auto origin = pi.center + pi.tangent_u * (su * pi.half_u) + pi.tangent_v * (sv * pi.half_v);

                for (uint32_t r1 = 0; r1 < nr; ++r1) {
                    const double u_rad = (r1 + 0.5) / static_cast<double>(nr); // sin^2(theta)
                    const double sin_theta = std::sqrt(u_rad);
                    const double cos_theta = std::sqrt(std::max(0.0, 1.0 - u_rad));
                    for (uint32_t r2 = 0; r2 < nr; ++r2) {
                        const double phi = 2.0 * kPi * (r2 + 0.5) / static_cast<double>(nr);
                        const auto dir = (pi.tangent_u * (std::cos(phi) * sin_theta) +
                                          pi.tangent_v * (std::sin(phi) * sin_theta) +
                                          pi.normal * cos_theta).normalized();
                        ++total_rays;

                        double best_t = 1e30;
                        int best_j = -1;
                        for (size_t j = 0; j < n; ++j) {
                            if (j == i) continue;
                            const auto& pj = patches[j];
                            const double denom = dir.dot(pj.normal);
                            if (denom >= -1e-12) continue; // only front-facing target
                            const double t = (pj.center - origin).dot(pj.normal) / denom;
                            if (t <= 1e-6 || t >= best_t) continue;
                            const auto hit = origin + dir * t;
                            const auto rel = hit - pj.center;
                            if (std::abs(rel.dot(pj.tangent_u)) <= pj.half_u + 1e-9 &&
                                std::abs(rel.dot(pj.tangent_v)) <= pj.half_v + 1e-9) {
                                best_t = t;
                                best_j = static_cast<int>(j);
                            }
                        }
                        if (best_j >= 0) {
                            ++hit_counts[static_cast<size_t>(best_j)];
                        }
                    }
                }
            }
        }

        if (total_rays > 0) {
            for (size_t j = 0; j < n; ++j) {
                F[i][j] = static_cast<double>(hit_counts[j]) / static_cast<double>(total_rays);
            }
        }
    }

    // Symmetrize via exact reciprocity A_i * F_ij = A_j * F_ji
    for (size_t i = 0; i < n; ++i) {
        const double ai = patches[i].area();
        for (size_t j = i + 1; j < n; ++j) {
            const double aj = patches[j].area();
            const double avg_exchange = 0.5 * (ai * F[i][j] + aj * F[j][i]);
            F[i][j] = (ai > 1e-18) ? (avg_exchange / ai) : 0.0;
            F[j][i] = (aj > 1e-18) ? (avg_exchange / aj) : 0.0;
        }
    }
    return F;
}

core::base::Result<std::vector<double>> RadiationSolver::solve_net_heat_flux_watts(
    const std::vector<RadiativePatch3D>& patches,
    const std::vector<std::vector<double>>& view_factors) {
    const size_t n = patches.size();
    if (n == 0 || view_factors.size() != n) {
        return core::base::Result<std::vector<double>>::err(
            core::base::Error{65, core::base::ErrorDomain::Validation, core::base::Severity::Error,
                              "RadiationSolver requires non-empty patches matching view factor matrix", ""});
    }

    core::domain::DenseMatrix M(n, n);
    std::vector<double> rhs(n, 0.0);
    std::vector<double> E_b(n, 0.0);

    for (size_t i = 0; i < n; ++i) {
        const double T = std::max(0.0, patches[i].temperature_k);
        const double T4 = T * T * T * T;
        E_b[i] = kStefanBoltzmann * T4;
        const double eps = std::clamp(patches[i].emissivity, 1e-6, 1.0);
        const double refl = 1.0 - eps;

        rhs[i] = eps * E_b[i];
        M(i, i) = 1.0;
        for (size_t j = 0; j < n; ++j) {
            M.add(i, j, -refl * view_factors[i][j]);
        }
    }

    auto j_res = M.solve_lu(rhs);
    if (!j_res.is_ok()) {
        return core::base::Result<std::vector<double>>::err(j_res.error());
    }
    const auto& J = j_res.value();

    std::vector<double> q_net(n, 0.0);
    for (size_t i = 0; i < n; ++i) {
        const double area = patches[i].area();
        const double eps = std::clamp(patches[i].emissivity, 1e-6, 1.0);
        if (eps < 1.0 - 1e-9) {
            q_net[i] = area * (eps / (1.0 - eps)) * (E_b[i] - J[i]);
        } else {
            double irr = 0.0;
            for (size_t j = 0; j < n; ++j) {
                irr += view_factors[i][j] * J[j];
            }
            q_net[i] = area * (E_b[i] - irr);
        }
    }
    return core::base::Result<std::vector<double>>::ok(std::move(q_net));
}

core::base::Result<std::pair<double, double>> RadiationSolver::simulate_parallel_plates_equilibrium(
    double t1_init_k,
    double t2_init_k,
    double area_m2,
    double thermal_mass_j_k,
    double total_time_s,
    double dt_s,
    double emissivity) {
    if (area_m2 <= 0.0 || thermal_mass_j_k <= 0.0 || dt_s <= 0.0 || total_time_s <= 0.0) {
        return core::base::Result<std::pair<double, double>>::err(
            core::base::Error{66, core::base::ErrorDomain::Validation, core::base::Severity::Error,
                              "Parallel plate equilibrium parameters must be positive", ""});
    }

    const double eps = std::clamp(emissivity, 1e-6, 1.0);
    // Exact diffuse gray view resistance between infinite parallel plates (F12 = F21 = 1)
    const double eps_eff = 1.0 / (2.0 / eps - 1.0);
    const double k_rad = area_m2 * eps_eff * kStefanBoltzmann;

    double T1 = t1_init_k;
    double T2 = t2_init_k;
    const size_t steps = static_cast<size_t>(std::ceil(total_time_s / dt_s));

    for (size_t s = 0; s < steps; ++s) {
        auto dq1 = [&](double ta, double tb) {
            return -k_rad * (std::pow(ta, 4) - std::pow(tb, 4)) / thermal_mass_j_k;
        };
        const double k1_1 = dq1(T1, T2);
        const double k1_2 = -k1_1;

        const double k2_1 = dq1(T1 + 0.5 * dt_s * k1_1, T2 + 0.5 * dt_s * k1_2);
        const double k2_2 = -k2_1;

        const double k3_1 = dq1(T1 + 0.5 * dt_s * k2_1, T2 + 0.5 * dt_s * k2_2);
        const double k3_2 = -k3_1;

        const double k4_1 = dq1(T1 + dt_s * k3_1, T2 + dt_s * k3_2);
        const double k4_2 = -k4_1;

        T1 += (dt_s / 6.0) * (k1_1 + 2.0 * k2_1 + 2.0 * k3_1 + k4_1);
        T2 += (dt_s / 6.0) * (k1_2 + 2.0 * k2_2 + 2.0 * k3_2 + k4_2);
    }

    return core::base::Result<std::pair<double, double>>::ok({T1, T2});
}

} // namespace shunya::phy
