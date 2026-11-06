#include "tests/test_framework.hpp"
#include "shunya/core/base/app_context.hpp"

using namespace shunya::core::base;

class MockPhysicsEngine {
public:
    std::string name() const { return "MockMNAEngine"; }
    int solver_count() const { return 1; }
};

class MockMaterialDatabase {
public:
    bool is_loaded() const { return true; }
};

TEST_CASE("FND-02: AppContext Lifecycle and Configuration") {
    auto& ctx = AppContext::instance();

    // Ensure stopped before test
    ctx.shutdown();
    CHECK(ctx.state() == AppLifecycleState::Stopped || ctx.state() == AppLifecycleState::Uninitialized);

    AppInitOptions opts;
    opts.app_name = "Shunya Unit Test Lab";
    opts.worker_threads = 2;
    opts.config_text = "sim.default_timestep = 1e-5\nphysics.substeps = 10\n";

    auto init_res = ctx.initialize(opts);
    REQUIRE(init_res.is_ok());
    CHECK(ctx.is_running());
    CHECK(ctx.state() == AppLifecycleState::Running);

    CHECK(ctx.config().get_string_or("app.name", "") == "Shunya Unit Test Lab");
    CHECK(ctx.config().get_double_or("sim.default_timestep", 0.0) == 1e-5);
    CHECK(ctx.config().get_int_or("physics.substeps", 0) == 10);

    // Double init guard
    auto second_init = ctx.initialize(opts);
    CHECK(!second_init.is_ok());
    CHECK(second_init.error().code == 1001);

    // Shutdown
    auto shut_res = ctx.shutdown();
    CHECK(shut_res.is_ok());
    CHECK(ctx.state() == AppLifecycleState::Stopped);
    CHECK(!ctx.is_running());
}

TEST_CASE("FND-02: Service Registry Registration and Query") {
    auto& ctx = AppContext::instance();
    AppInitOptions opts;
    opts.app_name = "ServiceRegistryTest";
    opts.worker_threads = 2;
    ctx.initialize(opts);

    auto& registry = ctx.services();
    CHECK(!registry.has_service<MockPhysicsEngine>());
    CHECK(registry.get_service<MockPhysicsEngine>() == nullptr);

    auto engine = std::make_shared<MockPhysicsEngine>();
    auto matdb = std::make_shared<MockMaterialDatabase>();

    registry.register_service<MockPhysicsEngine>(engine);
    registry.register_service<MockMaterialDatabase>(matdb);

    CHECK(registry.has_service<MockPhysicsEngine>());
    CHECK(registry.has_service<MockMaterialDatabase>());

    auto retrieved_engine = registry.get_service<MockPhysicsEngine>();
    REQUIRE(retrieved_engine != nullptr);
    CHECK(retrieved_engine->name() == "MockMNAEngine");
    CHECK(retrieved_engine->solver_count() == 1);

    auto retrieved_matdb = registry.get_service<MockMaterialDatabase>();
    REQUIRE(retrieved_matdb != nullptr);
    CHECK(retrieved_matdb->is_loaded());

    ctx.shutdown();
    CHECK(!registry.has_service<MockPhysicsEngine>());
}

TEST_CASE("FND-02: Thread Pool Coordination via AppContext") {
    auto& ctx = AppContext::instance();
    AppInitOptions opts;
    opts.app_name = "ThreadCoordTest";
    opts.worker_threads = 2;
    ctx.initialize(opts);

    auto fut = ctx.thread_pool().submit([]() {
        return 12345;
    });

    CHECK(fut.get() == 12345);
    ctx.shutdown();
}
