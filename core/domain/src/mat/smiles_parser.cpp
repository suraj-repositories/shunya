#include "shunya/mat/smiles_parser.hpp"

#include "shunya/matdb/elements_118_catalog.hpp"

#include <cctype>
#include <cmath>
#include <optional>
#include <stack>
#include <unordered_map>

namespace shunya::mat {

namespace {

core::base::Error make_smi_error(std::string msg) {
    return core::base::Error{
        .code = 4220,
        .domain = core::base::ErrorDomain::Materials,
        .severity = core::base::Severity::Error,
        .message = std::move(msg),
        .details = ""
    };
}

struct ParsedAtomToken {
    int z{6};
    int charge{0};
    bool is_aromatic{false};
    int explicit_h{-1};
};

core::base::Result<ParsedAtomToken> parse_bracket_atom(std::string_view s, size_t& pos) {
    // Assumes s[pos] == '['
    ++pos;
    ParsedAtomToken tok;
    // Skip optional isotope mass digits
    while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) ++pos;
    if (pos >= s.size()) return make_smi_error("Unclosed bracket atom '[' in SMILES");

    std::string sym;
    if (std::islower(static_cast<unsigned char>(s[pos]))) {
        tok.is_aromatic = true;
        sym.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(s[pos]))));
        ++pos;
    } else if (std::isupper(static_cast<unsigned char>(s[pos]))) {
        sym.push_back(s[pos]);
        ++pos;
        if (pos < s.size() && std::islower(static_cast<unsigned char>(s[pos]))) {
            // Do not swallow 'h' if followed by digit/charge for H? Element symbols max 2 chars and valid
            std::string two = sym + s[pos];
            if (matdb::Elements118Catalog::find_by_symbol(two) != nullptr) {
                sym = two;
                ++pos;
            }
        }
    } else {
        return make_smi_error("Invalid element symbol inside SMILES bracket atom");
    }

    const auto* e = matdb::Elements118Catalog::find_by_symbol(sym);
    if (!e) return make_smi_error("Unknown element inside bracket: " + sym);
    tok.z = e->atomic_number;

    // Optional chirality '@' and optional explicit H count 'H...'
    while (pos < s.size() && s[pos] == '@') ++pos;
    if (pos < s.size() && s[pos] == 'H') {
        ++pos;
        int hcnt = 0;
        bool digits = false;
        while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) {
            digits = true;
            hcnt = hcnt * 10 + (s[pos] - '0');
            ++pos;
        }
        tok.explicit_h = digits ? hcnt : 1;
    }
    if (pos < s.size() && (s[pos] == '+' || s[pos] == '-')) {
        int sign = (s[pos] == '+') ? 1 : -1;
        ++pos;
        int mag = 0;
        bool digits = false;
        while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) {
            digits = true;
            mag = mag * 10 + (s[pos] - '0');
            ++pos;
        }
        if (!digits) {
            mag = 1;
            while (pos < s.size() && ((sign > 0 && s[pos] == '+') || (sign < 0 && s[pos] == '-'))) {
                ++mag;
                ++pos;
            }
        }
        tok.charge = sign * mag;
    }
    while (pos < s.size() && s[pos] != ']') ++pos;
    if (pos >= s.size() || s[pos] != ']') {
        return make_smi_error("Missing closing ']' in SMILES bracket atom");
    }
    ++pos;
    return tok;
}

std::optional<ParsedAtomToken> try_parse_organic(std::string_view s, size_t& pos) {
    if (pos >= s.size()) return std::nullopt;
    if (pos + 1 < s.size()) {
        std::string_view two = s.substr(pos, 2);
        if (two == "Cl") { pos += 2; return ParsedAtomToken{17, 0, false, -1}; }
        if (two == "Br") { pos += 2; return ParsedAtomToken{35, 0, false, -1}; }
    }
    char c = s[pos];
    switch (c) {
    case 'B': ++pos; return ParsedAtomToken{5, 0, false, -1};
    case 'C': ++pos; return ParsedAtomToken{6, 0, false, -1};
    case 'N': ++pos; return ParsedAtomToken{7, 0, false, -1};
    case 'O': ++pos; return ParsedAtomToken{8, 0, false, -1};
    case 'P': ++pos; return ParsedAtomToken{15, 0, false, -1};
    case 'S': ++pos; return ParsedAtomToken{16, 0, false, -1};
    case 'F': ++pos; return ParsedAtomToken{9, 0, false, -1};
    case 'I': ++pos; return ParsedAtomToken{53, 0, false, -1};
    case 'b': ++pos; return ParsedAtomToken{5, 0, true, -1};
    case 'c': ++pos; return ParsedAtomToken{6, 0, true, -1};
    case 'n': ++pos; return ParsedAtomToken{7, 0, true, -1};
    case 'o': ++pos; return ParsedAtomToken{8, 0, true, -1};
    case 'p': ++pos; return ParsedAtomToken{15, 0, true, -1};
    case 's': ++pos; return ParsedAtomToken{16, 0, true, -1};
    default:  return std::nullopt;
    }
}

int implicit_h_target_valence(int z, int charge) noexcept {
    if (z == 6)  return 4 - std::abs(charge);
    if (z == 7)  return (charge > 0) ? 4 : 3;
    if (z == 8)  return (charge < 0) ? 1 : 2;
    if (z == 16) return 2;
    if (z == 15) return 3;
    if (z == 5)  return 3;
    if (z == 9 || z == 17 || z == 35 || z == 53) return 1;
    return 0;
}

} // namespace

core::base::Result<MolecularGraph> SmilesParser::parse(std::string_view smiles,
                                                       bool add_implicit_hydrogens) {
    if (smiles.empty()) {
        return make_smi_error("SMILES string cannot be empty");
    }

    MolecularGraph graph{std::string(smiles)};
    std::vector<bool> atom_is_aromatic;
    std::vector<int> explicit_h_override;

    std::stack<size_t> branch_stack;
    std::optional<size_t> current_atom;
    std::optional<BondOrder> pending_bond;

    struct RingOpen {
        size_t atom_idx;
        std::optional<BondOrder> bond;
    };
    std::unordered_map<int, RingOpen> open_rings;

    size_t pos = 0;
    while (pos < smiles.size()) {
        char c = smiles[pos];

        if (c == '(') {
            if (!current_atom) return make_smi_error("Branch '(' with no preceding atom");
            branch_stack.push(*current_atom);
            pending_bond.reset();
            ++pos;
            continue;
        }
        if (c == ')') {
            if (branch_stack.empty()) return make_smi_error("Unmatched closing ')' in SMILES");
            current_atom = branch_stack.top();
            branch_stack.pop();
            pending_bond.reset();
            ++pos;
            continue;
        }
        if (c == '-' || c == '=' || c == '#' || c == ':') {
            if (c == '-') pending_bond = BondOrder::Single;
            if (c == '=') pending_bond = BondOrder::Double;
            if (c == '#') pending_bond = BondOrder::Triple;
            if (c == ':') pending_bond = BondOrder::Aromatic;
            ++pos;
            continue;
        }
        if (c == '/' || c == '\\') {
            ++pos; // Stereochemical cis/trans markers
            continue;
        }
        if (c == '.') {
            current_atom.reset();
            pending_bond.reset();
            ++pos;
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '%') {
            if (!current_atom) return make_smi_error("Ring closure digit with no preceding atom");
            int ring_id = 0;
            if (c == '%') {
                if (pos + 2 >= smiles.size() ||
                    !std::isdigit(static_cast<unsigned char>(smiles[pos + 1])) ||
                    !std::isdigit(static_cast<unsigned char>(smiles[pos + 2]))) {
                    return make_smi_error("Invalid '%' ring closure syntax in SMILES");
                }
                ring_id = (smiles[pos + 1] - '0') * 10 + (smiles[pos + 2] - '0');
                pos += 3;
            } else {
                ring_id = c - '0';
                ++pos;
            }
            auto it = open_rings.find(ring_id);
            if (it == open_rings.end()) {
                open_rings[ring_id] = RingOpen{*current_atom, pending_bond};
            } else {
                const size_t prev_atom = it->second.atom_idx;
                BondOrder ord = BondOrder::Single;
                if (pending_bond) ord = *pending_bond;
                else if (it->second.bond) ord = *it->second.bond;
                else if (atom_is_aromatic[prev_atom] && atom_is_aromatic[*current_atom]) {
                    ord = BondOrder::Aromatic;
                }
                auto br = graph.add_bond(prev_atom, *current_atom, ord);
                if (!br) return br.error();
                open_rings.erase(it);
            }
            pending_bond.reset();
            continue;
        }

        ParsedAtomToken tok;
        if (c == '[') {
            auto bres = parse_bracket_atom(smiles, pos);
            if (!bres) return bres.error();
            tok = bres.value();
        } else {
            auto ores = try_parse_organic(smiles, pos);
            if (!ores) {
                return make_smi_error(std::string("Unsupported token in SMILES near '") + c + "'");
            }
            tok = *ores;
        }

        const size_t new_id = graph.add_atom(
            tok.z, tok.charge,
            tok.is_aromatic ? Hybridization::Sp2 : Hybridization::Unknown);
        atom_is_aromatic.push_back(tok.is_aromatic);
        explicit_h_override.push_back(tok.explicit_h);

        if (current_atom) {
            BondOrder ord = BondOrder::Single;
            if (pending_bond) {
                ord = *pending_bond;
            } else if (atom_is_aromatic[*current_atom] && tok.is_aromatic) {
                ord = BondOrder::Aromatic;
            }
            auto br = graph.add_bond(*current_atom, new_id, ord);
            if (!br) return br.error();
        }
        current_atom = new_id;
        pending_bond.reset();
    }

    if (!branch_stack.empty()) {
        return make_smi_error("Unclosed branch '(' in SMILES string");
    }
    if (!open_rings.empty()) {
        return make_smi_error("Unclosed ring closure index in SMILES string");
    }

    if (add_implicit_hydrogens) {
        const size_t heavy_count = graph.atom_count();
        for (size_t i = 0; i < heavy_count; ++i) {
            int h_needed = 0;
            if (explicit_h_override[i] >= 0) {
                h_needed = explicit_h_override[i];
            } else {
                const auto& atom = graph.atoms()[i];
                const int target = implicit_h_target_valence(atom.atomic_number, atom.formal_charge);
                const double bo_sum = graph.atom_bond_order_sum(i);
                const int used = static_cast<int>(std::ceil(bo_sum - 1e-6));
                h_needed = std::max(0, target - used);
            }
            for (int h = 0; h < h_needed; ++h) {
                const size_t hid = graph.add_atom(1);
                (void)graph.add_bond(i, hid, BondOrder::Single);
            }
        }
    }

    return graph;
}

} // namespace shunya::mat
