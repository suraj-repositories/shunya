#include "shunya/mat/formula_parser.hpp"

#include "shunya/matdb/elements_118_catalog.hpp"

#include <cctype>

namespace shunya::mat {

namespace {

core::base::Error make_fp_error(std::string msg) {
    return core::base::Error{
        .code = 4210,
        .domain = core::base::ErrorDomain::Materials,
        .severity = core::base::Severity::Error,
        .message = std::move(msg),
        .details = ""
    };
}

class RecursiveFormulaParser {
public:
    explicit RecursiveFormulaParser(std::string_view src) : src_(src) {}

    core::base::Result<std::map<int, int>> parse_full() {
        if (src_.empty()) {
            return make_fp_error("Chemical formula string cannot be empty");
        }
        std::map<int, int> total;
        while (pos_ < src_.size()) {
            skip_whitespace();
            if (pos_ >= src_.size()) break;

            int mult = parse_leading_multiplier();
            auto group_res = parse_group('\0');
            if (!group_res) return group_res.error();
            for (const auto& [z, cnt] : group_res.value()) {
                total[z] += cnt * mult;
            }

            skip_whitespace();
            if (pos_ < src_.size()) {
                if (src_[pos_] == '.' || src_[pos_] == '*') {
                    ++pos_;
                    if (pos_ >= src_.size()) {
                        return make_fp_error("Trailing hydrate separator in formula");
                    }
                } else {
                    return make_fp_error(std::string("Unexpected character '") + src_[pos_] + "'");
                }
            }
        }
        if (total.empty()) {
            return make_fp_error("No elements found in chemical formula");
        }
        return total;
    }

private:
    std::string_view src_;
    size_t pos_{0};

    void skip_whitespace() noexcept {
        while (pos_ < src_.size() && std::isspace(static_cast<unsigned char>(src_[pos_]))) {
            ++pos_;
        }
    }

    int parse_leading_multiplier() noexcept {
        if (pos_ >= src_.size() || !std::isdigit(static_cast<unsigned char>(src_[pos_]))) {
            return 1;
        }
        int val = 0;
        while (pos_ < src_.size() && std::isdigit(static_cast<unsigned char>(src_[pos_]))) {
            val = val * 10 + (src_[pos_] - '0');
            ++pos_;
        }
        return (val > 0) ? val : 1;
    }

    int parse_subscript() noexcept {
        return parse_leading_multiplier();
    }

    static char matching_close(char open) noexcept {
        if (open == '(') return ')';
        if (open == '[') return ']';
        if (open == '{') return '}';
        return '\0';
    }

    core::base::Result<std::map<int, int>> parse_group(char expected_close) {
        std::map<int, int> counts;
        while (pos_ < src_.size()) {
            char c = src_[pos_];
            if (c == expected_close) {
                ++pos_;
                return counts;
            }
            if (c == '.' || c == '*') {
                if (expected_close != '\0') {
                    return make_fp_error("Unclosed group before hydrate dot separator");
                }
                return counts;
            }
            if (c == '(' || c == '[' || c == '{') {
                ++pos_;
                auto sub_res = parse_group(matching_close(c));
                if (!sub_res) return sub_res.error();
                int mult = parse_subscript();
                for (const auto& [z, cnt] : sub_res.value()) {
                    counts[z] += cnt * mult;
                }
                continue;
            }
            if (std::isupper(static_cast<unsigned char>(c))) {
                std::string sym(1, c);
                ++pos_;
                while (pos_ < src_.size() && std::islower(static_cast<unsigned char>(src_[pos_]))) {
                    sym.push_back(src_[pos_]);
                    ++pos_;
                }
                const auto* elem = matdb::Elements118Catalog::find_by_symbol(sym);
                if (!elem) {
                    return make_fp_error("Unknown element symbol in formula: " + sym);
                }
                int mult = parse_subscript();
                counts[elem->atomic_number] += mult;
                continue;
            }
            return make_fp_error(std::string("Unexpected character '") + c + "' in formula");
        }
        if (expected_close != '\0') {
            return make_fp_error(std::string("Missing closing bracket '") + expected_close + "'");
        }
        return counts;
    }
};

} // namespace

int FormulaComposition::count_of(std::string_view symbol) const noexcept {
    const auto* e = matdb::Elements118Catalog::find_by_symbol(symbol);
    if (!e) return 0;
    auto it = counts_by_z.find(e->atomic_number);
    return (it != counts_by_z.end()) ? it->second : 0;
}

int FormulaComposition::total_atom_count() const noexcept {
    int sum = 0;
    for (const auto& [z, cnt] : counts_by_z) {
        (void)z;
        sum += cnt;
    }
    return sum;
}

std::map<std::string, double> FormulaComposition::mass_fractions() const {
    std::map<std::string, double> fracs;
    if (molecular_weight_g_mol <= 0.0) return fracs;
    for (const auto& [z, cnt] : counts_by_z) {
        const auto* e = matdb::Elements118Catalog::find_by_z(z);
        if (!e) continue;
        fracs[e->symbol] = (static_cast<double>(cnt) * e->atomic_weight_amu) / molecular_weight_g_mol;
    }
    return fracs;
}

std::string FormulaComposition::hill_formula() const {
    std::map<std::string, int> by_sym;
    for (const auto& [z, cnt] : counts_by_z) {
        const auto* e = matdb::Elements118Catalog::find_by_z(z);
        if (e && cnt > 0) by_sym[e->symbol] = cnt;
    }
    std::string out;
    auto append_sym = [&](const std::string& sym) {
        auto it = by_sym.find(sym);
        if (it == by_sym.end()) return;
        out += it->first;
        if (it->second > 1) out += std::to_string(it->second);
        by_sym.erase(it);
    };
    if (by_sym.count("C")) {
        append_sym("C");
        append_sym("H");
    }
    for (const auto& [sym, cnt] : by_sym) {
        out += sym;
        if (cnt > 1) out += std::to_string(cnt);
    }
    return out;
}

core::base::Result<FormulaComposition> FormulaParser::parse(std::string_view formula) {
    RecursiveFormulaParser parser(formula);
    auto counts_res = parser.parse_full();
    if (!counts_res) return counts_res.error();

    FormulaComposition comp;
    comp.raw_formula = std::string(formula);
    comp.counts_by_z = std::move(counts_res.value());
    double mw = 0.0;
    for (const auto& [z, cnt] : comp.counts_by_z) {
        const auto* e = matdb::Elements118Catalog::find_by_z(z);
        if (e) mw += static_cast<double>(cnt) * e->atomic_weight_amu;
    }
    comp.molecular_weight_g_mol = mw;
    return comp;
}

} // namespace shunya::mat
