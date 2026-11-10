#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/matdb/isotope_repository.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace shunya::matdb {

struct NuclideNode {
    int atomic_number{92};
    int mass_number{238};
    std::string label;
    double half_life_seconds{0.0};
    bool is_stable{false};
};

struct DecayTransitionEdge {
    size_t from_index{0};
    size_t to_index{0};
    DecayMode mode{DecayMode::Alpha};
    double branching_ratio{1.0};
    int delta_z{-2};
    int delta_a{-4};
};

struct DecayChainGraph {
    std::string series_name;
    std::vector<NuclideNode> nodes;
    std::vector<DecayTransitionEdge> edges;
    size_t terminal_stable_count() const noexcept;
};

/**
 * @brief Constructs Segre-chart radioactive decay chains (U-238, Th-232, C-14) with conservation checks.
 */
class DecayChainGraphBuilder {
public:
    [[nodiscard]] static DecayChainGraph build_uranium_238_series();
    [[nodiscard]] static DecayChainGraph build_thorium_232_series();
    [[nodiscard]] static DecayChainGraph build_carbon_14_decay();
    [[nodiscard]] static core::base::Result<DecayChainGraph> build_series(std::string_view series_id);
    [[nodiscard]] static core::base::ValidationReport audit_conservation(const DecayChainGraph& graph);
};

} // namespace shunya::matdb
