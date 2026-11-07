#include "shunya/core/domain/adr.hpp"

#include <sstream>

namespace shunya::core::domain::adr {

ADRRegistry::ADRRegistry() = default;

void ADRRegistry::register_adr(ArchitectureDecisionRecord record) {
    std::string id = record.id;
    if (id_map_.find(id) != id_map_.end()) {
        records_[id_map_[id]] = std::move(record);
    } else {
        id_map_[id] = records_.size();
        records_.push_back(std::move(record));
    }
}

std::optional<ArchitectureDecisionRecord> ADRRegistry::find_adr(const std::string& id) const {
    auto it = id_map_.find(id);
    if (it != id_map_.end() && it->second < records_.size()) {
        return records_[it->second];
    }
    return std::nullopt;
}

ADRRegistry ADRRegistry::create_standard_registry() {
    ADRRegistry reg;

    // ADR-001
    reg.register_adr(ArchitectureDecisionRecord{
        .id = "ADR-001",
        .title = "Desktop UI & Core Simulation Decoupling",
        .status = ADRStatus::Accepted,
        .context = "Desktop UI must remain responsive and decoupled from numerical solvers.",
        .decision = "Adopt Qt 6 (Qt Quick/Widgets) for UI while core/base and core/domain have zero Qt dependencies.",
        .positive_consequences = {"Core compiles headlessly", "Enables CLI and automated CI benchmarking"},
        .negative_consequences = {"Requires dedicated view-model adapter layer"},
        .compliance_rules = {"core/base and core/domain must never include Qt headers"}
    });

    // ADR-002
    reg.register_adr(ArchitectureDecisionRecord{
        .id = "ADR-002",
        .title = "Renderer Hardware Abstraction",
        .status = ADRStatus::Accepted,
        .context = "Viewport rendering requires multi-backend support (Vulkan/OpenGL).",
        .decision = "Abstract graphics commands behind an agnostic RenderDevice interface.",
        .positive_consequences = {"Cross-platform fallback", "Enables headless off-screen image rendering"},
        .negative_consequences = {"Higher abstraction overhead"},
        .compliance_rules = {"Renderer consumes read-only immutable simulation snapshots"}
    });

    // ADR-003
    reg.register_adr(ArchitectureDecisionRecord{
        .id = "ADR-003",
        .title = "Numerical Linear Algebra & Sparse Solver",
        .status = ADRStatus::Accepted,
        .context = "Multiphysics and circuits require high-performance sparse matrix factorizations.",
        .decision = "Standardize on Eigen 3.4+ and custom SparseMatrix CSR structures with MIT/MPL compliance.",
        .positive_consequences = {"Header-only", "Robust sparse LU and iterative solvers"},
        .negative_consequences = {"Requires careful template instantiation management"},
        .compliance_rules = {"Zero proprietary non-free linear algebra dependencies"}
    });

    // ADR-004
    reg.register_adr(ArchitectureDecisionRecord{
        .id = "ADR-004",
        .title = "Sandboxed Lua 5.4 Scripting Engine",
        .status = ADRStatus::Accepted,
        .context = "User programmability must be safe, embeddable and deterministic.",
        .decision = "Embed Lua 5.4 inside a capability-restricted sandbox without raw OS or memory pointers.",
        .positive_consequences = {"Guaranteed memory isolation", "Instruction count timeout quotas"},
        .negative_consequences = {"Slight overhead compared to native C++ extensions"},
        .compliance_rules = {"Scripts receive capability tokens, never raw C++ pointers"}
    });

    // ADR-005
    reg.register_adr(ArchitectureDecisionRecord{
        .id = "ADR-005",
        .title = "Atomic JSON Project Manifests & Binary Telemetry Payloads",
        .status = ADRStatus::Accepted,
        .context = "Project files must guarantee corruption resilience and atomic persistence.",
        .decision = "Use versioned JSON for project metadata and scene hierarchy, and SQLite/HDF5 for time-series.",
        .positive_consequences = {"Human-readable git diffs", "Crash-safe write-replace saves"},
        .negative_consequences = {"Two-part file format for large telemetry runs"},
        .compliance_rules = {"Atomic save with temporary files and schema version migration"}
    });

    // ADR-006
    reg.register_adr(ArchitectureDecisionRecord{
        .id = "ADR-006",
        .title = "Single-Writer Threading & Lock-Free Observability",
        .status = ADRStatus::Accepted,
        .context = "Simulation loop running at 100 kHz must not stutter due to 60 Hz UI rendering.",
        .decision = "Enforce single simulation writer thread publishing to lock-free ring buffers.",
        .positive_consequences = {"Zero mutex contention", "Deterministic reproducibility"},
        .negative_consequences = {"Requires ring buffer capacity management"},
        .compliance_rules = {"Simulation clock advances deterministically without mutex locks in solver loop"}
    });

    // ADR-007
    reg.register_adr(ArchitectureDecisionRecord{
        .id = "ADR-007",
        .title = "Native MNA Baseline & External SPICE Engine Adapter",
        .status = ADRStatus::Accepted,
        .context = "Electronic circuit simulation requires both fast built-in prototyping and industrial SPICE accuracy.",
        .decision = "Implement native Modified Nodal Analysis (MNA) solver alongside ngspice external engine bridge.",
        .positive_consequences = {"Zero external dependency for baseline circuits", "Industry standard validation via ngspice"},
        .negative_consequences = {"Dual circuit model maintenance"},
        .compliance_rules = {"Third-party engine types must never leak across public platform APIs"}
    });

    // ADR-008
    reg.register_adr(ArchitectureDecisionRecord{
        .id = "ADR-008",
        .title = "CAD Interoperability Pipeline",
        .status = ADRStatus::Accepted,
        .context = "Physical device construction requires 3D geometric import without monolithic CAD lock-in.",
        .decision = "Support STEP, OBJ, and STL mesh parsing with tetrahedral mesh generation.",
        .positive_consequences = {"Broad CAD interoperability", "Clean mesh boundary tagging"},
        .negative_consequences = {"Faceted approximation of complex curved NURBS surfaces"},
        .compliance_rules = {"Mesh geometry must assign physical materials to all structural regions"}
    });

    // ADR-009
    reg.register_adr(ArchitectureDecisionRecord{
        .id = "ADR-009",
        .title = "Cryptographic Trust & Supply Chain Security",
        .status = ADRStatus::Accepted,
        .context = "Sharing component libraries and third-party extensions poses security threats.",
        .decision = "Require Ed25519 cryptographic signatures, SBOM audits, and sandboxed package installs.",
        .positive_consequences = {"Complete defense against malicious packages", "Instant rollback capabilities"},
        .negative_consequences = {"Authors must sign packages before publication to public catalog"},
        .compliance_rules = {"Never execute untrusted native code or scripts automatically on install"}
    });

    // ADR-010
    reg.register_adr(ArchitectureDecisionRecord{
        .id = "ADR-010",
        .title = "5-Tier Scientific Validation Governance (L0-L4)",
        .status = ADRStatus::Accepted,
        .context = "Preventing pseudoscientific animation artifacts and ensuring scientific honesty.",
        .decision = "Mandate explicit fidelity tier declarations (L0-Visual to L4-Certified) with benchmark evidence.",
        .positive_consequences = {"Uncompromising scientific honesty", "Traceable benchmark validation"},
        .negative_consequences = {"Requires rigorous benchmark datasets for higher tier models"},
        .compliance_rules = {"Every physical model must declare assumptions, units, and validation benchmarks"}
    });

    return reg;
}

base::Result<void> ADRRegistry::verify_system_compliance() const {
    if (records_.size() < 10) {
        return base::Result<void>::err(base::Error{
            .code = 1,
            .domain = base::ErrorDomain::System,
            .message = "Missing required architectural decision records (expected 10)",
            .details = {}
        });
    }

    for (const auto& rec : records_) {
        if (rec.status != ADRStatus::Accepted) {
            return base::Result<void>::err(base::Error{
                .code = 2,
                .domain = base::ErrorDomain::System,
                .message = "ADR " + rec.id + " is not accepted",
                .details = {}
            });
        }
        if (rec.compliance_rules.empty()) {
            return base::Result<void>::err(base::Error{
                .code = 3,
                .domain = base::ErrorDomain::System,
                .message = "ADR " + rec.id + " lacks enforceable compliance rules",
                .details = {}
            });
        }
    }

    return base::Result<void>::ok();
}

std::string ADRRegistry::export_markdown(const ArchitectureDecisionRecord& adr) {
    std::ostringstream oss;
    oss << "# " << adr.id << ": " << adr.title << "\n\n";
    oss << "**Status:** " << adr_status_to_string(adr.status) << "\n\n";
    oss << "## Context\n" << adr.context << "\n\n";
    oss << "## Decision\n" << adr.decision << "\n\n";
    oss << "## Consequences\n\n### Positive\n";
    for (const auto& p : adr.positive_consequences) {
        oss << "- " << p << "\n";
    }
    oss << "\n### Negative\n";
    for (const auto& n : adr.negative_consequences) {
        oss << "- " << n << "\n";
    }
    oss << "\n## Compliance Rules\n";
    for (const auto& r : adr.compliance_rules) {
        oss << "- " << r << "\n";
    }
    return oss.str();
}

} // namespace shunya::core::domain::adr
