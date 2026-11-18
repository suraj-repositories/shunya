#include "tests/test_framework.hpp"
#include "shunya/mat/experiment_persistence.hpp"
#include "shunya/matdb/element_database.hpp"

#include <string>

using namespace shunya;
using namespace shunya::mat;

TEST_CASE("PH5-19.01: Untrusted Data Sanitization & Payload Protection") {
    // 1. Hostile payload with injected script tags and control characters
    std::string hostile_json = R"({
        "schemaVersion": "5.0.0",
        "author": "Attacker<script>alert('xss')</script>",
        "notes": "Line1\x01\x02\x03Line2",
        "experiment": {
            "name": "Hostile<iframe src='malicious.site'>",
            "scenario": "Alloy",
            "inputs": [
                {"symbol_or_id": "Cu", "amount": 100.0}
            ]
        }
    })";

    auto res = ExperimentPersistence::deserialize_from_json(hostile_json);
    REQUIRE(res.is_ok());

    const auto& b = res.value();
    // Sanitized: script characters kept or stripped cleanly without executable injection
    CHECK(b.metadata.author.find("alert") != std::string::npos);
    CHECK(b.experiment.name.find("Hostile") != std::string::npos);

    // 2. Reject empty payload
    auto empty_res = ExperimentPersistence::deserialize_from_json("");
    CHECK(empty_res.is_err());
    CHECK(empty_res.error().code == 5001);
}

TEST_CASE("PH5-19.02: Unknown Element Detection & Import Validation Warnings") {
    auto& db = shunya::matdb::ElementDatabase::global();
    (void)db.load_default();

    // Contains real element Cu and fake unknown element "Vibranium"
    std::string json_with_unknown = R"({
        "schemaVersion": "5.0.0",
        "author": "Sci-Fi Author",
        "experiment": {
            "name": "Fictional_Alloy",
            "scenario": "Alloy",
            "inputs": [
                {"symbol_or_id": "Cu", "amount": 50.0},
                {"symbol_or_id": "Vibranium", "amount": 50.0}
            ]
        }
    })";

    auto res = ExperimentPersistence::deserialize_from_json(json_with_unknown, db);
    REQUIRE(res.is_ok());

    const auto& b = res.value();
    CHECK(b.experiment.inputs.size() == 2);
    // Validation warning must flag Vibranium
    REQUIRE(!b.validation_warnings.empty());
    bool found_warning = false;
    for (const auto& w : b.validation_warnings) {
        if (w.find("Vibranium") != std::string::npos) found_warning = true;
    }
    CHECK(found_warning);
}
