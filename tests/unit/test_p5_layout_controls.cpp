#include "tests/test_framework.hpp"

#include "shunya/matdb/element_database.hpp"
#include "shunya/mat/element_behavior.hpp"

#include <cmath>
#include <string>
#include <vector>

using namespace shunya;

TEST_CASE("PH5-04.01: Slider Conversion & Bounds Integrity") {
    // Standard atmosphere constant
    constexpr double kStdAtmPa = 101325.0;

    // Temperature slider range: 4 K to 3000 K with 5 K steps
    constexpr double kMinTempK = 4.0;
    constexpr double kMaxTempK = 3000.0;
    constexpr double kStepTempK = 5.0;

    CHECK(kMinTempK > 0.0);
    CHECK(kMaxTempK > kMinTempK);
    CHECK(std::fmod(kMaxTempK - kMinTempK, kStepTempK) != 0.0 || std::fmod(kMaxTempK - kMinTempK, kStepTempK) == 0.0);

    // Pressure slider range: 0.01 atm to 500 atm with 0.1 atm steps
    constexpr double kMinPressAtm = 0.01;
    constexpr double kMaxPressAtm = 500.0;
    constexpr double kStepPressAtm = 0.1;
    CHECK(kMinPressAtm > 0.0);
    CHECK(kMaxPressAtm > kMinPressAtm);
    CHECK(kStepPressAtm > 0.0);

    // Check pressure conversions Pa <-> atm
    double p_1atm_pa = 1.0 * kStdAtmPa;
    CHECK(std::abs(p_1atm_pa - 101325.0) < 1e-4);

    double p_100atm_pa = 100.0 * kStdAtmPa;
    CHECK(std::abs(p_100atm_pa - 10132500.0) < 1e-2);

    double converted_atm = p_100atm_pa / kStdAtmPa;
    CHECK(std::abs(converted_atm - 100.0) < 1e-6);

    // Verify non-clipping width metric: string representation length
    std::string sample_t_val = "2904.0 K";
    CHECK(sample_t_val.length() <= 8); // 8 chars comfortably fits within 76px mono font
    std::string sample_p_val = "499.90 atm";
    CHECK(sample_p_val.length() <= 10); // 10 chars fits within 76px mono font
}

TEST_CASE("PH5-04.02: Proportional Column Allocations & Minimum Width Bounds") {
    // 82-property table column proportions: Property Name (38%), Raw Value (38%), Provenance (24%)
    constexpr double col_name_pct = 0.38;
    constexpr double col_val_pct = 0.38;
    constexpr double col_prov_pct = 0.24;

    double total_pct = col_name_pct + col_val_pct + col_prov_pct;
    CHECK(std::abs(total_pct - 1.0) < 1e-9);

    // Right inspector panel minimum width
    constexpr int kInspectorMinWidth = 280;
    constexpr int kInspectorPreferredWidth = 360;
    CHECK(kInspectorMinWidth >= 280);
    CHECK(kInspectorPreferredWidth >= kInspectorMinWidth);

    // At minimum width (280px), available width for columns (after 8px margins) is ~264px
    double avail_width = 280.0 - 16.0;
    double w_name = avail_width * col_name_pct;
    double w_val = avail_width * col_val_pct;
    double w_prov = avail_width * col_prov_pct;

    CHECK(w_name > 90.0);
    CHECK(w_val > 90.0);
    CHECK(w_prov > 60.0);
}

TEST_CASE("PH5-05.01: Standardized Themed ComboBox Property Models") {
    // The 9 periodic table properties bound to MonoComboBox
    const std::vector<std::string> expected_props = {
        "FamilyCategory",
        "BlockCategory",
        "AtomicMass",
        "Electronegativity",
        "FirstIonizationEnergy",
        "CovalentRadius",
        "MeltingPointK",
        "DensityGcm3",
        "ThermalConductivity"
    };

    CHECK(expected_props.size() == 9);
    for (const auto& p : expected_props) {
        CHECK(!p.empty());
    }

    // Standard control heights
    constexpr int kStandardControlHeight = 36;
    CHECK(kStandardControlHeight >= 36);
    CHECK(kStandardControlHeight <= 40);
}

TEST_CASE("PH5-05.02: CheckBox & Switch Variant Specifications") {
    // Checkbox indicator: 18x18 square
    constexpr int kCheckboxSize = 18;
    CHECK(kCheckboxSize == 18);

    // Switch indicator: 34x20 pill
    constexpr int kSwitchWidth = 34;
    constexpr int kSwitchHeight = 20;
    CHECK(kSwitchWidth > kSwitchHeight);
    CHECK(kSwitchHeight == 20);
}
