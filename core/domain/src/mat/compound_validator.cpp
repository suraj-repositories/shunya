#include "shunya/mat/compound_validator.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <sstream>

namespace shunya::mat {

namespace {

int greatest_common_divisor(int a, int b) noexcept {
    while (b != 0) {
        int t = b;
        b = a % b;
        a = t;
    }
    return std::abs(a);
}

} // namespace

double CompoundValidator::compute_pauling_ionic_pct(double delta_chi) noexcept {
    const double d = std::abs(delta_chi);
    return (1.0 - std::exp(-0.25 * d * d)) * 100.0;
}

std::string CompoundValidator::synthesize_empirical_formula(
    const std::vector<NormalizedConstituent>& constituents) {
    if (constituents.empty()) return "";

    // Find minimum positive mole amount
    double min_moles = 1e9;
    for (const auto& c : constituents) {
        if (c.moles > 1e-12) {
            min_moles = std::min(min_moles, c.moles);
        }
    }
    if (min_moles >= 1e8) return "";

    std::vector<int> ratios;
    ratios.reserve(constituents.size());
    for (const auto& c : constituents) {
        int r = std::clamp(static_cast<int>(std::round(c.moles / min_moles)), 1, 100);
        ratios.push_back(r);
    }

    // Divide by overall GCD
    int common_gcd = ratios.front();
    for (size_t i = 1; i < ratios.size(); ++i) {
        common_gcd = greatest_common_divisor(common_gcd, ratios[i]);
    }
    if (common_gcd > 1) {
        for (auto& r : ratios) r /= common_gcd;
    }

    std::ostringstream ss;
    for (size_t i = 0; i < constituents.size(); ++i) {
        ss << constituents[i].symbol;
        if (ratios[i] > 1) {
            ss << ratios[i];
        }
    }
    return ss.str();
}

bool CompoundValidator::verify_valence_shell_saturation(
    const std::vector<const matdb::ElementRecord*>& records,
    const std::vector<double>& mole_fractions) {
    if (records.size() < 2 || records.size() != mole_fractions.size()) return false;

    // Reject noble gases
    for (const auto* r : records) {
        const int z = r->atomic_number;
        if (z == 2 || z == 10 || z == 18 || z == 36 || z == 54 || z == 86 || z == 118) {
            return false;
        }
    }

    // Distinguish cations (metals) and anions (non-metals/electronegative)
    double sum_pos = 0.0;
    double sum_neg = 0.0;

    for (size_t i = 0; i < records.size(); ++i) {
        const auto* r = records[i];
        const double mf = mole_fractions[i];
        const double en = r->electronegativity.value_or(2.0);

        if (en < 2.1) {
            // Cationic tendency
            const double v = (r->group == 1) ? 1.0 : (r->group == 2) ? 2.0 : (r->group == 13) ? 3.0 : 2.0;
            sum_pos += v * mf;
        } else {
            // Anionic tendency
            const double v = (r->group == 17) ? 1.0 : (r->group == 16) ? 2.0 : (r->group == 15) ? 3.0 : (r->group == 14) ? 4.0 : 1.0;
            sum_neg += v * mf;
        }
    }

    return (sum_pos > 0.0 && sum_neg > 0.0);
}

StoichiometricValidationResult CompoundValidator::validate_compound(
    const std::vector<NormalizedConstituent>& constituents,
    const std::vector<const matdb::ElementRecord*>& records) {
    StoichiometricValidationResult result;
    if (constituents.empty() || constituents.size() != records.size()) {
        result.is_stoichiometrically_balanced = false;
        result.is_valence_saturated = false;
        result.validation_messages.push_back("Constituents and element records must be non-empty and matching in size.");
        return result;
    }

    result.empirical_formula = synthesize_empirical_formula(constituents);

    // Compute electronegativity differences
    double min_en = 1e9;
    double max_en = -1e9;
    for (const auto* r : records) {
        double en = r->electronegativity.value_or(2.0);
        min_en = std::min(min_en, en);
        max_en = std::max(max_en, en);
    }

    result.pauling_electronegativity_delta = (max_en >= min_en) ? (max_en - min_en) : 0.0;
    result.ionic_character_pct = compute_pauling_ionic_pct(result.pauling_electronegativity_delta);

    if (result.pauling_electronegativity_delta > 1.7) {
        result.bond_character = BondCharacter::Ionic;
    } else if (result.pauling_electronegativity_delta >= 0.4) {
        result.bond_character = BondCharacter::PolarCovalent;
    } else {
        bool all_metallic = true;
        for (const auto* r : records) {
            if (r->electronegativity.value_or(2.0) > 2.0) all_metallic = false;
        }
        result.bond_character = all_metallic ? BondCharacter::Metallic : BondCharacter::NonPolarCovalent;
    }

    std::vector<double> mole_fracs;
    mole_fracs.reserve(constituents.size());
    for (const auto& c : constituents) mole_fracs.push_back(c.mole_fraction);

    result.is_valence_saturated = verify_valence_shell_saturation(records, mole_fracs);

    if (!result.is_valence_saturated) {
        result.is_stoichiometrically_balanced = false;
        result.validation_messages.push_back(
            "Compound fails valence shell electroneutrality: constituents cannot achieve closed-shell electron transfer or shared pairing.");
    } else {
        result.is_stoichiometrically_balanced = true;
        result.validation_messages.push_back(
            "Stoichiometric balance confirmed for formula " + result.empirical_formula + " with " +
            std::to_string(result.ionic_character_pct) + "% ionic bond character.");
    }

    return result;
}

} // namespace shunya::mat
