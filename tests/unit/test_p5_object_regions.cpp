#include "tests/test_framework.hpp"
#include "shunya/core/base/uuid.hpp"
#include "shunya/core/domain/material.hpp"
#include "shunya/core/domain/material_assignment.hpp"
#include "shunya/eng/physical_object.hpp"
#include "shunya/matdb/element_database.hpp"
#include "shunya/mat/material_mixing_lab.hpp"
#include <cmath>

using namespace shunya;

TEST_CASE("PH5-23.01: Multi-Region Functional Roles & Volume Normalization") {
    eng::PhysicalObjectManager mgr;
    core::domain::MaterialRegistry reg;

    core::domain::Material cu(core::base::Uuid::generate_random(), "Copper Core",
                              core::domain::MaterialPhysicalState::Solid, 8960.0);
    core::domain::ElectricalProperties ep_cu;
    ep_cu.electrical_conductivity = 5.96e7;
    cu.set_electrical(ep_cu);

    core::domain::Material ptfe(core::base::Uuid::generate_random(), "PTFE Sheath",
                               core::domain::MaterialPhysicalState::Solid, 2200.0);
    core::domain::ElectricalProperties ep_ptfe;
    ep_ptfe.electrical_conductivity = 1e-16;
    ptfe.set_electrical(ep_ptfe);

    const auto cu_id = cu.id();
    const auto ptfe_id = ptfe.id();
    REQUIRE(reg.register_material(std::move(cu)).is_ok());
    REQUIRE(reg.register_material(std::move(ptfe)).is_ok());

    eng::PhysicalObject cable;
    cable.name = "Coaxial_Cable";
    cable.geometry = eng::ObjectGeometrySpec::make_cylinder(0.010, 1.0); // 10mm radius, 1m length
    cable.operating_temp_k = 293.15;

    // Region 1: Conductive core (40% vol)
    eng::ObjectRegion r1("core", "Copper Core", cu_id, 0.40, 0.005, true, "",
                         eng::RegionFunctionalRole::ConductivePath);
    // Region 2: Dielectric sheath (60% vol)
    eng::ObjectRegion r2("sheath", "PTFE Sheath", ptfe_id, 0.60, 0.005, false, "",
                         eng::RegionFunctionalRole::DielectricSpacer);

    cable.regions = {r1, r2};

    auto add_res = mgr.add_object(cable, reg);
    CHECK(add_res.is_ok());

    const auto* stored = mgr.get_object(add_res.value());
    CHECK(stored != nullptr);
    CHECK(stored->regions.size() == 2);
    CHECK(stored->computed.valid == true);
    CHECK(stored->computed.has_conductive_path == true);

    // Verify composite density: 0.40 * 8960 + 0.60 * 2200 = 3584 + 1320 = 4904 kg/m³
    const double expected_rho = 0.40 * 8960.0 + 0.60 * 2200.0;
    CHECK(std::abs(stored->computed.effective_density_kg_m3 - expected_rho) < 1.0);

    // Verify total mass: V * rho_eff
    const double expected_mass = stored->computed.volume_m3 * expected_rho;
    CHECK(std::abs(stored->computed.mass_kg - expected_mass) < 1e-6);

    // Verify functional role strings
    CHECK(eng::region_functional_role_to_string(eng::RegionFunctionalRole::ConductivePath) == "ConductivePath");
    CHECK(eng::region_functional_role_to_string(eng::RegionFunctionalRole::DielectricSpacer) == "DielectricSpacer");
    CHECK(eng::region_functional_role_to_string(eng::RegionFunctionalRole::ThermalConductor) == "ThermalConductor");
    CHECK(eng::region_functional_role_to_string(eng::RegionFunctionalRole::ActiveEmitter) == "ActiveEmitter");
}

TEST_CASE("PH5-23.02: Multi-Region Physical Tensor Derivation (Resistance, Thermal Mass, Inertia)") {
    eng::PhysicalObjectManager mgr;
    core::domain::MaterialRegistry reg;

    core::domain::Material al(core::base::Uuid::generate_random(), "Aluminium Conductor",
                              core::domain::MaterialPhysicalState::Solid, 2700.0);
    core::domain::ElectricalProperties ep;
    ep.electrical_conductivity = 3.77e7; // S/m
    al.set_electrical(ep);
    core::domain::ThermalProperties tp;
    tp.thermal_conductivity = 237.0; // W/mK
    tp.specific_heat_capacity = 900.0; // J/kgK
    al.set_thermal(tp);

    core::domain::Material ceramic(core::base::Uuid::generate_random(), "Alumina Insulator",
                                  core::domain::MaterialPhysicalState::Solid, 3950.0);
    core::domain::ElectricalProperties ep_c;
    ep_c.electrical_conductivity = 1e-12; // insulator
    ceramic.set_electrical(ep_c);
    core::domain::ThermalProperties tp_c;
    tp_c.thermal_conductivity = 30.0;
    tp_c.specific_heat_capacity = 880.0;
    ceramic.set_thermal(tp_c);

    const auto al_id = al.id();
    const auto cer_id = ceramic.id();
    REQUIRE(reg.register_material(std::move(al)).is_ok());
    REQUIRE(reg.register_material(std::move(ceramic)).is_ok());

    eng::PhysicalObject rod;
    rod.name = "Bimetallic_Power_Rod";
    rod.geometry = eng::ObjectGeometrySpec::make_box(0.05, 0.20, 0.05); // 50x200x50 mm
    rod.operating_temp_k = 293.15;

    // 50% Aluminium conductor, 50% Ceramic
    eng::ObjectRegion r1("al_core", "Al Core", al_id, 0.50, 0.0, true, "",
                         eng::RegionFunctionalRole::ConductivePath);
    eng::ObjectRegion r2("cer_jacket", "Ceramic Jacket", cer_id, 0.50, 0.0, false, "",
                         eng::RegionFunctionalRole::Structural);
    rod.regions = {r1, r2};

    auto add_res = mgr.add_object(rod, reg);
    CHECK(add_res.is_ok());

    const auto* stored = mgr.get_object(add_res.value());
    CHECK(stored != nullptr);
    CHECK(stored->computed.valid == true);

    // Thermal mass: m_al * cp_al + m_cer * cp_cer
    const double v_tot = stored->computed.volume_m3;
    const double m_al = 0.50 * v_tot * 2700.0;
    const double m_cer = 0.50 * v_tot * 3950.0;
    const double expected_thermal_mass = m_al * 900.0 + m_cer * 880.0;
    CHECK(std::abs(stored->computed.thermal_mass_j_k - expected_thermal_mass) < 1e-3);

    // Axial resistance finite and reasonable (< 1.0 Ohm)
    CHECK(stored->computed.axial_electrical_resistance_ohm > 0.0);
    CHECK(stored->computed.axial_electrical_resistance_ohm < 1.0);

    // Inertia tensors are positive definite along principal axes
    CHECK(stored->computed.inertia_ixx > 0.0);
    CHECK(stored->computed.inertia_iyy > 0.0);
    CHECK(stored->computed.inertia_izz > 0.0);
}

TEST_CASE("PH5-23.03: Pure Insulator Object Electrical Resistance") {
    eng::PhysicalObjectManager mgr;
    core::domain::MaterialRegistry reg;

    core::domain::Material glass(core::base::Uuid::generate_random(), "Dielectric Glass",
                                 core::domain::MaterialPhysicalState::Solid, 2500.0);
    core::domain::ElectricalProperties ep;
    ep.electrical_conductivity = 1e-14;
    glass.set_electrical(ep);
    const auto g_id = glass.id();
    REQUIRE(reg.register_material(std::move(glass)).is_ok());

    eng::PhysicalObject spacer;
    spacer.name = "Insulating_Spacer";
    spacer.geometry = eng::ObjectGeometrySpec::make_box(0.02, 0.02, 0.02);
    spacer.operating_temp_k = 293.15;
    spacer.regions = {
        {"body", "Spacer Body", g_id, 1.0, 0.0, false, "", eng::RegionFunctionalRole::DielectricSpacer}
    };

    auto add_res = mgr.add_object(spacer, reg);
    CHECK(add_res.is_ok());

    const auto* stored = mgr.get_object(add_res.value());
    CHECK(stored != nullptr);
    CHECK(stored->computed.has_conductive_path == false);
    // Resistance should be at insulator floor (~1e12 Ohm)
    CHECK(stored->computed.axial_electrical_resistance_ohm >= 1e11);
}
