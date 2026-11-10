#include "shunya/matdb/orbital_engine.hpp"

#include "shunya/matdb/elements_118_catalog.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <unordered_set>

namespace shunya::matdb {

namespace {

core::base::Error make_orb_error(std::string msg) {
    return core::base::Error{
        .code = 4110,
        .domain = core::base::ErrorDomain::Materials,
        .severity = core::base::Severity::Error,
        .message = std::move(msg),
        .details = ""
    };
}

int char_to_l(char c) noexcept {
    switch (c) {
    case 's': return 0;
    case 'p': return 1;
    case 'd': return 2;
    case 'f': return 3;
    case 'g': return 4;
    default:  return -1;
    }
}

void merge_subshell(std::vector<SubshellOccupancy>& out, SubshellOccupancy sub) {
    for (auto& existing : out) {
        if (existing.n == sub.n && existing.l == sub.l) {
            existing.occupancy += sub.occupancy;
            return;
        }
    }
    out.push_back(sub);
}

std::vector<SubshellOccupancy> noble_gas_core(std::string_view core_sym) {
    if (core_sym == "[He]") {
        return {{1, 0, 2}};
    }
    if (core_sym == "[Ne]") {
        return {{1, 0, 2}, {2, 0, 2}, {2, 1, 6}};
    }
    if (core_sym == "[Ar]") {
        return {{1, 0, 2}, {2, 0, 2}, {2, 1, 6}, {3, 0, 2}, {3, 1, 6}};
    }
    if (core_sym == "[Kr]") {
        return {{1, 0, 2}, {2, 0, 2}, {2, 1, 6}, {3, 0, 2}, {3, 1, 6},
                {3, 2, 10}, {4, 0, 2}, {4, 1, 6}};
    }
    if (core_sym == "[Xe]") {
        return {{1, 0, 2}, {2, 0, 2}, {2, 1, 6}, {3, 0, 2}, {3, 1, 6},
                {3, 2, 10}, {4, 0, 2}, {4, 1, 6},
                {4, 2, 10}, {5, 0, 2}, {5, 1, 6}};
    }
    if (core_sym == "[Rn]") {
        return {{1, 0, 2}, {2, 0, 2}, {2, 1, 6}, {3, 0, 2}, {3, 1, 6},
                {3, 2, 10}, {4, 0, 2}, {4, 1, 6},
                {4, 2, 10}, {4, 3, 14}, {5, 0, 2}, {5, 1, 6},
                {5, 2, 10}, {6, 0, 2}, {6, 1, 6}};
    }
    return {};
}

bool is_known_aufbau_exception_z(int z) noexcept {
    // Ground-state exceptions to naive Madelung filling rule in NIST/IUPAC
    // 24(Cr), 29(Cu), 41(Nb), 42(Mo), 44(Ru), 45(Rh), 46(Pd), 47(Ag),
    // 57(La), 58(Ce), 64(Gd), 78(Pt), 79(Au), 89(Ac), 90(Th), 91(Pa),
    // 92(U), 93(Np), 96(Cm), 103(Lr)
    static const std::unordered_set<int> exc = {
        24, 29, 41, 42, 44, 45, 46, 47, 57, 58, 64, 78, 79,
        89, 90, 91, 92, 93, 96, 103
    };
    return exc.count(z) > 0;
}

bool configurations_equal(const ElectronConfiguration& a, const ElectronConfiguration& b) noexcept {
    if (a.subshells.size() != b.subshells.size()) return false;
    for (size_t i = 0; i < a.subshells.size(); ++i) {
        if (a.subshells[i].n != b.subshells[i].n ||
            a.subshells[i].l != b.subshells[i].l ||
            a.subshells[i].occupancy != b.subshells[i].occupancy) {
            return false;
        }
    }
    return true;
}

} // namespace

core::base::Result<ElectronConfiguration> ElectronConfigParser::parse(std::string_view config_str) {
    if (config_str.empty()) {
        return make_orb_error("Electron configuration string cannot be empty");
    }

    ElectronConfiguration cfg;
    std::istringstream iss{std::string(config_str)};
    std::string token;
    while (iss >> token) {
        if (token.front() == '[' && token.back() == ']') {
            auto core = noble_gas_core(token);
            if (core.empty()) {
                return make_orb_error("Unknown noble gas core: " + token);
            }
            for (const auto& s : core) {
                merge_subshell(cfg.subshells, s);
            }
            continue;
        }

        // Token format: <n><s|p|d|f|g><occ>
        size_t pos = 0;
        while (pos < token.size() && std::isdigit(static_cast<unsigned char>(token[pos]))) {
            ++pos;
        }
        if (pos == 0 || pos >= token.size()) {
            return make_orb_error("Invalid orbital token (missing principal quantum number n): " + token);
        }
        int n = std::stoi(token.substr(0, pos));
        int l = char_to_l(token[pos]);
        if (l < 0 || n < 1 || l >= n) {
            return make_orb_error("Invalid quantum numbers (n, l) in token: " + token);
        }
        if (pos + 1 >= token.size()) {
            return make_orb_error("Missing electron occupancy in token: " + token);
        }
        std::string occ_str = token.substr(pos + 1);
        for (char ch : occ_str) {
            if (!std::isdigit(static_cast<unsigned char>(ch))) {
                return make_orb_error("Invalid occupancy characters in token: " + token);
            }
        }
        int occ = std::stoi(occ_str);
        SubshellOccupancy sub{n, l, occ};
        if (occ < 1 || occ > sub.max_capacity()) {
            return make_orb_error("Pauli exclusion violation (occupancy out of range) in token: " + token);
        }
        merge_subshell(cfg.subshells, sub);
    }

    if (cfg.subshells.empty()) {
        return make_orb_error("No orbitals parsed from configuration string");
    }

    std::sort(cfg.subshells.begin(), cfg.subshells.end(),
              [](const SubshellOccupancy& a, const SubshellOccupancy& b) {
                  return (a.n != b.n) ? (a.n < b.n) : (a.l < b.l);
              });

    // Re-check merged capacities in case duplicate orbital appeared across tokens
    for (const auto& s : cfg.subshells) {
        if (s.occupancy > s.max_capacity()) {
            return make_orb_error("Pauli exclusion violation after merging subshell " + s.label());
        }
    }

    return cfg;
}

const std::vector<std::pair<int, int>>& OrbitalFillingEngine::madelung_order() {
    static const std::vector<std::pair<int, int>> order = {
        {1, 0},
        {2, 0}, {2, 1},
        {3, 0}, {3, 1},
        {4, 0}, {3, 2}, {4, 1},
        {5, 0}, {4, 2}, {5, 1},
        {6, 0}, {4, 3}, {5, 2}, {6, 1},
        {7, 0}, {5, 3}, {6, 2}, {7, 1}
    };
    return order;
}

core::base::Result<ElectronConfiguration> OrbitalFillingEngine::predict_madelung(int atomic_number) {
    if (atomic_number < 1 || atomic_number > 118) {
        return make_orb_error("Atomic number Z out of range [1, 118]");
    }
    ElectronConfiguration cfg;
    int rem = atomic_number;
    for (const auto& [n, l] : madelung_order()) {
        if (rem <= 0) break;
        int cap = 2 * (2 * l + 1);
        int fill = std::min(rem, cap);
        cfg.subshells.push_back(SubshellOccupancy{n, l, fill});
        rem -= fill;
    }
    std::sort(cfg.subshells.begin(), cfg.subshells.end(),
              [](const SubshellOccupancy& a, const SubshellOccupancy& b) {
                  return (a.n != b.n) ? (a.n < b.n) : (a.l < b.l);
              });
    return cfg;
}

core::base::Result<OrbitalValidationResult> OrbitalFillingEngine::validate(int atomic_number,
                                                                           std::string_view config_str) {
    if (atomic_number < 1 || atomic_number > 118) {
        return make_orb_error("Atomic number Z must be in [1, 118]");
    }
    auto parsed_res = ElectronConfigParser::parse(config_str);
    if (!parsed_res) return parsed_res.error();

    auto parsed = std::move(parsed_res.value());
    if (parsed.total_electrons() != atomic_number) {
        return make_orb_error("Electron count mismatch for Z=" + std::to_string(atomic_number) +
                              " (parsed " + std::to_string(parsed.total_electrons()) + ")");
    }

    auto predicted_res = predict_madelung(atomic_number);
    if (!predicted_res) return predicted_res.error();

    bool matches = configurations_equal(parsed, predicted_res.value());
    OrbitalValidationResult out;
    out.atomic_number = atomic_number;
    out.valence_electrons = parsed.valence_electrons();
    out.unpaired_electrons = parsed.unpaired_electrons();
    out.spin_multiplicity = parsed.spin_multiplicity();
    out.matches_madelung_aufbau = matches;
    out.is_known_exception = (!matches) && is_known_aufbau_exception_z(atomic_number);
    out.parsed = std::move(parsed);
    return out;
}

core::base::ValidationReport OrbitalFillingEngine::audit_all_118_elements() {
    core::base::ValidationReport report;
    for (const auto& elem : Elements118Catalog::all_elements()) {
        auto res = validate(elem.atomic_number, elem.electron_configuration);
        if (!res) {
            report.add_error(elem.symbol, res.error().message);
            continue;
        }
        if (!res.value().matches_madelung_aufbau && !res.value().is_known_exception) {
            report.add_error(elem.symbol,
                             "Unexpected Aufbau deviation at Z=" + std::to_string(elem.atomic_number) +
                             " (" + elem.electron_configuration + ")");
        }
    }
    return report;
}

} // namespace shunya::matdb
