#include "tests/test_framework.hpp"

#include "shunya/mat/molecular_graph.hpp"
#include "shunya/mat/molecular_mesh_builder.hpp"
#include "shunya/mat/vsepr_conformation.hpp"

#include <cmath>

using namespace shunya::mat;

namespace {

MolecularGraph build_glucose_graph() {
    // Open-chain D-glucose C6H12O6: 6 carbons, 6 oxygens, 12 hydrogens (24 atoms)
    MolecularGraph g("D-Glucose (C6H12O6)");
    size_t c[6];
    for (int i = 0; i < 6; ++i) c[i] = g.add_atom(6);
    for (int i = 0; i < 5; ++i) (void)g.add_bond(c[i], c[i + 1], BondOrder::Single);

    // C1 carbonyl O(=) + H; C2..C6 each have -OH and remaining H's
    auto o1 = g.add_atom(8);
    (void)g.add_bond(c[0], o1, BondOrder::Double);
    (void)g.add_bond(c[0], g.add_atom(1), BondOrder::Single);

    for (int i = 1; i <= 5; ++i) {
        auto oh = g.add_atom(8);
        (void)g.add_bond(c[i], oh, BondOrder::Single);
        (void)g.add_bond(oh, g.add_atom(1), BondOrder::Single);
        (void)g.add_bond(c[i], g.add_atom(1), BondOrder::Single);
    }
    (void)g.add_bond(c[5], g.add_atom(1), BondOrder::Single);
    return g;
}

MolecularGraph build_caffeine_graph() {
    // Caffeine C8H10N4O2 skeleton (24 atoms)
    MolecularGraph g("Caffeine (C8H10N4O2)");
    // 6-membered pyrimidinedione ring: N1-C2(=O)-N3-C4-C5-C6(=O)
    auto n1 = g.add_atom(7), c2 = g.add_atom(6), n3 = g.add_atom(7);
    auto c4 = g.add_atom(6), c5 = g.add_atom(6), c6 = g.add_atom(6);
    // Fused 5-membered imidazole: C4-N9-C8-N7-C5
    auto n7 = g.add_atom(7), c8 = g.add_atom(6), n9 = g.add_atom(7);
    auto o2 = g.add_atom(8), o6 = g.add_atom(8);

    (void)g.add_bond(n1, c2);
    (void)g.add_bond(c2, n3);
    (void)g.add_bond(n3, c4);
    (void)g.add_bond(c4, c5, BondOrder::Double);
    (void)g.add_bond(c5, c6);
    (void)g.add_bond(c6, n1);
    (void)g.add_bond(c2, o2, BondOrder::Double);
    (void)g.add_bond(c6, o6, BondOrder::Double);

    (void)g.add_bond(c5, n7);
    (void)g.add_bond(n7, c8, BondOrder::Single);
    (void)g.add_bond(c8, n9, BondOrder::Double);
    (void)g.add_bond(n9, c4);
    (void)g.add_bond(c8, g.add_atom(1));

    // Three methyl groups on N1, N3, N7
    for (size_t n_idx : {n1, n3, n7}) {
        auto cm = g.add_atom(6);
        (void)g.add_bond(n_idx, cm);
        for (int h = 0; h < 3; ++h) {
            (void)g.add_bond(cm, g.add_atom(1));
        }
    }
    return g;
}

} // namespace

TEST_CASE("MolecularGraph & ValenceValidator: methane, benzene, H2SO4, SF6 valid; 5-valent C and 2-valent F invalid") {
    REQUIRE(MolecularGraph::make_methane().validate_valences().is_valid());
    REQUIRE(MolecularGraph::make_water().validate_valences().is_valid());
    REQUIRE(MolecularGraph::make_ammonia().validate_valences().is_valid());
    REQUIRE(MolecularGraph::make_benzene().validate_valences().is_valid());
    REQUIRE(MolecularGraph::make_sulfuric_acid().validate_valences().is_valid());
    REQUIRE(MolecularGraph::make_sulfur_hexafluoride().validate_valences().is_valid());

    // 5-valent Carbon detection
    MolecularGraph bad_c("Pentavalent-C");
    auto c = bad_c.add_atom(6);
    for (int i = 0; i < 5; ++i) (void)bad_c.add_bond(c, bad_c.add_atom(1));
    REQUIRE(!bad_c.validate_valences().is_valid());

    // 2-valent neutral Fluorine detection
    MolecularGraph bad_f("Divalent-F");
    auto f = bad_f.add_atom(9);
    (void)bad_f.add_bond(f, bad_f.add_atom(1));
    (void)bad_f.add_bond(f, bad_f.add_atom(1));
    REQUIRE(!bad_f.validate_valences().is_valid());

    auto ch4 = MolecularGraph::make_methane();
    REQUIRE(ch4.effective_hybridization(0) == Hybridization::Sp3);
    REQUIRE(std::abs(ch4.molecular_weight_amu() - 16.043) < 0.01);
}

TEST_CASE("VseprConformationEngine: predicts H2O (104.5 deg), NH3 (107 deg), CH4 (109.47 deg), SF6 (90 deg)") {
    auto water = MolecularGraph::make_water();
    auto w_conf = VseprConformationEngine::predict_conformation(water);
    const double hoh = w_conf.bond_angle_deg(1, 0, 2);
    REQUIRE(std::abs(hoh - 104.45) < 0.5);

    auto ch4 = MolecularGraph::make_methane();
    auto c_conf = VseprConformationEngine::predict_conformation(ch4);
    const double hch = c_conf.bond_angle_deg(1, 0, 2);
    REQUIRE(std::abs(hch - 109.471) < 0.5);

    auto nh3 = MolecularGraph::make_ammonia();
    auto n_conf = VseprConformationEngine::predict_conformation(nh3);
    const double hnh = n_conf.bond_angle_deg(1, 0, 2);
    REQUIRE(std::abs(hnh - 107.0) < 0.5);

    auto sf6 = MolecularGraph::make_sulfur_hexafluoride();
    auto s_conf = VseprConformationEngine::predict_conformation(sf6);
    const double fsf = s_conf.bond_angle_deg(1, 0, 3);
    REQUIRE(std::abs(fsf - 90.0) < 0.5);
}

TEST_CASE("MolecularMeshBuilder: CPK colors, Ball-and-Stick vs Space-Filling CPK on Glucose & Caffeine") {
    REQUIRE(CpkColorTable::color_for_element(6).r < 80);   // Carbon black/charcoal
    REQUIRE(CpkColorTable::color_for_element(8).r > 200);  // Oxygen red
    REQUIRE(CpkColorTable::color_for_element(7).b > 200);  // Nitrogen blue
    REQUIRE(CpkColorTable::color_for_element(1).g > 240);  // Hydrogen white

    auto glu = build_glucose_graph();
    REQUIRE(glu.atom_count() == 24);
    REQUIRE(glu.validate_valences().is_valid());
    auto glu_conf = VseprConformationEngine::predict_conformation(glu);
    auto glu_bas = MolecularMeshBuilder::build_mesh(glu, glu_conf, MolecularRenderStyle::BallAndStick);
    auto glu_cpk = MolecularMeshBuilder::build_mesh(glu, glu_conf, MolecularRenderStyle::SpaceFillingCpk);

    REQUIRE(glu_bas.atoms.size() == 24);
    REQUIRE(glu_bas.bonds.size() == glu.bond_count());
    REQUIRE(glu_cpk.bonds.empty());
    REQUIRE(!glu_bas.mesh.faces.empty());
    REQUIRE(glu_cpk.bounding_radius_angstrom > glu_bas.bounding_radius_angstrom);

    auto caf = build_caffeine_graph();
    REQUIRE(caf.atom_count() == 24);
    REQUIRE(caf.validate_valences().is_valid());
    auto caf_conf = VseprConformationEngine::predict_conformation(caf);
    auto caf_mesh = MolecularMeshBuilder::build_mesh(caf, caf_conf, MolecularRenderStyle::BallAndStick);
    REQUIRE(caf_mesh.atoms.size() == 24);
    REQUIRE(caf_mesh.bounding_radius_angstrom > 2.5);
}
