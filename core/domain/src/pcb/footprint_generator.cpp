#include "shunya/pcb/footprint_generator.hpp"

#include <cmath>

namespace shunya::pcb {

namespace {

core::base::Error make_fp_gen_error(std::string msg) {
    return core::base::Error{
        .code = 5020,
        .domain = core::base::ErrorDomain::Validation,
        .severity = core::base::Severity::Error,
        .message = std::move(msg),
        .details = ""
    };
}

double round_to_001(double v) noexcept {
    return std::round(v * 100.0) * 0.01;
}

} // namespace

core::base::Result<FootprintDefinition> FootprintGenerator::generate_two_sided_gullwing(
    const GullWingPackageParams& pkg,
    DensityLevel density) {
    if (pkg.pin_count < 2 || (pkg.pin_count % 2) != 0) {
        return make_fp_gen_error("Two-sided gullwing pin count must be even and >= 2");
    }
    if (pkg.pitch_mm <= 0.0 || pkg.span_mm <= 0.0) {
        return make_fp_gen_error("Pitch and lead span must be positive");
    }

    const auto f = ipc7351_gullwing_fillets(density, pkg.pitch_mm);
    // IPC-7351B formulas:
    // Z_max = L + 2 * J_T, G_min = L - 2*T - 2*J_H
    // Pad Length X = (Z_max - G_min) / 2 = T + J_T + J_H
    // Pad Center C = (Z_max + G_min) / 2 = L - T + (J_T - J_H)
    const double z_max = pkg.span_mm + 2.0 * f.toe_mm;
    const double g_min = pkg.span_mm - 2.0 * pkg.lead_length_mm - 2.0 * f.heel_mm;
    const double pad_len = round_to_001((z_max - g_min) * 0.5);
    const double pad_wid = round_to_001(pkg.lead_width_mm + 2.0 * f.side_mm);
    const double center_span = round_to_001((z_max + g_min) * 0.5);

    FootprintDefinition fp;
    fp.name = pkg.name;
    fp.body_width_mm = pkg.body_width_mm;
    fp.body_height_mm = pkg.body_length_mm;
    fp.courtyard_width_mm = round_to_001(center_span + pad_len + 2.0 * f.courtyard_excess_mm);
    fp.courtyard_height_mm = round_to_001(pkg.body_length_mm + 2.0 * f.courtyard_excess_mm);

    const int side_pins = pkg.pin_count / 2;
    const double y_top = 0.5 * static_cast<double>(side_pins - 1) * pkg.pitch_mm;
    for (int i = 0; i < side_pins; ++i) {
        const double y = y_top - static_cast<double>(i) * pkg.pitch_mm;
        fp.pads.push_back({std::to_string(i + 1), {-0.5 * center_span, y}, pad_len, pad_wid, 0.0, 0.05, false});
    }
    for (int i = 0; i < side_pins; ++i) {
        const double y = -y_top + static_cast<double>(i) * pkg.pitch_mm;
        fp.pads.push_back({std::to_string(side_pins + i + 1), {0.5 * center_span, y}, pad_len, pad_wid, 0.0, 0.05, false});
    }
    return fp;
}

core::base::Result<FootprintDefinition> FootprintGenerator::generate_qfp(
    std::string name,
    int total_pins,
    double pitch_mm,
    double span_mm,
    double lead_length_mm,
    double lead_width_mm,
    double body_size_mm,
    DensityLevel density) {
    if (total_pins < 4 || (total_pins % 4) != 0) {
        return make_fp_gen_error("QFP pin count must be divisible by 4");
    }

    const auto f = ipc7351_gullwing_fillets(density, pitch_mm);
    const double z_max = span_mm + 2.0 * f.toe_mm;
    const double g_min = span_mm - 2.0 * lead_length_mm - 2.0 * f.heel_mm;
    const double pad_len = round_to_001((z_max - g_min) * 0.5);
    const double pad_wid = round_to_001(lead_width_mm + 2.0 * f.side_mm);
    const double center_dist = round_to_001(0.5 * (z_max + g_min) * 0.5);

    FootprintDefinition fp;
    fp.name = std::move(name);
    fp.body_width_mm = body_size_mm;
    fp.body_height_mm = body_size_mm;
    const double cy = round_to_001(2.0 * center_dist + pad_len + 2.0 * f.courtyard_excess_mm);
    fp.courtyard_width_mm = cy;
    fp.courtyard_height_mm = cy;

    const int n = total_pins / 4;
    const double half_row = 0.5 * static_cast<double>(n - 1) * pitch_mm;
    int pin = 1;
    // Left (top->bottom)
    for (int i = 0; i < n; ++i) {
        fp.pads.push_back({std::to_string(pin++), {-center_dist, half_row - i * pitch_mm}, pad_len, pad_wid, 0.0, 0.05, false});
    }
    // Bottom (left->right)
    for (int i = 0; i < n; ++i) {
        fp.pads.push_back({std::to_string(pin++), {-half_row + i * pitch_mm, -center_dist}, pad_wid, pad_len, 0.0, 0.05, false});
    }
    // Right (bottom->top)
    for (int i = 0; i < n; ++i) {
        fp.pads.push_back({std::to_string(pin++), {center_dist, -half_row + i * pitch_mm}, pad_len, pad_wid, 0.0, 0.05, false});
    }
    // Top (right->left)
    for (int i = 0; i < n; ++i) {
        fp.pads.push_back({std::to_string(pin++), {half_row - i * pitch_mm, center_dist}, pad_wid, pad_len, 0.0, 0.05, false});
    }
    return fp;
}

core::base::Result<FootprintDefinition> FootprintGenerator::generate_chip_passive(
    std::string_view code_imperial,
    DensityLevel density) {
    double L = 1.60, W = 0.80, T = 0.35; // 0603 default
    if (code_imperial == "0402")      { L = 1.00; W = 0.50; T = 0.25; }
    else if (code_imperial == "0603") { L = 1.60; W = 0.80; T = 0.35; }
    else if (code_imperial == "0805") { L = 2.00; W = 1.25; T = 0.45; }
    else if (code_imperial == "1206") { L = 3.20; W = 1.60; T = 0.50; }
    else {
        return make_fp_gen_error("Unsupported chip passive package code: " + std::string(code_imperial));
    }

    const auto f = ipc7351_chip_fillets(density);
    const double z_max = L + 2.0 * f.toe_mm;
    const double g_min = L - 2.0 * T - 2.0 * f.heel_mm;
    const double pad_len = round_to_001((z_max - g_min) * 0.5);
    const double pad_wid = round_to_001(W + 2.0 * f.side_mm);
    const double half_c = round_to_001(0.25 * (z_max + g_min));

    FootprintDefinition fp;
    fp.name = std::string(code_imperial);
    fp.body_width_mm = L;
    fp.body_height_mm = W;
    fp.courtyard_width_mm = round_to_001(z_max + 2.0 * f.courtyard_excess_mm);
    fp.courtyard_height_mm = round_to_001(pad_wid + 2.0 * f.courtyard_excess_mm);
    fp.pads = {
        {"1", {-half_c, 0.0}, pad_len, pad_wid, 0.0, 0.05, false},
        {"2", { half_c, 0.0}, pad_len, pad_wid, 0.0, 0.05, false}
    };
    return fp;
}

core::base::Result<FootprintDefinition> FootprintGenerator::generate_bga(
    std::string name,
    int rows,
    int cols,
    double pitch_mm,
    double ball_diameter_mm) {
    if (rows <= 0 || cols <= 0 || pitch_mm <= 0.0 || ball_diameter_mm <= 0.0) {
        return make_fp_gen_error("Invalid BGA dimensions");
    }
    // IPC-7351B BGA non-solder-mask-defined pad ~ 0.80 * ball diameter
    const double pad_d = round_to_001(ball_diameter_mm * 0.80);
    FootprintDefinition fp;
    fp.name = std::move(name);
    fp.body_width_mm = static_cast<double>(cols) * pitch_mm + 1.0;
    fp.body_height_mm = static_cast<double>(rows) * pitch_mm + 1.0;
    fp.courtyard_width_mm = fp.body_width_mm + 1.0;
    fp.courtyard_height_mm = fp.body_height_mm + 1.0;

    const double x0 = -0.5 * static_cast<double>(cols - 1) * pitch_mm;
    const double y0 = 0.5 * static_cast<double>(rows - 1) * pitch_mm;
    for (int r = 0; r < rows; ++r) {
        const std::string row_lbl(1, static_cast<char>('A' + (r % 26)));
        for (int c = 0; c < cols; ++c) {
            fp.pads.push_back({
                row_lbl + std::to_string(c + 1),
                {x0 + c * pitch_mm, y0 - r * pitch_mm},
                pad_d, pad_d, 0.0, 0.05, true
            });
        }
    }
    return fp;
}

FootprintDefinition FootprintGenerator::preset_soic_8() {
    return generate_two_sided_gullwing({"SOIC-8", 8, 1.27, 6.00, 0.85, 0.42, 3.90, 4.90}).value();
}

FootprintDefinition FootprintGenerator::preset_lqfp_48() {
    return generate_qfp("LQFP-48", 48, 0.50, 9.00, 0.60, 0.22, 7.00).value();
}

} // namespace shunya::pcb
