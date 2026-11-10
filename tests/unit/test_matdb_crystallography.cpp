#include "tests/test_framework.hpp"

#include "shunya/matdb/crystal_database.hpp"
#include "shunya/matdb/unit_cell_builder.hpp"

#include <cmath>

using namespace shunya::matdb;

TEST_CASE("CrystallographyDatabase: FCC/BCC/HCP/Diamond lattice constants, APF, and theoretical density < 1.5% error") {
    auto audit = CrystallographyDatabase::audit_densities(1.5);
    REQUIRE(audit.is_valid());
    REQUIRE(CrystallographyDatabase::all_structures().size() >= 25);

    const auto* cu = CrystallographyDatabase::find_by_id("Cu-FCC");
    REQUIRE(cu != nullptr);
    REQUIRE(cu->space_group_number == 225);
    REQUIRE(std::abs(cu->lattice.a_angstrom - 3.6149) < 1e-4);
    REQUIRE(std::abs(cu->atomic_packing_factor() - 0.74048) < 1e-4);
    REQUIRE(cu->density_relative_error_pct() < 1.0);

    const auto* fe_a = CrystallographyDatabase::find_by_id("Fe-alpha");
    const auto* fe_g = CrystallographyDatabase::find_by_id("Fe-gamma");
    REQUIRE(fe_a != nullptr);
    REQUIRE(fe_g != nullptr);
    REQUIRE(fe_a->prototype == StructurePrototype::BCC);
    REQUIRE(fe_g->prototype == StructurePrototype::FCC);
    REQUIRE(std::abs(fe_a->atomic_packing_factor() - 0.68017) < 1e-4);

    const auto* c_dia = CrystallographyDatabase::find_by_id("C-diamond");
    const auto* c_gra = CrystallographyDatabase::find_by_id("C-graphite");
    REQUIRE(c_dia != nullptr);
    REQUIRE(c_gra != nullptr);
    REQUIRE(std::abs(c_dia->atomic_packing_factor() - 0.34009) < 1e-4);
    REQUIRE(c_dia->density_relative_error_pct() < 0.5);
    REQUIRE(c_gra->density_relative_error_pct() < 0.5);

    REQUIRE(!CrystallographyDatabase::to_json().empty());
}

TEST_CASE("UnitCellGeometryBuilder: net atom count SC(1), BCC(2), FCC(4), HCP(6), Diamond(8) and 3D mesh generation") {
    // Simple Cubic (Polonium alpha): 8 corners * 1/8 = 1.0
    const auto* po = CrystallographyDatabase::find_by_id("Po-SC");
    REQUIRE(po != nullptr);
    auto sc_geom = UnitCellGeometryBuilder::build_unit_cell(*po);
    REQUIRE(sc_geom.sites.size() == 8);
    REQUIRE(std::abs(sc_geom.net_atom_count - 1.0) < 1e-9);
    REQUIRE(sc_geom.wireframe_edges.size() == 12);
    REQUIRE(!sc_geom.sphere_mesh.faces.empty());

    // BCC (Fe-alpha): 8 corners (1/8) + 1 center (1.0) = 2.0
    const auto* fe = CrystallographyDatabase::find_by_id("Fe-alpha");
    auto bcc_geom = UnitCellGeometryBuilder::build_unit_cell(*fe);
    REQUIRE(bcc_geom.sites.size() == 9);
    REQUIRE(std::abs(bcc_geom.net_atom_count - 2.0) < 1e-9);
    REQUIRE(bcc_geom.wireframe_edges.size() == 12);

    // FCC (Cu): 8 corners (1/8) + 6 face centers (1/2) = 4.0
    const auto* cu = CrystallographyDatabase::find_by_id("Cu-FCC");
    auto fcc_geom = UnitCellGeometryBuilder::build_unit_cell(*cu);
    REQUIRE(fcc_geom.sites.size() == 14);
    REQUIRE(std::abs(fcc_geom.net_atom_count - 4.0) < 1e-9);
    REQUIRE(fcc_geom.wireframe_edges.size() == 12);

    // HCP (Ti-alpha): 12 hex corners (1/6) + 2 basal face centers (1/2) + 3 interior (1) = 6.0
    const auto* ti = CrystallographyDatabase::find_by_id("Ti-alpha-HCP");
    REQUIRE(ti != nullptr);
    auto hcp_geom = UnitCellGeometryBuilder::build_unit_cell(*ti);
    REQUIRE(hcp_geom.sites.size() == 17);
    REQUIRE(std::abs(hcp_geom.net_atom_count - 6.0) < 1e-9);
    REQUIRE(hcp_geom.wireframe_edges.size() == 18);

    // Diamond Cubic (Si): 8 corners (1/8) + 6 face centers (1/2) + 4 interior (1) = 8.0
    const auto* si = CrystallographyDatabase::find_by_id("Si-diamond");
    REQUIRE(si != nullptr);
    auto dia_geom = UnitCellGeometryBuilder::build_unit_cell(*si);
    REQUIRE(dia_geom.sites.size() == 18);
    REQUIRE(std::abs(dia_geom.net_atom_count - 8.0) < 1e-9);
}
