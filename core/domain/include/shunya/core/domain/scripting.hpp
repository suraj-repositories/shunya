#pragma once

#include "shunya/core/base/math.hpp"
#include "shunya/core/base/uuid.hpp"
#include "shunya/core/base/error.hpp"
#include "shunya/core/base/result.hpp"
#include "shunya/core/base/units.hpp"

#include <vector>
#include <string>
#include <memory>
#include <optional>
#include <variant>
#include <functional>
#include <unordered_map>
#include <cstdint>
#include <stack>

namespace shunya::core::domain::scripting {

using Vector3d = base::math::Vector3<double>;

// =============================================================================
// CODE-01: Rule Types & Controller Models
// =============================================================================

enum class RuleType {
    EventDriven,
    TimeDriven,
    ConditionAction,
    Equation,
    PIDController,
    CustomBehavior
};

struct PIDConfig {
    double kp = 1.0;
    double ki = 0.0;
    double kd = 0.0;
    double setpoint = 0.0;
    double output_min = -100.0;
    double output_max = 100.0;
    double integral_min = -50.0;
    double integral_max = 50.0;
};

class PIDController {
public:
    explicit PIDController(PIDConfig config = {}) : cfg_(config) {}

    void set_setpoint(double sp) noexcept { cfg_.setpoint = sp; }
    [[nodiscard]] double setpoint() const noexcept { return cfg_.setpoint; }

    [[nodiscard]] double update(double measured, double dt) noexcept;
    void reset() noexcept;

    [[nodiscard]] double integral_state() const noexcept { return integral_; }
    [[nodiscard]] double last_error() const noexcept { return last_error_; }

private:
    PIDConfig cfg_;
    double integral_ = 0.0;
    double last_error_ = 0.0;
    bool has_prev_ = false;
};

// =============================================================================
// CODE-04: Capabilities, Permissions & Sandbox Limits
// =============================================================================

enum class ScriptCapability : uint32_t {
    None         = 0,
    ReadState    = 1 << 0,
    WriteControl = 1 << 1,
    EmitEvents   = 1 << 2,
    AccessTime   = 1 << 3,
    RandomGen    = 1 << 4
};

[[nodiscard]] constexpr ScriptCapability operator|(ScriptCapability a, ScriptCapability b) noexcept {
    return static_cast<ScriptCapability>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

[[nodiscard]] constexpr bool has_capability(ScriptCapability mask, ScriptCapability cap) noexcept {
    return (static_cast<uint32_t>(mask) & static_cast<uint32_t>(cap)) == static_cast<uint32_t>(cap);
}

struct SandboxLimits {
    uint64_t max_instructions = 100000; // max operations before timeout
    size_t max_memory_bytes = 1024 * 1024; // 1 MB allocation limit
    double max_execution_time_ms = 50.0;
    ScriptCapability granted_capabilities = ScriptCapability::ReadState | ScriptCapability::AccessTime;
};

// =============================================================================
// CODE-02: Typed Host API for Sandboxed Execution
// =============================================================================

using ScriptValue = std::variant<double, bool, std::string, Vector3d>;

struct ScriptHostContext {
    std::unordered_map<std::string, ScriptValue> state_registers;
    std::unordered_map<std::string, ScriptValue> control_registers;
    std::vector<std::pair<std::string, ScriptValue>> emitted_events;
    double simulation_time = 0.0;
    double delta_time = 0.01;
};

class HostSandboxAPI {
public:
    explicit HostSandboxAPI(ScriptHostContext& context, SandboxLimits limits = {});

    [[nodiscard]] base::Result<ScriptValue> read_state(const std::string& key) const;
    [[nodiscard]] base::Result<void> write_control(const std::string& key, ScriptValue value);
    [[nodiscard]] base::Result<void> emit_event(const std::string& event_type, ScriptValue payload);
    [[nodiscard]] base::Result<double> get_simulation_time() const;

    [[nodiscard]] const SandboxLimits& limits() const noexcept { return limits_; }

private:
    ScriptHostContext& ctx_;
    SandboxLimits limits_;
};

// =============================================================================
// CODE-05: Deterministic PRNG
// =============================================================================

class DeterministicPRNG {
public:
    explicit DeterministicPRNG(uint64_t seed = 42);

    void set_seed(uint64_t seed) noexcept;
    [[nodiscard]] uint64_t next_u64() noexcept;
    [[nodiscard]] double next_double() noexcept; // uniform [0.0, 1.0)
    [[nodiscard]] double next_gaussian(double mean = 0.0, double stddev = 1.0) noexcept;

private:
    uint64_t state_;
};

// =============================================================================
// CODE-09: Safe Formula Expression Parser & Evaluator
// =============================================================================

class FormulaParser {
public:
    FormulaParser() = default;

    /// Evaluates formula expression safely with variable bindings and instruction count limits
    [[nodiscard]] static base::Result<double> evaluate(
        const std::string& expression,
        const std::unordered_map<std::string, double>& variables = {},
        uint64_t instruction_limit = 10000);
};

// =============================================================================
// CODE-06 & CODE-03: Script Lifecycle & Runtime
// =============================================================================

enum class ScriptLifecycleState {
    Uncompiled,
    Ready,
    Attached,
    Running,
    Paused,
    Error,
    Terminated
};

struct ScriptEvent {
    std::string name;
    ScriptValue payload;
};

class ScriptEngine {
public:
    explicit ScriptEngine(std::string script_id, std::string source_code, SandboxLimits limits = {});

    [[nodiscard]] const std::string& script_id() const noexcept { return script_id_; }
    [[nodiscard]] ScriptLifecycleState state() const noexcept { return state_; }
    [[nodiscard]] const std::string& last_error() const noexcept { return last_error_; }

    [[nodiscard]] base::Result<void> compile();
    [[nodiscard]] base::Result<void> init(ScriptHostContext& ctx);
    [[nodiscard]] base::Result<void> execute_step(ScriptHostContext& ctx);
    void pause() noexcept;
    void resume() noexcept;
    void reset() noexcept;

    void add_breakpoint(int line_number);
    void remove_breakpoint(int line_number);
    [[nodiscard]] bool has_breakpoint(int line_number) const noexcept;

    [[nodiscard]] const std::vector<std::string>& execution_trace() const noexcept { return trace_log_; }

private:
    std::string script_id_;
    std::string source_;
    SandboxLimits limits_;
    ScriptLifecycleState state_ = ScriptLifecycleState::Uncompiled;
    std::string last_error_;
    std::vector<int> breakpoints_;
    std::vector<std::string> trace_log_;
    DeterministicPRNG prng_;
};

// =============================================================================
// CODE-07: Code Editor Diagnostics & Autocomplete
// =============================================================================

enum class TokenType {
    Keyword,
    Identifier,
    Number,
    Operator,
    StringLiteral,
    Comment,
    Unknown
};

struct CodeToken {
    TokenType type;
    std::string text;
    int line = 1;
    int column = 1;
};

struct AutocompleteSuggestion {
    std::string label;
    std::string insert_text;
    std::string documentation;
};

class CodeEditorModel {
public:
    [[nodiscard]] static std::vector<CodeToken> tokenize(const std::string& code);
    [[nodiscard]] static std::vector<AutocompleteSuggestion> get_suggestions(const std::string& prefix);
};

// =============================================================================
// CODE-08: Visual Rule Builder Blocks
// =============================================================================

enum class VisualBlockType {
    ConstantNumber,
    ReadState,
    ArithmeticOp,
    CompareOp,
    PIDUpdate,
    WriteControl
};

struct VisualRuleBlock {
    std::string block_id;
    VisualBlockType type;
    std::string parameter;
    std::vector<std::string> input_block_ids;
};

class VisualRuleGraph {
public:
    void add_block(const VisualRuleBlock& block);
    [[nodiscard]] std::string compile_to_formula() const;

private:
    std::vector<VisualRuleBlock> blocks_;
};

// =============================================================================
// CODE-11: Script Package Versioning & Provenance
// =============================================================================

struct ScriptPackageMetadata {
    std::string package_name;
    std::string version = "1.0.0";
    std::string author;
    uint32_t checksum_fnv1a = 0;
    ScriptCapability required_capabilities = ScriptCapability::ReadState;

    [[nodiscard]] static uint32_t compute_checksum(const std::string& content) noexcept;
};

} // namespace shunya::core::domain::scripting
