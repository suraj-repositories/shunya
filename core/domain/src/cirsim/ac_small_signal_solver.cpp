#include "shunya/cirsim/ac_small_signal_solver.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace shunya::cirsim {

using namespace core::domain;

core::base::Result<std::vector<ComplexNumber>> ComplexDenseMatrix::solve_lu(
    std::vector<ComplexNumber> b
) const {
    if (rows_ != cols_ || rows_ != b.size() || rows_ == 0) {
        return core::base::Result<std::vector<ComplexNumber>>::err(core::base::Error{
            .code = 1,
            .domain = core::base::ErrorDomain::Simulation,
            .severity = core::base::Severity::Error,
            .message = "Matrix dimension mismatch in complex LU solver.",
            .details = ""
        });
    }

    const size_t n = rows_;
    auto A = data_; // Copy for in-place factorization

    auto at = [&](size_t r, size_t c) -> ComplexNumber& {
        return A[r * n + c];
    };

    // Forward elimination with partial pivoting
    for (size_t k = 0; k < n; ++k) {
        // Find pivot
        size_t p = k;
        double max_val = std::abs(at(k, k));
        for (size_t i = k + 1; i < n; ++i) {
            double val = std::abs(at(i, k));
            if (val > max_val) {
                max_val = val;
                p = i;
            }
        }

        if (max_val < 1.0e-18) {
            return core::base::Result<std::vector<ComplexNumber>>::err(core::base::Error{
                .code = 2,
                .domain = core::base::ErrorDomain::Simulation,
                .severity = core::base::Severity::Error,
                .message = "Singular complex admittance matrix detected in LU factorization.",
                .details = ""
            });
        }

        // Swap rows in A and b
        if (p != k) {
            for (size_t j = k; j < n; ++j) {
                std::swap(at(k, j), at(p, j));
            }
            std::swap(b[k], b[p]);
        }

        // Eliminate below pivot
        const ComplexNumber pivot = at(k, k);
        for (size_t i = k + 1; i < n; ++i) {
            const ComplexNumber factor = at(i, k) / pivot;
            at(i, k) = factor;
            for (size_t j = k + 1; j < n; ++j) {
                at(i, j) -= factor * at(k, j);
            }
            b[i] -= factor * b[k];
        }
    }

    // Back-substitution
    std::vector<ComplexNumber> x(n, ComplexNumber{0.0, 0.0});
    for (int i = static_cast<int>(n) - 1; i >= 0; --i) {
        ComplexNumber sum = b[static_cast<size_t>(i)];
        for (size_t j = static_cast<size_t>(i) + 1; j < n; ++j) {
            sum -= at(static_cast<size_t>(i), j) * x[j];
        }
        x[static_cast<size_t>(i)] = sum / at(static_cast<size_t>(i), static_cast<size_t>(i));
    }

    return core::base::Result<std::vector<ComplexNumber>>::ok(std::move(x));
}

std::vector<double> AcSmallSignalSolver::generate_frequencies() const {
    std::vector<double> freqs;
    if (options_.f_start <= 0.0 || options_.f_stop <= options_.f_start) {
        freqs.push_back(1000.0);
        return freqs;
    }

    if (options_.spacing == FrequencySweepSpacing::Decade) {
        const double decades = std::log10(options_.f_stop / options_.f_start);
        const size_t total_points = std::max(2ULL, static_cast<size_t>(decades * options_.points_per_decade) + 1);
        freqs.reserve(total_points);

        for (size_t i = 0; i < total_points; ++i) {
            double exponent = std::log10(options_.f_start) + decades * (static_cast<double>(i) / (total_points - 1));
            freqs.push_back(std::pow(10.0, exponent));
        }
    } else if (options_.spacing == FrequencySweepSpacing::Linear) {
        const size_t total_points = std::max(2U, options_.points_per_decade * 5);
        freqs.reserve(total_points);
        for (size_t i = 0; i < total_points; ++i) {
            double f = options_.f_start + (options_.f_stop - options_.f_start) * (static_cast<double>(i) / (total_points - 1));
            freqs.push_back(f);
        }
    } else { // Octave
        const double octaves = std::log2(options_.f_stop / options_.f_start);
        const size_t total_points = std::max(2ULL, static_cast<size_t>(octaves * options_.points_per_decade) + 1);
        freqs.reserve(total_points);
        for (size_t i = 0; i < total_points; ++i) {
            double exponent = std::log2(options_.f_start) + octaves * (static_cast<double>(i) / (total_points - 1));
            freqs.push_back(std::pow(2.0, exponent));
        }
    }
    return freqs;
}

core::base::Result<std::pair<ComplexDenseMatrix, std::vector<ComplexNumber>>>
AcSmallSignalSolver::assemble_admittance_system(
    const ParsedSchematicTopology& topology,
    const std::vector<ElectricalComponent>& components,
    double frequency_hz
) const {
    // Map non-ground nodes to row indices
    std::vector<uint32_t> sorted_indices;
    for (const auto& [node_idx, info] : topology.nodes) {
        if (!info.is_ground) {
            sorted_indices.push_back(node_idx);
        }
    }
    std::sort(sorted_indices.begin(), sorted_indices.end());

    std::unordered_map<uint32_t, size_t> node_to_row;
    for (size_t i = 0; i < sorted_indices.size(); ++i) {
        node_to_row[sorted_indices[i]] = i;
    }
    const size_t num_nodes = sorted_indices.size();

    // Voltage source branches
    std::vector<const ElectricalComponent*> v_sources;
    for (const auto& comp : components) {
        if (comp.type == ElectricalComponentType::VoltageSourceAC ||
            comp.type == ElectricalComponentType::VoltageSourceDC ||
            comp.type == ElectricalComponentType::VoltageSourcePulse) {
            v_sources.push_back(&comp);
        }
    }
    const size_t num_branches = v_sources.size();
    const size_t dim = num_nodes + num_branches;

    ComplexDenseMatrix Y(dim, dim);
    std::vector<ComplexNumber> I(dim, ComplexNumber{0.0, 0.0});

    const double omega = 2.0 * std::numbers::pi * std::max(1.0e-6, frequency_hz);

    auto get_node_of_pin = [&](const std::string& comp_id, const std::string& pin_id) -> uint32_t {
        std::string key = comp_id + ":" + pin_id;
        auto it = topology.pin_to_node_map.find(key);
        return (it != topology.pin_to_node_map.end()) ? it->second : 0;
    };

    auto get_row_of_node = [&](uint32_t n) -> int {
        if (n == 0) return -1;
        auto it = node_to_row.find(n);
        return (it != node_to_row.end()) ? static_cast<int>(it->second) : -1;
    };

    // Minimum shunt conductance for conditioning
    for (size_t r = 0; r < num_nodes; ++r) {
        Y.add(r, r, ComplexNumber{1.0e-12, 0.0});
    }

    // Stamp passive admittances
    for (const auto& comp : components) {
        if (comp.pins.size() < 2) continue;
        const int p = get_row_of_node(get_node_of_pin(comp.id, comp.pins[0].pin_id));
        const int q = get_row_of_node(get_node_of_pin(comp.id, comp.pins[1].pin_id));

        ComplexNumber y_comp{0.0, 0.0};
        if (comp.type == ElectricalComponentType::Resistor) {
            const double r = std::max(1.0e-12, comp.value_primary);
            y_comp = ComplexNumber{1.0 / r, 0.0};
        } else if (comp.type == ElectricalComponentType::Capacitor) {
            const double c = std::max(1.0e-18, comp.value_primary);
            y_comp = ComplexNumber{0.0, omega * c}; // j*w*C
        } else if (comp.type == ElectricalComponentType::Inductor) {
            const double l = std::max(1.0e-18, comp.value_primary);
            y_comp = ComplexNumber{0.0, -1.0 / (omega * l)}; // 1 / (j*w*L) = -j / (w*L)
        } else if (comp.type == ElectricalComponentType::CurrentSourceAC) {
            ComplexNumber is{comp.value_primary, 0.0};
            if (p >= 0) I[p] += is;
            if (q >= 0) I[q] -= is;
            continue;
        }

        if (p >= 0) Y.add(p, p, y_comp);
        if (q >= 0) Y.add(q, q, y_comp);
        if (p >= 0 && q >= 0) {
            Y.add(p, q, -y_comp);
            Y.add(q, p, -y_comp);
        }
    }

    // Stamp voltage source constraints
    for (size_t b = 0; b < num_branches; ++b) {
        const auto* vsrc = v_sources[b];
        const int p = get_row_of_node(get_node_of_pin(vsrc->id, vsrc->pins[0].pin_id));
        const int q = get_row_of_node(get_node_of_pin(vsrc->id, vsrc->pins[1].pin_id));

        if (p >= 0) {
            Y.add(p, num_nodes + b, ComplexNumber{1.0, 0.0});
            Y.add(num_nodes + b, p, ComplexNumber{1.0, 0.0});
        }
        if (q >= 0) {
            Y.add(q, num_nodes + b, ComplexNumber{-1.0, 0.0});
            Y.add(num_nodes + b, q, ComplexNumber{-1.0, 0.0});
        }

        // AC input excitation phasor
        if (vsrc->id == options_.input_source_id || vsrc->type == ElectricalComponentType::VoltageSourceAC) {
            I[num_nodes + b] = ComplexNumber{options_.ac_input_amplitude, 0.0};
        } else {
            I[num_nodes + b] = ComplexNumber{0.0, 0.0}; // AC ground
        }
    }

    return core::base::Result<std::pair<ComplexDenseMatrix, std::vector<ComplexNumber>>>::ok(
        std::make_pair(std::move(Y), std::move(I))
    );
}

core::base::Result<AcSimulationResult> AcSmallSignalSolver::solve_frequency_sweep(
    const ParsedSchematicTopology& topology,
    const std::vector<ElectricalComponent>& components
) const {
    AcSimulationResult result;
    result.frequencies = generate_frequencies();

    // Map non-ground nodes
    std::vector<uint32_t> sorted_indices;
    for (const auto& [node_idx, info] : topology.nodes) {
        if (!info.is_ground) {
            sorted_indices.push_back(node_idx);
            result.node_name_to_index[info.node_name] = node_idx;
        }
    }
    std::sort(sorted_indices.begin(), sorted_indices.end());

    for (uint32_t n : sorted_indices) {
        AcNodeResponse resp;
        resp.frequencies = result.frequencies;
        resp.complex_v.reserve(result.frequencies.size());
        resp.magnitude_linear.reserve(result.frequencies.size());
        resp.magnitude_db.reserve(result.frequencies.size());
        resp.phase_degrees.reserve(result.frequencies.size());
        resp.unwrapped_phase_deg.reserve(result.frequencies.size());
        result.node_responses[n] = resp;
    }

    const double v_ref = std::max(1.0e-12, options_.ac_input_amplitude);

    for (double f : result.frequencies) {
        auto sys_res = assemble_admittance_system(topology, components, f);
        if (sys_res.is_err()) {
            return core::base::Result<AcSimulationResult>::err(sys_res.error());
        }

        auto [Y, I] = std::move(sys_res.value());
        auto sol_res = Y.solve_lu(I);
        if (sol_res.is_err()) {
            return core::base::Result<AcSimulationResult>::err(sol_res.error());
        }

        const auto& x_sol = sol_res.value();
        for (size_t i = 0; i < sorted_indices.size(); ++i) {
            uint32_t node_idx = sorted_indices[i];
            ComplexNumber v = x_sol[i];
            double mag = std::abs(v);
            double mag_db = 20.0 * std::log10(std::max(1.0e-18, mag / v_ref));
            double phase_deg = std::arg(v) * (180.0 / std::numbers::pi);

            auto& resp = result.node_responses[node_idx];
            resp.complex_v.push_back(v);
            resp.magnitude_linear.push_back(mag);
            resp.magnitude_db.push_back(mag_db);
            resp.phase_degrees.push_back(phase_deg);

            // Phase unwrapping
            if (resp.unwrapped_phase_deg.empty()) {
                resp.unwrapped_phase_deg.push_back(phase_deg);
            } else {
                double prev_unwrapped = resp.unwrapped_phase_deg.back();
                double prev_phase = resp.phase_degrees[resp.phase_degrees.size() - 2];
                double diff = phase_deg - prev_phase;
                if (diff > 180.0) diff -= 360.0;
                else if (diff < -180.0) diff += 360.0;
                resp.unwrapped_phase_deg.push_back(prev_unwrapped + diff);
            }
        }
    }

    result.success = true;
    return core::base::Result<AcSimulationResult>::ok(std::move(result));
}

BodeAnalysisMetrics AcSmallSignalSolver::compute_bode_metrics(
    const AcNodeResponse& response
) {
    BodeAnalysisMetrics metrics;
    if (response.frequencies.size() < 2) return metrics;

    metrics.passband_gain_db = response.magnitude_db.front();
    const double target_3db = metrics.passband_gain_db - 3.0103;

    // 1. Cutoff frequency (3dB drop)
    for (size_t i = 0; i + 1 < response.frequencies.size(); ++i) {
        double g1 = response.magnitude_db[i];
        double g2 = response.magnitude_db[i + 1];
        if ((g1 >= target_3db && g2 <= target_3db) || (g1 <= target_3db && g2 >= target_3db)) {
            double frac = (target_3db - g1) / (g2 - g1 + 1e-18);
            double log_f1 = std::log10(response.frequencies[i]);
            double log_f2 = std::log10(response.frequencies[i + 1]);
            metrics.cutoff_frequency_hz = std::pow(10.0, log_f1 + frac * (log_f2 - log_f1));
            metrics.has_cutoff = true;
            break;
        }
    }

    // 2. Gain Crossover (0 dB gain) -> Phase Margin
    for (size_t i = 0; i + 1 < response.frequencies.size(); ++i) {
        double g1 = response.magnitude_db[i];
        double g2 = response.magnitude_db[i + 1];
        if ((g1 >= 0.0 && g2 <= 0.0) || (g1 <= 0.0 && g2 >= 0.0)) {
            double frac = (0.0 - g1) / (g2 - g1 + 1e-18);
            double log_f1 = std::log10(response.frequencies[i]);
            double log_f2 = std::log10(response.frequencies[i + 1]);
            metrics.gain_crossover_freq_hz = std::pow(10.0, log_f1 + frac * (log_f2 - log_f1));
            metrics.has_gain_crossover = true;

            // Interpolate phase
            double p1 = response.phase_degrees[i];
            double p2 = response.phase_degrees[i + 1];
            double phase_at_cross = p1 + frac * (p2 - p1);
            metrics.phase_margin_deg = 180.0 + phase_at_cross;
            break;
        }
    }

    // 3. Phase Crossover (-180 deg phase) -> Gain Margin
    for (size_t i = 0; i + 1 < response.frequencies.size(); ++i) {
        double p1 = response.phase_degrees[i];
        double p2 = response.phase_degrees[i + 1];
        if ((p1 >= -180.0 && p2 <= -180.0) || (p1 <= -180.0 && p2 >= -180.0)) {
            double frac = (-180.0 - p1) / (p2 - p1 + 1e-18);
            double log_f1 = std::log10(response.frequencies[i]);
            double log_f2 = std::log10(response.frequencies[i + 1]);
            metrics.phase_crossover_freq_hz = std::pow(10.0, log_f1 + frac * (log_f2 - log_f1));
            metrics.has_phase_crossover = true;

            // Interpolate gain
            double g1 = response.magnitude_db[i];
            double g2 = response.magnitude_db[i + 1];
            double gain_at_cross = g1 + frac * (g2 - g1);
            metrics.gain_margin_db = -gain_at_cross;
            break;
        }
    }

    return metrics;
}

RlcResonanceMetrics AcSmallSignalSolver::analyze_rlc_resonance(
    const std::vector<double>& frequencies,
    const std::vector<ComplexNumber>& input_impedances
) {
    RlcResonanceMetrics metrics;
    if (frequencies.size() < 2 || frequencies.size() != input_impedances.size()) return metrics;

    // Scan for zero-crossing of imaginary part (reactance Im(Z) = 0)
    for (size_t i = 0; i + 1 < frequencies.size(); ++i) {
        double x1 = input_impedances[i].imag();
        double x2 = input_impedances[i + 1].imag();

        if ((x1 <= 0.0 && x2 >= 0.0) || (x1 >= 0.0 && x2 <= 0.0)) {
            double frac = (0.0 - x1) / (x2 - x1 + 1e-18);
            double log_f1 = std::log10(frequencies[i]);
            double log_f2 = std::log10(frequencies[i + 1]);
            metrics.resonant_frequency_hz = std::pow(10.0, log_f1 + frac * (log_f2 - log_f1));
            metrics.resonance_detected = true;

            double r1 = input_impedances[i].real();
            double r2 = input_impedances[i + 1].real();
            metrics.impedance_at_resonance_ohms = r1 + frac * (r2 - r1);
            break;
        }
    }

    // Estimate Q factor based on minimum/maximum impedance bandwidth
    if (metrics.resonance_detected && metrics.impedance_at_resonance_ohms > 0.0) {
        double r0 = metrics.impedance_at_resonance_ohms;
        double target_z = r0 * std::sqrt(2.0); // 3dB bandwidth for series resonance
        double f_low = metrics.resonant_frequency_hz;
        double f_high = metrics.resonant_frequency_hz;

        for (size_t i = 0; i < frequencies.size(); ++i) {
            if (frequencies[i] < metrics.resonant_frequency_hz && std::abs(input_impedances[i]) <= target_z) {
                f_low = frequencies[i];
                break;
            }
        }
        for (size_t i = frequencies.size(); i-- > 0;) {
            if (frequencies[i] > metrics.resonant_frequency_hz && std::abs(input_impedances[i]) <= target_z) {
                f_high = frequencies[i];
                break;
            }
        }
        metrics.bandwidth_3db_hz = std::max(1e-6, f_high - f_low);
        metrics.quality_factor_q = metrics.resonant_frequency_hz / metrics.bandwidth_3db_hz;
    }

    return metrics;
}

ImpedanceMatchingReport AcSmallSignalSolver::calculate_impedance_matching(
    double frequency_hz,
    ComplexNumber z_in,
    double z0_ref
) {
    ImpedanceMatchingReport rep;
    rep.frequency_hz = frequency_hz;
    rep.z_in = z_in;
    rep.z_matched_load = std::conj(z_in); // Maximum power transfer condition Z_L = Z_in*

    const ComplexNumber z0{std::max(1e-6, z0_ref), 0.0};
    rep.reflection_coefficient = (z_in - z0) / (z_in + z0);
    double gamma_mag = std::abs(rep.reflection_coefficient);

    rep.return_loss_db = -20.0 * std::log10(std::clamp(gamma_mag, 1.0e-9, 1.0));
    if (gamma_mag < 0.99999) {
        rep.vswr = (1.0 + gamma_mag) / (1.0 - gamma_mag);
    } else {
        rep.vswr = 1000.0;
    }

    return rep;
}

BodePlotSeries AcSmallSignalSolver::export_for_scientific_plot(
    const AcNodeResponse& response,
    std::string_view node_name,
    bool log_frequency_axis
) {
    BodePlotSeries series;
    series.node_name = std::string(node_name);

    if (response.frequencies.empty()) return series;

    double x_min = log_frequency_axis ? std::log10(response.frequencies.front()) : response.frequencies.front();
    double x_max = log_frequency_axis ? std::log10(response.frequencies.back()) : response.frequencies.back();

    double mag_min = 1e9, mag_max = -1e9;
    double phase_min = 1e9, phase_max = -1e9;

    for (size_t i = 0; i < response.frequencies.size(); ++i) {
        double f = response.frequencies[i];
        double x = log_frequency_axis ? std::log10(f) : f;

        double y_mag = response.magnitude_db[i];
        double y_phase = response.phase_degrees[i];

        if (y_mag < mag_min) mag_min = y_mag;
        if (y_mag > mag_max) mag_max = y_mag;
        if (y_phase < phase_min) phase_min = y_phase;
        if (y_phase > phase_max) phase_max = y_phase;

        series.magnitude_series.push_back(core::domain::PlotPoint2D{x, y_mag});
        series.phase_series.push_back(core::domain::PlotPoint2D{x, y_phase});
    }

    series.magnitude_range = core::domain::PlotRange{
        .x_min = x_min,
        .x_max = x_max,
        .y_min = mag_min - 2.0,
        .y_max = mag_max + 2.0
    };

    series.phase_range = core::domain::PlotRange{
        .x_min = x_min,
        .x_max = x_max,
        .y_min = phase_min - 5.0,
        .y_max = phase_max + 5.0
    };

    return series;
}

} // namespace shunya::cirsim
