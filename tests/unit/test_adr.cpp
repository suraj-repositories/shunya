#include "tests/test_framework.hpp"
#include "shunya/core/domain/adr.hpp"

#include <string>
#include <vector>

using namespace shunya::core::domain::adr;

// =============================================================================
// ADR-001 through ADR-010: Architecture Decision Records Suite
// =============================================================================

TEST_CASE("ADR-001 through ADR-010: Standard Architecture Decision Records") {
    auto registry = ADRRegistry::create_standard_registry();
    CHECK(registry.all_adrs().size() == 10u);

    // Verify all 10 ADRs exist and are Accepted
    for (int i = 1; i <= 10; ++i) {
        std::string id = (i < 10) ? ("ADR-00" + std::to_string(i)) : ("ADR-0" + std::to_string(i));
        auto adr_opt = registry.find_adr(id);
        REQUIRE(adr_opt.has_value());
        CHECK(adr_opt->status == ADRStatus::Accepted);
        CHECK(!adr_opt->title.empty());
        CHECK(!adr_opt->decision.empty());
        CHECK(!adr_opt->compliance_rules.empty());
    }

    // Specific ADR Content Verification:
    // ADR-001: UI decoupling
    auto adr1 = registry.find_adr("ADR-001");
    REQUIRE(adr1.has_value());
    CHECK(adr1->title == "Desktop UI & Core Simulation Decoupling");

    // ADR-004: Lua scripting
    auto adr4 = registry.find_adr("ADR-004");
    REQUIRE(adr4.has_value());
    CHECK(adr4->title == "Sandboxed Lua 5.4 Scripting Engine");

    // ADR-007: Electronics compatibility
    auto adr7 = registry.find_adr("ADR-007");
    REQUIRE(adr7.has_value());
    CHECK(adr7->title == "Native MNA Baseline & External SPICE Engine Adapter");

    // ADR-010: Scientific governance L0-L4
    auto adr10 = registry.find_adr("ADR-010");
    REQUIRE(adr10.has_value());
    CHECK(adr10->title == "5-Tier Scientific Validation Governance (L0-L4)");
}

TEST_CASE("ADR System Architectural Compliance & Markdown Export") {
    auto registry = ADRRegistry::create_standard_registry();

    auto comp_res = registry.verify_system_compliance();
    CHECK(comp_res.is_ok());

    auto adr1 = registry.find_adr("ADR-001");
    REQUIRE(adr1.has_value());
    std::string md = ADRRegistry::export_markdown(*adr1);
    CHECK(md.find("# ADR-001") != std::string::npos);
    CHECK(md.find("## Context") != std::string::npos);
    CHECK(md.find("## Decision") != std::string::npos);
    CHECK(md.find("## Compliance Rules") != std::string::npos);
}
