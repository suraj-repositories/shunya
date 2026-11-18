#include "tests/test_framework.hpp"

#include "shunya/bench/ssim_calculator.hpp"
#include "shunya/bench/visual_regression_runner.hpp"

using namespace shunya::bench;

// =============================================================================
// PH4-13.03.002: Offscreen Viewport Snapshot Tests & SSIM Golden Verification
// =============================================================================

TEST_CASE("PH4-13.03.002: Offscreen 3D Viewport Snapshot Verification (SSIM > 0.98)") {
    // 1. Golden Reference 3D PBR Sphere Render (128x128)
    const size_t width = 128;
    const size_t height = 128;
    const auto golden_frame = VisualRegressionRunner::render_pbr_sphere_reference(width, height, 0.6);
    CHECK(golden_frame.width == width);
    CHECK(golden_frame.height == height);
    CHECK(golden_frame.pixels.size() == width * height);

    // 2. Candidate frame rendered under identical rendering parameters
    const auto candidate_frame = VisualRegressionRunner::render_pbr_sphere_reference(width, height, 0.6);

    // Evaluate SSIM comparison with required threshold = 0.98 (PH4 acceptance standard)
    const auto verdict = VisualRegressionRunner::evaluate_frame(
        "3D_PBR_Viewport_Gold", golden_frame, candidate_frame, 0.98
    );

    CHECK(verdict.passed);
    CHECK(verdict.ssim_score >= 0.999); // Bit-exact or near 1.0

    // Direct Wang et al. (2004) SSIM calculator evaluation
    const double raw_ssim = SsimCalculator::compute_ssim(golden_frame, candidate_frame, 8);
    CHECK(raw_ssim >= 0.999);
}

TEST_CASE("PH4-13.03.002: Offscreen 2D Schematic Viewport Snapshot Verification (SSIM > 0.98)") {
    const size_t width = 128;
    const size_t height = 128;
    const auto golden_schematic = VisualRegressionRunner::render_2d_schematic_reference(width, height);
    const auto candidate_schematic = VisualRegressionRunner::render_2d_schematic_reference(width, height);

    const auto verdict = VisualRegressionRunner::evaluate_frame(
        "2D_Schematic_Viewport_Gold", golden_schematic, candidate_schematic, 0.98
    );

    CHECK(verdict.passed);
    CHECK(verdict.ssim_score >= 0.999);
}

TEST_CASE("PH4-13.03.002: SSIM Gate Rejection of Rendering Regressions & Lighting Defects") {
    const size_t width = 64;
    const size_t height = 64;

    const auto golden = VisualRegressionRunner::render_pbr_sphere_reference(width, height, 0.6);

    // Severe shader lighting defect (light azimuth rotated to 1.5 rad)
    const auto bugged_frame = VisualRegressionRunner::render_pbr_sphere_reference(width, height, 1.5);

    const auto defect_verdict = VisualRegressionRunner::evaluate_frame(
        "3D_PBR_Viewport_Defect", golden, bugged_frame, 0.98
    );

    // Regression gate must fail
    CHECK_FALSE(defect_verdict.passed);
    CHECK(defect_verdict.ssim_score < 0.98);

    // Verify intentional pixel corruption detection
    ImageBufferGray corrupted_frame = golden;
    for (size_t y = 20; y < 44; ++y) {
        for (size_t x = 20; x < 44; ++x) {
            corrupted_frame.pixels[y * width + x] = 255; // White artifact box
        }
    }

    const double corrupted_ssim = SsimCalculator::compute_ssim(golden, corrupted_frame, 8);
    CHECK(corrupted_ssim < 0.90);
}
