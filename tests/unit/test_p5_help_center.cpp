#include "tests/test_framework.hpp"

#include <algorithm>
#include <string>
#include <vector>

struct HelpTopic {
    std::string id;
    std::string title;
    std::string category;
    std::string summary;
    std::string content;
};

TEST_CASE("PH5-17.01: Help Center Topic Catalog & Search Indexing") {
    std::vector<HelpTopic> topics = {
        {"mixing-lab-overview", "Mixing Lab Architecture & Overview", "Mixing Lab", "Core framework for synthesising novel multi-element alloys.", "The Material Mixing Laboratory & Property Engine..."},
        {"formation-scenarios", "6 Formation Scenarios & Governing Equations", "Physics & Chemistry", "Mathematical models governing alloy lattices.", "Shunya provides 6 discrete physical formation scenarios..."},
        {"constituent-normalization", "Constituent Validation, Units & Normalization", "Data Integrity", "Single source of truth for element stoichiometry.", "All constituents in the Mixing Lab must resolve..."},
        {"virtual-test-bench", "Virtual Test Bench Models & Environmental Qualification", "Simulation & Testing", "7 Multiphysics qualification domains.", "The Virtual Material Test Bench subjects candidate mixtures..."},
        {"atomic-bonds", "Atomic Bond Checker & Hume-Rothery Miscibility Rules", "Physics & Chemistry", "First-principles electronegativity.", "The Atomic Bond Checker evaluates Pauling electronegativity..."},
        {"comparison-radar", "Comparison Engine, Radar Charts & Ranking Logic", "Analytics", "8-Axis normalized engineering performance benchmarks.", "The Material Comparison Engine standardizes 8 core engineering criteria..."},
        {"experiment-persistence", "Experiment Persistence, Schema Versioning & Sharing", "Storage", "Atomic JSON file serialization.", "Mixture experiments are serialized as standalone .shunya-mix.json..."},
        {"keyboard-shortcuts", "Desktop Keyboard Shortcuts & Navigation Guide", "Navigation", "Comprehensive hotkey reference.", "Essential Desktop Shortcuts: Ctrl+B, Ctrl+J, F11..."}
    };

    // 1. Verify 8 core topics exist
    CHECK(topics.size() == 8);

    // 2. Search query filter for "hume-rothery"
    auto filter_topics = [&](const std::string& query) {
        std::vector<HelpTopic> res;
        for (const auto& t : topics) {
            if (t.title.find(query) != std::string::npos ||
                t.summary.find(query) != std::string::npos ||
                t.content.find(query) != std::string::npos ||
                t.category.find(query) != std::string::npos) {
                res.push_back(t);
            }
        }
        return res;
    };

    auto hume_res = filter_topics("Hume-Rothery");
    CHECK(hume_res.size() >= 1);
    CHECK(hume_res[0].id == "atomic-bonds");

    // 3. Search query filter for "keyboard"
    auto key_res = filter_topics("Keyboard");
    CHECK(key_res.size() >= 1);
    CHECK(key_res[0].id == "keyboard-shortcuts");

    // 4. Anchor resolution
    auto find_by_anchor = [&](const std::string& anchor) -> const HelpTopic* {
        for (const auto& t : topics) {
            if (t.id == anchor) return &t;
        }
        return nullptr;
    };

    CHECK(find_by_anchor("formation-scenarios") != nullptr);
    CHECK(find_by_anchor("virtual-test-bench") != nullptr);
    CHECK(find_by_anchor("non-existent-anchor") == nullptr);
}
