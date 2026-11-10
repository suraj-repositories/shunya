#include "shunya/pcb/board_stackup.hpp"

#include <cmath>

namespace shunya::pcb {

namespace {

core::base::Error make_st_error(std::string msg) {
    return core::base::Error{
        .code = 5101,
        .domain = core::base::ErrorDomain::Validation,
        .severity = core::base::Severity::Error,
        .message = std::move(msg),
        .details = ""
    };
}

bool is_copper(StackupLayerKind k) noexcept {
    return k == StackupLayerKind::Signal || k == StackupLayerKind::PowerPlane ||
           k == StackupLayerKind::GroundPlane || k == StackupLayerKind::Mixed;
}

} // namespace

core::base::Result<double> ImpedanceCalculator::microstrip_z0_ipc2141(const MicrostripParams& p) {
    if (p.trace_width_mm <= 0.0 || p.dielectric_height_mm <= 0.0 || p.epsilon_r < 1.0) {
        return make_st_error("Invalid microstrip physical dimensions or permittivity");
    }
    const double denom = 0.8 * p.trace_width_mm + p.trace_thickness_mm;
    const double arg = (5.98 * p.dielectric_height_mm) / denom;
    if (arg <= 1.0) {
        return make_st_error("Microstrip trace too wide relative to dielectric height for IPC-2141 domain");
    }
    return (87.0 / std::sqrt(p.epsilon_r + 1.41)) * std::log(arg);
}

core::base::Result<double> ImpedanceCalculator::stripline_z0_ipc2141(const StriplineParams& p) {
    if (p.trace_width_mm <= 0.0 || p.plane_spacing_mm <= 0.0 || p.epsilon_r < 1.0) {
        return make_st_error("Invalid stripline parameters");
    }
    constexpr double kPi = 3.14159265358979323846;
    const double denom = 0.67 * kPi * (0.8 * p.trace_width_mm + p.trace_thickness_mm);
    const double arg = (4.0 * p.plane_spacing_mm) / denom;
    if (arg <= 1.0) {
        return make_st_error("Stripline trace geometry outside IPC-2141 validity range");
    }
    return (60.0 / std::sqrt(p.epsilon_r)) * std::log(arg);
}

core::base::Result<double> ImpedanceCalculator::solve_microstrip_width_mm(
    double target_z0_ohm,
    double trace_thickness_mm,
    double dielectric_height_mm,
    double epsilon_r) {
    if (target_z0_ohm <= 5.0 || dielectric_height_mm <= 0.0 || epsilon_r < 1.0) {
        return make_st_error("Invalid target impedance or substrate parameters");
    }
    double lo = 0.02;
    double hi = 20.0;
    for (int i = 0; i < 64; ++i) {
        const double mid = 0.5 * (lo + hi);
        auto z = microstrip_z0_ipc2141({mid, trace_thickness_mm, dielectric_height_mm, epsilon_r});
        if (!z) {
            hi = mid;
            continue;
        }
        if (z.value() > target_z0_ohm) lo = mid;
        else hi = mid;
    }
    return 0.5 * (lo + hi);
}

core::base::Result<void> BoardStackupManager::add_layer(StackupLayer layer) {
    if (layer.thickness_mm <= 0.0) {
        return make_st_error("Layer thickness must be strictly positive");
    }
    if (is_copper(layer.kind) && copper_layer_count() >= 32) {
        return make_st_error("Maximum 32 copper layers supported");
    }
    layers_.push_back(std::move(layer));
    return {};
}

core::base::Result<void> BoardStackupManager::validate() const {
    const int cu = copper_layer_count();
    if (cu < 1 || cu > 32) {
        return make_st_error("Copper layer count must be between 1 and 32");
    }
    return {};
}

int BoardStackupManager::copper_layer_count() const noexcept {
    int count = 0;
    for (const auto& l : layers_) {
        if (is_copper(l.kind)) ++count;
    }
    return count;
}

double BoardStackupManager::total_thickness_mm() const noexcept {
    double sum = 0.0;
    for (const auto& l : layers_) sum += l.thickness_mm;
    return sum;
}

core::base::Result<double> BoardStackupManager::compute_top_microstrip_z0(double trace_width_mm) const {
    const StackupLayer* top_cu = nullptr;
    const StackupLayer* diel = nullptr;
    for (const auto& l : layers_) {
        if (!top_cu && is_copper(l.kind)) {
            top_cu = &l;
            continue;
        }
        if (top_cu && !diel &&
            (l.kind == StackupLayerKind::DielectricCore || l.kind == StackupLayerKind::DielectricPrepreg)) {
            diel = &l;
            break;
        }
    }
    if (!top_cu || !diel) {
        return make_st_error("Stackup does not contain a Top signal + adjacent dielectric pair");
    }
    return ImpedanceCalculator::microstrip_z0_ipc2141({
        trace_width_mm, top_cu->thickness_mm, diel->thickness_mm, diel->epsilon_r
    });
}

BoardStackupManager BoardStackupManager::standard_2layer_fr4() {
    BoardStackupManager mgr;
    (void)mgr.add_layer({"F.Cu",    StackupLayerKind::Signal,         0.035, 1.0, 1.0, 0.0});
    (void)mgr.add_layer({"Core FR4",StackupLayerKind::DielectricCore, 1.530, 0.0, 4.4, 0.02});
    (void)mgr.add_layer({"B.Cu",    StackupLayerKind::Signal,         0.035, 1.0, 1.0, 0.0});
    return mgr;
}

BoardStackupManager BoardStackupManager::standard_4layer_fr4() {
    BoardStackupManager mgr;
    (void)mgr.add_layer({"F.Cu",      StackupLayerKind::Signal,            0.035, 1.0, 1.0, 0.0});
    (void)mgr.add_layer({"Prepreg 1", StackupLayerKind::DielectricPrepreg, 0.200, 0.0, 4.4, 0.02});
    (void)mgr.add_layer({"In1.GND",   StackupLayerKind::GroundPlane,       0.035, 1.0, 1.0, 0.0});
    (void)mgr.add_layer({"Core FR4",  StackupLayerKind::DielectricCore,    1.060, 0.0, 4.4, 0.02});
    (void)mgr.add_layer({"In2.PWR",   StackupLayerKind::PowerPlane,        0.035, 1.0, 1.0, 0.0});
    (void)mgr.add_layer({"Prepreg 2", StackupLayerKind::DielectricPrepreg, 0.200, 0.0, 4.4, 0.02});
    (void)mgr.add_layer({"B.Cu",      StackupLayerKind::Signal,            0.035, 1.0, 1.0, 0.0});
    return mgr;
}

} // namespace shunya::pcb
