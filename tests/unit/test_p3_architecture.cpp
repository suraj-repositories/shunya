#include "tests/test_framework.hpp"
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "shunya/eng/engineering_pipeline.hpp"

using namespace shunya::eng;
using namespace shunya::core::base;
using namespace shunya::core::domain;

TEST_CASE("ARCH: EngineeringWorkspaceState reactive notification & material invalidation") {
    EngineeringWorkspaceState ws;
    REQUIRE(ws.project() != nullptr);
    CHECK(ws.validate_workspace().is_valid());

    int change_count = 0;
    DependencyChangeKind last_kind = DependencyChangeKind::ProjectReset;
    auto tok = ws.subscribe([&](const WorkspaceChangeEvent& ev) {
        ++change_count;
        last_kind = ev.kind;
    });

    Uuid invalidated_mat;
    ws.register_material_dependent_invalidator([&](const Uuid& mid) {
        invalidated_mat = mid;
    });

    Material mat(Uuid::generate_random(), "TestCopper", MaterialPhysicalState::Solid, 8920.0);
    const Uuid mat_id = mat.id();
    REQUIRE(ws.material_registry().register_material(mat).is_ok());

    ws.notify_material_modified(mat_id, "Updated Cu composition");
    CHECK(invalidated_mat == mat_id);
    CHECK(change_count == 1);
    CHECK(last_kind == DependencyChangeKind::MaterialUpdated);
    CHECK(ws.project()->is_dirty());

    AmbientEnvironmentConfig env;
    env.temperature_k = -10.0;
    ws.set_environment(env);
    CHECK_FALSE(ws.validate_workspace().is_valid());

    ws.unsubscribe(tok);
    ws.notify_change(DependencyChangeKind::CircuitUpdated);
    CHECK(change_count == 2); // set_environment fired 1, unsubscribed before CircuitUpdated
}

TEST_CASE("ARCH-PH4: Core header isolation asserts zero Qt/GUI dependencies in core domain") {
    namespace fs = std::filesystem;
    const fs::path root = fs::path(SHUNYA_SOURCE_DIR);
    std::vector<fs::path> search_dirs = {
        root / "core" / "base" / "include",
        root / "core" / "domain" / "include"
    };

    int scanned_files = 0;
    bool forbidden_found = false;
    for (const auto& dir : search_dirs) {
        if (!fs::exists(dir)) continue;
        for (const auto& entry : fs::recursive_directory_iterator(dir)) {
            if (entry.is_regular_file() && (entry.path().extension() == ".hpp" || entry.path().extension() == ".h")) {
                ++scanned_files;
                std::ifstream infile(entry.path());
                std::string line;
                while (std::getline(infile, line)) {
                    if (line.find("#include <Q") != std::string::npos && line.find("<queue>") == std::string::npos) {
                        forbidden_found = true;
                        std::cerr << "Forbidden Qt include found: " << entry.path().string() << ": " << line << "\n";
                    }
                    if (line.find("#include <Qt") != std::string::npos) {
                        forbidden_found = true;
                        std::cerr << "Forbidden Qt include found: " << entry.path().string() << ": " << line << "\n";
                    }
                }
            }
        }
    }
    CHECK_FALSE(forbidden_found);
    CHECK(scanned_files > 30);
}

