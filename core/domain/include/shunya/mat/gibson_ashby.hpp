#pragma once

#include <cstdint>

namespace shunya::mat {

enum class CellularMorphology : uint8_t {
    OpenCell,
    ClosedCell
};

struct GibsonAshbyConstants {
    double c1_modulus{1.0};
    double c1_membrane{1.0};
    double c2_yield{0.30};
    double solid_strut_fraction{0.80}; // Used for closed-cell face/strut split (phi)
};

} // namespace shunya::mat
