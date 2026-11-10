#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace shunya::mat {

enum class Hybridization : uint8_t {
    Unknown,
    Sp,
    Sp2,
    Sp3,
    Sp3d,
    Sp3d2
};

enum class BondOrder : uint8_t {
    Single,
    Double,
    Triple,
    Aromatic,
    Ionic,
    Metallic
};

[[nodiscard]] constexpr double bond_order_value(BondOrder order) noexcept {
    switch (order) {
    case BondOrder::Single:   return 1.0;
    case BondOrder::Double:   return 2.0;
    case BondOrder::Triple:   return 3.0;
    case BondOrder::Aromatic: return 1.5;
    case BondOrder::Ionic:    return 1.0;
    case BondOrder::Metallic: return 1.0;
    }
    return 1.0;
}

[[nodiscard]] constexpr std::string_view hybridization_to_string(Hybridization h) noexcept {
    switch (h) {
    case Hybridization::Unknown: return "Unknown";
    case Hybridization::Sp:      return "sp";
    case Hybridization::Sp2:     return "sp2";
    case Hybridization::Sp3:     return "sp3";
    case Hybridization::Sp3d:    return "sp3d";
    case Hybridization::Sp3d2:   return "sp3d2";
    }
    return "Unknown";
}

struct ChemicalBond {
    size_t atom_a{0};
    size_t atom_b{0};
    BondOrder order{BondOrder::Single};
};

} // namespace shunya::mat
