#include "tests/test_framework.hpp"

#include "shunya/bench/perf_regression_gate.hpp"
#include "shunya/bench/scientific_ci_runner.hpp"
#include "shunya/bench/visual_regression_runner.hpp"

using namespace shunya::bench;

TEST_CASE("TEST-03-001 ScientificCiRunner executes 500+ numerical checks and detects 1e-4 perturbation") {
    ScientificCiRunner runner;
    constexpr double kGravityExpected = 9.80665;
    for (int i = 0; i < 520; ++i) {
        runner.add_check("gravity_check_" + std::to_string(i), kGravityExpected, [&]() {
            return kGravityExpected;
        }, 1e-5);
    }

    const auto clean_rep = runner.execute_all();
    CHECK(clean_rep.total_checks == 520);
    CHECK(clean_rep.passed_checks == 520);
    CHECK(clean_rep.exit_code == 0);
    CHECK(clean_rep.elapsed_ms < 500.0); // Well under 15 s CI budget

    // Intentionally inject 1e-4 relative perturbation in gravity constant -> gate must fail with diagnostic
    runner.add_check("gravity_perturbed", kGravityExpected, [&]() {
        return kGravityExpected * (1.0 + 1.0e-4);
    }, 1e-5);

    const auto fail_rep = runner.execute_all();
    CHECK(fail_rep.exit_code == 1);
    CHECK(fail_rep.failed_checks == 1);
    CHECK(fail_rep.json_report.find("gravity_perturbed") != std::string::npos);
}

TEST_CASE("TEST-03-001 VisualRegressionRunner SSIM golden PBR sphere & 2D schematic checks (>0.995)") {
    const auto sphere_gold = VisualRegressionRunner::render_pbr_sphere_reference(64, 64, 0.6);
    const auto sphere_same = VisualRegressionRunner::render_pbr_sphere_reference(64, 64, 0.6);
    const auto v_sphere = VisualRegressionRunner::evaluate_frame("PBR_Sphere", sphere_gold, sphere_same, 0.995);
    CHECK(v_sphere.passed);
    CHECK(v_sphere.ssim_score >= 0.9999);

    const auto schem_gold = VisualRegressionRunner::render_2d_schematic_reference(64, 64);
    const auto v_schem = VisualRegressionRunner::evaluate_frame("Schematic_2D", schem_gold, schem_gold, 0.995);
    CHECK(v_schem.passed);

    // Shader light regression alters highlights -> SSIM drops cleanly below 0.995
    const auto sphere_bugged = VisualRegressionRunner::render_pbr_sphere_reference(64, 64, 1.3);
    const auto v_reg = VisualRegressionRunner::evaluate_frame("PBR_Sphere_Bug", sphere_gold, sphere_bugged, 0.995);
    CHECK_FALSE(v_reg.passed);
    CHECK(v_reg.ssim_score < 0.995);
}

TEST_CASE("TEST-03-001 PerfRegressionGate Welch t-test detects 10% slowdown & passes nominal variance") {
    PerfRegressionGate gate;
    gate.record_baseline("sph_step", {10.0, 10.1, 9.9, 10.05, 9.95, 10.02, 9.98, 10.01});

    // Nominal run (+0.8% noise) -> must pass
    const auto ok_eval = gate.evaluate_candidate("sph_step", {10.05, 10.12, 10.08, 10.02, 10.11, 10.09});
    CHECK_FALSE(ok_eval.regression_detected);

    // 10% regression (~11.0 ms) -> Welch t-test flags slowdown > 5%
    const auto slow_eval = gate.evaluate_candidate("sph_step", {11.0, 11.1, 10.95, 11.05, 11.02, 11.08});
    CHECK(slow_eval.regression_detected);
    CHECK(slow_eval.slowdown_fraction > 0.09);
    CHECK(slow_eval.welch_t_stat > 5.0);
}
