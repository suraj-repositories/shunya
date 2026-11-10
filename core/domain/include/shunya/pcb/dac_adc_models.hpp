#pragma once

#include "shunya/pcb/ieee1164_resolver.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace shunya::pcb {

/**
 * @brief Analog-to-digital threshold comparator with optional hysteresis.
 */
class ThresholdComparatorAdc {
public:
    double v_il{0.8};
    double v_ih{2.0};
    double hysteresis_v{0.05};

    [[nodiscard]] StdLogic sample(double analog_v, StdLogic prev_state = StdLogic::Zero) const noexcept {
        if (analog_v >= v_ih + hysteresis_v * 0.5) {
            return StdLogic::One;
        }
        if (analog_v <= v_il - hysteresis_v * 0.5) {
            return StdLogic::Zero;
        }
        if (analog_v >= v_ih) {
            return (prev_state == StdLogic::One) ? StdLogic::One : StdLogic::X;
        }
        if (analog_v <= v_il) {
            return (prev_state == StdLogic::Zero) ? StdLogic::Zero : StdLogic::X;
        }
        return (prev_state == StdLogic::One || prev_state == StdLogic::Zero) ? prev_state : StdLogic::X;
    }
};

/**
 * @brief N-bit Flash/SAR ADC model converting continuous analog voltage in [v_low, v_high] to StdLogic bits (LSB first).
 */
class NBitAdcModel {
public:
    uint32_t bits{4};
    double v_low{0.0};
    double v_high{5.0};

    [[nodiscard]] uint64_t quantize_code(double analog_v) const noexcept {
        if (bits == 0) return 0;
        const uint64_t max_code = (1ULL << std::min<uint32_t>(bits, 30)) - 1ULL;
        const double span = std::max(1e-9, v_high - v_low);
        const double norm = std::clamp((analog_v - v_low) / span, 0.0, 1.0);
        return static_cast<uint64_t>(std::llround(norm * static_cast<double>(max_code)));
    }

    [[nodiscard]] std::vector<StdLogic> sample_bits(double analog_v) const {
        const uint64_t code = quantize_code(analog_v);
        std::vector<StdLogic> out(bits, StdLogic::Zero);
        for (uint32_t b = 0; b < bits; ++b) {
            out[b] = ((code >> b) & 1ULL) ? StdLogic::One : StdLogic::Zero;
        }
        return out;
    }
};

/**
 * @brief N-bit R-2R Ladder DAC model converting StdLogic bits (LSB first) into analog voltage V_ref * (code / 2^N).
 */
class R2RDacModel {
public:
    uint32_t bits{4};
    double v_ref{5.0};
    double v_gnd{0.0};
    double r_ladder_ohm{10000.0}; // R-2R Thevenin output impedance is R_ladder

    [[nodiscard]] double evaluate_voltage(const std::vector<StdLogic>& bits_lsb_first) const noexcept {
        if (bits == 0) return v_gnd;
        double weighted_sum = 0.0;
        const double scale = static_cast<double>(1ULL << std::min<uint32_t>(bits, 30));
        for (uint32_t b = 0; b < bits && b < bits_lsb_first.size(); ++b) {
            const StdLogic s = bits_lsb_first[b];
            double bit_weight = 0.5;
            if (s == StdLogic::One || s == StdLogic::H) {
                bit_weight = 1.0;
            } else if (s == StdLogic::Zero || s == StdLogic::L) {
                bit_weight = 0.0;
            }
            weighted_sum += bit_weight * static_cast<double>(1ULL << b);
        }
        return v_gnd + (v_ref - v_gnd) * (weighted_sum / scale);
    }
};

} // namespace shunya::pcb
