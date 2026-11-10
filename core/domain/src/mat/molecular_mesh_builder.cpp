#include "shunya/mat/molecular_mesh_builder.hpp"

#include "shunya/matdb/periodic_table_extended.hpp"

#include <cmath>
#include <cstdio>

namespace shunya::mat {

namespace {

inline constexpr double kPi = 3.14159265358979323846;

void append_sphere(core::domain::TriangleMesh& mesh,
                   const core::base::math::Vector3<double>& center,
                   double radius,
                   int segments,
                   int rings) {
    const uint32_t base = static_cast<uint32_t>(mesh.vertices.size());
    for (int r = 0; r <= rings; ++r) {
        const double theta = (static_cast<double>(r) / static_cast<double>(rings)) * kPi;
        const double st = std::sin(theta);
        const double ct = std::cos(theta);
        for (int s = 0; s <= segments; ++s) {
            const double phi = (static_cast<double>(s) / static_cast<double>(segments)) * 2.0 * kPi;
            core::base::math::Vector3<double> n{st * std::cos(phi), st * std::sin(phi), ct};
            mesh.normals.push_back(n);
            mesh.vertices.push_back(center + n * radius);
        }
    }
    const uint32_t stride = static_cast<uint32_t>(segments + 1);
    for (int r = 0; r < rings; ++r) {
        for (int s = 0; s < segments; ++s) {
            const uint32_t i0 = base + static_cast<uint32_t>(r) * stride + static_cast<uint32_t>(s);
            const uint32_t i1 = i0 + 1;
            const uint32_t i2 = i0 + stride;
            const uint32_t i3 = i2 + 1;
            if (r > 0) mesh.faces.push_back({i0, i2, i1});
            if (r + 1 < rings) mesh.faces.push_back({i1, i2, i3});
        }
    }
}

void append_cylinder(core::domain::TriangleMesh& mesh,
                     const core::base::math::Vector3<double>& a,
                     const core::base::math::Vector3<double>& b,
                     double radius,
                     int segments) {
    const auto axis = b - a;
    const double len = axis.length();
    if (len <= 1e-9) return;
    const auto w = axis / len;
    const core::base::math::Vector3<double> ref = (std::abs(w.z) < 0.9)
                                                      ? core::base::math::Vector3<double>{0.0, 0.0, 1.0}
                                                      : core::base::math::Vector3<double>{1.0, 0.0, 0.0};
    const auto u = w.cross(ref).normalized();
    const auto v = w.cross(u).normalized();

    const uint32_t base = static_cast<uint32_t>(mesh.vertices.size());
    for (int s = 0; s <= segments; ++s) {
        const double phi = (static_cast<double>(s) / static_cast<double>(segments)) * 2.0 * kPi;
        const auto radial = u * std::cos(phi) + v * std::sin(phi);
        mesh.normals.push_back(radial);
        mesh.vertices.push_back(a + radial * radius);
        mesh.normals.push_back(radial);
        mesh.vertices.push_back(b + radial * radius);
    }
    for (int s = 0; s < segments; ++s) {
        const uint32_t i0 = base + static_cast<uint32_t>(2 * s);
        const uint32_t i1 = i0 + 1;
        const uint32_t i2 = i0 + 2;
        const uint32_t i3 = i0 + 3;
        mesh.faces.push_back({i0, i2, i1});
        mesh.faces.push_back({i1, i2, i3});
    }
}

} // namespace

std::string CpkRgb::to_hex() const {
    char buf[8];
    std::snprintf(buf, sizeof(buf), "#%02x%02x%02x", r, g, b);
    return std::string(buf);
}

double MolecularMeshBuilder::display_radius_angstrom(int atomic_number,
                                                     MolecularRenderStyle style) noexcept {
    const auto* ext = matdb::PeriodicTableExtended::find_by_z(atomic_number);
    if (style == MolecularRenderStyle::SpaceFillingCpk) {
        const double vdw_pm = (ext && ext->radii_pm.vdw_pm) ? *ext->radii_pm.vdw_pm : 170.0;
        return vdw_pm * 0.01;
    }
    const double cov_pm = (ext && ext->radii_pm.covalent_single_pm)
                              ? *ext->radii_pm.covalent_single_pm
                              : 75.0;
    return std::max(0.22, cov_pm * 0.01 * 0.48);
}

MolecularMeshScene MolecularMeshBuilder::build_mesh(const MolecularGraph& graph,
                                                    const MolecularConformation& conformation,
                                                    MolecularRenderStyle style,
                                                    int sphere_segments,
                                                    int sphere_rings,
                                                    int cylinder_segments) {
    MolecularMeshScene scene;

    core::base::math::Vector3<double> centroid{0.0, 0.0, 0.0};
    if (!conformation.coordinates.empty()) {
        for (const auto& c : conformation.coordinates) {
            centroid = centroid + c.position_angstrom;
        }
        centroid = centroid / static_cast<double>(conformation.coordinates.size());
    }

    for (const auto& atom : graph.atoms()) {
        const auto* coord = conformation.find_atom(atom.id);
        if (!coord) continue;
        const double r = display_radius_angstrom(atom.atomic_number, style);
        append_sphere(scene.mesh, coord->position_angstrom, r, sphere_segments, sphere_rings);
        scene.atoms.push_back(AtomMeshInstance{
            atom.id, atom.atomic_number, coord->position_angstrom, r,
            CpkColorTable::color_for_element(atom.atomic_number)
        });
        const double bound = (coord->position_angstrom - centroid).length() + r;
        if (bound > scene.bounding_radius_angstrom) {
            scene.bounding_radius_angstrom = bound;
        }
    }

    if (style == MolecularRenderStyle::BallAndStick) {
        constexpr double kStickR = 0.12;
        for (const auto& bond : graph.bonds()) {
            const auto* ca = conformation.find_atom(bond.atom_a);
            const auto* cb = conformation.find_atom(bond.atom_b);
            if (!ca || !cb) continue;
            append_cylinder(scene.mesh, ca->position_angstrom, cb->position_angstrom,
                            kStickR, cylinder_segments);
            scene.bonds.push_back(BondCylinderInstance{
                bond.atom_a, bond.atom_b, ca->position_angstrom, cb->position_angstrom,
                kStickR, bond.order
            });
        }
    }

    return scene;
}

} // namespace shunya::mat
