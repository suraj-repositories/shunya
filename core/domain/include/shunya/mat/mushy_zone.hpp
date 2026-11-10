#pragma once

#include <cstdint>

namespace shunya::mat {

enum class MushyFractionProfile : uint8_t {
    Linear,
    SmoothStepCubic
};

struct LatentPhaseParameters {
    double solidus_temp_k{272.65};
    double liquidus_temp_k{273.65};
    double cp_solid_j_kg_k{2108.0};
    double cp_liquid_j_kg_k{4184.0};
    double latent_heat_fusion_j_kg{334000.0};
    double k_solid_w_mk{2.22};
    double solid_density_kg_m3{917.0};
    MushyFractionProfile profile{MushyFractionProfile::SmoothStepCubic};
};

} // namespace shunya::mat
