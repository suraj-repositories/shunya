#pragma once

#include "shunya/core/base/error.hpp"
#include "shunya/core/base/result.hpp"

#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace shunya::core::domain {

// =============================================================================
// LOGIC-01: Logic Levels, 4-State Logic, Propagation Delays
// =============================================================================

enum class LogicLevel : uint8_t {
    Low = 0,     // '0'
    High = 1,    // '1'
    Unknown = 2, // 'X' (Indeterminate / conflict)
    HighZ = 3    // 'Z' (High Impedance / floating)
};

[[nodiscard]] std::string_view logic_level_to_string(LogicLevel lvl) noexcept;
[[nodiscard]] char logic_level_to_char(LogicLevel lvl) noexcept;
[[nodiscard]] std::optional<LogicLevel> logic_level_from_char(char c) noexcept;

struct PropagationDelay {
    uint64_t t_lh_ps = 100; // Low-to-High propagation delay in picoseconds
    uint64_t t_hl_ps = 100; // High-to-Low propagation delay in picoseconds

    [[nodiscard]] uint64_t delay_for_transition(LogicLevel old_lvl, LogicLevel new_lvl) const noexcept {
        if (old_lvl == LogicLevel::Low && new_lvl == LogicLevel::High) return t_lh_ps;
        if (old_lvl == LogicLevel::High && new_lvl == LogicLevel::Low) return t_hl_ps;
        return (t_lh_ps + t_hl_ps) / 2;
    }
};

// Bus resolution function (IEEE 1164 wired-OR / Tristate resolution)
[[nodiscard]] LogicLevel resolve_bus_levels(const std::vector<LogicLevel>& drivers) noexcept;

// =============================================================================
// LOGIC-02: Functional Boolean Gates
// =============================================================================

enum class GateType {
    Buffer,
    Not,
    And,
    Or,
    Nand,
    Nor,
    Xor,
    Xnor,
    TristateBuffer // In, Enable -> Out
};

[[nodiscard]] std::string_view gate_type_to_string(GateType type) noexcept;

class GateEvaluator {
public:
    [[nodiscard]] static LogicLevel eval_not(LogicLevel in) noexcept;
    [[nodiscard]] static LogicLevel eval_and(const std::vector<LogicLevel>& inputs) noexcept;
    [[nodiscard]] static LogicLevel eval_or(const std::vector<LogicLevel>& inputs) noexcept;
    [[nodiscard]] static LogicLevel eval_nand(const std::vector<LogicLevel>& inputs) noexcept;
    [[nodiscard]] static LogicLevel eval_nor(const std::vector<LogicLevel>& inputs) noexcept;
    [[nodiscard]] static LogicLevel eval_xor(const std::vector<LogicLevel>& inputs) noexcept;
    [[nodiscard]] static LogicLevel eval_xnor(const std::vector<LogicLevel>& inputs) noexcept;
    [[nodiscard]] static LogicLevel eval_tristate(LogicLevel in, LogicLevel enable) noexcept;

    [[nodiscard]] static LogicLevel evaluate(GateType type, const std::vector<LogicLevel>& inputs) noexcept;
};

struct LogicGateInstance {
    std::string id;
    GateType type = GateType::And;
    std::vector<std::string> input_nets;
    std::string output_net;
    PropagationDelay delay;
};

// =============================================================================
// LOGIC-03: Combinational Circuit & Truth Table Test Harness
// =============================================================================

struct TruthTableRow {
    std::vector<LogicLevel> inputs;
    std::vector<LogicLevel> outputs;
};

class DigitalNetlist {
public:
    DigitalNetlist() = default;

    void add_input_port(std::string name);
    void add_output_port(std::string name);
    void add_gate(LogicGateInstance gate);

    [[nodiscard]] const std::vector<std::string>& input_ports() const noexcept { return input_ports_; }
    [[nodiscard]] const std::vector<std::string>& output_ports() const noexcept { return output_ports_; }
    [[nodiscard]] const std::vector<LogicGateInstance>& gates() const noexcept { return gates_; }

    [[nodiscard]] std::unordered_map<std::string, LogicLevel> evaluate_combinational(
        const std::unordered_map<std::string, LogicLevel>& input_values) const;

private:
    std::vector<std::string> input_ports_;
    std::vector<std::string> output_ports_;
    std::vector<LogicGateInstance> gates_;
};

class TruthTableGenerator {
public:
    [[nodiscard]] static std::vector<TruthTableRow> generate(const DigitalNetlist& circuit);
    [[nodiscard]] static bool verify_matches(
        const std::vector<TruthTableRow>& actual,
        const std::vector<TruthTableRow>& expected) noexcept;
};

// =============================================================================
// LOGIC-04: Sequential Elements (Latch, Flip-Flops, Counter, Registers)
// =============================================================================

class DFlipFlop {
public:
    DFlipFlop() = default;

    // Clock edge triggered evaluation (positive edge)
    void tick(LogicLevel clk, LogicLevel d, LogicLevel async_reset = LogicLevel::Low, LogicLevel async_set = LogicLevel::Low);

    [[nodiscard]] LogicLevel q() const noexcept { return q_; }
    [[nodiscard]] LogicLevel q_bar() const noexcept { return GateEvaluator::eval_not(q_); }

private:
    LogicLevel q_ = LogicLevel::Low;
    LogicLevel last_clk_ = LogicLevel::Low;
};

class JKFlipFlop {
public:
    JKFlipFlop() = default;

    void tick(LogicLevel clk, LogicLevel j, LogicLevel k);

    [[nodiscard]] LogicLevel q() const noexcept { return q_; }
    [[nodiscard]] LogicLevel q_bar() const noexcept { return GateEvaluator::eval_not(q_); }

private:
    LogicLevel q_ = LogicLevel::Low;
    LogicLevel last_clk_ = LogicLevel::Low;
};

class BinaryCounter {
public:
    explicit BinaryCounter(uint32_t width = 4);

    void tick(LogicLevel clk, LogicLevel enable = LogicLevel::High, LogicLevel reset = LogicLevel::Low);

    [[nodiscard]] uint64_t count() const noexcept { return count_; }
    [[nodiscard]] uint32_t width() const noexcept { return width_; }
    [[nodiscard]] std::vector<LogicLevel> count_bits() const;

private:
    uint32_t width_;
    uint64_t count_ = 0;
    LogicLevel last_clk_ = LogicLevel::Low;
};

// =============================================================================
// LOGIC-05: Discrete-Event Scheduler & Setup/Hold Timing Checks
// =============================================================================

struct LogicEvent {
    uint64_t timestamp_ps = 0;
    std::string target_net;
    LogicLevel new_level = LogicLevel::Low;

    // For min-heap priority queue
    bool operator>(const LogicEvent& other) const noexcept {
        return timestamp_ps > other.timestamp_ps;
    }
};

class LogicEventEngine {
public:
    LogicEventEngine() = default;

    void schedule_event(uint64_t time_ps, std::string target_net, LogicLevel level);
    void run_until(uint64_t end_time_ps);

    [[nodiscard]] LogicLevel get_net_level(std::string_view net) const noexcept;
    [[nodiscard]] uint64_t current_time() const noexcept { return current_time_ps_; }
    [[nodiscard]] size_t pending_events_count() const noexcept { return event_queue_.size(); }

    void set_net_level(std::string net, LogicLevel level);
    void clear() noexcept;

private:
    uint64_t current_time_ps_ = 0;
    std::priority_queue<LogicEvent, std::vector<LogicEvent>, std::greater<LogicEvent>> event_queue_;
    std::unordered_map<std::string, LogicLevel> net_states_;
};

struct TimingCheckReport {
    bool violation_detected = false;
    std::string signal_name;
    std::string clock_name;
    uint64_t violation_time_ps = 0;
    std::string details;
};

class SetupHoldChecker {
public:
    SetupHoldChecker(uint64_t setup_time_ps = 50, uint64_t hold_time_ps = 30);

    void record_data_transition(uint64_t time_ps, std::string_view signal);
    [[nodiscard]] std::optional<TimingCheckReport> record_clock_edge(
        uint64_t time_ps, std::string_view clock_name, std::string_view data_signal);

private:
    uint64_t setup_time_ps_;
    uint64_t hold_time_ps_;
    std::unordered_map<std::string, uint64_t> last_data_transition_ps_;
};

// =============================================================================
// LOGIC-06: Bus Abstractions & Hierarchical Modules
// =============================================================================

class LogicBus {
public:
    explicit LogicBus(uint32_t width = 8, LogicLevel init_level = LogicLevel::Low);

    [[nodiscard]] uint32_t width() const noexcept { return width_; }
    [[nodiscard]] LogicLevel get_bit(uint32_t bit) const;
    void set_bit(uint32_t bit, LogicLevel level);

    [[nodiscard]] uint64_t to_uint64() const noexcept;
    void from_uint64(uint64_t val) noexcept;

    [[nodiscard]] LogicBus slice(uint32_t high_bit, uint32_t low_bit) const;
    [[nodiscard]] std::string to_binary_string() const;

private:
    uint32_t width_;
    std::vector<LogicLevel> bits_;
};

// =============================================================================
// LOGIC-07: Computing Abstractions (ALU, Register File, Memory RAM/ROM)
// =============================================================================

enum class AluOpcode {
    Add,
    Sub,
    And,
    Or,
    Xor,
    Not,
    ShiftLeft,
    ShiftRight,
    PassA
};

struct AluResult {
    LogicBus result;
    bool zero_flag = false;
    bool carry_flag = false;
    bool overflow_flag = false;
    bool negative_flag = false;
};

class ArithmeticLogicUnit {
public:
    explicit ArithmeticLogicUnit(uint32_t width = 8);

    [[nodiscard]] AluResult execute(AluOpcode op, const LogicBus& a, const LogicBus& b, bool carry_in = false) const;

private:
    uint32_t width_;
};

class RegisterFile {
public:
    RegisterFile(uint32_t num_registers = 8, uint32_t data_width = 8);

    [[nodiscard]] LogicBus read_port1(uint32_t reg_index) const;
    [[nodiscard]] LogicBus read_port2(uint32_t reg_index) const;

    void tick_write(LogicLevel clk, LogicLevel write_enable, uint32_t reg_index, const LogicBus& data);

private:
    uint32_t num_registers_;
    uint32_t data_width_;
    std::vector<LogicBus> registers_;
    LogicLevel last_clk_ = LogicLevel::Low;
};

class MemoryBlock {
public:
    explicit MemoryBlock(size_t capacity_bytes = 1024);

    [[nodiscard]] uint8_t read_byte(uint32_t address) const noexcept;
    void write_byte(uint32_t address, uint8_t val) noexcept;

    void load_rom(const std::vector<uint8_t>& image, uint32_t start_address = 0);
    [[nodiscard]] size_t capacity() const noexcept { return storage_.size(); }

private:
    std::vector<uint8_t> storage_;
};

// =============================================================================
// LOGIC-08: Mixed-Signal Bridge (Analog Voltages <-> Digital States)
// =============================================================================

struct LogicThresholdConfig {
    double v_il = 0.8;  // Max low voltage
    double v_ih = 2.0;  // Min high voltage
    double v_ol = 0.2;  // Standard low output voltage
    double v_oh = 4.8;  // Standard high output voltage
    double r_out = 50.0; // Driver output impedance in Ohms
};

class AnalogToDigitalBridge {
public:
    explicit AnalogToDigitalBridge(LogicThresholdConfig cfg = {});

    // Continuous voltage to 4-state logic level conversion
    [[nodiscard]] LogicLevel voltage_to_logic(double voltage) const noexcept;

private:
    LogicThresholdConfig cfg_;
};

class DigitalToAnalogBridge {
public:
    explicit DigitalToAnalogBridge(LogicThresholdConfig cfg = {});

    // 4-state logic level to equivalent voltage and driver conductance
    void logic_to_analog(LogicLevel level, double& out_voltage, double& out_impedance) const noexcept;

private:
    LogicThresholdConfig cfg_;
};

// =============================================================================
// LOGIC-09: Circuit Validation Helpers
// =============================================================================

class DigitalCircuitFactory {
public:
    // 1-bit Full Adder: inputs (A, B, Cin) -> outputs (Sum, Cout)
    [[nodiscard]] static DigitalNetlist create_full_adder();

    // 2-to-1 Multiplexer: inputs (D0, D1, S) -> output (Y)
    [[nodiscard]] static DigitalNetlist create_mux_2to1();
};

} // namespace shunya::core::domain
