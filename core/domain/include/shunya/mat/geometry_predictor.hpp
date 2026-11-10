#pragma once

#include "shunya/core/base/math.hpp"
#include "shunya/mat/molecular_graph.hpp"

#include <cstdint>

namespace shunya::mat {

enum class VseprGeometryClass : uint8_t {
    Linear,
    Bent,
    TrigonalPlanar,
    TrigonalPyramidal,
    Tetrahedral,
    Seesaw,
    TShaped,
    TrigonalBipyramidal,
    SquarePyramidal,
    Octahedral,
    SquarePlanar
};

struct AtomCoordinate3D {
    size_t atom_id{0};
    int atomic_number{1};
    core::base::math::Vector3<double> position_angstrom;
};

struct MolecularConformation {
    std::vector<AtomCoordinate3D> coordinates;

    [[nodiscard]] const AtomCoordinate3D* find_atom(size_t atom_id) const noexcept;
    [[nodiscard]] double bond_length_angstrom(size_t a, size_t b) const noexcept;
    [[nodiscard]] double bond_angle_deg(size_t end_a, size_t center, size_t end_b) const noexcept;
};

} // namespace shunya::mat
