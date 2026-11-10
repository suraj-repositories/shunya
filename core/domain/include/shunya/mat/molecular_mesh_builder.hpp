#pragma once

#include "shunya/core/domain/data_exchange.hpp"
#include "shunya/mat/cpk_color_table.hpp"
#include "shunya/mat/vsepr_conformation.hpp"

#include <vector>

namespace shunya::mat {

enum class MolecularRenderStyle : uint8_t {
    BallAndStick,
    SpaceFillingCpk
};

struct AtomMeshInstance {
    size_t atom_id{0};
    int atomic_number{6};
    core::base::math::Vector3<double> center_angstrom;
    double radius_angstrom{0.5};
    CpkRgb color;
};

struct BondCylinderInstance {
    size_t atom_a{0};
    size_t atom_b{0};
    core::base::math::Vector3<double> start_angstrom;
    core::base::math::Vector3<double> end_angstrom;
    double radius_angstrom{0.12};
    BondOrder order{BondOrder::Single};
};

struct MolecularMeshScene {
    core::domain::TriangleMesh mesh;
    std::vector<AtomMeshInstance> atoms;
    std::vector<BondCylinderInstance> bonds;
    double bounding_radius_angstrom{0.0};
};

/**
 * @brief Generates 3D Ball-and-Stick and CPK Space-Filling triangle meshes and instances for molecules.
 */
class MolecularMeshBuilder {
public:
    [[nodiscard]] static double display_radius_angstrom(int atomic_number,
                                                        MolecularRenderStyle style) noexcept;

    [[nodiscard]] static MolecularMeshScene build_mesh(const MolecularGraph& graph,
                                                       const MolecularConformation& conformation,
                                                       MolecularRenderStyle style = MolecularRenderStyle::BallAndStick,
                                                       int sphere_segments = 14,
                                                       int sphere_rings = 8,
                                                       int cylinder_segments = 10);
};

} // namespace shunya::mat
