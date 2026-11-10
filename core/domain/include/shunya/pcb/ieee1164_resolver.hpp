#pragma once

#include "shunya/core/domain/digital_logic.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

namespace shunya::pcb {

/**
 * @brief IEEE 1164 9-state standard logic values.
 */
enum class StdLogic : uint8_t {
    U = 0, // Uninitialized
    X = 1, // Forcing Unknown
    Zero = 2, // Forcing 0
    One = 3,  // Forcing 1
    Z = 4,    // High Impedance
    W = 5,    // Weak Unknown
    L = 6,    // Weak 0
    H = 7,    // Weak 1
    DontCare = 8 // '-' Don't care
};

class Ieee1164Resolver {
public:
    [[nodiscard]] static constexpr char to_char(StdLogic s) noexcept {
        constexpr std::array<char, 9> kChars{'U', 'X', '0', '1', 'Z', 'W', 'L', 'H', '-'};
        const auto idx = static_cast<size_t>(s);
        return (idx < kChars.size()) ? kChars[idx] : 'X';
    }

    [[nodiscard]] static constexpr StdLogic from_char(char c) noexcept {
        switch (c) {
            case 'U': case 'u': return StdLogic::U;
            case 'X': case 'x': return StdLogic::X;
            case '0': return StdLogic::Zero;
            case '1': return StdLogic::One;
            case 'Z': case 'z': return StdLogic::Z;
            case 'W': case 'w': return StdLogic::W;
            case 'L': case 'l': return StdLogic::L;
            case 'H': case 'h': return StdLogic::H;
            case '-': return StdLogic::DontCare;
            default: return StdLogic::X;
        }
    }

    [[nodiscard]] static constexpr StdLogic from_4state(core::domain::LogicLevel lvl) noexcept {
        switch (lvl) {
            case core::domain::LogicLevel::Low: return StdLogic::Zero;
            case core::domain::LogicLevel::High: return StdLogic::One;
            case core::domain::LogicLevel::HighZ: return StdLogic::Z;
            case core::domain::LogicLevel::Unknown: return StdLogic::X;
        }
        return StdLogic::X;
    }

    [[nodiscard]] static constexpr core::domain::LogicLevel to_4state(StdLogic s) noexcept {
        switch (s) {
            case StdLogic::Zero:
            case StdLogic::L:
                return core::domain::LogicLevel::Low;
            case StdLogic::One:
            case StdLogic::H:
                return core::domain::LogicLevel::High;
            case StdLogic::Z:
                return core::domain::LogicLevel::HighZ;
            default:
                return core::domain::LogicLevel::Unknown;
        }
    }

    /**
     * @brief Standard IEEE 1164 9x9 resolution table between two drivers on the same net.
     */
    [[nodiscard]] static constexpr StdLogic resolve(StdLogic a, StdLogic b) noexcept {
        using S = StdLogic;
        // Official IEEE Std 1164 resolution_table (U, X, 0, 1, Z, W, L, H, -)
        constexpr std::array<std::array<StdLogic, 9>, 9> kResTable{{
            /* U */ {{ S::U, S::U, S::U, S::U, S::U, S::U, S::U, S::U, S::U }},
            /* X */ {{ S::U, S::X, S::X, S::X, S::X, S::X, S::X, S::X, S::X }},
            /* 0 */ {{ S::U, S::X, S::Zero, S::X, S::Zero, S::Zero, S::Zero, S::Zero, S::X }},
            /* 1 */ {{ S::U, S::X, S::X, S::One, S::One, S::One, S::One, S::One, S::X }},
            /* Z */ {{ S::U, S::X, S::Zero, S::One, S::Z, S::W, S::L, S::H, S::X }},
            /* W */ {{ S::U, S::X, S::Zero, S::One, S::W, S::W, S::W, S::W, S::X }},
            /* L */ {{ S::U, S::X, S::Zero, S::One, S::L, S::W, S::L, S::W, S::X }},
            /* H */ {{ S::U, S::X, S::Zero, S::One, S::H, S::W, S::W, S::H, S::X }},
            /* - */ {{ S::U, S::X, S::X, S::X, S::X, S::X, S::X, S::X, S::X }}
        }};
        const auto ia = static_cast<size_t>(a);
        const auto ib = static_cast<size_t>(b);
        if (ia >= 9 || ib >= 9) return S::X;
        return kResTable[ia][ib];
    }

    [[nodiscard]] static StdLogic resolve_bus(std::span<const StdLogic> drivers) noexcept {
        if (drivers.empty()) return StdLogic::Z;
        StdLogic acc = drivers[0];
        for (size_t i = 1; i < drivers.size(); ++i) {
            acc = resolve(acc, drivers[i]);
        }
        return acc;
    }

    [[nodiscard]] static constexpr StdLogic eval_not(StdLogic a) noexcept {
        switch (a) {
            case StdLogic::U: return StdLogic::U;
            case StdLogic::Zero:
            case StdLogic::L: return StdLogic::One;
            case StdLogic::One:
            case StdLogic::H: return StdLogic::Zero;
            default: return StdLogic::X;
        }
    }

    [[nodiscard]] static constexpr StdLogic eval_and2(StdLogic a, StdLogic b) noexcept {
        using S = StdLogic;
        if (a == S::Zero || a == S::L || b == S::Zero || b == S::L) return S::Zero;
        if ((a == S::One || a == S::H) && (b == S::One || b == S::H)) return S::One;
        if (a == S::U || b == S::U) return S::U;
        return S::X;
    }

    [[nodiscard]] static constexpr StdLogic eval_or2(StdLogic a, StdLogic b) noexcept {
        using S = StdLogic;
        if (a == S::One || a == S::H || b == S::One || b == S::H) return S::One;
        if ((a == S::Zero || a == S::L) && (b == S::Zero || b == S::L)) return S::Zero;
        if (a == S::U || b == S::U) return S::U;
        return S::X;
    }

    [[nodiscard]] static constexpr StdLogic eval_xor2(StdLogic a, StdLogic b) noexcept {
        using S = StdLogic;
        if (a == S::U || b == S::U) return S::U;
        const bool a_known = (a == S::Zero || a == S::L || a == S::One || a == S::H);
        const bool b_known = (b == S::Zero || b == S::L || b == S::One || b == S::H);
        if (!a_known || !b_known) return S::X;
        const bool va = (a == S::One || a == S::H);
        const bool vb = (b == S::One || b == S::H);
        return (va != vb) ? S::One : S::Zero;
    }

    [[nodiscard]] static StdLogic evaluate_gate(
        core::domain::GateType type,
        std::span<const StdLogic> inputs) noexcept {
        if (inputs.empty()) return StdLogic::U;
        switch (type) {
            case core::domain::GateType::Buffer:
                return (inputs[0] == StdLogic::L) ? StdLogic::Zero :
                       (inputs[0] == StdLogic::H) ? StdLogic::One :
                       (inputs[0] == StdLogic::Zero || inputs[0] == StdLogic::One || inputs[0] == StdLogic::U) ? inputs[0] : StdLogic::X;
            case core::domain::GateType::Not:
                return eval_not(inputs[0]);
            case core::domain::GateType::And:
            case core::domain::GateType::Nand: {
                StdLogic acc = inputs[0];
                if (acc == StdLogic::L) acc = StdLogic::Zero;
                if (acc == StdLogic::H) acc = StdLogic::One;
                for (size_t i = 1; i < inputs.size(); ++i) acc = eval_and2(acc, inputs[i]);
                return (type == core::domain::GateType::Nand) ? eval_not(acc) : acc;
            }
            case core::domain::GateType::Or:
            case core::domain::GateType::Nor: {
                StdLogic acc = inputs[0];
                if (acc == StdLogic::L) acc = StdLogic::Zero;
                if (acc == StdLogic::H) acc = StdLogic::One;
                for (size_t i = 1; i < inputs.size(); ++i) acc = eval_or2(acc, inputs[i]);
                return (type == core::domain::GateType::Nor) ? eval_not(acc) : acc;
            }
            case core::domain::GateType::Xor:
            case core::domain::GateType::Xnor: {
                StdLogic acc = inputs[0];
                for (size_t i = 1; i < inputs.size(); ++i) acc = eval_xor2(acc, inputs[i]);
                return (type == core::domain::GateType::Xnor) ? eval_not(acc) : acc;
            }
            case core::domain::GateType::TristateBuffer: {
                if (inputs.size() < 2) return StdLogic::X;
                const StdLogic en = inputs[1];
                if (en == StdLogic::One || en == StdLogic::H) {
                    if (inputs[0] == StdLogic::One || inputs[0] == StdLogic::H) return StdLogic::One;
                    if (inputs[0] == StdLogic::Zero || inputs[0] == StdLogic::L) return StdLogic::Zero;
                    return StdLogic::X;
                }
                if (en == StdLogic::Zero || en == StdLogic::L) return StdLogic::Z;
                return StdLogic::X;
            }
        }
        return StdLogic::X;
    }
};

} // namespace shunya::pcb
