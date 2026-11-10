#pragma once

#include "shunya/core/base/math.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <span>

namespace shunya::phy {

/**
 * @brief Trilinear staggered-grid interpolation & BFECC (Back-and-Forth Error Compensation) advector.
 */
class SemiLagrangianAdvector {
public:
    using VelocitySampleFn = std::function<core::base::math::Vector3<double>(double x, double y, double z)>;

    [[nodiscard]] static double sample_trilinear(
        std::span<const double> field,
        size_t nx, size_t ny, size_t nz,
        double gx, double gy, double gz) noexcept {

        gx = std::clamp(gx, 0.0, static_cast<double>(nx - 1));
        gy = std::clamp(gy, 0.0, static_cast<double>(ny - 1));
        gz = (nz > 1) ? std::clamp(gz, 0.0, static_cast<double>(nz - 1)) : 0.0;

        const size_t i0 = std::min(nx - 1, static_cast<size_t>(std::floor(gx)));
        const size_t j0 = std::min(ny - 1, static_cast<size_t>(std::floor(gy)));
        const size_t k0 = (nz > 1) ? std::min(nz - 1, static_cast<size_t>(std::floor(gz))) : 0;

        const size_t i1 = std::min(nx - 1, i0 + 1);
        const size_t j1 = std::min(ny - 1, j0 + 1);
        const size_t k1 = (nz > 1) ? std::min(nz - 1, k0 + 1) : 0;

        const double tx = gx - static_cast<double>(i0);
        const double ty = gy - static_cast<double>(j0);
        const double tz = gz - static_cast<double>(k0);

        auto at = [&](size_t i, size_t j, size_t k) noexcept {
            return field[(k * ny + j) * nx + i];
        };

        const double c000 = at(i0, j0, k0), c100 = at(i1, j0, k0);
        const double c010 = at(i0, j1, k0), c110 = at(i1, j1, k0);
        const double c001 = at(i0, j0, k1), c101 = at(i1, j0, k1);
        const double c011 = at(i0, j1, k1), c111 = at(i1, j1, k1);

        const double c00 = c000 * (1.0 - tx) + c100 * tx;
        const double c10 = c010 * (1.0 - tx) + c110 * tx;
        const double c01 = c001 * (1.0 - tx) + c101 * tx;
        const double c11 = c011 * (1.0 - tx) + c111 * tx;

        const double c0 = c00 * (1.0 - ty) + c10 * ty;
        const double c1 = c01 * (1.0 - ty) + c11 * ty;

        return c0 * (1.0 - tz) + c1 * tz;
    }

    /**
     * @brief Backtraces world position (x, y, z) using 2nd-order Runge-Kutta (RK2) in velocity field.
     */
    [[nodiscard]] static core::base::math::Vector3<double> trace_rk2(
        double x, double y, double z,
        double dt_s,
        const VelocitySampleFn& vel_fn) noexcept {
        const auto v0 = vel_fn(x, y, z);
        const auto v_mid = vel_fn(
            x - 0.5 * dt_s * v0.x,
            y - 0.5 * dt_s * v0.y,
            z - 0.5 * dt_s * v0.z);
        return {
            x - dt_s * v_mid.x,
            y - dt_s * v_mid.y,
            z - dt_s * v_mid.z
        };
    }

    /**
     * @brief Limited BFECC / MacCormack 2nd-order advection reducing numerical dissipation cleanly.
     */
    static void advect_bfecc(
        std::span<const double> src,
        std::span<double> dst,
        size_t nx, size_t ny, size_t nz,
        double dx,
        double origin_x_off, double origin_y_off, double origin_z_off,
        double dt_s,
        const VelocitySampleFn& vel_fn) {

        const size_t total = nx * ny * nz;
        std::vector<double> fwd(total, 0.0);
        std::vector<double> bwd(total, 0.0);

        auto idx = [&](size_t i, size_t j, size_t k) noexcept {
            return (k * ny + j) * nx + i;
        };

        auto clamp_stencil_bounds = [&](std::span<const double> f, double gx, double gy, double gz, double& mn, double& mx) {
            gx = std::clamp(gx, 0.0, static_cast<double>(nx - 1));
            gy = std::clamp(gy, 0.0, static_cast<double>(ny - 1));
            gz = (nz > 1) ? std::clamp(gz, 0.0, static_cast<double>(nz - 1)) : 0.0;
            const size_t i0 = std::min(nx - 1, static_cast<size_t>(std::floor(gx)));
            const size_t j0 = std::min(ny - 1, static_cast<size_t>(std::floor(gy)));
            const size_t k0 = (nz > 1) ? std::min(nz - 1, static_cast<size_t>(std::floor(gz))) : 0;
            const size_t i1 = std::min(nx - 1, i0 + 1);
            const size_t j1 = std::min(ny - 1, j0 + 1);
            const size_t k1 = (nz > 1) ? std::min(nz - 1, k0 + 1) : 0;
            mn = 1e30;
            mx = -1e30;
            for (size_t kk : {k0, k1}) {
                for (size_t jj : {j0, j1}) {
                    for (size_t ii : {i0, i1}) {
                        const double val = f[idx(ii, jj, kk)];
                        mn = std::min(mn, val);
                        mx = std::max(mx, val);
                    }
                }
            }
        };

        // Step 1: Forward semi-Lagrangian pass
        for (size_t k = 0; k < nz; ++k) {
            const double wz = (static_cast<double>(k) + origin_z_off) * dx;
            for (size_t j = 0; j < ny; ++j) {
                const double wy = (static_cast<double>(j) + origin_y_off) * dx;
                for (size_t i = 0; i < nx; ++i) {
                    const double wx = (static_cast<double>(i) + origin_x_off) * dx;
                    const auto bp = trace_rk2(wx, wy, wz, dt_s, vel_fn);
                    const double gx = bp.x / dx - origin_x_off;
                    const double gy = bp.y / dx - origin_y_off;
                    const double gz = (nz > 1) ? (bp.z / dx - origin_z_off) : 0.0;
                    fwd[idx(i, j, k)] = sample_trilinear(src, nx, ny, nz, gx, gy, gz);
                }
            }
        }

        // Step 2: Backward error estimation pass (-dt)
        for (size_t k = 0; k < nz; ++k) {
            const double wz = (static_cast<double>(k) + origin_z_off) * dx;
            for (size_t j = 0; j < ny; ++j) {
                const double wy = (static_cast<double>(j) + origin_y_off) * dx;
                for (size_t i = 0; i < nx; ++i) {
                    const double wx = (static_cast<double>(i) + origin_x_off) * dx;
                    const auto fp = trace_rk2(wx, wy, wz, -dt_s, vel_fn);
                    const double gx = fp.x / dx - origin_x_off;
                    const double gy = fp.y / dx - origin_y_off;
                    const double gz = (nz > 1) ? (fp.z / dx - origin_z_off) : 0.0;
                    bwd[idx(i, j, k)] = sample_trilinear(fwd, nx, ny, nz, gx, gy, gz);
                }
            }
        }

        // Step 3: Compensated MacCormack / BFECC update with extrema limiter
        for (size_t k = 0; k < nz; ++k) {
            const double wz = (static_cast<double>(k) + origin_z_off) * dx;
            for (size_t j = 0; j < ny; ++j) {
                const double wy = (static_cast<double>(j) + origin_y_off) * dx;
                for (size_t i = 0; i < nx; ++i) {
                    const size_t c = idx(i, j, k);
                    const double corr = fwd[c] + 0.5 * (src[c] - bwd[c]);

                    const double wx = (static_cast<double>(i) + origin_x_off) * dx;
                    const auto bp = trace_rk2(wx, wy, wz, dt_s, vel_fn);
                    const double gx = bp.x / dx - origin_x_off;
                    const double gy = bp.y / dx - origin_y_off;
                    const double gz = (nz > 1) ? (bp.z / dx - origin_z_off) : 0.0;
                    double mn = 0.0, mx = 0.0;
                    clamp_stencil_bounds(src, gx, gy, gz, mn, mx);
                    dst[c] = (corr < mn || corr > mx) ? fwd[c] : corr;
                }
            }
        }
    }
};

} // namespace shunya::phy
