#pragma once

namespace shunya::mat {

struct MetalResistivityParams {
    double rho_0_ohm_m{1.68e-8};
    double ref_temp_k{293.15};
    double alpha_per_k{0.00393};
};

struct SemiconductorVarshniParams {
    double eg0_ev{1.1695};
    double alpha_ev_k{4.73e-4};
    double beta_k{636.0};
    double nc_300_cm3{2.86e19};
    double nv_300_cm3{3.10e19};
};

} // namespace shunya::mat
