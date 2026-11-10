#pragma once

#include <cstdint>

namespace shunya::pcb {

enum class DensityLevel : uint8_t {
    Most_A,    // Level A: Maximum land protrusion
    Nominal_B, // Level B: Median nominal (IPC-7351 default)
    Least_C    // Level C: Minimum high-density
};

struct Ipc7351Fillets {
    double toe_mm{0.35};
    double heel_mm{0.35};
    double side_mm{0.03};
    double courtyard_excess_mm{0.25};
};

[[nodiscard]] constexpr Ipc7351Fillets ipc7351_gullwing_fillets(DensityLevel lvl, double pitch_mm) noexcept {
    // IPC-7351B Gull-Wing Leads (pitch >= 0.65mm vs fine pitch < 0.65mm)
    const bool fine_pitch = (pitch_mm < 0.63);
    switch (lvl) {
    case DensityLevel::Most_A:
        return fine_pitch ? Ipc7351Fillets{0.55, 0.45, 0.01, 0.50} : Ipc7351Fillets{0.55, 0.45, 0.05, 0.50};
    case DensityLevel::Nominal_B:
        return fine_pitch ? Ipc7351Fillets{0.35, 0.35, -0.02, 0.25} : Ipc7351Fillets{0.35, 0.35, 0.03, 0.25};
    case DensityLevel::Least_C:
        return fine_pitch ? Ipc7351Fillets{0.15, 0.25, -0.04, 0.10} : Ipc7351Fillets{0.15, 0.25, 0.01, 0.10};
    }
    return {0.35, 0.35, 0.03, 0.25};
}

[[nodiscard]] constexpr Ipc7351Fillets ipc7351_chip_fillets(DensityLevel lvl) noexcept {
    // IPC-7351B Two-Terminal Chip Capacitors / Resistors (>= 0402)
    switch (lvl) {
    case DensityLevel::Most_A:    return {0.40, 0.00, 0.05, 0.50};
    case DensityLevel::Nominal_B: return {0.30, 0.00, 0.00, 0.25};
    case DensityLevel::Least_C:   return {0.20, 0.00, -0.05, 0.10};
    }
    return {0.30, 0.00, 0.00, 0.25};
}

} // namespace shunya::pcb
