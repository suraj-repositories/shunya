#pragma once

#include "shunya/core/base/error.hpp"
#include "shunya/mat/bond_model.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace shunya::mat {

struct AtomNode {
    size_t id{0};
    int atomic_number{6};
    int formal_charge{0};
    int lone_pairs{0};
    Hybridization hybridization{Hybridization::Unknown};
    std::string label;
};

/**
 * @brief Rules engine verifying formal valence caps, octet constraints, and allowed hypervalency.
 */
class ValenceValidator {
public:
    /**
     * @brief Maximum permitted bond order sum for an element at a given formal charge.
     */
    [[nodiscard]] static int max_allowed_valence(int atomic_number, int formal_charge) noexcept;

    /**
     * @brief Canonical neutral target bond order sum (when formal_charge == 0).
     */
    [[nodiscard]] static int typical_valence(int atomic_number) noexcept;

    /**
     * @brief Estimates lone pairs on an atom given atomic number, formal charge, and bond order sum.
     */
    [[nodiscard]] static int estimate_lone_pairs(int atomic_number, int formal_charge, double bond_order_sum) noexcept;

    [[nodiscard]] static core::base::ValidationReport validate_atom(const AtomNode& atom,
                                                                    double bond_order_sum);
};

} // namespace shunya::mat
