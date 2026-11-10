#pragma once

#include "shunya/core/base/math.hpp"
#include "shunya/core/domain/data_exchange.hpp"
#include "shunya/matdb/crystal_database.hpp"

#include <vector>

namespace shunya::matdb {

struct UnitCellSite {
    core::base::math::Vector3<double> fractional;
    core::base::math::Vector3<double> cartesian_angstrom;
    double occupancy_fraction{1.0};
    double display_radius_angstrom{0.5};
    int atomic_number{29};
};

struct WireframeEdge {
    core::base::math::Vector3<double> start;
    core::base::math::Vector3<double> end;
};

struct UnitCellGeometry {
    std::vector<UnitCellSite> sites;
    std::vector<WireframeEdge> wireframe_edges;
    core::domain::TriangleMesh sphere_mesh;
    double net_atom_count{0.0};
    double atomic_packing_factor{0.0};
};

/**
 * @brief Generates 3D unit cell wireframe, fractional-weighted boundary atoms, and sphere meshes.
 */
class UnitCellGeometryBuilder {
public:
    struct BasisVectors {
        core::base::math::Vector3<double> a1;
        core::base::math::Vector3<double> a2;
        core::base::math::Vector3<double> a3;
    };

    [[nodiscard]] static BasisVectors compute_lattice_vectors(const LatticeParameters& params) noexcept;

    [[nodiscard]] static UnitCellGeometry build_unit_cell(const CrystalStructureRecord& record,
                                                          double radius_scale = 0.35,
                                                          int sphere_segments = 16,
                                                          int sphere_rings = 10);
};

} // namespace shunya::matdb
