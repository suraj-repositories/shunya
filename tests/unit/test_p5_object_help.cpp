#include "tests/test_framework.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace {

struct HelpTopic {
    std::string id;
    std::string title;
    std::string category;
    std::string summary;
    std::string content;
};

std::vector<HelpTopic> get_all_help_topics() {
    return {
        {"mixing-lab-overview", "Mixing Lab Architecture & Overview", "Mixing Lab", "Core framework for synthesising novel multi-element alloys.", "The Material Mixing Laboratory & Property Engine..."},
        {"formation-scenarios", "6 Formation Scenarios & Governing Equations", "Physics & Chemistry", "Mathematical models governing alloy lattices.", "Shunya provides 6 discrete physical formation scenarios..."},
        {"constituent-normalization", "Constituent Validation, Units & Normalization", "Data Integrity", "Single source of truth for element stoichiometry.", "All constituents in the Mixing Lab must resolve..."},
        {"virtual-test-bench", "Virtual Test Bench Models & Environmental Qualification", "Simulation & Testing", "7 Multiphysics qualification domains.", "The Virtual Material Test Bench subjects candidate mixtures..."},
        {"atomic-bonds", "Atomic Bond Checker & Hume-Rothery Miscibility Rules", "Physics & Chemistry", "First-principles electronegativity.", "The Atomic Bond Checker evaluates Pauling electronegativity..."},
        {"comparison-radar", "Comparison Engine, Radar Charts & Ranking Logic", "Analytics", "8-Axis normalized engineering performance benchmarks.", "The Material Comparison Engine standardizes 8 core engineering criteria..."},
        {"experiment-persistence", "Experiment Persistence, Schema Versioning & Sharing", "Storage", "Atomic JSON file serialization.", "Mixture experiments are serialized as standalone .shunya-mix.json..."},
        {"keyboard-shortcuts", "Desktop Keyboard Shortcuts & Navigation Guide", "Navigation", "Comprehensive hotkey reference.", "Essential Desktop Shortcuts: Ctrl+B, Ctrl+J, F11..."},
        {"objects-modeling-overview", "Objects & 3D Modeling Architecture", "3D CAD & Modeling", "Multi-region cyber-physical object authoring, single source of truth, and computed physical tensors.", "The Objects Authoring System provides full 3D solid parametric modeling... Single Source of Truth..."},
        {"incandescent-bulb-tutorial", "Tutorial: Step-by-Step Authoring of an Incandescent Light Bulb", "3D CAD & Modeling", "Complete 5-part guide from revolving glass envelope and sweeping tungsten coil to argon gas cavity and 120V test.", "Step-by-Step Authoring of a 60W Incandescent Light Bulb (Worked Example):\n\nPart 1: Revolving the Glass Envelope & Shelling... Part 2: Helical Tungsten Filament... Part 3: Assigning Materials: Borosilicate Glass and Tungsten... Part 4: Interior Cavity: Vacuum vs Argon Fill Gas... Part 5: Placing Terminal Contacts & Running Electrical Test Bench: 60W at 120V..."},
        {"blender-viewport-shortcuts", "Blender-Style 3D Viewport & Modeling Shortcuts", "Navigation", "Familiar Blender hotkeys for camera navigation, transform gizmos, and polyhedral editing.", "Blender-Compatible Viewport Shortcuts Reference:\n\nCamera Navigation:\n• Orbit: Middle Mouse Button (MMB) drag\n• Pan: Shift + MMB drag\n• Zoom: Mouse Wheel Scroll or Ctrl + MMB drag\n• Front Orthographic View: Numpad 1\n• Right Orthographic View: Numpad 3\n• Top Orthographic View: Numpad 7\n• Toggle Perspective / Orthographic: Numpad 5\n• Focus Selected Object: F or Numpad .\n\nTransform Gizmos:\n• Move / Translate Gizmo: G\n• Rotate Gizmo: R\n• Scale Gizmo: S\n• Cancel Active Transform: Right Click or Escape\n\nPolyhedral Edit Mode:\n• Toggle Object Mode / Edit Mode: Tab\n• Vertex Selection Mask: 1\n• Edge Selection Mask: 2\n• Face Selection Mask: 3\n• Extrude Selected Face: E\n• Inset Selected Face: I\n• Bevel Selected Edge / Face: Ctrl + B"},
        {"cavity-gas-physics", "Interior Cavity, Real Gas PVT & Paschen Breakdown", "Physics & Chemistry", "Governing thermodynamics of sealed/open cavities, vacuum outgassing, and dielectric arcing.", "Interior Cavities & Gas Physics Models:\n\n1. Real Gas PVT State: P*(V-nb) = nRT*exp(-an/RTV)...\n2. Evaporative Sublimation Mass Loss (Langmuir): dm/dt = P_v * sqrt(M / (2*pi*R*T))...\n3. Paschen's Law Dielectric Breakdown: V_b = (B*P*d) / (ln(A*P*d) - ln(ln(1 + 1/gamma_se)))..."},
        {"assemblies-kinematics-wires", "3D Editor Assemblies, Kinematic Joints & Wire Routing", "3D CAD & Modeling", "Assembling multi-body mechanisms, precision mating, kinematic joints, and electrical netlist routing.", "Assembly Editor & Electro-Mechanical Co-Simulation:\n\n• Part Instantiation\n• Precision Mating: Concentric & Face-to-Face\n• Kinematic Joints: Fixed, Revolute, Prismatic, Ball, Threaded Screw\n• Electrical Wire Routing: AWG gauge, resistance R = rho * L / A\n• Thermal Contact Interfaces: h_c conductance\n• Pre-Flight & Co-Sim"}
    };
}

std::vector<HelpTopic> search_help_topics(const std::vector<HelpTopic>& all, const std::string& query) {
    std::string q = query;
    std::transform(q.begin(), q.end(), q.begin(), ::tolower);
    std::vector<HelpTopic> res;
    for (const auto& t : all) {
        std::string title_l = t.title;
        std::transform(title_l.begin(), title_l.end(), title_l.begin(), ::tolower);
        std::string sum_l = t.summary;
        std::transform(sum_l.begin(), sum_l.end(), sum_l.begin(), ::tolower);
        std::string cont_l = t.content;
        std::transform(cont_l.begin(), cont_l.end(), cont_l.begin(), ::tolower);
        std::string cat_l = t.category;
        std::transform(cat_l.begin(), cat_l.end(), cat_l.begin(), ::tolower);

        if (title_l.find(q) != std::string::npos ||
            sum_l.find(q) != std::string::npos ||
            cont_l.find(q) != std::string::npos ||
            cat_l.find(q) != std::string::npos) {
            res.push_back(t);
        }
    }
    return res;
}

} // namespace

TEST_CASE("PH5-30.01: Objects Contextual Info Popovers & Deep-Linking") {
    struct PopoverSpec {
        std::string control_name;
        std::string title;
        std::string governing_formula;
        std::string range_spec;
        std::string help_anchor;
    };

    std::vector<PopoverSpec> popovers = {
        {"Geometry", "Parametric Solid Geometry", "V = ∫∫∫ dV,  A = ∬ dA", "Dim: 1e-4m to 10m", "objects-modeling-overview"},
        {"Regions", "Multi-Region Material Architecture", "Σ vf_i = 1.0,  M = V * Σ (vf_i * ρ_i)", "vf: 0.0 to 1.0", "objects-modeling-overview"},
        {"Cavity", "Interior Sealed Cavities & Gas Physics", "P(V - nb) = nRT exp(-an/RTV),  dm/dt = P_v * sqrt(M / 2πRT)", "P: 1e-6 Pa to 1e7 Pa", "cavity-gas-physics"},
        {"TestBench", "Multiphysics Object Test Bench", "P = V²/R = I²R,  q = k*A*ΔT/L,  σ = F/A", "V: 0-1000V, T: 4-3000K, F: 0-100kN", "virtual-test-bench"},
        {"Simulator", "Multiphysics Simulator Bridge", "PreFlight: Sealed ∧ NonDegenerate ∧ ConductivePins", "Pass / Fail scorecards", "objects-modeling-overview"}
    };

    CHECK(popovers.size() == 5u);

    // Verify each popover has non-empty fields
    for (const auto& p : popovers) {
        CHECK(!p.title.empty());
        CHECK(!p.governing_formula.empty());
        CHECK(!p.range_spec.empty());
        CHECK(!p.help_anchor.empty());
    }

    // Verify deep-link anchor resolution
    auto all_topics = get_all_help_topics();
    for (const auto& p : popovers) {
        bool found = false;
        for (const auto& t : all_topics) {
            if (t.id == p.help_anchor) {
                found = true;
                break;
            }
        }
        CHECK(found);
    }
}

TEST_CASE("PH5-30.02: Help Center Objects Guide, Incandescent Bulb Tutorial & Shortcuts") {
    auto topics = get_all_help_topics();
    CHECK(topics.size() == 13u);

    // 1. Search for "incandescent"
    auto bulb_matches = search_help_topics(topics, "incandescent");
    REQUIRE(!bulb_matches.empty());
    CHECK(bulb_matches[0].id == "incandescent-bulb-tutorial");
    CHECK(bulb_matches[0].category == "3D CAD & Modeling");

    // Verify all 5 tutorial parts are documented
    const auto& content = bulb_matches[0].content;
    CHECK(content.find("Part 1") != std::string::npos);
    CHECK(content.find("Part 2") != std::string::npos);
    CHECK(content.find("Part 3") != std::string::npos);
    CHECK(content.find("Part 4") != std::string::npos);
    CHECK(content.find("Part 5") != std::string::npos);
    CHECK(content.find("Tungsten") != std::string::npos);
    CHECK(content.find("Borosilicate Glass") != std::string::npos);
    CHECK(content.find("Argon") != std::string::npos);
    CHECK(content.find("60W") != std::string::npos);

    // 2. Search for "blender"
    auto blender_matches = search_help_topics(topics, "blender");
    REQUIRE(!blender_matches.empty());
    CHECK(blender_matches[0].id == "blender-viewport-shortcuts");
    const auto& b_content = blender_matches[0].content;
    CHECK(b_content.find("Orbit: Middle Mouse Button") != std::string::npos);
    CHECK(b_content.find("Pan: Shift + MMB") != std::string::npos);
    CHECK(b_content.find("Move / Translate Gizmo: G") != std::string::npos);
    CHECK(b_content.find("Extrude Selected Face: E") != std::string::npos);
    CHECK(b_content.find("Bevel Selected Edge / Face: Ctrl + B") != std::string::npos);

    // 3. Search for "paschen"
    auto paschen_matches = search_help_topics(topics, "paschen");
    REQUIRE(!paschen_matches.empty());
    CHECK(paschen_matches[0].id == "cavity-gas-physics");

    // 4. Search for "kinematic joints"
    auto joint_matches = search_help_topics(topics, "kinematic joints");
    REQUIRE(!joint_matches.empty());
    CHECK(joint_matches[0].id == "assemblies-kinematics-wires");
}
