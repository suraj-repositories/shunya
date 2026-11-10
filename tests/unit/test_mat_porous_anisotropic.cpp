#include "tests/test_framework.hpp"

#include "shunya/mat/anisotropic_tensor.hpp"
#include "shunya/mat/porous_material.hpp"

#include <cmath>

using namespace shunya::mat;

TEST_CASE("PorousMaterialModel: Gibson-Ashby scaling laws and compressive stress-strain regimes for Al & Ti foams") {
    PorousSolidInput al_foam;
    al_foam.solid_density_kg_m3 = 2700.0;
    al_foam.solid_youngs_gpa = 69.0;
    al_foam.solid_yield_mpa = 150.0;
    al_foam.solid_thermal_k_w_mk = 200.0;
    al_foam.relative_density = 0.20;
    al_foam.morphology = CellularMorphology::OpenCell;

    auto eff_res = PorousMaterialModel::compute(al_foam);
    REQUIRE(eff_res.is_ok());
    const auto& eff = eff_res.value();
    REQUIRE(std::abs(eff.effective_density_kg_m3 - 540.0) < 1e-6);
    REQUIRE(std::abs(eff.porosity - 0.80) < 1e-6);
    // Open cell: E/Es = (0.20)^2 = 0.04 -> E_eff = 2.76 GPa
    REQUIRE(std::abs(eff.effective_youngs_gpa - 2.76) < 1e-4);
    // Compressive curve: elastic -> plateau -> steep densification rise
    const double s_elastic = PorousMaterialModel::compressive_stress_mpa(al_foam, eff, 0.0005);
    const double s_plateau = PorousMaterialModel::compressive_stress_mpa(al_foam, eff, 0.25);
    const double s_dense = PorousMaterialModel::compressive_stress_mpa(al_foam, eff, eff.densification_strain + 0.10);
    REQUIRE(s_elastic > 0.0 && s_elastic < eff.plateau_yield_mpa);
    REQUIRE(s_plateau >= eff.plateau_yield_mpa);
    REQUIRE(s_dense > s_plateau * 2.0);

    // Closed-cell titanium foam is stiffer at same relative density than open-cell
    PorousSolidInput ti_open{4506.0, 114.0, 400.0, 21.9, 0.026, 0.25, CellularMorphology::OpenCell, {}};
    PorousSolidInput ti_closed = ti_open;
    ti_closed.morphology = CellularMorphology::ClosedCell;
    REQUIRE(PorousMaterialModel::compute(ti_closed).value().effective_youngs_gpa >
            PorousMaterialModel::compute(ti_open).value().effective_youngs_gpa);
}

TEST_CASE("AnisotropicElasticityTensor: CFRP positive definiteness, stability guard, and Bond 3D rotation invariance") {
    // Unidirectional AS4/3501-6 CFRP laminate
    auto cfrp_res = AnisotropicElasticityTensor::from_orthotropic({
        142.0, 10.3, 10.3, 0.27, 0.27, 0.42, 7.2, 7.2, 3.6
    });
    REQUIRE(cfrp_res.is_ok());
    const auto& cfrp = cfrp_res.value();
    const auto stab = cfrp.analyze_stability();
    REQUIRE(stab.is_symmetric);
    REQUIRE(stab.is_positive_definite);
    REQUIRE(stab.min_eigenvalue_gpa > 0.0);

    // 90-degree Z-rotation swaps C1111 and C2222 while preserving eigenvalues
    constexpr double kHalfPi = 1.5707963267948966;
    const auto rot90 = cfrp.rotated_about_z(kHalfPi);
    REQUIRE(std::abs(rot90.stiffness_gpa()[0][0] - cfrp.stiffness_gpa()[1][1]) < 1e-6);
    REQUIRE(std::abs(rot90.stiffness_gpa()[1][1] - cfrp.stiffness_gpa()[0][0]) < 1e-6);

    // Arbitrary 3D SO(3) quaternion rotation preserves all 6 orthogonal Kelvin/Mandel eigenvalues invariants
    const auto q = shunya::core::base::math::Quaternion<double>::from_axis_angle(
        shunya::core::base::math::Vector3<double>{1.0, 2.0, -0.5}.normalized(), 0.73);
    const auto rot3d = cfrp.rotated_by(q);
    const auto stab_rot = rot3d.analyze_stability();
    REQUIRE(stab_rot.is_symmetric);
    REQUIRE(stab_rot.is_positive_definite);
    const auto kev_orig = cfrp.kelvin_eigenvalues_gpa();
    const auto kev_rot = rot3d.kelvin_eigenvalues_gpa();
    for (size_t i = 0; i < 6; ++i) {
        REQUIRE(std::abs(kev_orig[i] - kev_rot[i]) < 1e-6);
    }

    // Invalid thermodynamic Poisson ratio (violating positive definiteness) is rejected
    REQUIRE(AnisotropicElasticityTensor::from_orthotropic({100.0, 10.0, 10.0, 0.9, 0.9, 0.9, 5.0, 5.0, 5.0}).is_err());

    // Negative eigenvalue matrix fails TensorValidator::validate_stiffness
    Matrix6x6 bad{};
    bad[0][0] = 100.0; bad[1][1] = -5.0; bad[2][2] = 50.0;
    bad[3][3] = 10.0;  bad[4][4] = 10.0; bad[5][5] = 10.0;
    REQUIRE(!TensorValidator::validate_stiffness(bad).is_valid());
}
