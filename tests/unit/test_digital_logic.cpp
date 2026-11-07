#include "tests/test_framework.hpp"
#include "shunya/core/domain/digital_logic.hpp"

#include <cmath>
#include <string>
#include <vector>

using namespace shunya::core;
using namespace shunya::core::domain;

// =============================================================================
// LOGIC-01: Logic Levels, 4-State Logic, Propagation Delays
// =============================================================================

TEST_CASE("LOGIC-01: 4-State Logic Levels, Strings, and Bus Resolution") {
    CHECK(logic_level_to_string(LogicLevel::Low) == "Low");
    CHECK(logic_level_to_string(LogicLevel::High) == "High");
    CHECK(logic_level_to_string(LogicLevel::Unknown) == "Unknown");
    CHECK(logic_level_to_string(LogicLevel::HighZ) == "HighZ");

    CHECK(logic_level_to_char(LogicLevel::Low) == '0');
    CHECK(logic_level_to_char(LogicLevel::High) == '1');
    CHECK(logic_level_to_char(LogicLevel::Unknown) == 'X');
    CHECK(logic_level_to_char(LogicLevel::HighZ) == 'Z');

    CHECK(logic_level_from_char('0').value() == LogicLevel::Low);
    CHECK(logic_level_from_char('1').value() == LogicLevel::High);
    CHECK(logic_level_from_char('X').value() == LogicLevel::Unknown);
    CHECK(logic_level_from_char('Z').value() == LogicLevel::HighZ);
    CHECK_FALSE(logic_level_from_char('Q').has_value());

    // Bus resolution
    CHECK(resolve_bus_levels({}) == LogicLevel::HighZ);
    CHECK(resolve_bus_levels({LogicLevel::HighZ, LogicLevel::HighZ}) == LogicLevel::HighZ);
    CHECK(resolve_bus_levels({LogicLevel::HighZ, LogicLevel::Low}) == LogicLevel::Low);
    CHECK(resolve_bus_levels({LogicLevel::HighZ, LogicLevel::High}) == LogicLevel::High);
    CHECK(resolve_bus_levels({LogicLevel::Low, LogicLevel::High}) == LogicLevel::Unknown); // Contention!

    // Propagation delay
    PropagationDelay delay{.t_lh_ps = 80, .t_hl_ps = 60};
    CHECK(delay.delay_for_transition(LogicLevel::Low, LogicLevel::High) == 80u);
    CHECK(delay.delay_for_transition(LogicLevel::High, LogicLevel::Low) == 60u);
    CHECK(delay.delay_for_transition(LogicLevel::Low, LogicLevel::Low) == 70u);
}

// =============================================================================
// LOGIC-02: Functional Boolean Gates
// =============================================================================

TEST_CASE("LOGIC-02: Boolean Gates Evaluation (NOT, AND, OR, XOR, Tristate)") {
    CHECK(gate_type_to_string(GateType::And) == "And");
    CHECK(gate_type_to_string(GateType::Xor) == "Xor");

    // NOT
    CHECK(GateEvaluator::eval_not(LogicLevel::Low) == LogicLevel::High);
    CHECK(GateEvaluator::eval_not(LogicLevel::High) == LogicLevel::Low);
    CHECK(GateEvaluator::eval_not(LogicLevel::Unknown) == LogicLevel::Unknown);

    // AND (with dominant 0)
    CHECK(GateEvaluator::eval_and({LogicLevel::High, LogicLevel::High}) == LogicLevel::High);
    CHECK(GateEvaluator::eval_and({LogicLevel::High, LogicLevel::Low}) == LogicLevel::Low);
    CHECK(GateEvaluator::eval_and({LogicLevel::Low, LogicLevel::Unknown}) == LogicLevel::Low); // Dominant 0
    CHECK(GateEvaluator::eval_and({LogicLevel::High, LogicLevel::Unknown}) == LogicLevel::Unknown);

    // OR (with dominant 1)
    CHECK(GateEvaluator::eval_or({LogicLevel::Low, LogicLevel::Low}) == LogicLevel::Low);
    CHECK(GateEvaluator::eval_or({LogicLevel::High, LogicLevel::Low}) == LogicLevel::High);
    CHECK(GateEvaluator::eval_or({LogicLevel::High, LogicLevel::Unknown}) == LogicLevel::High); // Dominant 1
    CHECK(GateEvaluator::eval_or({LogicLevel::Low, LogicLevel::Unknown}) == LogicLevel::Unknown);

    // XOR
    CHECK(GateEvaluator::eval_xor({LogicLevel::Low, LogicLevel::Low}) == LogicLevel::Low);
    CHECK(GateEvaluator::eval_xor({LogicLevel::High, LogicLevel::Low}) == LogicLevel::High);
    CHECK(GateEvaluator::eval_xor({LogicLevel::High, LogicLevel::High}) == LogicLevel::Low);
    CHECK(GateEvaluator::eval_xor({LogicLevel::High, LogicLevel::Unknown}) == LogicLevel::Unknown);

    // Tristate Buffer
    CHECK(GateEvaluator::eval_tristate(LogicLevel::High, LogicLevel::High) == LogicLevel::High);
    CHECK(GateEvaluator::eval_tristate(LogicLevel::Low, LogicLevel::High) == LogicLevel::Low);
    CHECK(GateEvaluator::eval_tristate(LogicLevel::High, LogicLevel::Low) == LogicLevel::HighZ);
    CHECK(GateEvaluator::eval_tristate(LogicLevel::High, LogicLevel::Unknown) == LogicLevel::Unknown);
}

// =============================================================================
// LOGIC-03: Combinational Circuit & Truth Table Test Harness
// =============================================================================

TEST_CASE("LOGIC-03: Combinational Circuit Evaluation and Truth Table Harness") {
    // 2-input NAND circuit
    DigitalNetlist nand_circuit;
    nand_circuit.add_input_port("A");
    nand_circuit.add_input_port("B");
    nand_circuit.add_output_port("Y");

    LogicGateInstance nand_gate;
    nand_gate.id = "G1";
    nand_gate.type = GateType::Nand;
    nand_gate.input_nets = {"A", "B"};
    nand_gate.output_net = "Y";
    nand_circuit.add_gate(nand_gate);

    auto table = TruthTableGenerator::generate(nand_circuit);
    REQUIRE(table.size() == 4u);

    // Row 0: 0, 0 -> 1
    const std::vector<LogicLevel> expected_row0_in = {LogicLevel::Low, LogicLevel::Low};
    const std::vector<LogicLevel> expected_row0_out = {LogicLevel::High};
    CHECK(table[0].inputs == expected_row0_in);
    CHECK(table[0].outputs == expected_row0_out);

    // Row 1: 0, 1 -> 1
    CHECK(table[1].outputs == std::vector<LogicLevel>{LogicLevel::High});

    // Row 2: 1, 0 -> 1
    CHECK(table[2].outputs == std::vector<LogicLevel>{LogicLevel::High});

    // Row 3: 1, 1 -> 0
    CHECK(table[3].outputs == std::vector<LogicLevel>{LogicLevel::Low});

    CHECK(TruthTableGenerator::verify_matches(table, table));
}

// =============================================================================
// LOGIC-04: Sequential Elements (D-FF, JK-FF, Counter)
// =============================================================================

TEST_CASE("LOGIC-04: Flip-Flops and Binary Counter") {
    // D Flip-Flop
    DFlipFlop dff;
    CHECK(dff.q() == LogicLevel::Low);
    CHECK(dff.q_bar() == LogicLevel::High);

    // Clock Low -> High with D = High
    dff.tick(LogicLevel::High, LogicLevel::High);
    CHECK(dff.q() == LogicLevel::High);

    // Clock remains High (no edge) with D = Low -> Q remains High
    dff.tick(LogicLevel::High, LogicLevel::Low);
    CHECK(dff.q() == LogicLevel::High);

    // Clock Low (falling edge)
    dff.tick(LogicLevel::Low, LogicLevel::Low);
    CHECK(dff.q() == LogicLevel::High);

    // Next rising edge with D = Low
    dff.tick(LogicLevel::High, LogicLevel::Low);
    CHECK(dff.q() == LogicLevel::Low);

    // Asynchronous reset
    dff.tick(LogicLevel::High, LogicLevel::High); // Q = 1
    dff.tick(LogicLevel::High, LogicLevel::High, LogicLevel::High, LogicLevel::Low); // Reset
    CHECK(dff.q() == LogicLevel::Low);

    // 4-bit Binary Counter
    BinaryCounter counter(4);
    CHECK(counter.count() == 0u);

    // Pulse clock 5 times
    for (int i = 0; i < 5; ++i) {
        counter.tick(LogicLevel::Low);
        counter.tick(LogicLevel::High);
    }
    CHECK(counter.count() == 5u);
    auto bits = counter.count_bits();
    // 5 in binary is 0101 (bit0=1, bit1=0, bit2=1, bit3=0)
    CHECK(bits[0] == LogicLevel::High);
    CHECK(bits[1] == LogicLevel::Low);
    CHECK(bits[2] == LogicLevel::High);
    CHECK(bits[3] == LogicLevel::Low);

    // Reset counter
    counter.tick(LogicLevel::Low);
    counter.tick(LogicLevel::High, LogicLevel::High, LogicLevel::High);
    CHECK(counter.count() == 0u);
}

// =============================================================================
// LOGIC-05: Discrete-Event Engine & Setup/Hold Timing Checks
// =============================================================================

TEST_CASE("LOGIC-05: Event Scheduler and Setup/Hold Violation Checks") {
    LogicEventEngine engine;
    engine.schedule_event(100, "clk", LogicLevel::High);
    engine.schedule_event(50, "data", LogicLevel::High);
    engine.schedule_event(200, "clk", LogicLevel::Low);

    CHECK(engine.pending_events_count() == 3u);

    // Run until 80 ps -> data should be High, clk still Low
    engine.run_until(80);
    CHECK(engine.get_net_level("data") == LogicLevel::High);
    CHECK(engine.get_net_level("clk") == LogicLevel::Low);
    CHECK(engine.current_time() == 80u);

    // Run until 150 ps -> clk becomes High
    engine.run_until(150);
    CHECK(engine.get_net_level("clk") == LogicLevel::High);

    // Setup/Hold checker (setup = 50 ps)
    SetupHoldChecker checker(50, 30);
    checker.record_data_transition(100, "d");

    // Clock edge at 180 ps (180 - 100 = 80 ps >= 50 ps setup) -> No violation
    auto rep1 = checker.record_clock_edge(180, "clk", "d");
    CHECK_FALSE(rep1.has_value());

    // Data changes at 190 ps
    checker.record_data_transition(190, "d");
    // Clock edge at 200 ps (200 - 190 = 10 ps < 50 ps setup) -> Setup Violation!
    auto rep2 = checker.record_clock_edge(200, "clk", "d");
    REQUIRE(rep2.has_value());
    CHECK(rep2->violation_detected);
}

// =============================================================================
// LOGIC-06: Bus Abstractions and Slicing
// =============================================================================

TEST_CASE("LOGIC-06: LogicBus Conversions and Slicing") {
    LogicBus bus(8);
    CHECK(bus.width() == 8u);
    CHECK(bus.to_uint64() == 0u);

    bus.from_uint64(0xA5); // 10100101
    CHECK(bus.to_uint64() == 0xA5);
    CHECK(bus.get_bit(0) == LogicLevel::High);
    CHECK(bus.get_bit(1) == LogicLevel::Low);
    CHECK(bus.get_bit(7) == LogicLevel::High);

    // Slicing: upper nibble [7:4] should be 0xA = 10
    LogicBus upper = bus.slice(7, 4);
    CHECK(upper.width() == 4u);
    CHECK(upper.to_uint64() == 0x0A);

    // Lower nibble [3:0] should be 0x5 = 5
    LogicBus lower = bus.slice(3, 0);
    CHECK(lower.width() == 4u);
    CHECK(lower.to_uint64() == 0x05);

    std::string bin_str = bus.to_binary_string();
    CHECK(bin_str == "10100101");
}

// =============================================================================
// LOGIC-07: Computing Abstractions (ALU, Register File, Memory)
// =============================================================================

TEST_CASE("LOGIC-07: Arithmetic Logic Unit and Memory Blocks") {
    ArithmeticLogicUnit alu(8);
    LogicBus a(8), b(8);
    a.from_uint64(15);
    b.from_uint64(25);

    // Addition: 15 + 25 = 40
    auto add_res = alu.execute(AluOpcode::Add, a, b);
    CHECK(add_res.result.to_uint64() == 40u);
    CHECK_FALSE(add_res.zero_flag);
    CHECK_FALSE(add_res.carry_flag);

    // Subtraction: 25 - 25 = 0 (Zero flag set)
    auto sub_res = alu.execute(AluOpcode::Sub, b, b);
    CHECK(sub_res.result.to_uint64() == 0u);
    CHECK(sub_res.zero_flag);

    // Bitwise AND: 15 (00001111) & 25 (00011001) = 9 (00001001)
    auto and_res = alu.execute(AluOpcode::And, a, b);
    CHECK(and_res.result.to_uint64() == 9u);

    // Register File
    RegisterFile regfile(8, 8);
    regfile.tick_write(LogicLevel::High, LogicLevel::High, 3, a); // Write 15 to R3
    auto read_r3 = regfile.read_port1(3);
    CHECK(read_r3.to_uint64() == 15u);

    // Memory Block
    MemoryBlock mem(256);
    mem.write_byte(0x10, 0x42);
    CHECK(mem.read_byte(0x10) == 0x42);
    CHECK(mem.read_byte(0x20) == 0x00);
}

// =============================================================================
// LOGIC-08: Mixed-Signal Bridges
// =============================================================================

TEST_CASE("LOGIC-08: Mixed-Signal Analog-Digital Bridges") {
    LogicThresholdConfig cfg;
    cfg.v_il = 0.8;
    cfg.v_ih = 2.0;
    cfg.v_ol = 0.1;
    cfg.v_oh = 4.9;

    AnalogToDigitalBridge a2d(cfg);
    CHECK(a2d.voltage_to_logic(0.2) == LogicLevel::Low);
    CHECK(a2d.voltage_to_logic(3.3) == LogicLevel::High);
    CHECK(a2d.voltage_to_logic(1.4) == LogicLevel::Unknown); // Inside threshold transition region

    DigitalToAnalogBridge d2a(cfg);
    double v_out = 0.0, r_out = 0.0;
    d2a.logic_to_analog(LogicLevel::High, v_out, r_out);
    CHECK(v_out == 4.9);
    CHECK(r_out == 50.0);

    d2a.logic_to_analog(LogicLevel::HighZ, v_out, r_out);
    CHECK(r_out >= 1e9);
}

// =============================================================================
// LOGIC-09: Circuit Validation Helpers
// =============================================================================

TEST_CASE("LOGIC-09: Full Adder and Multiplexer Validation Suite") {
    // 1-bit Full Adder truth table verification
    auto fa = DigitalCircuitFactory::create_full_adder();
    auto table = TruthTableGenerator::generate(fa);
    REQUIRE(table.size() == 8u);

    // Check A=1, B=1, Cin=0 -> Sum=0, Cout=1 (row index 6: 110)
    const std::vector<LogicLevel> expected_row6_in = {LogicLevel::High, LogicLevel::High, LogicLevel::Low};
    CHECK(table[6].inputs == expected_row6_in);
    CHECK(table[6].outputs[0] == LogicLevel::Low);  // Sum
    CHECK(table[6].outputs[1] == LogicLevel::High); // Cout

    // Check A=1, B=1, Cin=1 -> Sum=1, Cout=1 (row index 7: 111)
    CHECK(table[7].outputs[0] == LogicLevel::High); // Sum
    CHECK(table[7].outputs[1] == LogicLevel::High); // Cout

    // 2-to-1 Multiplexer
    auto mux = DigitalCircuitFactory::create_mux_2to1();
    // When S = 0, Y should follow D0
    auto eval0 = mux.evaluate_combinational({
        {"D0", LogicLevel::High}, {"D1", LogicLevel::Low}, {"S", LogicLevel::Low}
    });
    CHECK(eval0["Y"] == LogicLevel::High);

    // When S = 1, Y should follow D1
    auto eval1 = mux.evaluate_combinational({
        {"D0", LogicLevel::High}, {"D1", LogicLevel::Low}, {"S", LogicLevel::High}
    });
    CHECK(eval1["Y"] == LogicLevel::Low);
}
