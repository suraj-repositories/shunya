#include "shunya/core/domain/digital_logic.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace shunya::core::domain {

// =============================================================================
// LOGIC-01: Logic Levels, 4-State Logic, Propagation Delays
// =============================================================================

std::string_view logic_level_to_string(LogicLevel lvl) noexcept {
    switch (lvl) {
        case LogicLevel::Low: return "Low";
        case LogicLevel::High: return "High";
        case LogicLevel::Unknown: return "Unknown";
        case LogicLevel::HighZ: return "HighZ";
    }
    return "Unknown";
}

char logic_level_to_char(LogicLevel lvl) noexcept {
    switch (lvl) {
        case LogicLevel::Low: return '0';
        case LogicLevel::High: return '1';
        case LogicLevel::Unknown: return 'X';
        case LogicLevel::HighZ: return 'Z';
    }
    return 'X';
}

std::optional<LogicLevel> logic_level_from_char(char c) noexcept {
    switch (c) {
        case '0': return LogicLevel::Low;
        case '1': return LogicLevel::High;
        case 'X': case 'x': return LogicLevel::Unknown;
        case 'Z': case 'z': return LogicLevel::HighZ;
    }
    return std::nullopt;
}

LogicLevel resolve_bus_levels(const std::vector<LogicLevel>& drivers) noexcept {
    if (drivers.empty()) return LogicLevel::HighZ;

    bool has_low = false;
    bool has_high = false;
    bool has_unknown = false;

    for (auto d : drivers) {
        if (d == LogicLevel::Low) has_low = true;
        else if (d == LogicLevel::High) has_high = true;
        else if (d == LogicLevel::Unknown) has_unknown = true;
    }

    if (has_unknown) return LogicLevel::Unknown;
    if (has_low && has_high) return LogicLevel::Unknown; // Bus contention!
    if (has_high) return LogicLevel::High;
    if (has_low) return LogicLevel::Low;
    return LogicLevel::HighZ;
}

// =============================================================================
// LOGIC-02: Functional Boolean Gates
// =============================================================================

std::string_view gate_type_to_string(GateType type) noexcept {
    switch (type) {
        case GateType::Buffer: return "Buffer";
        case GateType::Not: return "Not";
        case GateType::And: return "And";
        case GateType::Or: return "Or";
        case GateType::Nand: return "Nand";
        case GateType::Nor: return "Nor";
        case GateType::Xor: return "Xor";
        case GateType::Xnor: return "Xnor";
        case GateType::TristateBuffer: return "TristateBuffer";
    }
    return "Unknown";
}

LogicLevel GateEvaluator::eval_not(LogicLevel in) noexcept {
    switch (in) {
        case LogicLevel::Low: return LogicLevel::High;
        case LogicLevel::High: return LogicLevel::Low;
        case LogicLevel::Unknown: case LogicLevel::HighZ: return LogicLevel::Unknown;
    }
    return LogicLevel::Unknown;
}

LogicLevel GateEvaluator::eval_and(const std::vector<LogicLevel>& inputs) noexcept {
    if (inputs.empty()) return LogicLevel::Low;
    bool has_unknown = false;
    for (auto in : inputs) {
        if (in == LogicLevel::Low) return LogicLevel::Low; // Dominant 0
        if (in == LogicLevel::Unknown || in == LogicLevel::HighZ) has_unknown = true;
    }
    return has_unknown ? LogicLevel::Unknown : LogicLevel::High;
}

LogicLevel GateEvaluator::eval_or(const std::vector<LogicLevel>& inputs) noexcept {
    if (inputs.empty()) return LogicLevel::Low;
    bool has_unknown = false;
    for (auto in : inputs) {
        if (in == LogicLevel::High) return LogicLevel::High; // Dominant 1
        if (in == LogicLevel::Unknown || in == LogicLevel::HighZ) has_unknown = true;
    }
    return has_unknown ? LogicLevel::Unknown : LogicLevel::Low;
}

LogicLevel GateEvaluator::eval_nand(const std::vector<LogicLevel>& inputs) noexcept {
    return eval_not(eval_and(inputs));
}

LogicLevel GateEvaluator::eval_nor(const std::vector<LogicLevel>& inputs) noexcept {
    return eval_not(eval_or(inputs));
}

LogicLevel GateEvaluator::eval_xor(const std::vector<LogicLevel>& inputs) noexcept {
    if (inputs.empty()) return LogicLevel::Low;
    int high_count = 0;
    for (auto in : inputs) {
        if (in == LogicLevel::Unknown || in == LogicLevel::HighZ) return LogicLevel::Unknown;
        if (in == LogicLevel::High) high_count++;
    }
    return (high_count % 2 == 1) ? LogicLevel::High : LogicLevel::Low;
}

LogicLevel GateEvaluator::eval_xnor(const std::vector<LogicLevel>& inputs) noexcept {
    return eval_not(eval_xor(inputs));
}

LogicLevel GateEvaluator::eval_tristate(LogicLevel in, LogicLevel enable) noexcept {
    if (enable == LogicLevel::Low) return LogicLevel::HighZ;
    if (enable == LogicLevel::High) return in;
    return LogicLevel::Unknown;
}

LogicLevel GateEvaluator::evaluate(GateType type, const std::vector<LogicLevel>& inputs) noexcept {
    switch (type) {
        case GateType::Buffer: return inputs.empty() ? LogicLevel::Unknown : inputs.front();
        case GateType::Not: return inputs.empty() ? LogicLevel::Unknown : eval_not(inputs.front());
        case GateType::And: return eval_and(inputs);
        case GateType::Or: return eval_or(inputs);
        case GateType::Nand: return eval_nand(inputs);
        case GateType::Nor: return eval_nor(inputs);
        case GateType::Xor: return eval_xor(inputs);
        case GateType::Xnor: return eval_xnor(inputs);
        case GateType::TristateBuffer:
            if (inputs.size() < 2) return LogicLevel::Unknown;
            return eval_tristate(inputs[0], inputs[1]);
    }
    return LogicLevel::Unknown;
}

// =============================================================================
// LOGIC-03: Combinational Circuit & Truth Table Test Harness
// =============================================================================

void DigitalNetlist::add_input_port(std::string name) {
    input_ports_.push_back(std::move(name));
}

void DigitalNetlist::add_output_port(std::string name) {
    output_ports_.push_back(std::move(name));
}

void DigitalNetlist::add_gate(LogicGateInstance gate) {
    gates_.push_back(std::move(gate));
}

std::unordered_map<std::string, LogicLevel> DigitalNetlist::evaluate_combinational(
    const std::unordered_map<std::string, LogicLevel>& input_values) const {

    std::unordered_map<std::string, LogicLevel> net_values = input_values;

    // Iterative relaxation until stable state or iteration limit
    for (size_t iter = 0; iter < 100; ++iter) {
        bool changed = false;
        for (const auto& gate : gates_) {
            std::vector<LogicLevel> in_levels;
            in_levels.reserve(gate.input_nets.size());
            for (const auto& in_net : gate.input_nets) {
                auto it = net_values.find(in_net);
                in_levels.push_back(it != net_values.end() ? it->second : LogicLevel::Unknown);
            }

            LogicLevel out_level = GateEvaluator::evaluate(gate.type, in_levels);
            auto it = net_values.find(gate.output_net);
            if (it == net_values.end() || it->second != out_level) {
                net_values[gate.output_net] = out_level;
                changed = true;
            }
        }
        if (!changed) break;
    }

    return net_values;
}

std::vector<TruthTableRow> TruthTableGenerator::generate(const DigitalNetlist& circuit) {
    std::vector<TruthTableRow> rows;
    size_t num_inputs = circuit.input_ports().size();
    if (num_inputs == 0) return rows;

    uint64_t total_combinations = 1ULL << num_inputs;
    rows.reserve(total_combinations);

    for (uint64_t comb = 0; comb < total_combinations; ++comb) {
        std::unordered_map<std::string, LogicLevel> in_map;
        std::vector<LogicLevel> in_vec;
        in_vec.reserve(num_inputs);

        for (size_t b = 0; b < num_inputs; ++b) {
            bool bit_val = ((comb >> (num_inputs - 1 - b)) & 1ULL) != 0;
            LogicLevel lvl = bit_val ? LogicLevel::High : LogicLevel::Low;
            in_map[circuit.input_ports()[b]] = lvl;
            in_vec.push_back(lvl);
        }

        auto eval_res = circuit.evaluate_combinational(in_map);
        std::vector<LogicLevel> out_vec;
        out_vec.reserve(circuit.output_ports().size());

        for (const auto& out_name : circuit.output_ports()) {
            auto it = eval_res.find(out_name);
            out_vec.push_back(it != eval_res.end() ? it->second : LogicLevel::Unknown);
        }

        rows.push_back(TruthTableRow{std::move(in_vec), std::move(out_vec)});
    }

    return rows;
}

bool TruthTableGenerator::verify_matches(
    const std::vector<TruthTableRow>& actual,
    const std::vector<TruthTableRow>& expected) noexcept {

    if (actual.size() != expected.size()) return false;
    for (size_t i = 0; i < actual.size(); ++i) {
        if (actual[i].inputs != expected[i].inputs || actual[i].outputs != expected[i].outputs) {
            return false;
        }
    }
    return true;
}

// =============================================================================
// LOGIC-04: Sequential Elements
// =============================================================================

void DFlipFlop::tick(LogicLevel clk, LogicLevel d, LogicLevel async_reset, LogicLevel async_set) {
    if (async_reset == LogicLevel::High) {
        q_ = LogicLevel::Low;
    } else if (async_set == LogicLevel::High) {
        q_ = LogicLevel::High;
    } else {
        // Positive edge: Low -> High
        if (last_clk_ == LogicLevel::Low && clk == LogicLevel::High) {
            q_ = d;
        }
    }
    last_clk_ = clk;
}

void JKFlipFlop::tick(LogicLevel clk, LogicLevel j, LogicLevel k) {
    if (last_clk_ == LogicLevel::Low && clk == LogicLevel::High) {
        if (j == LogicLevel::Low && k == LogicLevel::High) {
            q_ = LogicLevel::Low;
        } else if (j == LogicLevel::High && k == LogicLevel::Low) {
            q_ = LogicLevel::High;
        } else if (j == LogicLevel::High && k == LogicLevel::High) {
            q_ = GateEvaluator::eval_not(q_);
        }
    }
    last_clk_ = clk;
}

BinaryCounter::BinaryCounter(uint32_t width)
    : width_(width) {}

void BinaryCounter::tick(LogicLevel clk, LogicLevel enable, LogicLevel reset) {
    if (last_clk_ == LogicLevel::Low && clk == LogicLevel::High) {
        if (reset == LogicLevel::High) {
            count_ = 0;
        } else if (enable == LogicLevel::High) {
            uint64_t max_val = (1ULL << width_) - 1;
            count_ = (count_ + 1) & max_val;
        }
    }
    last_clk_ = clk;
}

std::vector<LogicLevel> BinaryCounter::count_bits() const {
    std::vector<LogicLevel> bits(width_, LogicLevel::Low);
    for (uint32_t i = 0; i < width_; ++i) {
        bits[i] = ((count_ >> i) & 1ULL) ? LogicLevel::High : LogicLevel::Low;
    }
    return bits;
}

// =============================================================================
// LOGIC-05: Discrete-Event Scheduler & Timing Checks
// =============================================================================

void LogicEventEngine::schedule_event(uint64_t time_ps, std::string target_net, LogicLevel level) {
    event_queue_.push(LogicEvent{time_ps, std::move(target_net), level});
}

void LogicEventEngine::run_until(uint64_t end_time_ps) {
    while (!event_queue_.empty()) {
        const auto& ev = event_queue_.top();
        if (ev.timestamp_ps > end_time_ps) break;

        current_time_ps_ = ev.timestamp_ps;
        net_states_[ev.target_net] = ev.new_level;
        event_queue_.pop();
    }
    current_time_ps_ = end_time_ps;
}

LogicLevel LogicEventEngine::get_net_level(std::string_view net) const noexcept {
    auto it = net_states_.find(std::string(net));
    if (it != net_states_.end()) {
        return it->second;
    }
    return LogicLevel::Low;
}

void LogicEventEngine::set_net_level(std::string net, LogicLevel level) {
    net_states_[std::move(net)] = level;
}

void LogicEventEngine::clear() noexcept {
    current_time_ps_ = 0;
    while (!event_queue_.empty()) event_queue_.pop();
    net_states_.clear();
}

SetupHoldChecker::SetupHoldChecker(uint64_t setup_time_ps, uint64_t hold_time_ps)
    : setup_time_ps_(setup_time_ps), hold_time_ps_(hold_time_ps) {}

void SetupHoldChecker::record_data_transition(uint64_t time_ps, std::string_view signal) {
    last_data_transition_ps_[std::string(signal)] = time_ps;
}

std::optional<TimingCheckReport> SetupHoldChecker::record_clock_edge(
    uint64_t time_ps, std::string_view clock_name, std::string_view data_signal) {

    auto it = last_data_transition_ps_.find(std::string(data_signal));
    if (it != last_data_transition_ps_.end()) {
        uint64_t data_time = it->second;
        // Check setup violation: data changed too close before clock edge
        if (data_time <= time_ps && (time_ps - data_time) < setup_time_ps_) {
            return TimingCheckReport{
                .violation_detected = true,
                .signal_name = std::string(data_signal),
                .clock_name = std::string(clock_name),
                .violation_time_ps = time_ps,
                .details = "Setup time violation: data changed " + std::to_string(time_ps - data_time) +
                           " ps before clock edge (min required: " + std::to_string(setup_time_ps_) + " ps)."
            };
        }
    }
    return std::nullopt;
}

// =============================================================================
// LOGIC-06: Bus Abstractions
// =============================================================================

LogicBus::LogicBus(uint32_t width, LogicLevel init_level)
    : width_(width), bits_(width, init_level) {}

LogicLevel LogicBus::get_bit(uint32_t bit) const {
    if (bit < width_) return bits_[bit];
    return LogicLevel::Unknown;
}

void LogicBus::set_bit(uint32_t bit, LogicLevel level) {
    if (bit < width_) {
        bits_[bit] = level;
    }
}

uint64_t LogicBus::to_uint64() const noexcept {
    uint64_t val = 0;
    for (uint32_t i = 0; i < width_ && i < 64; ++i) {
        if (bits_[i] == LogicLevel::High) {
            val |= (1ULL << i);
        }
    }
    return val;
}

void LogicBus::from_uint64(uint64_t val) noexcept {
    for (uint32_t i = 0; i < width_ && i < 64; ++i) {
        bits_[i] = ((val >> i) & 1ULL) ? LogicLevel::High : LogicLevel::Low;
    }
}

LogicBus LogicBus::slice(uint32_t high_bit, uint32_t low_bit) const {
    if (high_bit < low_bit || high_bit >= width_) return LogicBus(1, LogicLevel::Unknown);
    uint32_t slice_w = high_bit - low_bit + 1;
    LogicBus res(slice_w);
    for (uint32_t i = 0; i < slice_w; ++i) {
        res.set_bit(i, bits_[low_bit + i]);
    }
    return res;
}

std::string LogicBus::to_binary_string() const {
    std::string s;
    s.reserve(width_);
    for (int i = static_cast<int>(width_) - 1; i >= 0; --i) {
        s += logic_level_to_char(bits_[i]);
    }
    return s;
}

// =============================================================================
// LOGIC-07: Computing Abstractions (ALU, Register File, Memory)
// =============================================================================

ArithmeticLogicUnit::ArithmeticLogicUnit(uint32_t width)
    : width_(width) {}

AluResult ArithmeticLogicUnit::execute(AluOpcode op, const LogicBus& a, const LogicBus& b, bool carry_in) const {
    AluResult res;
    res.result = LogicBus(width_);
    uint64_t val_a = a.to_uint64();
    uint64_t val_b = b.to_uint64();
    uint64_t mask = (1ULL << width_) - 1;
    uint64_t calc = 0;

    switch (op) {
        case AluOpcode::Add:
            calc = val_a + val_b + (carry_in ? 1 : 0);
            res.carry_flag = (calc > mask);
            break;
        case AluOpcode::Sub:
            calc = val_a - val_b - (carry_in ? 1 : 0);
            res.carry_flag = (val_a >= val_b + (carry_in ? 1 : 0));
            break;
        case AluOpcode::And:
            calc = val_a & val_b;
            break;
        case AluOpcode::Or:
            calc = val_a | val_b;
            break;
        case AluOpcode::Xor:
            calc = val_a ^ val_b;
            break;
        case AluOpcode::Not:
            calc = ~val_a;
            break;
        case AluOpcode::ShiftLeft:
            calc = val_a << 1;
            res.carry_flag = (val_a & (1ULL << (width_ - 1))) != 0;
            break;
        case AluOpcode::ShiftRight:
            calc = val_a >> 1;
            res.carry_flag = (val_a & 1ULL) != 0;
            break;
        case AluOpcode::PassA:
            calc = val_a;
            break;
    }

    uint64_t masked_res = calc & mask;
    res.result.from_uint64(masked_res);
    res.zero_flag = (masked_res == 0);
    res.negative_flag = (masked_res & (1ULL << (width_ - 1))) != 0;

    return res;
}

RegisterFile::RegisterFile(uint32_t num_registers, uint32_t data_width)
    : num_registers_(num_registers), data_width_(data_width) {
    registers_.assign(num_registers, LogicBus(data_width));
}

LogicBus RegisterFile::read_port1(uint32_t reg_index) const {
    if (reg_index < num_registers_) return registers_[reg_index];
    return LogicBus(data_width_, LogicLevel::Unknown);
}

LogicBus RegisterFile::read_port2(uint32_t reg_index) const {
    if (reg_index < num_registers_) return registers_[reg_index];
    return LogicBus(data_width_, LogicLevel::Unknown);
}

void RegisterFile::tick_write(LogicLevel clk, LogicLevel write_enable, uint32_t reg_index, const LogicBus& data) {
    if (last_clk_ == LogicLevel::Low && clk == LogicLevel::High) {
        if (write_enable == LogicLevel::High && reg_index < num_registers_) {
            registers_[reg_index] = data;
        }
    }
    last_clk_ = clk;
}

MemoryBlock::MemoryBlock(size_t capacity_bytes)
    : storage_(capacity_bytes, 0) {}

uint8_t MemoryBlock::read_byte(uint32_t address) const noexcept {
    if (address < storage_.size()) {
        return storage_[address];
    }
    return 0xFF;
}

void MemoryBlock::write_byte(uint32_t address, uint8_t val) noexcept {
    if (address < storage_.size()) {
        storage_[address] = val;
    }
}

void MemoryBlock::load_rom(const std::vector<uint8_t>& image, uint32_t start_address) {
    for (size_t i = 0; i < image.size() && (start_address + i) < storage_.size(); ++i) {
        storage_[start_address + i] = image[i];
    }
}

// =============================================================================
// LOGIC-08: Mixed-Signal Bridge
// =============================================================================

AnalogToDigitalBridge::AnalogToDigitalBridge(LogicThresholdConfig cfg)
    : cfg_(cfg) {}

LogicLevel AnalogToDigitalBridge::voltage_to_logic(double voltage) const noexcept {
    if (voltage <= cfg_.v_il) return LogicLevel::Low;
    if (voltage >= cfg_.v_ih) return LogicLevel::High;
    return LogicLevel::Unknown; // In transition region
}

DigitalToAnalogBridge::DigitalToAnalogBridge(LogicThresholdConfig cfg)
    : cfg_(cfg) {}

void DigitalToAnalogBridge::logic_to_analog(
    LogicLevel level, double& out_voltage, double& out_impedance) const noexcept {

    switch (level) {
        case LogicLevel::Low:
            out_voltage = cfg_.v_ol;
            out_impedance = cfg_.r_out;
            break;
        case LogicLevel::High:
            out_voltage = cfg_.v_oh;
            out_impedance = cfg_.r_out;
            break;
        case LogicLevel::HighZ:
            out_voltage = 0.0;
            out_impedance = 1e9; // 1 Giga-Ohm floating
            break;
        case LogicLevel::Unknown:
            out_voltage = 0.5 * (cfg_.v_il + cfg_.v_ih); // Intermediate threshold voltage
            out_impedance = cfg_.r_out;
            break;
    }
}

// =============================================================================
// LOGIC-09: Circuit Validation Helpers
// =============================================================================

DigitalNetlist DigitalCircuitFactory::create_full_adder() {
    DigitalNetlist netlist;
    netlist.add_input_port("A");
    netlist.add_input_port("B");
    netlist.add_input_port("Cin");
    netlist.add_output_port("Sum");
    netlist.add_output_port("Cout");

    // Sum = A ^ B ^ Cin
    LogicGateInstance xor1{.id = "G1", .type = GateType::Xor, .input_nets = {"A", "B"}, .output_net = "AxorB", .delay = {}};
    LogicGateInstance xor2{.id = "G2", .type = GateType::Xor, .input_nets = {"AxorB", "Cin"}, .output_net = "Sum", .delay = {}};

    // Cout = (A & B) | (Cin & (A ^ B))
    LogicGateInstance and1{.id = "G3", .type = GateType::And, .input_nets = {"A", "B"}, .output_net = "AandB", .delay = {}};
    LogicGateInstance and2{.id = "G4", .type = GateType::And, .input_nets = {"Cin", "AxorB"}, .output_net = "CinAndAxorB", .delay = {}};
    LogicGateInstance or1{.id = "G5", .type = GateType::Or, .input_nets = {"AandB", "CinAndAxorB"}, .output_net = "Cout", .delay = {}};

    netlist.add_gate(xor1);
    netlist.add_gate(xor2);
    netlist.add_gate(and1);
    netlist.add_gate(and2);
    netlist.add_gate(or1);

    return netlist;
}

DigitalNetlist DigitalCircuitFactory::create_mux_2to1() {
    DigitalNetlist netlist;
    netlist.add_input_port("D0");
    netlist.add_input_port("D1");
    netlist.add_input_port("S");
    netlist.add_output_port("Y");

    // Y = (D0 & ~S) | (D1 & S)
    LogicGateInstance not1{.id = "G_not", .type = GateType::Not, .input_nets = {"S"}, .output_net = "S_bar", .delay = {}};
    LogicGateInstance and1{.id = "G_and0", .type = GateType::And, .input_nets = {"D0", "S_bar"}, .output_net = "term0", .delay = {}};
    LogicGateInstance and2{.id = "G_and1", .type = GateType::And, .input_nets = {"D1", "S"}, .output_net = "term1", .delay = {}};
    LogicGateInstance or1{.id = "G_or", .type = GateType::Or, .input_nets = {"term0", "term1"}, .output_net = "Y", .delay = {}};

    netlist.add_gate(not1);
    netlist.add_gate(and1);
    netlist.add_gate(and2);
    netlist.add_gate(or1);

    return netlist;
}

} // namespace shunya::core::domain
