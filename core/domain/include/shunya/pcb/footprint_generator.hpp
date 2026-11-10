#pragma once

#include "shunya/core/base/math.hpp"
#include "shunya/core/base/result.hpp"
#include "shunya/pcb/ipc7351_tables.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace shunya::pcb {

struct PadDefinition {
    std::string number;
    core::base::math::Vector2<double> center_mm;
    double width_mm{0.60};
    double height_mm{1.55};
    double drill_mm{0.0}; // 0.0 = SMD pad; > 0.0 = THT plated drill
    double solder_mask_expansion_mm{0.05};
    bool is_circular{false};
};

struct FootprintDefinition {
    std::string name;
    double body_width_mm{3.9};
    double body_height_mm{4.9};
    double courtyard_width_mm{6.5};
    double courtyard_height_mm{5.4};
    std::vector<PadDefinition> pads;
};

struct GullWingPackageParams {
    std::string name{"SOIC-8"};
    int pin_count{8};
    double pitch_mm{1.27};
    double span_mm{6.00};       // L_nom tip-to-tip
    double lead_length_mm{0.835}; // T_nom foot contact length
    double lead_width_mm{0.42};   // W_nom
    double body_width_mm{3.90};
    double body_length_mm{4.90};
};

/**
 * @brief Generates IPC-7351B compliant SMD & THT PCB land patterns (SOIC, QFP, QFN, BGA, Chip passives).
 */
class FootprintGenerator {
public:
    [[nodiscard]] static core::base::Result<FootprintDefinition> generate_two_sided_gullwing(
        const GullWingPackageParams& pkg,
        DensityLevel density = DensityLevel::Nominal_B);

    [[nodiscard]] static core::base::Result<FootprintDefinition> generate_qfp(
        std::string name,
        int total_pins,
        double pitch_mm,
        double span_mm,
        double lead_length_mm,
        double lead_width_mm,
        double body_size_mm,
        DensityLevel density = DensityLevel::Nominal_B);

    [[nodiscard]] static core::base::Result<FootprintDefinition> generate_chip_passive(
        std::string_view code_imperial, // "0402", "0603", "0805", "1206"
        DensityLevel density = DensityLevel::Nominal_B);

    [[nodiscard]] static core::base::Result<FootprintDefinition> generate_bga(
        std::string name,
        int rows,
        int cols,
        double pitch_mm,
        double ball_diameter_mm);

    [[nodiscard]] static FootprintDefinition preset_soic_8();
    [[nodiscard]] static FootprintDefinition preset_lqfp_48();
};

} // namespace shunya::pcb
