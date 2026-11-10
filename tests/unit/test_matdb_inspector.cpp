#include "tests/test_framework.hpp"

#include "shunya/matdb/bohr_orbital_model.hpp"
#include "shunya/matdb/decay_chain_builder.hpp"

#include <cmath>

using namespace shunya::matdb;

TEST_CASE("BohrOrbitalModel: shell counts match Z across all 118 neutral elements and spherical harmonic density") {
    auto audit = BohrOrbitalModel::audit_all_118_bohr_counts();
    REQUIRE(audit.is_valid());

    // Copper (Z=29): 2, 8, 18, 1
    auto cu = BohrOrbitalModel::compute_for_element(29);
    REQUIRE(cu.is_ok());
    REQUIRE(cu.value().shells.size() == 4);
    REQUIRE(cu.value().shells[0].electrons == 2);
    REQUIRE(cu.value().shells[1].electrons == 8);
    REQUIRE(cu.value().shells[2].electrons == 18);
    REQUIRE(cu.value().shells[3].electrons == 1);

    // Uranium (Z=92): 2, 8, 18, 32, 21, 9, 2 = 92
    auto u = BohrOrbitalModel::compute_for_element(92);
    REQUIRE(u.is_ok());
    REQUIRE(u.value().shells.size() == 7);
    REQUIRE(u.value().total_electrons == 92);

    // Spherical harmonic s orbital is isotropic; p_z vanishes in equatorial plane (theta=pi/2)
    constexpr double kHalfPi = 1.5707963267948966;
    const double s0 = BohrOrbitalModel::spherical_harmonic_density(0, 0, 0.0, 0.0);
    const double s_eq = BohrOrbitalModel::spherical_harmonic_density(0, 0, kHalfPi, 1.0);
    REQUIRE(std::abs(s0 - s_eq) < 1e-9);
    REQUIRE(BohrOrbitalModel::spherical_harmonic_density(1, 0, kHalfPi, 0.0) < 1e-9);
    REQUIRE(!BohrOrbitalModel::angular_orbital_glsl_fragment().empty());
}

TEST_CASE("DecayChainGraphBuilder: U-238 14-step chain to Pb-206, Th-232, C-14, and A/Z conservation") {
    auto u238 = DecayChainGraphBuilder::build_uranium_238_series();
    REQUIRE(u238.edges.size() == 14);
    REQUIRE(u238.nodes.front().label == "U-238");
    REQUIRE(u238.nodes.back().label == "Pb-206");
    REQUIRE(u238.nodes.back().is_stable);
    REQUIRE(u238.terminal_stable_count() == 1);
    REQUIRE(DecayChainGraphBuilder::audit_conservation(u238).is_valid());

    auto th232 = DecayChainGraphBuilder::build_thorium_232_series();
    REQUIRE(th232.nodes.back().label == "Pb-208");
    REQUIRE(DecayChainGraphBuilder::audit_conservation(th232).is_valid());

    auto c14 = DecayChainGraphBuilder::build_carbon_14_decay();
    REQUIRE(c14.edges.size() == 1);
    REQUIRE(c14.nodes.back().label == "N-14");
    REQUIRE(DecayChainGraphBuilder::audit_conservation(c14).is_valid());

    REQUIRE(DecayChainGraphBuilder::build_series("Unknown").is_err());
}
