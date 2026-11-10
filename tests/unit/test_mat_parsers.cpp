#include "tests/test_framework.hpp"

#include "shunya/mat/formula_parser.hpp"
#include "shunya/mat/smiles_parser.hpp"

#include <cmath>

using namespace shunya::mat;

TEST_CASE("FormulaParser: stoichiometry, hydrates, nested complexes, and IUPAC 2021 molecular weight < 0.01 g/mol") {
    struct Case { const char* formula; double expected_mw; const char* hill; };
    const Case cases[] = {
        {"C2H5OH",         46.069,  "C2H6O"},
        {"CuSO4.5H2O",    249.681,  "CuH10O9S"},
        {"K4[Fe(CN)6]",   368.343,  "C6FeK4N6"},
        {"(NH4)2SO4",     132.139,  "H8N2O4S"},
        {"Ca(NO3)2.4H2O", 236.148,  "CaH8N2O10"},
        {"C55H72MgN4O5",  893.505,  "C55H72MgN4O5"}, // Chlorophyll a
        {"C6H12O6",       180.156,  "C6H12O6"},
        {"H2SO4",          98.078,  "H2O4S"},
        {"Al2(SO4)3",     342.150,  "Al2O12S3"},
        {"Na2CO3.10H2O",  286.141,  "CNa2H20O13"} // note Hill checks sorted alphabetically after C & H: C H20 Na2 O13
    };

    for (size_t i = 0; i < 9; ++i) {
        auto res = FormulaParser::parse(cases[i].formula);
        REQUIRE(res.is_ok());
        REQUIRE(std::abs(res.value().molecular_weight_g_mol - cases[i].expected_mw) < 0.05);
        REQUIRE(res.value().hill_formula() == cases[i].hill);
    }

    auto decahydrate = FormulaParser::parse("Na2CO3.10H2O");
    REQUIRE(decahydrate.is_ok());
    REQUIRE(decahydrate.value().count_of("O") == 13);
    REQUIRE(decahydrate.value().count_of("H") == 20);
    REQUIRE(decahydrate.value().hill_formula() == "CH20Na2O13");

    // Mass fractions sum to 1.0
    auto ethanol = FormulaParser::parse("C2H5OH").value();
    double mf_sum = 0.0;
    for (const auto& [sym, frac] : ethanol.mass_fractions()) {
        (void)sym;
        mf_sum += frac;
    }
    REQUIRE(std::abs(mf_sum - 1.0) < 1e-9);

    // Invalid syntax rejection
    REQUIRE(FormulaParser::parse("").is_err());
    REQUIRE(FormulaParser::parse("Fe(CN)6").is_ok());
    REQUIRE(FormulaParser::parse("Fe(CN").is_err());
    REQUIRE(FormulaParser::parse("Xx2O3").is_err());
}

TEST_CASE("SmilesParser: linear, branched, aromatic ring closure, naphthalene, and aspirin topologies") {
    // Benzene c1ccccc1 -> 6 C (sp2 aromatic) + 6 H = 12 atoms, 12 bonds
    auto bz = SmilesParser::parse("c1ccccc1");
    REQUIRE(bz.is_ok());
    REQUIRE(bz.value().atom_count() == 12);
    REQUIRE(bz.value().bond_count() == 12);
    REQUIRE(bz.value().validate_valences().is_valid());
    REQUIRE(std::abs(bz.value().molecular_weight_amu() - 78.114) < 0.02);

    // Acetic acid CC(=O)O -> C2H4O2 = 8 atoms, 7 bonds
    auto aa = SmilesParser::parse("CC(=O)O");
    REQUIRE(aa.is_ok());
    REQUIRE(aa.value().atom_count() == 8);
    REQUIRE(aa.value().bond_count() == 7);
    REQUIRE(aa.value().validate_valences().is_valid());
    REQUIRE(std::abs(aa.value().molecular_weight_amu() - 60.052) < 0.02);

    // Ethanol CCO -> C2H6O = 9 atoms, 8 bonds
    auto eth = SmilesParser::parse("CCO");
    REQUIRE(eth.is_ok());
    REQUIRE(eth.value().atom_count() == 9);
    REQUIRE(eth.value().validate_valences().is_valid());

    // Aspirin CC(=O)Oc1ccccc1C(=O)O -> C9H8O4 = 21 atoms, 21 bonds
    auto asp = SmilesParser::parse("CC(=O)Oc1ccccc1C(=O)O");
    REQUIRE(asp.is_ok());
    REQUIRE(asp.value().atom_count() == 21);
    REQUIRE(asp.value().bond_count() == 21);
    REQUIRE(asp.value().validate_valences().is_valid());
    REQUIRE(std::abs(asp.value().molecular_weight_amu() - 180.159) < 0.03);

    // Naphthalene c1ccc2ccccc2c1 -> C10H8 = 18 atoms, 19 bonds
    auto naph = SmilesParser::parse("c1ccc2ccccc2c1");
    REQUIRE(naph.is_ok());
    REQUIRE(naph.value().atom_count() == 18);
    REQUIRE(naph.value().bond_count() == 19);
    REQUIRE(naph.value().validate_valences().is_valid());

    // Invalid SMILES syntax rejection
    REQUIRE(SmilesParser::parse("").is_err());
    REQUIRE(SmilesParser::parse("CC(=O").is_err());
    REQUIRE(SmilesParser::parse("c1ccccc").is_err());
}
