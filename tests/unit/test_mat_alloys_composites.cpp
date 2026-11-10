#include "tests/test_framework.hpp"

#include "shunya/mat/alloy_builder.hpp"
#include "shunya/mat/composite_homogenizer.hpp"

#include <cmath>

using namespace shunya::mat;

TEST_CASE("CompositeHomogenizer: Voigt, Reuss, and Hashin-Shtrikman bounds on W-Cu composite (0%..100%)") {
    // Tungsten: K = 310 GPa, G = 161 GPa; Copper: K = 140 GPa, G = 48 GPa
    for (int pct = 0; pct <= 100; pct += 10) {
        const double f_cu = static_cast<double>(pct) * 0.01;
        const double f_w = 1.0 - f_cu;
        auto res = CompositeHomogenizer::evaluate({
            {"W",  f_w,  310.0, 161.0},
            {"Cu", f_cu, 140.0,  48.0}
        });
        REQUIRE(res.is_ok());
        const auto& b = res.value();
        REQUIRE(b.bulk_reuss_gpa <= b.bulk_hs_lower_gpa + 1e-9);
        REQUIRE(b.bulk_hs_lower_gpa <= b.bulk_hs_upper_gpa + 1e-9);
        REQUIRE(b.bulk_hs_upper_gpa <= b.bulk_voigt_gpa + 1e-9);

        REQUIRE(b.shear_reuss_gpa <= b.shear_hs_lower_gpa + 1e-9);
        REQUIRE(b.shear_hs_lower_gpa <= b.shear_hs_upper_gpa + 1e-9);
        REQUIRE(b.shear_hs_upper_gpa <= b.shear_voigt_gpa + 1e-9);

        REQUIRE(b.effective_youngs_gpa > 0.0);
        REQUIRE(b.effective_poisson > 0.0 && b.effective_poisson < 0.5);
    }

    // Invalid inputs rejected
    REQUIRE(CompositeHomogenizer::evaluate({}).is_err());
    REQUIRE(CompositeHomogenizer::evaluate({{"Cu", 0.6, 140.0, 48.0}}).is_err());
}

TEST_CASE("AlloyBuilder: Ti-6Al-4V density (4430 kg/m3 +- 0.5%), auto-balancing, SS 316L, and molar balance") {
    auto ti64 = AlloyBuilder::preset_ti_6al_4v();
    REQUIRE(ti64.elements.size() == 3);
    const double rel_err_pct = 100.0 * std::abs(ti64.theoretical_density_kg_m3 - 4430.0) / 4430.0;
    REQUIRE(rel_err_pct <= 0.5);

    auto ss316 = AlloyBuilder::preset_ss_316l();
    REQUIRE(ss316.theoretical_density_kg_m3 > 7600.0 && ss316.theoretical_density_kg_m3 < 8100.0);

    auto brass = AlloyBuilder::preset_brass_c260();
    REQUIRE(brass.theoretical_density_kg_m3 > 8200.0 && brass.theoretical_density_kg_m3 < 8600.0);

    // Auto-balance remainder onto base element (Cu base + 30% Zn -> 70% Cu + 30% Zn)
    auto auto_brass = AlloyBuilder::build_from_weight_pct("AutoBrass", {{29, 0.0}, {30, 30.0}}, true);
    REQUIRE(auto_brass.is_ok());
    REQUIRE(std::abs(auto_brass.value().elements[0].weight_pct - 70.0) < 1e-9);

    // 50-50 molar CuNi alloy -> ~52 wt% Cu, ~48 wt% Ni
    auto cu_ni = AlloyBuilder::build_from_molar_pct("CuNi50", {{29, 50.0}, {28, 50.0}});
    REQUIRE(cu_ni.is_ok());
    REQUIRE(std::abs(cu_ni.value().elements[0].molar_pct - 50.0) < 1e-6);
    REQUIRE(cu_ni.value().elements[0].weight_pct > 51.0 && cu_ni.value().elements[0].weight_pct < 53.0);

    REQUIRE(!AlloyBuilder::standard_alloys_json().empty());
}
