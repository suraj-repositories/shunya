#pragma once

#include "shunya/mat/geometry_predictor.hpp"

#include <vector>

namespace shunya::mat {

/**
 * @brief Predicts 3D molecular conformation and bond angles via VSEPR steric-number and lone-pair repulsion.
 */
class VseprConformationEngine {
public:
    [[nodiscard]] static VseprGeometryClass classify_geometry(int steric_number, int lone_pairs) noexcept;

    /**
     * @brief Ideal inter-bond angle in degrees accounting for lone-pair compression.
     */
    [[nodiscard]] static double ideal_bond_angle_deg(int steric_number, int lone_pairs) noexcept;

    /**
     * @brief Estimates equilibrium covalent bond length (angstrom) from atomic radii and bond order.
     */
    [[nodiscard]] static double estimate_bond_length_angstrom(int z_a, int z_b, BondOrder order) noexcept;

    /**
     * @brief Generates local unit direction vectors for `neighbor_count` bonds around a steric center.
     */
    [[nodiscard]] static std::vector<core::base::math::Vector3<double>> local_bond_directions(
        int steric_number, int lone_pairs, size_t neighbor_count,
        const core::base::math::Vector3<double>& incoming_axis = {0.0, 0.0, -1.0},
        bool has_incoming_bond = false);

    /**
     * @brief Places all atoms in the molecular graph in 3D space (angstrom).
     */
    [[nodiscard]] static MolecularConformation predict_conformation(const MolecularGraph& graph);
};

} // namespace shunya::mat
