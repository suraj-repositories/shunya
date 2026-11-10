#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/mat/bond_model.hpp"
#include "shunya/mat/valence_validator.hpp"

#include <string>
#include <vector>

namespace shunya::mat {

/**
 * @brief Adjacency-list molecular topology storing typed atoms, covalent/ionic bonds, and hybridization.
 */
class MolecularGraph {
public:
    MolecularGraph() = default;
    explicit MolecularGraph(std::string name) : name_(std::move(name)) {}

    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    void set_name(std::string name) { name_ = std::move(name); }

    size_t add_atom(int atomic_number, int formal_charge = 0, Hybridization hyb = Hybridization::Unknown,
                    std::string label = "");
    core::base::Result<void> add_bond(size_t atom_a, size_t atom_b, BondOrder order = BondOrder::Single);

    [[nodiscard]] size_t atom_count() const noexcept { return atoms_.size(); }
    [[nodiscard]] size_t bond_count() const noexcept { return bonds_.size(); }
    [[nodiscard]] const std::vector<AtomNode>& atoms() const noexcept { return atoms_; }
    [[nodiscard]] const std::vector<ChemicalBond>& bonds() const noexcept { return bonds_; }
    [[nodiscard]] const std::vector<size_t>& incident_bond_indices(size_t atom_id) const;
    [[nodiscard]] std::vector<size_t> bonded_neighbors(size_t atom_id) const;

    [[nodiscard]] double atom_bond_order_sum(size_t atom_id) const noexcept;
    [[nodiscard]] int steric_number(size_t atom_id) const noexcept;
    [[nodiscard]] Hybridization effective_hybridization(size_t atom_id) const noexcept;
    [[nodiscard]] double molecular_weight_amu() const noexcept;

    [[nodiscard]] core::base::ValidationReport validate_valences() const;

    // Canonical benchmark molecule builders
    [[nodiscard]] static MolecularGraph make_methane();
    [[nodiscard]] static MolecularGraph make_water();
    [[nodiscard]] static MolecularGraph make_ammonia();
    [[nodiscard]] static MolecularGraph make_benzene();
    [[nodiscard]] static MolecularGraph make_sulfuric_acid();
    [[nodiscard]] static MolecularGraph make_sulfur_hexafluoride();

private:
    std::string name_{"Molecule"};
    std::vector<AtomNode> atoms_;
    std::vector<ChemicalBond> bonds_;
    std::vector<std::vector<size_t>> adjacency_;
};

} // namespace shunya::mat
