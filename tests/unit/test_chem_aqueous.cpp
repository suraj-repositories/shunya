#include "tests/test_framework.hpp"

#include "shunya/chem/aqueous_solution.hpp"
#include "shunya/chem/electrolyte_conductivity.hpp"
#include "shunya/chem/precipitation_checker.hpp"

#include <cmath>

using namespace shunya::chem;

TEST_CASE("CHEM-03-001-01: Strong/weak acid pH, polyprotic phosphate buffer & HCl-NaOH sigmoidal titration") {
    // 1) Pure water autoprotolysis -> pH == 7.000
    {
        AqueousSolutionEngine water;
        auto r = water.solve_equilibrium();
        REQUIRE(r.is_ok());
        CHECK(std::abs(r.value().pH - 7.0) < 1e-6);
    }

    // 2) 0.1 M weak acetic acid (pKa = 4.76) -> pH ~ 0.5 * (4.76 + 1) = 2.88
    {
        AqueousSolutionEngine acetic;
        acetic.add_polyprotic_acid({"CH3COOH", 0.1, {4.76}, 0});
        auto r = acetic.solve_equilibrium();
        REQUIRE(r.is_ok());
        CHECK(std::abs(r.value().pH - 2.883) < 0.01);
    }

    // 3) Equimolar phosphate buffer (0.05 M NaH2PO4 + 0.05 M Na2HPO4 -> 0.1 M H3PO4 + 0.15 M NaOH) -> pH ~ pKa2 = 7.20
    {
        AqueousSolutionEngine phos;
        phos.add_polyprotic_acid({"H3PO4", 0.1, {2.15, 7.20, 12.35}, 0});
        phos.add_strong_base(0.15);
        auto r = phos.solve_equilibrium();
        REQUIRE(r.is_ok());
        CHECK(std::abs(r.value().pH - 7.20) < 0.01);
    }

    // 4) Titration of 50 mL 0.1 M HCl with 0.1 M NaOH -> steep inflection at 50 mL (pH == 7.0)
    std::vector<double> vols{0.0, 0.049, 0.050, 0.051, 0.075};
    const auto curve = AqueousSolutionEngine::titrate_strong_acid_base(0.1, 0.050, 0.1, vols);
    REQUIRE(curve.size() == 5);
    CHECK(std::abs(curve[0].second - 1.0) < 0.01);   // 0 mL -> pH 1.0
    CHECK(curve[1].second < 3.5);                    // 49 mL -> pH ~ 3.0
    CHECK(std::abs(curve[2].second - 7.0) < 1e-6);   // 50 mL -> exact equivalence pH 7.0
    CHECK(curve[3].second > 10.5);                   // 51 mL -> steep jump > 10.5
    CHECK(curve[4].second > 12.0);
}

TEST_CASE("CHEM-03-001-02: Kohlrausch electrolyte conductivity & Ksp AgCl/BaSO4/CaCO3 precipitation") {
    // 1) AgNO3 (0.01 M) + NaCl (0.01 M) -> instantaneous AgCl precipitate (Ksp = 1.8e-10)
    const auto agcl = PrecipitationChecker::evaluate(PrecipitationChecker::agcl(), 0.01, 0.01);
    CHECK(agcl.precipitated);
    CHECK(agcl.reaction_quotient_qsp > agcl.k_sp);
    CHECK(std::abs(agcl.solid_precipitated_mol_l - 0.01) < 2e-5);
    CHECK(std::abs(agcl.remaining_cation_mol_l - std::sqrt(1.8e-10)) < 1e-6);

    // 2) Sub-saturation check: Ba2+ (1e-6 M) + SO4^2- (1e-6 M) -> Q = 1e-12 < 1.1e-10 -> NO precipitate
    const auto sub_ba = PrecipitationChecker::evaluate(PrecipitationChecker::baso4(), 1e-6, 1e-6);
    CHECK(!sub_ba.precipitated);

    // 3) Supersaturated CaCO3 check: Ca2+ (1e-3 M) + CO3^2- (1e-3 M) -> precipitates
    const auto caco3 = PrecipitationChecker::evaluate(PrecipitationChecker::caco3(), 1e-3, 1e-3);
    CHECK(caco3.precipitated);
    CHECK(std::abs(caco3.remaining_cation_mol_l * caco3.remaining_anion_mol_l - caco3.k_sp) < 1e-12);

    // 4) Kohlrausch NaCl molar & specific conductivity check (Lambda_0 = 0.00501 + 0.00763 = 0.01264 S*m^2/mol)
    const double l0 = ElectrolyteConductivityModel::limiting_molar_conductivity({
        {1, 0.00501}, {1, 0.00763}
    });
    CHECK(std::abs(l0 - 0.01264) < 1e-9);
    const double kappa = ElectrolyteConductivityModel::specific_conductivity_s_m(l0, 0.0089, 0.1);
    CHECK(kappa > 0.9 && kappa < 1.2); // 0.1 M NaCl ~ 1.07 S/m
}
