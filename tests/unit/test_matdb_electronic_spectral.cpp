#include "tests/test_framework.hpp"

#include "shunya/matdb/electron_config_parser.hpp"
#include "shunya/matdb/ionization_repository.hpp"
#include "shunya/matdb/orbital_engine.hpp"
#include "shunya/matdb/spectral_catalog.hpp"

#include <cmath>

using namespace shunya::matdb;

TEST_CASE("OrbitalFillingEngine: Madelung Aufbau, Hund's rule multiplicity, and all 118 IUPAC configurations") {
    auto audit = OrbitalFillingEngine::audit_all_118_elements();
    REQUIRE(audit.is_valid());

    // Copper (Z=29): Aufbau exception [Ar] 3d10 4s1, 1 unpaired electron, doublet (2S+1 = 2)
    auto cu = OrbitalFillingEngine::validate(29, "[Ar] 3d10 4s1");
    REQUIRE(cu.is_ok());
    REQUIRE(!cu.value().matches_madelung_aufbau);
    REQUIRE(cu.value().is_known_exception);
    REQUIRE(cu.value().parsed.total_electrons() == 29);
    REQUIRE(cu.value().unpaired_electrons == 1);
    REQUIRE(std::abs(cu.value().spin_multiplicity - 2.0) < 1e-9);

    // Chromium (Z=24): [Ar] 3d5 4s1 -> 6 unpaired electrons, septet (2S+1 = 7)
    auto cr = OrbitalFillingEngine::validate(24, "[Ar] 3d5 4s1");
    REQUIRE(cr.is_ok());
    REQUIRE(cr.value().is_known_exception);
    REQUIRE(cr.value().unpaired_electrons == 6);
    REQUIRE(std::abs(cr.value().spin_multiplicity - 7.0) < 1e-9);

    // Gadolinium (Z=64, Lanthanide exception) & Uranium (Z=92, Actinide exception)
    auto gd = OrbitalFillingEngine::validate(64, "[Xe] 4f7 5d1 6s2");
    REQUIRE(gd.is_ok());
    REQUIRE(gd.value().is_known_exception);
    REQUIRE(gd.value().unpaired_electrons == 8);

    auto u = OrbitalFillingEngine::validate(92, "[Rn] 5f3 6d1 7s2");
    REQUIRE(u.is_ok());
    REQUIRE(u.value().is_known_exception);
    REQUIRE(u.value().unpaired_electrons == 4);

    // Invalid Pauli exclusion & electron count rejection
    REQUIRE(OrbitalFillingEngine::validate(6, "[He] 2s2 2p7").is_err());
    REQUIRE(OrbitalFillingEngine::validate(6, "[He] 2s2 2p3").is_err());
}

TEST_CASE("IonizationEnergyRepository: noble gas peaks, alkali minima, strict IE monotonicity, and unit conversions") {
    auto audit = IonizationEnergyRepository::audit_monotonicity();
    REQUIRE(audit.is_valid());
    REQUIRE(IonizationEnergyRepository::all_profiles().size() == 118);

    // Helium holds highest 1st ionization energy (~24.587 eV), Cesium alkali metal minimum (~3.894 eV)
    auto he_ie1 = IonizationEnergyRepository::first_ionization_ev(2);
    auto cs_ie1 = IonizationEnergyRepository::first_ionization_ev(55);
    REQUIRE(he_ie1.has_value());
    REQUIRE(cs_ie1.has_value());
    REQUIRE(std::abs(*he_ie1 - 24.5874) < 1e-3);
    REQUIRE(std::abs(*cs_ie1 - 3.8939) < 1e-3);

    // Period 2 peaks at Ne (21.565 eV) and bottoms at Li (5.392 eV)
    auto li_ie1 = IonizationEnergyRepository::first_ionization_ev(3);
    auto ne_ie1 = IonizationEnergyRepository::first_ionization_ev(10);
    REQUIRE(*li_ie1 < 5.5);
    REQUIRE(*ne_ie1 > 21.5);
    for (int z = 3; z < 10; ++z) {
        REQUIRE(*IonizationEnergyRepository::first_ionization_ev(z) < *ne_ie1);
    }

    // eV -> kJ/mol & SI Joules conversion for H (13.5984 eV ~ 1312.05 kJ/mol)
    const double h_kj = ev_to_kj_per_mol(13.5984);
    REQUIRE(std::abs(h_kj - 1312.05) < 0.1);
    REQUIRE(ev_to_joules(1.0) > 1.602e-19);
    REQUIRE(!IonizationEnergyRepository::to_json().empty());
}

TEST_CASE("SpectralLineCatalog: analytical Rydberg hydrogen Balmer series & NIST emission line queries") {
    // Analytical Rydberg H-alpha (3->2): 656.469 nm vac, 656.28 nm air
    auto h_alpha_vac = rydberg_wavelength_vacuum_nm(1, 3, 2);
    auto h_beta_vac  = rydberg_wavelength_vacuum_nm(1, 4, 2);
    auto h_gamma_vac = rydberg_wavelength_vacuum_nm(1, 5, 2);
    REQUIRE(h_alpha_vac.has_value());
    REQUIRE(h_beta_vac.has_value());
    REQUIRE(h_gamma_vac.has_value());
    REQUIRE(std::abs(*h_alpha_vac - 656.469) < 0.05);
    REQUIRE(std::abs(vacuum_to_air_nm(*h_alpha_vac) - 656.28) < 0.05);
    REQUIRE(std::abs(vacuum_to_air_nm(*h_beta_vac)  - 486.13) < 0.05);
    REQUIRE(std::abs(vacuum_to_air_nm(*h_gamma_vac) - 434.05) < 0.05);

    auto na_lines = SpectralLineCatalog::query_by_symbol("Na");
    REQUIRE(na_lines.size() >= 2);
    REQUIRE(std::abs(na_lines[0].wavelength_air_nm - 588.995) < 0.05);

    auto hg_lines = SpectralLineCatalog::query_by_element(80);
    REQUIRE(hg_lines.size() >= 6);

    auto vis_lines = SpectralLineCatalog::query_wavelength_range(400.0, 700.0);
    REQUIRE(!vis_lines.empty());
    REQUIRE(!SpectralLineCatalog::to_json().empty());
}
