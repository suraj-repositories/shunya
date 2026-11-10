#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/mat/molecular_graph.hpp"

#include <string_view>

namespace shunya::mat {

/**
 * @brief Parses SMILES (Simplified Molecular Input Line Entry System) strings into MolecularGraph topologies.
 */
class SmilesParser {
public:
    [[nodiscard]] static core::base::Result<MolecularGraph> parse(std::string_view smiles,
                                                                  bool add_implicit_hydrogens = true);
};

} // namespace shunya::mat
