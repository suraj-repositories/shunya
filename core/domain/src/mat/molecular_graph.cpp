#include "shunya/mat/molecular_graph.hpp"

#include "shunya/matdb/elements_118_catalog.hpp"

#include <cmath>
#include <stdexcept>

namespace shunya::mat {

namespace {

core::base::Error make_mat_error(std::string msg) {
    return core::base::Error{
        .code = 4201,
        .domain = core::base::ErrorDomain::Materials,
        .severity = core::base::Severity::Error,
        .message = std::move(msg),
        .details = ""
    };
}

} // namespace

int ValenceValidator::typical_valence(int atomic_number) noexcept {
    switch (atomic_number) {
    case 1:  return 1; // H
    case 2:  return 0; // He
    case 3:  return 1; // Li
    case 4:  return 2; // Be
    case 5:  return 3; // B
    case 6:  return 4; // C
    case 7:  return 3; // N
    case 8:  return 2; // O
    case 9:  return 1; // F
    case 10: return 0; // Ne
    case 11: return 1; // Na
    case 12: return 2; // Mg
    case 13: return 3; // Al
    case 14: return 4; // Si
    case 15: return 3; // P (or 5)
    case 16: return 2; // S (or 4, 6)
    case 17: return 1; // Cl
    case 35: return 1; // Br
    case 53: return 1; // I
    default: return 4;
    }
}

int ValenceValidator::max_allowed_valence(int atomic_number, int formal_charge) noexcept {
    switch (atomic_number) {
    case 1:  return (formal_charge == 0) ? 1 : 0;   // H: max 1 covalent bond
    case 2:
    case 10:
    case 18: return 0;                              // Noble gases
    case 5:  return (formal_charge == -1) ? 4 : 3;  // B: BF4- allowed when -1
    case 6:  return 4;                              // C: strictly capped at 4 (prevents 5-valent C)
    case 7:  return (formal_charge > 0) ? 4 : 3;    // N: 4 in NH4+/NO2+, max 3 neutral
    case 8:  return (formal_charge > 0) ? 3 : 2;    // O: 3 in H3O+, max 2 neutral
    case 9:  return (formal_charge == 0) ? 1 : 0;   // F: strictly 1 neutral (prevents 2-valent F)
    case 14: return 6;                              // Si: hexacoordinate allowed (SiF6^2-)
    case 15: return 5;                              // P: pentavalent hypervalence (PF5, H3PO4)
    case 16: return 6;                              // S: hexavalent hypervalence (SF6, H2SO4)
    case 17:
    case 35:
    case 53: return 7;                              // Heavy halogens: hypervalent oxyacids/IF7
    default: return 6;
    }
}

int ValenceValidator::estimate_lone_pairs(int atomic_number,
                                            int formal_charge,
                                            double bond_order_sum) noexcept {
    const auto* elem = matdb::Elements118Catalog::find_by_z(atomic_number);
    if (!elem) return 0;
    int valence_e = 0;
    if (atomic_number == 1) valence_e = 1;
    else if (elem->group >= 1 && elem->group <= 2) valence_e = elem->group;
    else if (elem->group >= 13 && elem->group <= 18) valence_e = elem->group - 10;
    else return 0;

    const int rem = valence_e - formal_charge - static_cast<int>(std::round(bond_order_sum));
    if (rem < 2) return 0;
    return rem / 2;
}

core::base::ValidationReport ValenceValidator::validate_atom(const AtomNode& atom,
                                                             double bond_order_sum) {
    core::base::ValidationReport report;
    const std::string path = "Atom[" + std::to_string(atom.id) + "](Z=" +
                             std::to_string(atom.atomic_number) + ")";
    const int max_v = max_allowed_valence(atom.atomic_number, atom.formal_charge);
    if (bond_order_sum > static_cast<double>(max_v) + 1e-6) {
        report.add_error(path, "Hypervalent/octet violation: bond order sum " +
                               std::to_string(bond_order_sum) +
                               " exceeds maximum allowed valence " + std::to_string(max_v));
    }
    if (atom.atomic_number <= 10) {
        // Strict octet electron count <= 8 (or <= 2 for H/He)
        const int lone_p = (atom.lone_pairs > 0)
                               ? atom.lone_pairs
                               : estimate_lone_pairs(atom.atomic_number, atom.formal_charge, bond_order_sum);
        const int electron_count = 2 * static_cast<int>(std::round(bond_order_sum)) + 2 * lone_p;
        const int limit = (atom.atomic_number <= 2) ? 2 : 8;
        if (electron_count > limit) {
            report.add_error(path, "Period 1/2 octet violation (total valence electrons = " +
                                   std::to_string(electron_count) + ")");
        }
    }
    return report;
}

size_t MolecularGraph::add_atom(int atomic_number,
                                int formal_charge,
                                Hybridization hyb,
                                std::string label) {
    const size_t id = atoms_.size();
    if (label.empty()) {
        const auto* e = matdb::Elements118Catalog::find_by_z(atomic_number);
        label = (e ? e->symbol : "X") + std::to_string(id + 1);
    }
    atoms_.push_back(AtomNode{id, atomic_number, formal_charge, 0, hyb, std::move(label)});
    adjacency_.emplace_back();
    return id;
}

core::base::Result<void> MolecularGraph::add_bond(size_t atom_a, size_t atom_b, BondOrder order) {
    if (atom_a >= atoms_.size() || atom_b >= atoms_.size()) {
        return make_mat_error("Bond endpoint atom index out of range");
    }
    if (atom_a == atom_b) {
        return make_mat_error("Self-loop bond is not permitted");
    }
    for (size_t b_idx : adjacency_[atom_a]) {
        const auto& b = bonds_[b_idx];
        if ((b.atom_a == atom_a && b.atom_b == atom_b) ||
            (b.atom_a == atom_b && b.atom_b == atom_a)) {
            return make_mat_error("Duplicate bond between same atoms");
        }
    }
    const size_t idx = bonds_.size();
    bonds_.push_back(ChemicalBond{atom_a, atom_b, order});
    adjacency_[atom_a].push_back(idx);
    adjacency_[atom_b].push_back(idx);
    return {};
}

const std::vector<size_t>& MolecularGraph::incident_bond_indices(size_t atom_id) const {
    static const std::vector<size_t> kEmpty;
    if (atom_id >= adjacency_.size()) return kEmpty;
    return adjacency_[atom_id];
}

std::vector<size_t> MolecularGraph::bonded_neighbors(size_t atom_id) const {
    std::vector<size_t> out;
    if (atom_id >= adjacency_.size()) return out;
    for (size_t b_idx : adjacency_[atom_id]) {
        const auto& b = bonds_[b_idx];
        out.push_back((b.atom_a == atom_id) ? b.atom_b : b.atom_a);
    }
    return out;
}

double MolecularGraph::atom_bond_order_sum(size_t atom_id) const noexcept {
    if (atom_id >= adjacency_.size()) return 0.0;
    double sum = 0.0;
    for (size_t b_idx : adjacency_[atom_id]) {
        sum += bond_order_value(bonds_[b_idx].order);
    }
    return sum;
}

int MolecularGraph::steric_number(size_t atom_id) const noexcept {
    if (atom_id >= atoms_.size()) return 0;
    const int bonded_domains = static_cast<int>(adjacency_[atom_id].size());
    const int lp = ValenceValidator::estimate_lone_pairs(
        atoms_[atom_id].atomic_number,
        atoms_[atom_id].formal_charge,
        atom_bond_order_sum(atom_id));
    return bonded_domains + lp;
}

Hybridization MolecularGraph::effective_hybridization(size_t atom_id) const noexcept {
    if (atom_id >= atoms_.size()) return Hybridization::Unknown;
    if (atoms_[atom_id].hybridization != Hybridization::Unknown) {
        return atoms_[atom_id].hybridization;
    }
    switch (steric_number(atom_id)) {
    case 2: return Hybridization::Sp;
    case 3: return Hybridization::Sp2;
    case 4: return Hybridization::Sp3;
    case 5: return Hybridization::Sp3d;
    case 6: return Hybridization::Sp3d2;
    default: return Hybridization::Unknown;
    }
}

double MolecularGraph::molecular_weight_amu() const noexcept {
    double total = 0.0;
    for (const auto& a : atoms_) {
        const auto* e = matdb::Elements118Catalog::find_by_z(a.atomic_number);
        if (e) total += e->atomic_weight_amu;
    }
    return total;
}

core::base::ValidationReport MolecularGraph::validate_valences() const {
    core::base::ValidationReport report;
    for (size_t i = 0; i < atoms_.size(); ++i) {
        double bo_sum = atom_bond_order_sum(i);
        // Delocalized fused aromatic junctions (e.g. naphthalene bridgehead C) share 1 pi-bond per atom
        int arom_count = 0;
        double non_arom_sum = 0.0;
        for (size_t b_idx : adjacency_[i]) {
            if (bonds_[b_idx].order == BondOrder::Aromatic) {
                ++arom_count;
            } else {
                non_arom_sum += bond_order_value(bonds_[b_idx].order);
            }
        }
        if (arom_count >= 2) {
            bo_sum = non_arom_sum + static_cast<double>(arom_count) + 1.0;
        }
        auto sub = ValenceValidator::validate_atom(atoms_[i], bo_sum);
        for (const auto& issue : sub.issues()) {
            report.add_issue(issue);
        }
    }
    return report;
}

MolecularGraph MolecularGraph::make_methane() {
    MolecularGraph g("Methane (CH4)");
    auto c = g.add_atom(6, 0, Hybridization::Sp3);
    for (int i = 0; i < 4; ++i) {
        auto h = g.add_atom(1);
        (void)g.add_bond(c, h, BondOrder::Single);
    }
    return g;
}

MolecularGraph MolecularGraph::make_water() {
    MolecularGraph g("Water (H2O)");
    auto o = g.add_atom(8, 0, Hybridization::Sp3);
    auto h1 = g.add_atom(1);
    auto h2 = g.add_atom(1);
    (void)g.add_bond(o, h1, BondOrder::Single);
    (void)g.add_bond(o, h2, BondOrder::Single);
    return g;
}

MolecularGraph MolecularGraph::make_ammonia() {
    MolecularGraph g("Ammonia (NH3)");
    auto n = g.add_atom(7, 0, Hybridization::Sp3);
    for (int i = 0; i < 3; ++i) {
        auto h = g.add_atom(1);
        (void)g.add_bond(n, h, BondOrder::Single);
    }
    return g;
}

MolecularGraph MolecularGraph::make_benzene() {
    MolecularGraph g("Benzene (C6H6)");
    size_t c[6];
    for (int i = 0; i < 6; ++i) {
        c[i] = g.add_atom(6, 0, Hybridization::Sp2);
    }
    for (int i = 0; i < 6; ++i) {
        (void)g.add_bond(c[i], c[(i + 1) % 6], BondOrder::Aromatic);
        auto h = g.add_atom(1);
        (void)g.add_bond(c[i], h, BondOrder::Single);
    }
    return g;
}

MolecularGraph MolecularGraph::make_sulfuric_acid() {
    MolecularGraph g("Sulfuric Acid (H2SO4)");
    auto s = g.add_atom(16, 0, Hybridization::Sp3);
    auto o_oxo1 = g.add_atom(8, 0, Hybridization::Sp2);
    auto o_oxo2 = g.add_atom(8, 0, Hybridization::Sp2);
    auto o_oh1 = g.add_atom(8, 0, Hybridization::Sp3);
    auto o_oh2 = g.add_atom(8, 0, Hybridization::Sp3);
    auto h1 = g.add_atom(1);
    auto h2 = g.add_atom(1);
    (void)g.add_bond(s, o_oxo1, BondOrder::Double);
    (void)g.add_bond(s, o_oxo2, BondOrder::Double);
    (void)g.add_bond(s, o_oh1, BondOrder::Single);
    (void)g.add_bond(s, o_oh2, BondOrder::Single);
    (void)g.add_bond(o_oh1, h1, BondOrder::Single);
    (void)g.add_bond(o_oh2, h2, BondOrder::Single);
    return g;
}

MolecularGraph MolecularGraph::make_sulfur_hexafluoride() {
    MolecularGraph g("Sulfur Hexafluoride (SF6)");
    auto s = g.add_atom(16, 0, Hybridization::Sp3d2);
    for (int i = 0; i < 6; ++i) {
        auto f = g.add_atom(9);
        (void)g.add_bond(s, f, BondOrder::Single);
    }
    return g;
}

} // namespace shunya::mat
