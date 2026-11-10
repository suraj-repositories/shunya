#include "shunya/mat/vsepr_conformation.hpp"

#include "shunya/matdb/periodic_table_extended.hpp"

#include <algorithm>
#include <cmath>
#include <queue>

namespace shunya::mat {

namespace {

inline constexpr double kPi = 3.14159265358979323846;

std::pair<core::base::math::Vector3<double>, core::base::math::Vector3<double>> orthonormal_basis(
    const core::base::math::Vector3<double>& axis_unit) noexcept {
    core::base::math::Vector3<double> ref = (std::abs(axis_unit.z) < 0.9)
                                                ? core::base::math::Vector3<double>{0.0, 0.0, 1.0}
                                                : core::base::math::Vector3<double>{1.0, 0.0, 0.0};
    auto u = axis_unit.cross(ref).normalized();
    auto v = axis_unit.cross(u).normalized();
    return {u, v};
}

} // namespace

const AtomCoordinate3D* MolecularConformation::find_atom(size_t atom_id) const noexcept {
    for (const auto& c : coordinates) {
        if (c.atom_id == atom_id) return &c;
    }
    return nullptr;
}

double MolecularConformation::bond_length_angstrom(size_t a, size_t b) const noexcept {
    const auto* ca = find_atom(a);
    const auto* cb = find_atom(b);
    if (!ca || !cb) return 0.0;
    return (ca->position_angstrom - cb->position_angstrom).length();
}

double MolecularConformation::bond_angle_deg(size_t end_a, size_t center, size_t end_b) const noexcept {
    const auto* ca = find_atom(end_a);
    const auto* cc = find_atom(center);
    const auto* cb = find_atom(end_b);
    if (!ca || !cc || !cb) return 0.0;
    auto va = (ca->position_angstrom - cc->position_angstrom).normalized();
    auto vb = (cb->position_angstrom - cc->position_angstrom).normalized();
    const double d = std::clamp(va.dot(vb), -1.0, 1.0);
    return std::acos(d) * (180.0 / kPi);
}

VseprGeometryClass VseprConformationEngine::classify_geometry(int steric_number, int lone_pairs) noexcept {
    switch (steric_number) {
    case 2:
        return VseprGeometryClass::Linear;
    case 3:
        return (lone_pairs >= 1) ? VseprGeometryClass::Bent : VseprGeometryClass::TrigonalPlanar;
    case 4:
        if (lone_pairs == 0) return VseprGeometryClass::Tetrahedral;
        if (lone_pairs == 1) return VseprGeometryClass::TrigonalPyramidal;
        return VseprGeometryClass::Bent;
    case 5:
        if (lone_pairs == 0) return VseprGeometryClass::TrigonalBipyramidal;
        if (lone_pairs == 1) return VseprGeometryClass::Seesaw;
        if (lone_pairs == 2) return VseprGeometryClass::TShaped;
        return VseprGeometryClass::Linear;
    case 6:
        if (lone_pairs == 0) return VseprGeometryClass::Octahedral;
        if (lone_pairs == 1) return VseprGeometryClass::SquarePyramidal;
        return VseprGeometryClass::SquarePlanar;
    default:
        return VseprGeometryClass::Tetrahedral;
    }
}

double VseprConformationEngine::ideal_bond_angle_deg(int steric_number, int lone_pairs) noexcept {
    switch (steric_number) {
    case 2: return 180.0;
    case 3: return (lone_pairs >= 1) ? 116.5 : 120.0;
    case 4:
        if (lone_pairs == 0) return 109.4712206;
        if (lone_pairs == 1) return 107.0; // Ammonia experimental lone-pair compression
        return 104.45;                       // Water H-O-H experimental 104.5 deg
    case 5:
        return (lone_pairs == 0) ? 90.0 : 89.0;
    case 6:
        return 90.0;
    default:
        return 109.4712206;
    }
}

double VseprConformationEngine::estimate_bond_length_angstrom(int z_a, int z_b, BondOrder order) noexcept {
    auto radius_pm = [&](int z) -> double {
        const auto* ext = matdb::PeriodicTableExtended::find_by_z(z);
        if (!ext) return 75.0;
        if (order == BondOrder::Triple && ext->radii_pm.covalent_triple_pm) {
            return *ext->radii_pm.covalent_triple_pm;
        }
        if ((order == BondOrder::Double || order == BondOrder::Aromatic) && ext->radii_pm.covalent_double_pm) {
            return *ext->radii_pm.covalent_double_pm;
        }
        return ext->radii_pm.covalent_single_pm.value_or(75.0);
    };
    const double sum_pm = radius_pm(z_a) + radius_pm(z_b);
    return sum_pm * 0.01; // 1 pm = 0.01 angstrom
}

std::vector<core::base::math::Vector3<double>> VseprConformationEngine::local_bond_directions(
    int steric_number, int lone_pairs, size_t neighbor_count,
    const core::base::math::Vector3<double>& incoming_axis,
    bool has_incoming_bond) {
    using Vec3 = core::base::math::Vector3<double>;
    std::vector<Vec3> dirs;
    if (neighbor_count == 0) return dirs;

    const Vec3 w = incoming_axis.normalized();
    const auto [u, v] = orthonormal_basis(w);
    const double angle_rad = ideal_bond_angle_deg(steric_number, lone_pairs) * (kPi / 180.0);

    if (!has_incoming_bond) {
        if (steric_number <= 2) {
            dirs.push_back({1.0, 0.0, 0.0});
            dirs.push_back({-1.0, 0.0, 0.0});
        } else if (steric_number == 3) {
            for (size_t k = 0; k < neighbor_count; ++k) {
                const double a = static_cast<double>(k) * (2.0 * kPi / 3.0);
                dirs.push_back({std::cos(a), std::sin(a), 0.0});
            }
        } else if (steric_number == 4) {
            if (neighbor_count == 2) {
                // Bent (H2O): bisecting -y / symmetric in xy at exact ideal_angle (104.45 deg)
                const double half = 0.5 * angle_rad;
                dirs.push_back({ std::sin(half), std::cos(half), 0.0});
                dirs.push_back({-std::sin(half), std::cos(half), 0.0});
            } else if (neighbor_count == 3) {
                // Trigonal Pyramidal (NH3, 107 deg)
                const double cos_alpha_sq = std::clamp((2.0 * std::cos(angle_rad) + 1.0) / 3.0, 0.0, 1.0);
                const double cos_a = std::sqrt(cos_alpha_sq);
                const double sin_a = std::sqrt(std::max(0.0, 1.0 - cos_alpha_sq));
                for (int k = 0; k < 3; ++k) {
                    const double phi = static_cast<double>(k) * (2.0 * kPi / 3.0);
                    dirs.push_back({sin_a * std::cos(phi), sin_a * std::sin(phi), cos_a});
                }
            } else {
                // Regular Tetrahedral (CH4, 109.47 deg)
                dirs.push_back(Vec3{ 1.0,  1.0,  1.0}.normalized());
                dirs.push_back(Vec3{-1.0, -1.0,  1.0}.normalized());
                dirs.push_back(Vec3{-1.0,  1.0, -1.0}.normalized());
                dirs.push_back(Vec3{ 1.0, -1.0, -1.0}.normalized());
            }
        } else if (steric_number == 5) {
            dirs.push_back({0.0, 0.0,  1.0});
            dirs.push_back({0.0, 0.0, -1.0});
            for (int k = 0; k < 3; ++k) {
                const double phi = static_cast<double>(k) * (2.0 * kPi / 3.0);
                dirs.push_back({std::cos(phi), std::sin(phi), 0.0});
            }
        } else {
            // Octahedral
            dirs = {
                {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}
            };
        }
    } else {
        // Place remaining neighbors cleanly away from parent bond `w`
        if (steric_number <= 2) {
            dirs.push_back(w * -1.0);
        } else if (steric_number == 3) {
            const double cos_b = std::cos(kPi - angle_rad);
            const double sin_b = std::sin(kPi - angle_rad);
            dirs.push_back((w * -cos_b + u * sin_b).normalized());
            dirs.push_back((w * -cos_b - u * sin_b).normalized());
        } else {
            // 4 or 6 coordinate cone away from parent
            const double cone = (steric_number == 6) ? (0.5 * kPi) : (kPi - angle_rad);
            const double cos_c = std::cos(cone);
            const double sin_c = std::sin(cone);
            const size_t n = std::max<size_t>(1, neighbor_count);
            for (size_t k = 0; k < n; ++k) {
                const double phi = static_cast<double>(k) * (2.0 * kPi / static_cast<double>(n));
                Vec3 d = w * (-cos_c) + (u * std::cos(phi) + v * std::sin(phi)) * sin_c;
                dirs.push_back(d.normalized());
            }
            if (steric_number == 6 && neighbor_count == 5) {
                // 4 equatorial + 1 trans axial
                dirs[4] = w * -1.0;
            }
        }
    }

    if (dirs.size() > neighbor_count) {
        dirs.resize(neighbor_count);
    }
    return dirs;
}

MolecularConformation VseprConformationEngine::predict_conformation(const MolecularGraph& graph) {
    MolecularConformation conf;
    const size_t n = graph.atom_count();
    if (n == 0) return conf;

    std::vector<bool> placed(n, false);
    std::vector<core::base::math::Vector3<double>> pos(n);
    std::vector<size_t> parent_of(n, n);

    for (size_t root = 0; root < n; ++root) {
        if (placed[root]) continue;
        placed[root] = true;
        pos[root] = {0.0, 0.0, 0.0};

        std::queue<size_t> q;
        q.push(root);
        while (!q.empty()) {
            const size_t cur = q.front();
            q.pop();

            std::vector<std::pair<size_t, BondOrder>> children;
            for (size_t b_idx : graph.incident_bond_indices(cur)) {
                const auto& b = graph.bonds()[b_idx];
                const size_t nb = (b.atom_a == cur) ? b.atom_b : b.atom_a;
                if (!placed[nb]) {
                    children.emplace_back(nb, b.order);
                }
            }
            if (children.empty()) continue;

            const auto& atom = graph.atoms()[cur];
            const int lp = ValenceValidator::estimate_lone_pairs(
                atom.atomic_number, atom.formal_charge, graph.atom_bond_order_sum(cur));
            const int sn = graph.steric_number(cur);

            const bool has_parent = (parent_of[cur] < n);
            core::base::math::Vector3<double> incoming{0.0, 0.0, -1.0};
            if (has_parent) {
                incoming = (pos[parent_of[cur]] - pos[cur]).normalized();
            }

            auto dirs = local_bond_directions(sn, lp, children.size(), incoming, has_parent);
            for (size_t k = 0; k < children.size(); ++k) {
                const size_t nb = children[k].first;
                const double bl = estimate_bond_length_angstrom(
                    atom.atomic_number, graph.atoms()[nb].atomic_number, children[k].second);
                const auto dir = (k < dirs.size()) ? dirs[k] : core::base::math::Vector3<double>{1.0, 0.0, 0.0};
                pos[nb] = pos[cur] + dir * bl;
                placed[nb] = true;
                parent_of[nb] = cur;
                q.push(nb);
            }
        }
    }

    conf.coordinates.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        conf.coordinates.push_back(AtomCoordinate3D{i, graph.atoms()[i].atomic_number, pos[i]});
    }
    return conf;
}

} // namespace shunya::mat
