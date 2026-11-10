#pragma once

#include "shunya/cad3d/solid_mesh.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace shunya::cad3d {

/**
 * @brief Trilinear-sampled 3D floating-point scalar field volume cache for volumetric raymarching.
 */
class VolumeGrid3D {
public:
    BBox3D world_bounds{{-1.0, -1.0, -1.0}, {1.0, 1.0, 1.0}};
    uint32_t nx{16};
    uint32_t ny{16};
    uint32_t nz{16};
    std::vector<double> data;

    void allocate(uint32_t sx, uint32_t sy, uint32_t sz, const BBox3D& bounds, double init_val = 0.0) {
        nx = std::max<uint32_t>(2, sx);
        ny = std::max<uint32_t>(2, sy);
        nz = std::max<uint32_t>(2, sz);
        world_bounds = bounds;
        data.assign(static_cast<size_t>(nx) * ny * nz, init_val);
    }

    void set_voxel(uint32_t x, uint32_t y, uint32_t z, double val) {
        if (x < nx && y < ny && z < nz) {
            data[(static_cast<size_t>(z) * ny + y) * nx + x] = val;
        }
    }

    [[nodiscard]] double voxel(uint32_t x, uint32_t y, uint32_t z) const noexcept {
        if (x >= nx || y >= ny || z >= nz || data.empty()) return 0.0;
        return data[(static_cast<size_t>(z) * ny + y) * nx + x];
    }

    [[nodiscard]] double sample_world(const Point3D& p) const noexcept {
        if (data.empty() || !world_bounds.is_valid()) return 0.0;
        const Vec3d ext = world_bounds.size();
        if (ext.x <= 1e-12 || ext.y <= 1e-12 || ext.z <= 1e-12) return 0.0;

        const double u = (p.x - world_bounds.min.x) / ext.x;
        const double v = (p.y - world_bounds.min.y) / ext.y;
        const double w = (p.z - world_bounds.min.z) / ext.z;
        if (u < 0.0 || u > 1.0 || v < 0.0 || v > 1.0 || w < 0.0 || w > 1.0) return 0.0;

        const double fx = std::clamp(u, 0.0, 1.0) * static_cast<double>(nx - 1);
        const double fy = std::clamp(v, 0.0, 1.0) * static_cast<double>(ny - 1);
        const double fz = std::clamp(w, 0.0, 1.0) * static_cast<double>(nz - 1);

        const uint32_t x0 = static_cast<uint32_t>(std::floor(fx));
        const uint32_t y0 = static_cast<uint32_t>(std::floor(fy));
        const uint32_t z0 = static_cast<uint32_t>(std::floor(fz));
        const uint32_t x1 = std::min(nx - 1, x0 + 1);
        const uint32_t y1 = std::min(ny - 1, y0 + 1);
        const uint32_t z1 = std::min(nz - 1, z0 + 1);

        const double tx = fx - x0;
        const double ty = fy - y0;
        const double tz = fz - z0;

        const auto lerp = [](double a, double b, double t) noexcept { return a + (b - a) * t; };
        const double c00 = lerp(voxel(x0, y0, z0), voxel(x1, y0, z0), tx);
        const double c10 = lerp(voxel(x0, y1, z0), voxel(x1, y1, z0), tx);
        const double c01 = lerp(voxel(x0, y0, z1), voxel(x1, y0, z1), tx);
        const double c11 = lerp(voxel(x0, y1, z1), voxel(x1, y1, z1), tx);
        return lerp(lerp(c00, c10, ty), lerp(c01, c11, ty), tz);
    }
};

} // namespace shunya::cad3d
