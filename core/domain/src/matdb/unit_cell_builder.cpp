#include "shunya/matdb/unit_cell_builder.hpp"

#include <array>
#include <cmath>

namespace shunya::matdb {

namespace {

inline constexpr double kPi = 3.14159265358979323846;

std::vector<double> periodic_images(double frac) {
    constexpr double kEps = 1e-6;
    if (std::abs(frac) < kEps) {
        return {0.0, 1.0};
    }
    return {frac};
}

double boundary_weight(double fx, double fy, double fz) noexcept {
    constexpr double kEps = 1e-6;
    int boundary_planes = 0;
    if (std::abs(fx) < kEps || std::abs(fx - 1.0) < kEps) ++boundary_planes;
    if (std::abs(fy) < kEps || std::abs(fy - 1.0) < kEps) ++boundary_planes;
    if (std::abs(fz) < kEps || std::abs(fz - 1.0) < kEps) ++boundary_planes;
    // 0 planes -> interior (1.0), 1 -> face (0.5), 2 -> edge (0.25), 3 -> corner (0.125)
    switch (boundary_planes) {
    case 0: return 1.0;
    case 1: return 0.5;
    case 2: return 0.25;
    case 3: return 0.125;
    default: return 1.0;
    }
}

void append_uv_sphere(core::domain::TriangleMesh& mesh,
                      const core::base::math::Vector3<double>& center,
                      double radius,
                      int segments,
                      int rings) {
    const uint32_t base_idx = static_cast<uint32_t>(mesh.vertices.size());
    for (int r = 0; r <= rings; ++r) {
        const double v = static_cast<double>(r) / static_cast<double>(rings);
        const double theta = v * kPi;
        const double sin_t = std::sin(theta);
        const double cos_t = std::cos(theta);
        for (int s = 0; s <= segments; ++s) {
            const double u = static_cast<double>(s) / static_cast<double>(segments);
            const double phi = u * 2.0 * kPi;
            core::base::math::Vector3<double> n{
                sin_t * std::cos(phi),
                sin_t * std::sin(phi),
                cos_t
            };
            mesh.normals.push_back(n);
            mesh.vertices.push_back(center + n * radius);
        }
    }

    const uint32_t stride = static_cast<uint32_t>(segments + 1);
    for (int r = 0; r < rings; ++r) {
        for (int s = 0; s < segments; ++s) {
            const uint32_t i0 = base_idx + static_cast<uint32_t>(r) * stride + static_cast<uint32_t>(s);
            const uint32_t i1 = i0 + 1;
            const uint32_t i2 = i0 + stride;
            const uint32_t i3 = i2 + 1;
            if (r > 0) {
                mesh.faces.push_back({i0, i2, i1});
            }
            if (r + 1 < rings) {
                mesh.faces.push_back({i1, i2, i3});
            }
        }
    }
}

void build_hcp_prism(const CrystalStructureRecord& record,
                     double radius_angstrom,
                     UnitCellGeometry& out) {
    const double a = record.lattice.a_angstrom;
    const double c = record.lattice.c_angstrom;

    std::array<core::base::math::Vector3<double>, 6> bot{};
    std::array<core::base::math::Vector3<double>, 6> top{};
    for (int k = 0; k < 6; ++k) {
        const double ang = static_cast<double>(k) * (kPi / 3.0);
        const double x = a * std::cos(ang);
        const double y = a * std::sin(ang);
        bot[static_cast<size_t>(k)] = {x, y, 0.0};
        top[static_cast<size_t>(k)] = {x, y, c};
        out.sites.push_back({{0.0, 0.0, 0.0}, bot[static_cast<size_t>(k)], 1.0 / 6.0, radius_angstrom, record.atomic_number});
        out.sites.push_back({{0.0, 0.0, 1.0}, top[static_cast<size_t>(k)], 1.0 / 6.0, radius_angstrom, record.atomic_number});
    }

    // 2 basal face centers (1/2 inside each)
    out.sites.push_back({{0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}, 0.5, radius_angstrom, record.atomic_number});
    out.sites.push_back({{0.0, 0.0, 1.0}, {0.0, 0.0, c},   0.5, radius_angstrom, record.atomic_number});

    // 3 interior mid-plane atoms (z = c/2, 1.0 inside each -> total 2 + 1 + 3 = 6 net atoms)
    const double r_mid = a / std::sqrt(3.0);
    for (int k = 0; k < 3; ++k) {
        const double ang = (static_cast<double>(k) * 2.0 + 1.0) * (kPi / 6.0);
        out.sites.push_back({
            {0.0, 0.0, 0.5},
            {r_mid * std::cos(ang), r_mid * std::sin(ang), 0.5 * c},
            1.0, radius_angstrom, record.atomic_number
        });
    }

    // 18 hexagonal prism wireframe edges
    for (int k = 0; k < 6; ++k) {
        const size_t cur = static_cast<size_t>(k);
        const size_t nxt = static_cast<size_t>((k + 1) % 6);
        out.wireframe_edges.push_back({bot[cur], bot[nxt]});
        out.wireframe_edges.push_back({top[cur], top[nxt]});
        out.wireframe_edges.push_back({bot[cur], top[cur]});
    }
}

} // namespace

UnitCellGeometryBuilder::BasisVectors UnitCellGeometryBuilder::compute_lattice_vectors(
    const LatticeParameters& params) noexcept {
    constexpr double kDegToRad = kPi / 180.0;
    const double alpha = params.alpha_deg * kDegToRad;
    const double beta = params.beta_deg * kDegToRad;
    const double gamma = params.gamma_deg * kDegToRad;

    const double ca = std::cos(alpha);
    const double cb = std::cos(beta);
    const double cg = std::cos(gamma);
    const double sg = std::sin(gamma);

    BasisVectors bv;
    bv.a1 = {params.a_angstrom, 0.0, 0.0};
    bv.a2 = {params.b_angstrom * cg, params.b_angstrom * sg, 0.0};

    const double cx = params.c_angstrom * cb;
    const double cy = (sg > 1e-12) ? (params.c_angstrom * (ca - cb * cg) / sg) : 0.0;
    const double cz_sq = params.c_angstrom * params.c_angstrom - cx * cx - cy * cy;
    const double cz = (cz_sq > 0.0) ? std::sqrt(cz_sq) : 0.0;
    bv.a3 = {cx, cy, cz};
    return bv;
}

UnitCellGeometry UnitCellGeometryBuilder::build_unit_cell(const CrystalStructureRecord& record,
                                                          double radius_scale,
                                                          int sphere_segments,
                                                          int sphere_rings) {
    UnitCellGeometry geom;
    geom.atomic_packing_factor = record.atomic_packing_factor();

    const double display_r = record.lattice.a_angstrom * radius_scale * 0.5;
    if (record.prototype == StructurePrototype::HCP) {
        build_hcp_prism(record, display_r, geom);
    } else {
        const auto bv = compute_lattice_vectors(record.lattice);
        auto to_cart = [&](double fx, double fy, double fz) {
            return bv.a1 * fx + bv.a2 * fy + bv.a3 * fz;
        };

        for (const auto& site : record.fractional_basis) {
            for (double fx : periodic_images(site.frac_x)) {
                for (double fy : periodic_images(site.frac_y)) {
                    for (double fz : periodic_images(site.frac_z)) {
                        geom.sites.push_back({
                            {fx, fy, fz},
                            to_cart(fx, fy, fz),
                            boundary_weight(fx, fy, fz),
                            display_r,
                            site.atomic_number
                        });
                    }
                }
            }
        }

        // 12 parallelepiped box edges
        const std::array<core::base::math::Vector3<double>, 8> corners = {
            to_cart(0, 0, 0), to_cart(1, 0, 0), to_cart(1, 1, 0), to_cart(0, 1, 0),
            to_cart(0, 0, 1), to_cart(1, 0, 1), to_cart(1, 1, 1), to_cart(0, 1, 1)
        };
        const int edges[12][2] = {
            {0, 1}, {1, 2}, {2, 3}, {3, 0},
            {4, 5}, {5, 6}, {6, 7}, {7, 4},
            {0, 4}, {1, 5}, {2, 6}, {3, 7}
        };
        for (const auto& e : edges) {
            geom.wireframe_edges.push_back({corners[static_cast<size_t>(e[0])],
                                            corners[static_cast<size_t>(e[1])]});
        }
    }

    double net_atoms = 0.0;
    for (const auto& s : geom.sites) {
        net_atoms += s.occupancy_fraction;
        append_uv_sphere(geom.sphere_mesh, s.cartesian_angstrom, s.display_radius_angstrom,
                         sphere_segments, sphere_rings);
    }
    geom.net_atom_count = net_atoms;
    return geom;
}

} // namespace shunya::matdb
