#include "tests/test_framework.hpp"
#include "shunya/core/base/config.hpp"

using namespace shunya::core::base;

TEST_CASE("FND-09: In-Memory Configuration Access and Conversion") {
    Configuration cfg;
    cfg.set("sim.timestep", 1e-4);
    cfg.set("sim.max_steps", 5000);
    cfg.set("sim.adaptive", true);
    cfg.set("sim.solver_name", "ModifiedNodalAnalysis");

    CHECK(cfg.has("sim.timestep"));
    CHECK(!cfg.has("sim.nonexistent"));

    CHECK(cfg.get_double_or("sim.timestep", 0.0) == 1e-4);
    CHECK(cfg.get_int_or("sim.max_steps", 0) == 5000);
    CHECK(cfg.get_bool_or("sim.adaptive", false) == true);
    CHECK(cfg.get_string_or("sim.solver_name", "") == "ModifiedNodalAnalysis");

    // Fallbacks
    CHECK(cfg.get_double_or("sim.missing", 9.81) == 9.81);
    CHECK(cfg.get_string_or("sim.missing_str", "default_val") == "default_val");
}

TEST_CASE("FND-09: Text Configuration Parsing and Serialization") {
    Configuration cfg;
    std::string text = R"(
# Shunya test configuration
app.name = Shunya Virtual Lab
app.threads = 8
app.headless = false
physics.gravity = 9.80665

# Solver settings
solver.rel_tol = 1.0e-6
)";

    auto load_res = cfg.load_from_text(text);
    REQUIRE(load_res.is_ok());

    CHECK(cfg.get_string_or("app.name", "") == "Shunya Virtual Lab");
    CHECK(cfg.get_int_or("app.threads", 0) == 8);
    CHECK(cfg.get_bool_or("app.headless", true) == false);
    CHECK(cfg.get_double_or("physics.gravity", 0.0) == 9.80665);
    CHECK(cfg.get_double_or("solver.rel_tol", 0.0) == 1.0e-6);

    std::string serialized = cfg.save_to_text();
    CHECK(serialized.find("app.name = Shunya Virtual Lab") != std::string::npos);
    CHECK(serialized.find("physics.gravity = 9.806650") != std::string::npos ||
          serialized.find("physics.gravity = 9.80665") != std::string::npos);
}

TEST_CASE("FND-09: Configuration Parsing Error Diagnostics") {
    Configuration cfg;
    std::string bad_text = "invalid_line_without_equals_sign\n";
    auto err_res = cfg.load_from_text(bad_text);

    CHECK(!err_res.is_ok());
    CHECK(err_res.error().code == 400);
    CHECK(err_res.error().message.find("missing '='") != std::string::npos);
}
