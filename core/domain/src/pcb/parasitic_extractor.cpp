#include "shunya/pcb/parasitic_extractor.hpp"
#include "shunya/pcb/impedance_calculator.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::pcb {

namespace {

constexpr double kPi = 3.14159265358979323846;

double segment_length_mm(const PcbTrackSegment& seg) noexcept {
    const double dx = seg.end_mm.x - seg.start_mm.x;
    const double dy = seg.end_mm.y - seg.start_mm.y;
    return std::hypot(dx, dy);
}

} // namespace

double ParasiticExtractor::copper_thickness_mm_from_oz(double oz) noexcept {
    return std::max(0.0, oz) * 0.035;
}

double ParasiticExtractor::copper_resistivity_ohm_m(double temp_c) noexcept {
    return kCopperRho20C * (1.0 + kCopperTempCoeff * (temp_c - 20.0));
}

core::base::Result<TraceParasitics> ParasiticExtractor::extract_track(
    const PcbTrackSegment& segment,
    double copper_thickness_mm,
    double dielectric_height_mm,
    double epsilon_r,
    double temp_c) {
    if (segment.width_mm <= 0.0 || copper_thickness_mm <= 0.0 ||
        dielectric_height_mm <= 0.0 || epsilon_r < 1.0) {
        return core::base::Result<TraceParasitics>::err(
            core::base::Error{1, core::base::ErrorDomain::Validation, core::base::Severity::Error,
                              "Trace parasitic parameters must be positive (width, thickness, h > 0, er >= 1)", ""});
    }

    const double len_mm = segment_length_mm(segment);
    if (len_mm <= 0.0) {
        TraceParasitics empty;
        empty.net_name = segment.net_name;
        empty.width_mm = segment.width_mm;
        empty.thickness_mm = copper_thickness_mm;
        return core::base::Result<TraceParasitics>::ok(empty);
    }

    // DC trace resistance: R = rho(T) * L / (W * t)
    const double rho = copper_resistivity_ohm_m(temp_c);
    const double len_m = len_mm * 1e-3;
    const double area_m2 = (segment.width_mm * 1e-3) * (copper_thickness_mm * 1e-3);
    const double r_ohm = rho * len_m / area_m2;

    // Effective microstrip permittivity & IPC-2141 characteristic impedance
    MicrostripParams mp;
    mp.trace_width_mm = segment.width_mm;
    mp.trace_thickness_mm = copper_thickness_mm;
    mp.dielectric_height_mm = dielectric_height_mm;
    mp.epsilon_r = epsilon_r;

    auto z0_res = ImpedanceCalculator::microstrip_z0_ipc2141(mp);
    const double z0 = z0_res.is_ok() ? z0_res.value() : 50.0;

    const double w_eff = segment.width_mm + 1.25 * copper_thickness_mm / kPi *
        (1.0 + std::log(2.0 * dielectric_height_mm / copper_thickness_mm));
    const double u = w_eff / dielectric_height_mm;
    const double eps_eff = 0.5 * (epsilon_r + 1.0) +
                           0.5 * (epsilon_r - 1.0) / std::sqrt(1.0 + 12.0 / std::max(0.05, u));

    // Distributed lossless transmission-line propagation velocity: v_p = c / sqrt(eps_eff)
    const double vp_mm_s = kSpeedOfLightMmS / std::sqrt(eps_eff);
    const double t_pd_s = len_mm / vp_mm_s;

    // C_total = t_pd / Z0 (F -> pF), L_total = t_pd * Z0 (H -> nH)
    const double c_pf = (t_pd_s / z0) * 1e12;
    const double l_nh = (t_pd_s * z0) * 1e9;

    TraceParasitics out;
    out.net_name = segment.net_name;
    out.length_mm = len_mm;
    out.width_mm = segment.width_mm;
    out.thickness_mm = copper_thickness_mm;
    out.resistance_ohm = r_ohm;
    out.inductance_nh = l_nh;
    out.capacitance_pf = c_pf;
    out.impedance_ohm = z0;
    return core::base::Result<TraceParasitics>::ok(out);
}

core::base::Result<ViaParasitics> ParasiticExtractor::extract_via(
    const PcbVia& via,
    double board_thickness_mm,
    double plating_thickness_mm,
    double epsilon_r,
    double temp_c) {
    if (via.drill_mm <= 0.0 || board_thickness_mm <= 0.0 || plating_thickness_mm <= 0.0) {
        return core::base::Result<ViaParasitics>::err(
            core::base::Error{2, core::base::ErrorDomain::Validation, core::base::Severity::Error,
                              "Via drill, board thickness, and plating thickness must be positive", ""});
    }

    const double r_in_m = (via.drill_mm * 0.5) * 1e-3;
    const double r_out_m = r_in_m + plating_thickness_mm * 1e-3;
    const double h_m = board_thickness_mm * 1e-3;
    const double barrel_area_m2 = kPi * (r_out_m * r_out_m - r_in_m * r_in_m);
    const double rho = copper_resistivity_ohm_m(temp_c);
    const double r_via = rho * h_m / barrel_area_m2;

    // Goldfarb / Bogatin cylindrical via self-inductance (SI ~ mu0/(2pi) * h * [ln(4h/d) + 1])
    const double ratio = std::max(1.1, 4.0 * board_thickness_mm / via.drill_mm);
    const double l_h = 2.0e-7 * h_m * (std::log(ratio) + 1.0);
    const double l_nh = l_h * 1e9;

    // Excess pad-to-antipad capacitance approximation (pF)
    const double antipad_mm = via.pad_diameter_mm * 1.6;
    const double c_pf = std::max(
        0.05,
        1.41 * epsilon_r * board_thickness_mm * via.pad_diameter_mm /
            std::max(0.1, antipad_mm - via.pad_diameter_mm));

    ViaParasitics out;
    out.net_name = via.net_name;
    out.drill_diameter_mm = via.drill_mm;
    out.board_thickness_mm = board_thickness_mm;
    out.resistance_ohm = r_via;
    out.inductance_nh = l_nh;
    out.capacitance_pf = c_pf;
    return core::base::Result<ViaParasitics>::ok(out);
}

std::unordered_map<std::string, NetParasiticSummary> ParasiticExtractor::extract_board_nets(
    const std::vector<PcbTrackSegment>& tracks,
    const std::vector<PcbVia>& vias,
    const BoardStackupManager& stackup,
    double temp_c) {
    double cu_thick_mm = 0.035;
    double diel_h_mm = 0.20;
    double er = 4.4;

    bool found_top_cu = false;
    for (const auto& layer : stackup.layers()) {
        if (!found_top_cu &&
            (layer.kind == StackupLayerKind::Signal ||
             layer.kind == StackupLayerKind::Mixed ||
             layer.kind == StackupLayerKind::PowerPlane ||
             layer.kind == StackupLayerKind::GroundPlane)) {
            cu_thick_mm = (layer.copper_oz > 0.0) ? copper_thickness_mm_from_oz(layer.copper_oz)
                                                  : layer.thickness_mm;
            found_top_cu = true;
        } else if (found_top_cu &&
                   (layer.kind == StackupLayerKind::DielectricPrepreg ||
                    layer.kind == StackupLayerKind::DielectricCore)) {
            diel_h_mm = std::max(0.02, layer.thickness_mm);
            er = std::max(1.0, layer.epsilon_r);
            break;
        }
    }

    const double board_thick_mm = std::max(0.2, stackup.total_thickness_mm());
    std::unordered_map<std::string, NetParasiticSummary> result;

    for (const auto& seg : tracks) {
        if (seg.net_name.empty()) continue;
        auto ext = extract_track(seg, cu_thick_mm, diel_h_mm, er, temp_c);
        if (!ext.is_ok()) continue;
        auto& s = result[seg.net_name];
        s.net_name = seg.net_name;
        s.total_length_mm += ext.value().length_mm;
        s.total_resistance_ohm += ext.value().resistance_ohm;
        s.total_inductance_nh += ext.value().inductance_nh;
        s.total_capacitance_pf += ext.value().capacitance_pf;
        s.segment_count += 1;
    }

    for (const auto& via : vias) {
        if (via.net_name.empty()) continue;
        auto ext = extract_via(via, board_thick_mm, 0.025, er, temp_c);
        if (!ext.is_ok()) continue;
        auto& s = result[via.net_name];
        s.net_name = via.net_name;
        s.total_resistance_ohm += ext.value().resistance_ohm;
        s.total_inductance_nh += ext.value().inductance_nh;
        s.total_capacitance_pf += ext.value().capacitance_pf;
        s.via_count += 1;
    }

    return result;
}

} // namespace shunya::pcb
