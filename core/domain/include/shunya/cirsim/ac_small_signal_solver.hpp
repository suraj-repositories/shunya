#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/cirsim/schematic_topology_parser.hpp"

#include <complex>
#include <string>
#include <unordered_map>
#include <vector>

namespace shunya::cirsim {

using ComplexNumber = std::complex<double>;

/**
 * @brief Complex Dense Matrix supporting Partial-Pivoting LU Decomposition.
 * Solves complex linear systems Y(w) * V(w) = I(w).
 */
class ComplexDenseMatrix {
public:
    ComplexDenseMatrix(size_t rows = 0, size_t cols = 0)
        : rows_(rows), cols_(cols), data_(rows * cols, ComplexNumber{0.0, 0.0}) {}

    [[nodiscard]] size_t rows() const noexcept { return rows_; }
    [[nodiscard]] size_t cols() const noexcept { return cols_; }

    [[nodiscard]] ComplexNumber& operator()(size_t r, size_t c) {
        return data_[r * cols_ + c];
    }
    [[nodiscard]] const ComplexNumber& operator()(size_t r, size_t c) const {
        return data_[r * cols_ + c];
    }

    void add(size_t r, size_t c, const ComplexNumber& val) {
        data_[r * cols_ + c] += val;
    }

    void resize(size_t rows, size_t cols) {
        rows_ = rows;
        cols_ = cols;
        data_.assign(rows * cols, ComplexNumber{0.0, 0.0});
    }

    [[nodiscard]] core::base::Result<std::vector<ComplexNumber>> solve_lu(
        std::vector<ComplexNumber> b
    ) const;

private:
    size_t rows_{0};
    size_t cols_{0};
    std::vector<ComplexNumber> data_;
};

enum class FrequencySweepSpacing {
    Decade,
    Linear,
    Octave
};

struct AcSweepOptions {
    double f_start{1.0};          // 1 Hz
    double f_stop{1.0e6};         // 1 MHz
    uint32_t points_per_decade{10};
    FrequencySweepSpacing spacing{FrequencySweepSpacing::Decade};
    double ac_input_amplitude{1.0}; // 1V reference for 0dB
    std::string input_source_id{"V1"};
};

struct AcNodeResponse {
    std::vector<double> frequencies;         // Hz
    std::vector<ComplexNumber> complex_v;    // raw phasor V(w)
    std::vector<double> magnitude_linear;    // Volts
    std::vector<double> magnitude_db;        // 20*log10(|V| / V_ref)
    std::vector<double> phase_degrees;       // -180 to +180 deg
    std::vector<double> unwrapped_phase_deg; // Continuous phase tracking
};

struct AcSimulationResult {
    bool success{false};
    std::vector<double> frequencies;
    // Node Name / Index -> Frequency response curve
    std::unordered_map<uint32_t, AcNodeResponse> node_responses;
    std::unordered_map<std::string, uint32_t> node_name_to_index;
};

/**
 * @brief AC Small-Signal Frequency Response Solver.
 * Assembles complex admittance matrices Y(w), solves Y(w) * V(w) = I(w) via LU,
 * and extracts magnitude (dB) and phase angles across frequency decades.
 * Fulfills roadmap tasks PH4-08.03.001 through PH4-08.03.003.
 */
class AcSmallSignalSolver {
public:
    explicit AcSmallSignalSolver(AcSweepOptions options = {})
        : options_(options) {}

    /**
     * @brief Generates frequency sampling points based on options (Decade / Linear / Octave).
     */
    [[nodiscard]] std::vector<double> generate_frequencies() const;

    /**
     * @brief Assembles complex admittance matrix Y(w) and source phasor vector I(w) at given frequency.
     */
    [[nodiscard]] core::base::Result<std::pair<ComplexDenseMatrix, std::vector<ComplexNumber>>>
    assemble_admittance_system(
        const ParsedSchematicTopology& topology,
        const std::vector<core::domain::ElectricalComponent>& components,
        double frequency_hz
    ) const;

    /**
     * @brief Executes full frequency sweep and returns frequency response for all nodes.
     */
    [[nodiscard]] core::base::Result<AcSimulationResult> solve_frequency_sweep(
        const ParsedSchematicTopology& topology,
        const std::vector<core::domain::ElectricalComponent>& components
    ) const;

private:
    AcSweepOptions options_;
};

} // namespace shunya::cirsim
