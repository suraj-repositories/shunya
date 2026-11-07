#include "shunya/core/domain/scripting.hpp"

#include <cmath>
#include <algorithm>
#include <cctype>
#include <sstream>

namespace shunya::core::domain::scripting {

namespace {

base::Error make_script_error(std::string msg, int code = 1) {
    base::Error err;
    err.code = code;
    err.domain = base::ErrorDomain::Scripting;
    err.severity = base::Severity::Error;
    err.message = std::move(msg);
    return err;
}

} // namespace

// =============================================================================
// PIDController
// =============================================================================

double PIDController::update(double measured, double dt) noexcept {
    if (dt <= 1e-12) return 0.0;

    const double error = cfg_.setpoint - measured;

    // Proportional term
    const double p_term = cfg_.kp * error;

    // Integral term with anti-windup clamping
    integral_ += error * dt;
    integral_ = std::clamp(integral_, cfg_.integral_min, cfg_.integral_max);
    const double i_term = cfg_.ki * integral_;

    // Derivative term
    double d_term = 0.0;
    if (has_prev_) {
        d_term = cfg_.kd * ((error - last_error_) / dt);
    }
    last_error_ = error;
    has_prev_ = true;

    const double output = p_term + i_term + d_term;
    return std::clamp(output, cfg_.output_min, cfg_.output_max);
}

void PIDController::reset() noexcept {
    integral_ = 0.0;
    last_error_ = 0.0;
    has_prev_ = false;
}

// =============================================================================
// HostSandboxAPI
// =============================================================================

HostSandboxAPI::HostSandboxAPI(ScriptHostContext& context, SandboxLimits limits)
    : ctx_(context), limits_(limits) {}

base::Result<ScriptValue> HostSandboxAPI::read_state(const std::string& key) const {
    if (!has_capability(limits_.granted_capabilities, ScriptCapability::ReadState)) {
        return base::Result<ScriptValue>::err(make_script_error("Permission denied: ReadState capability required", 403));
    }

    auto it = ctx_.state_registers.find(key);
    if (it == ctx_.state_registers.end()) {
        return base::Result<ScriptValue>::err(make_script_error("State register not found: " + key, 404));
    }
    return base::Result<ScriptValue>::ok(it->second);
}

base::Result<void> HostSandboxAPI::write_control(const std::string& key, ScriptValue value) {
    if (!has_capability(limits_.granted_capabilities, ScriptCapability::WriteControl)) {
        return base::Result<void>::err(make_script_error("Permission denied: WriteControl capability required", 403));
    }

    ctx_.control_registers[key] = std::move(value);
    return base::Result<void>::ok();
}

base::Result<void> HostSandboxAPI::emit_event(const std::string& event_type, ScriptValue payload) {
    if (!has_capability(limits_.granted_capabilities, ScriptCapability::EmitEvents)) {
        return base::Result<void>::err(make_script_error("Permission denied: EmitEvents capability required", 403));
    }

    ctx_.emitted_events.emplace_back(event_type, std::move(payload));
    return base::Result<void>::ok();
}

base::Result<double> HostSandboxAPI::get_simulation_time() const {
    if (!has_capability(limits_.granted_capabilities, ScriptCapability::AccessTime)) {
        return base::Result<double>::err(make_script_error("Permission denied: AccessTime capability required", 403));
    }

    return base::Result<double>::ok(ctx_.simulation_time);
}

// =============================================================================
// DeterministicPRNG (Xorshift64*)
// =============================================================================

DeterministicPRNG::DeterministicPRNG(uint64_t seed)
    : state_(seed == 0 ? 0x853c49e6748fea9bULL : seed) {}

void DeterministicPRNG::set_seed(uint64_t seed) noexcept {
    state_ = (seed == 0 ? 0x853c49e6748fea9bULL : seed);
}

uint64_t DeterministicPRNG::next_u64() noexcept {
    state_ ^= state_ >> 12;
    state_ ^= state_ << 25;
    state_ ^= state_ >> 27;
    return state_ * 0x2545F4914F6CDD1DULL;
}

double DeterministicPRNG::next_double() noexcept {
    // 53-bit resolution uniform in [0, 1)
    return static_cast<double>(next_u64() >> 11) * (1.0 / (1ULL << 53));
}

double DeterministicPRNG::next_gaussian(double mean, double stddev) noexcept {
    // Box-Muller transform
    const double u1 = std::max(1e-15, next_double());
    const double u2 = next_double();
    const double r = std::sqrt(-2.0 * std::log(u1));
    const double theta = 2.0 * base::math::PI * u2;
    return mean + stddev * (r * std::cos(theta));
}

// =============================================================================
// FormulaParser
// =============================================================================

namespace {

int get_precedence(char op) {
    if (op == '+' || op == '-') return 1;
    if (op == '*' || op == '/') return 2;
    if (op == '^') return 3;
    return 0;
}

double apply_op(double a, double b, char op, bool& error) {
    switch (op) {
        case '+': return a + b;
        case '-': return a - b;
        case '*': return a * b;
        case '/':
            if (std::abs(b) < 1e-15) {
                error = true;
                return 0.0;
            }
            return a / b;
        case '^': return std::pow(a, b);
        default: error = true; return 0.0;
    }
}

} // namespace

base::Result<double> FormulaParser::evaluate(
    const std::string& expression,
    const std::unordered_map<std::string, double>& variables,
    uint64_t instruction_limit) {

    std::vector<double> values;
    std::vector<char> ops;
    uint64_t instructions = 0;

    const std::size_t len = expression.size();
    std::size_t i = 0;
    bool expect_unary = true;

    while (i < len) {
        if (++instructions > instruction_limit) {
            return base::Result<double>::err(make_script_error("Execution instruction limit exceeded (timeout)", 408));
        }

        const char c = expression[i];

        if (std::isspace(static_cast<unsigned char>(c))) {
            ++i;
            continue;
        }

        if (c == '(') {
            ops.push_back('(');
            expect_unary = true;
            ++i;
        } else if (c == ')') {
            while (!ops.empty() && ops.back() != '(') {
                if (values.size() < 2) {
                    return base::Result<double>::err(make_script_error("Malformed expression syntax"));
                }
                const double b = values.back(); values.pop_back();
                const double a = values.back(); values.pop_back();
                const char op = ops.back(); ops.pop_back();
                bool err = false;
                const double res = apply_op(a, b, op, err);
                if (err) return base::Result<double>::err(make_script_error("Division by zero or invalid operation"));
                values.push_back(res);
            }
            if (ops.empty()) {
                return base::Result<double>::err(make_script_error("Mismatched parentheses in formula"));
            }
            ops.pop_back(); // Remove '('
            expect_unary = false;
            ++i;
        } else if (std::isdigit(static_cast<unsigned char>(c)) || (c == '.' && i + 1 < len && std::isdigit(static_cast<unsigned char>(expression[i + 1])))) {
            std::size_t next_idx = 0;
            const double val = std::stod(expression.substr(i), &next_idx);
            values.push_back(val);
            i += next_idx;
            expect_unary = false;
        } else if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            std::string ident;
            while (i < len && (std::isalnum(static_cast<unsigned char>(expression[i])) || expression[i] == '_')) {
                ident += expression[i++];
            }
            // Check for built-in functions
            if (ident == "sin" || ident == "cos" || ident == "exp" || ident == "sqrt" || ident == "abs") {
                // Must be followed by (
                while (i < len && std::isspace(static_cast<unsigned char>(expression[i]))) ++i;
                if (i >= len || expression[i] != '(') {
                    return base::Result<double>::err(make_script_error("Expected '(' after function " + ident));
                }
                // Find matching ')'
                std::size_t depth = 1;
                std::size_t j = i + 1;
                while (j < len && depth > 0) {
                    if (expression[j] == '(') ++depth;
                    else if (expression[j] == ')') --depth;
                    ++j;
                }
                const std::string arg_str = expression.substr(i + 1, j - i - 2);
                auto arg_eval = evaluate(arg_str, variables, instruction_limit - instructions);
                if (!arg_eval.is_ok()) return arg_eval;
                const double arg = arg_eval.value();
                double res = 0.0;
                if (ident == "sin") res = std::sin(arg);
                else if (ident == "cos") res = std::cos(arg);
                else if (ident == "exp") res = std::exp(arg);
                else if (ident == "sqrt") {
                    if (arg < 0.0) return base::Result<double>::err(make_script_error("Domain error: sqrt of negative"));
                    res = std::sqrt(arg);
                } else if (ident == "abs") res = std::abs(arg);
                values.push_back(res);
                i = j;
                expect_unary = false;
            } else {
                // Variable lookup
                auto it = variables.find(ident);
                if (it == variables.end()) {
                    return base::Result<double>::err(make_script_error("Undefined variable in formula: " + ident));
                }
                values.push_back(it->second);
                expect_unary = false;
            }
        } else if (c == '+' || c == '-' || c == '*' || c == '/' || c == '^') {
            if (expect_unary && c == '-') {
                // Unary minus: push 0.0 and subtract
                values.push_back(0.0);
            }
            while (!ops.empty() && get_precedence(ops.back()) >= get_precedence(c)) {
                if (values.size() < 2) {
                    return base::Result<double>::err(make_script_error("Malformed operator operand"));
                }
                const double b = values.back(); values.pop_back();
                const double a = values.back(); values.pop_back();
                const char op = ops.back(); ops.pop_back();
                bool err = false;
                const double res = apply_op(a, b, op, err);
                if (err) return base::Result<double>::err(make_script_error("Division by zero or invalid operation"));
                values.push_back(res);
            }
            ops.push_back(c);
            expect_unary = true;
            ++i;
        } else {
            return base::Result<double>::err(make_script_error("Unexpected character in formula: " + std::string(1, c)));
        }
    }

    while (!ops.empty()) {
        if (ops.back() == '(') {
            return base::Result<double>::err(make_script_error("Unmatched opening parenthesis in formula"));
        }
        if (values.size() < 2) {
            return base::Result<double>::err(make_script_error("Insufficient operands at end of expression"));
        }
        const double b = values.back(); values.pop_back();
        const double a = values.back(); values.pop_back();
        const char op = ops.back(); ops.pop_back();
        bool err = false;
        const double res = apply_op(a, b, op, err);
        if (err) return base::Result<double>::err(make_script_error("Division by zero or invalid operation"));
        values.push_back(res);
    }

    if (values.size() != 1) {
        return base::Result<double>::err(make_script_error("Malformed expression result"));
    }

    return base::Result<double>::ok(values.back());
}

// =============================================================================
// ScriptEngine
// =============================================================================

ScriptEngine::ScriptEngine(std::string script_id, std::string source_code, SandboxLimits limits)
    : script_id_(std::move(script_id)), source_(std::move(source_code)), limits_(limits) {}

base::Result<void> ScriptEngine::compile() {
    if (source_.empty()) {
        state_ = ScriptLifecycleState::Error;
        last_error_ = "Empty source code";
        return base::Result<void>::err(make_script_error(last_error_));
    }

    state_ = ScriptLifecycleState::Ready;
    last_error_.clear();
    trace_log_.push_back("Script compiled successfully");
    return base::Result<void>::ok();
}

base::Result<void> ScriptEngine::init(ScriptHostContext& /*ctx*/) {
    if (state_ == ScriptLifecycleState::Uncompiled) {
        auto comp_res = compile();
        if (!comp_res.is_ok()) return comp_res;
    }

    state_ = ScriptLifecycleState::Attached;
    trace_log_.push_back("Script initialized and attached to host");
    return base::Result<void>::ok();
}

base::Result<void> ScriptEngine::execute_step(ScriptHostContext& ctx) {
    if (state_ == ScriptLifecycleState::Paused) {
        return base::Result<void>::ok(); // paused
    }

    state_ = ScriptLifecycleState::Running;
    HostSandboxAPI api(ctx, limits_);

    // Execute script instructions
    std::istringstream stream(source_);
    std::string line;
    int line_num = 1;

    std::unordered_map<std::string, double> var_map;
    for (const auto& [k, v] : ctx.state_registers) {
        if (std::holds_alternative<double>(v)) {
            var_map[k] = std::get<double>(v);
        }
    }

    while (std::getline(stream, line)) {
        if (has_breakpoint(line_num)) {
            trace_log_.push_back("Breakpoint hit at line " + std::to_string(line_num));
            state_ = ScriptLifecycleState::Paused;
            return base::Result<void>::ok();
        }

        // Trim leading spaces
        std::size_t start = line.find_first_not_of(" \t");
        if (start != std::string::npos) {
            line = line.substr(start);
        }

        if (line.empty() || line[0] == '#') {
            line_num++;
            continue;
        }

        // SET key = expr
        if (line.rfind("SET ", 0) == 0) {
            std::size_t eq_pos = line.find('=');
            if (eq_pos != std::string::npos) {
                std::string key = line.substr(4, eq_pos - 4);
                key.erase(std::remove_if(key.begin(), key.end(), ::isspace), key.end());

                std::string expr = line.substr(eq_pos + 1);
                auto eval_res = FormulaParser::evaluate(expr, var_map, limits_.max_instructions);
                if (!eval_res.is_ok()) {
                    state_ = ScriptLifecycleState::Error;
                    last_error_ = eval_res.error().message;
                    return base::Result<void>::err(eval_res.error());
                }

                auto write_res = api.write_control(key, eval_res.value());
                if (!write_res.is_ok()) {
                    state_ = ScriptLifecycleState::Error;
                    last_error_ = write_res.error().message;
                    return write_res;
                }
                var_map[key] = eval_res.value();
                trace_log_.push_back("Assigned " + key + " = " + std::to_string(eval_res.value()));
            }
        }
        // EMIT event_name payload
        else if (line.rfind("EMIT ", 0) == 0) {
            std::istringstream iss(line.substr(5));
            std::string evt_name;
            double evt_val = 0.0;
            if (iss >> evt_name >> evt_val) {
                auto emit_res = api.emit_event(evt_name, evt_val);
                if (!emit_res.is_ok()) {
                    state_ = ScriptLifecycleState::Error;
                    last_error_ = emit_res.error().message;
                    return emit_res;
                }
                trace_log_.push_back("Emitted event " + evt_name);
            }
        }

        line_num++;
    }

    return base::Result<void>::ok();
}

void ScriptEngine::pause() noexcept {
    state_ = ScriptLifecycleState::Paused;
    trace_log_.push_back("Execution paused");
}

void ScriptEngine::resume() noexcept {
    state_ = ScriptLifecycleState::Running;
    trace_log_.push_back("Execution resumed");
}

void ScriptEngine::reset() noexcept {
    state_ = ScriptLifecycleState::Ready;
    trace_log_.clear();
    last_error_.clear();
}

void ScriptEngine::add_breakpoint(int line_number) {
    if (!has_breakpoint(line_number)) {
        breakpoints_.push_back(line_number);
    }
}

void ScriptEngine::remove_breakpoint(int line_number) {
    breakpoints_.erase(std::remove(breakpoints_.begin(), breakpoints_.end(), line_number), breakpoints_.end());
}

bool ScriptEngine::has_breakpoint(int line_number) const noexcept {
    return std::find(breakpoints_.begin(), breakpoints_.end(), line_number) != breakpoints_.end();
}

// =============================================================================
// CodeEditorModel
// =============================================================================

std::vector<CodeToken> CodeEditorModel::tokenize(const std::string& code) {
    std::vector<CodeToken> tokens;
    const std::size_t len = code.size();
    std::size_t i = 0;
    int line = 1;
    int col = 1;

    while (i < len) {
        const char c = code[i];

        if (c == '\n') {
            line++;
            col = 1;
            i++;
            continue;
        }

        if (std::isspace(static_cast<unsigned char>(c))) {
            col++;
            i++;
            continue;
        }

        if (c == '#') {
            std::string comment;
            while (i < len && code[i] != '\n') {
                comment += code[i++];
            }
            tokens.push_back(CodeToken{TokenType::Comment, comment, line, col});
            continue;
        }

        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            std::string word;
            int start_col = col;
            while (i < len && (std::isalnum(static_cast<unsigned char>(code[i])) || code[i] == '_')) {
                word += code[i++];
                col++;
            }

            TokenType ttype = TokenType::Identifier;
            if (word == "SET" || word == "IF" || word == "THEN" || word == "ELSE" || word == "EMIT" || word == "PID") {
                ttype = TokenType::Keyword;
            }
            tokens.push_back(CodeToken{ttype, word, line, start_col});
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') {
            std::string num;
            int start_col = col;
            while (i < len && (std::isdigit(static_cast<unsigned char>(code[i])) || code[i] == '.')) {
                num += code[i++];
                col++;
            }
            tokens.push_back(CodeToken{TokenType::Number, num, line, start_col});
            continue;
        }

        if (c == '=' || c == '+' || c == '-' || c == '*' || c == '/' || c == '(' || c == ')') {
            tokens.push_back(CodeToken{TokenType::Operator, std::string(1, c), line, col});
            i++;
            col++;
            continue;
        }

        tokens.push_back(CodeToken{TokenType::Unknown, std::string(1, c), line, col});
        i++;
        col++;
    }

    return tokens;
}

std::vector<AutocompleteSuggestion> CodeEditorModel::get_suggestions(const std::string& prefix) {
    static const std::vector<AutocompleteSuggestion> all_symbols = {
        {"SET", "SET ${1:control} = ${2:value}", "Sets a host control register value"},
        {"EMIT", "EMIT ${1:event_name} ${2:payload}", "Emits a discrete simulation event"},
        {"sin", "sin(${1:x})", "Sine function in radians"},
        {"cos", "cos(${1:x})", "Cosine function in radians"},
        {"sqrt", "sqrt(${1:x})", "Square root function"},
        {"clamp", "clamp(${1:val}, ${2:min}, ${3:max})", "Clamp value within interval"}
    };

    std::vector<AutocompleteSuggestion> matches;
    for (const auto& sym : all_symbols) {
        if (prefix.empty() || sym.label.rfind(prefix, 0) == 0) {
            matches.push_back(sym);
        }
    }
    return matches;
}

// =============================================================================
// VisualRuleGraph
// =============================================================================

void VisualRuleGraph::add_block(const VisualRuleBlock& block) {
    blocks_.push_back(block);
}

std::string VisualRuleGraph::compile_to_formula() const {
    if (blocks_.empty()) return "0.0";

    // Simple chain compilation: concatenates blocks into expression
    std::string expr;
    for (const auto& block : blocks_) {
        if (block.type == VisualBlockType::ConstantNumber || block.type == VisualBlockType::ReadState) {
            if (!expr.empty()) expr += " + ";
            expr += block.parameter;
        }
    }
    return expr.empty() ? "0.0" : expr;
}

// =============================================================================
// ScriptPackageMetadata
// =============================================================================

uint32_t ScriptPackageMetadata::compute_checksum(const std::string& content) noexcept {
    // 32-bit FNV-1a hash
    uint32_t hash = 2166136261u;
    for (char c : content) {
        hash ^= static_cast<uint8_t>(c);
        hash *= 16777619u;
    }
    return hash;
}

} // namespace shunya::core::domain::scripting
