#include <shunya/core/domain/periodic_table.hpp>

#include <algorithm>
#include <cmath>
#include <regex>
#include <sstream>

namespace shunya::core::domain {

namespace {

base::Error make_pt_error(std::string msg, int32_t code = 4101) {
    return base::Error{
        .code = code,
        .domain = base::ErrorDomain::Materials,
        .severity = base::Severity::Error,
        .message = std::move(msg),
        .details = ""
    };
}

} // namespace

base::Result<ConflictResolutionState> conflict_resolution_from_string(std::string_view s) {
    if (s == "PendingReview")  return ConflictResolutionState::PendingReview;
    if (s == "AcceptedSource") return ConflictResolutionState::AcceptedSource;
    if (s == "ManualOverride") return ConflictResolutionState::ManualOverride;
    if (s == "Rejected")       return ConflictResolutionState::Rejected;
    return make_pt_error(std::string("Unknown ConflictResolutionState: ") + std::string(s));
}

bool PeriodicElement::has_isotope(int mass_number) const noexcept {
    for (const auto& iso : isotopes) {
        if (iso.mass_number == mass_number) return true;
    }
    return false;
}

const ElementIsotope* PeriodicElement::get_isotope(int mass_number) const noexcept {
    for (const auto& iso : isotopes) {
        if (iso.mass_number == mass_number) return &iso;
    }
    return nullptr;
}

PeriodicTableDatabase::PeriodicTableDatabase() = default;

base::Result<void> PeriodicTableDatabase::add_element(PeriodicElement elem) {
    if (elem.atomic_number < 1 || elem.atomic_number > 118) {
        return make_pt_error("Atomic number Z must be between 1 and 118 (got " +
                             std::to_string(elem.atomic_number) + ")");
    }
    if (elem.symbol.empty()) {
        return make_pt_error("Element symbol cannot be empty");
    }
    if (elem.name.empty()) {
        return make_pt_error("Element name cannot be empty");
    }

    // Check duplicate symbol
    auto s_it = symbol_to_z_.find(elem.symbol);
    if (s_it != symbol_to_z_.end() && s_it->second != elem.atomic_number) {
        return make_pt_error("Element symbol '" + elem.symbol + "' already assigned to Z=" +
                             std::to_string(s_it->second));
    }

    symbol_to_z_[elem.symbol] = elem.atomic_number;
    elements_by_z_[elem.atomic_number] = std::move(elem);
    return {};
}

bool PeriodicTableDatabase::has_element(int atomic_number) const noexcept {
    return elements_by_z_.find(atomic_number) != elements_by_z_.end();
}

bool PeriodicTableDatabase::has_element(std::string_view symbol) const noexcept {
    return symbol_to_z_.find(std::string(symbol)) != symbol_to_z_.end();
}

const PeriodicElement* PeriodicTableDatabase::get_element(int atomic_number) const noexcept {
    auto it = elements_by_z_.find(atomic_number);
    if (it != elements_by_z_.end()) return &it->second;
    return nullptr;
}

const PeriodicElement* PeriodicTableDatabase::get_element(std::string_view symbol) const noexcept {
    auto s_it = symbol_to_z_.find(std::string(symbol));
    if (s_it != symbol_to_z_.end()) {
        return get_element(s_it->second);
    }
    return nullptr;
}

base::Result<void> PeriodicTableDatabase::import_dataset_record(std::string_view provider_id,
                                                              std::string_view provider_name,
                                                              int atomic_number,
                                                              std::string_view symbol,
                                                              std::string_view name,
                                                              const std::vector<AuditedValue>& values) {
    (void)provider_id;
    (void)provider_name;
    PeriodicElement elem;
    const PeriodicElement* existing = get_element(atomic_number);
    if (existing) {
        elem = *existing;
    } else {
        elem.atomic_number = atomic_number;
        elem.symbol = std::string(symbol);
        elem.name = std::string(name);
    }

    for (const auto& val : values) {
        elem.audited_fields[val.field_name] = val;

        // Map core fields
        if (val.field_name == "density") {
            elem.density_at_stp = val.normalized_value;
        } else if (val.field_name == "melting_point") {
            elem.melting_point_k = val.normalized_value;
        } else if (val.field_name == "boiling_point") {
            elem.boiling_point_k = val.normalized_value;
        } else if (val.field_name == "electronegativity") {
            elem.electronegativity = val.normalized_value;
        } else if (val.field_name == "specific_heat_capacity") {
            elem.specific_heat_capacity = val.normalized_value;
        } else if (val.field_name == "thermal_conductivity") {
            elem.thermal_conductivity = val.normalized_value;
        } else if (val.field_name == "electrical_conductivity") {
            elem.electrical_conductivity = val.normalized_value;
        } else if (val.field_name == "atomic_mass") {
            elem.standard_atomic_weight = val.normalized_value;
        }
    }

    return add_element(std::move(elem));
}

base::ValidationReport PeriodicTableDatabase::validate_periodic_consistency() const {
    base::ValidationReport report;

    for (const auto& pair : elements_by_z_) {
        const auto& elem = pair.second;
        std::string elem_path = "Element[" + elem.symbol + "]";

        if (elem.atomic_number < 1 || elem.atomic_number > 118) {
            report.add_error(elem_path, "Atomic number Z out of range [1, 118]: " +
                                        std::to_string(elem.atomic_number));
        }

        if (elem.period < 1 || elem.period > 7) {
            report.add_error(elem_path, "Period out of range [1, 7]: " + std::to_string(elem.period));
        }

        if (elem.group < 1 || elem.group > 18) {
            report.add_error(elem_path, "Group out of range [1, 18]: " + std::to_string(elem.group));
        }

        if (elem.block != 's' && elem.block != 'p' && elem.block != 'd' && elem.block != 'f') {
            report.add_error(elem_path, "Invalid block designation: " + std::string(1, elem.block));
        }

        if (elem.standard_atomic_weight <= 0.0) {
            report.add_error(elem_path, "Atomic weight must be positive: " +
                                        std::to_string(elem.standard_atomic_weight));
        }

        if (elem.melting_point_k.has_value() && elem.boiling_point_k.has_value()) {
            if (*elem.boiling_point_k < *elem.melting_point_k) {
                report.add_error(elem_path, "Boiling point (" + std::to_string(*elem.boiling_point_k) +
                                            " K) cannot be lower than melting point (" +
                                            std::to_string(*elem.melting_point_k) + " K)");
            }
        }

        // Validate isotopes if present
        if (!elem.isotopes.empty()) {
            double stable_abundance_sum = 0.0;
            bool has_stable = false;

            for (const auto& iso : elem.isotopes) {
                if (iso.mass_number < elem.atomic_number) {
                    report.add_error(elem_path, "Isotope mass number A (" + std::to_string(iso.mass_number) +
                                                ") is less than atomic number Z (" +
                                                std::to_string(elem.atomic_number) + ")");
                }
                if (iso.is_stable) {
                    has_stable = true;
                    stable_abundance_sum += iso.abundance_pct;
                }
            }

            if (has_stable && std::abs(stable_abundance_sum - 100.0) > 1.0) {
                report.add_warning(elem_path, "Sum of stable isotopic abundances deviates from 100%: " +
                                              std::to_string(stable_abundance_sum) + "%");
            }
        }
    }

    return report;
}

base::Result<base::Uuid> PeriodicTableDatabase::register_conflict(std::string element_symbol,
                                                                 std::string property_name,
                                                                 std::vector<AuditedValue> candidates) {
    if (element_symbol.empty()) {
        return make_pt_error("Cannot register conflict with empty element symbol");
    }
    if (property_name.empty()) {
        return make_pt_error("Cannot register conflict with empty property name");
    }
    if (candidates.size() < 2) {
        return make_pt_error("Conflict registration requires at least 2 candidate values");
    }

    ConflictRecord rec;
    rec.conflict_id = base::Uuid::generate_random();
    rec.element_symbol = std::move(element_symbol);
    rec.property_name = std::move(property_name);
    rec.candidate_values = std::move(candidates);
    rec.resolution_state = ConflictResolutionState::PendingReview;

    conflicts_.push_back(rec);
    return rec.conflict_id;
}

base::Result<void> PeriodicTableDatabase::resolve_conflict(const base::Uuid& conflict_id,
                                                           ConflictResolutionState state,
                                                           std::optional<AuditedValue> resolved_val,
                                                           std::string reviewer_notes) {
    auto it = std::find_if(conflicts_.begin(), conflicts_.end(), [&](const ConflictRecord& r) {
        return r.conflict_id == conflict_id;
    });

    if (it == conflicts_.end()) {
        return make_pt_error("Conflict ID not found in review queue: " + conflict_id.to_string());
    }

    it->resolution_state = state;
    it->resolved_value = std::move(resolved_val);
    it->reviewer_notes = std::move(reviewer_notes);

    // If accepted or manual override, update element audited field
    if ((state == ConflictResolutionState::AcceptedSource ||
         state == ConflictResolutionState::ManualOverride) &&
        it->resolved_value.has_value()) {
        auto* elem = const_cast<PeriodicElement*>(get_element(it->element_symbol));
        if (elem) {
            elem->audited_fields[it->property_name] = *it->resolved_value;
        }
    }

    return {};
}

std::vector<ConflictRecord> PeriodicTableDatabase::get_pending_conflicts() const {
    std::vector<ConflictRecord> pending;
    for (const auto& c : conflicts_) {
        if (c.resolution_state == ConflictResolutionState::PendingReview) {
            pending.push_back(c);
        }
    }
    return pending;
}

const ConflictRecord* PeriodicTableDatabase::get_conflict(const base::Uuid& id) const noexcept {
    for (const auto& c : conflicts_) {
        if (c.conflict_id == id) return &c;
    }
    return nullptr;
}

void PeriodicTableDatabase::seed_representative_elements() {
    // 1. Hydrogen (Z=1, s-block non-metal, diatomic gas)
    {
        PeriodicElement h;
        h.atomic_number = 1;
        h.symbol = "H";
        h.name = "Hydrogen";
        h.period = 1;
        h.group = 1;
        h.block = 's';
        h.standard_atomic_weight = 1.008;
        h.electron_configuration = "1s1";
        h.electronegativity = 2.20;
        h.density_at_stp = 0.08988; // kg/m^3
        h.melting_point_k = 14.01;
        h.boiling_point_k = 20.28;
        h.specific_heat_capacity = 14304.0;
        h.thermal_conductivity = 0.1805;
        h.isotopes = {
            {1, 1.007825, 99.9885, true, std::nullopt, "1/2+"},
            {2, 2.014102, 0.0115, true, std::nullopt, "1+"},
            {3, 3.016049, 0.0, false, 3.888e8, "1/2+"} // Tritium, t_half ~ 12.32 years
        };
        add_element(std::move(h));
    }

    // 2. Helium (Z=2, s-block noble gas)
    {
        PeriodicElement he;
        he.atomic_number = 2;
        he.symbol = "He";
        he.name = "Helium";
        he.period = 1;
        he.group = 18;
        he.block = 's';
        he.standard_atomic_weight = 4.002602;
        he.electron_configuration = "1s2";
        he.density_at_stp = 0.1786;
        he.melting_point_k = 0.95; // at 2.5 MPa
        he.boiling_point_k = 4.22;
        he.specific_heat_capacity = 5193.0;
        he.thermal_conductivity = 0.1513;
        he.isotopes = {
            {3, 3.016029, 0.000137, true, std::nullopt, "1/2+"},
            {4, 4.002603, 99.999863, true, std::nullopt, "0+"}
        };
        add_element(std::move(he));
    }

    // 3. Carbon (Z=6, p-block polyatomic non-metal)
    {
        PeriodicElement c;
        c.atomic_number = 6;
        c.symbol = "C";
        c.name = "Carbon";
        c.period = 2;
        c.group = 14;
        c.block = 'p';
        c.standard_atomic_weight = 12.011;
        c.electron_configuration = "[He] 2s2 2p2";
        c.electronegativity = 2.55;
        c.density_at_stp = 2267.0; // graphite
        c.melting_point_k = 3800.0; // sublimation
        c.boiling_point_k = 4300.0;
        c.specific_heat_capacity = 710.0;
        c.thermal_conductivity = 129.0;
        c.isotopes = {
            {12, 12.0, 98.93, true, std::nullopt, "0+"},
            {13, 13.003355, 1.07, true, std::nullopt, "1/2-"},
            {14, 14.003241, 0.0, false, 1.808e11, "0+"} // C-14, 5730 yr
        };
        add_element(std::move(c));
    }

    // 4. Silicon (Z=14, p-block metalloid semiconductor)
    {
        PeriodicElement si;
        si.atomic_number = 14;
        si.symbol = "Si";
        si.name = "Silicon";
        si.period = 3;
        si.group = 14;
        si.block = 'p';
        si.standard_atomic_weight = 28.085;
        si.electron_configuration = "[Ne] 3s2 3p2";
        si.electronegativity = 1.90;
        si.density_at_stp = 2329.0;
        si.melting_point_k = 1687.0;
        si.boiling_point_k = 3538.0;
        si.specific_heat_capacity = 705.0;
        si.thermal_conductivity = 149.0;
        si.electrical_conductivity = 1e-3;
        si.isotopes = {
            {28, 27.976927, 92.2297, true, std::nullopt, "0+"},
            {29, 28.976495, 4.6832, true, std::nullopt, "1/2+"},
            {30, 29.973770, 3.0872, true, std::nullopt, "0+"}
        };
        add_element(std::move(si));
    }

    // 5. Iron (Z=26, d-block transition metal, ferromagnetic)
    {
        PeriodicElement fe;
        fe.atomic_number = 26;
        fe.symbol = "Fe";
        fe.name = "Iron";
        fe.period = 4;
        fe.group = 8;
        fe.block = 'd';
        fe.standard_atomic_weight = 55.845;
        fe.electron_configuration = "[Ar] 3d6 4s2";
        fe.electronegativity = 1.83;
        fe.density_at_stp = 7874.0;
        fe.melting_point_k = 1811.0;
        fe.boiling_point_k = 3134.0;
        fe.specific_heat_capacity = 449.0;
        fe.thermal_conductivity = 80.4;
        fe.electrical_conductivity = 1.0e7;
        fe.isotopes = {
            {54, 53.939611, 5.845, true, std::nullopt, "0+"},
            {56, 55.934938, 91.754, true, std::nullopt, "0+"},
            {57, 56.935394, 2.119, true, std::nullopt, "1/2-"},
            {58, 57.933276, 0.282, true, std::nullopt, "0+"}
        };
        add_element(std::move(fe));
    }

    // 6. Copper (Z=29, d-block transition metal, electrical conductor)
    {
        PeriodicElement cu;
        cu.atomic_number = 29;
        cu.symbol = "Cu";
        cu.name = "Copper";
        cu.period = 4;
        cu.group = 11;
        cu.block = 'd';
        cu.standard_atomic_weight = 63.546;
        cu.electron_configuration = "[Ar] 3d10 4s1";
        cu.electronegativity = 1.90;
        cu.density_at_stp = 8960.0;
        cu.melting_point_k = 1357.77;
        cu.boiling_point_k = 2835.0;
        cu.specific_heat_capacity = 385.0;
        cu.thermal_conductivity = 401.0;
        cu.electrical_conductivity = 5.96e7;
        cu.isotopes = {
            {63, 62.929601, 69.15, true, std::nullopt, "3/2-"},
            {65, 64.927794, 30.85, true, std::nullopt, "3/2-"}
        };

        // MATDB-10: Extreme-condition curve for thermal conductivity vs temperature
        MaterialCurve k_curve("thermal_conductivity_vs_T", CurveVariable::Temperature,
                              InterpolationMode::CubicSpline, ExtrapolationPolicy::ClampToBoundary);
        k_curve.add_point(10.0, 16000.0); // Cryogenic peak
        k_curve.add_point(20.0, 10000.0);
        k_curve.add_point(50.0, 1200.0);
        k_curve.add_point(100.0, 480.0);
        k_curve.add_point(300.0, 401.0);
        k_curve.add_point(600.0, 379.0);
        k_curve.add_point(1000.0, 342.0);
        cu.curves["thermal_conductivity_vs_T"] = std::move(k_curve);

        add_element(std::move(cu));
    }

    // 7. Gold (Z=79, d-block noble transition metal)
    {
        PeriodicElement au;
        au.atomic_number = 79;
        au.symbol = "Au";
        au.name = "Gold";
        au.period = 6;
        au.group = 11;
        au.block = 'd';
        au.standard_atomic_weight = 196.966569;
        au.electron_configuration = "[Xe] 4f14 5d10 6s1";
        au.electronegativity = 2.54;
        au.density_at_stp = 19300.0;
        au.melting_point_k = 1337.33;
        au.boiling_point_k = 3243.0;
        au.specific_heat_capacity = 129.0;
        au.thermal_conductivity = 318.0;
        au.electrical_conductivity = 4.52e7;
        au.isotopes = {
            {197, 196.966569, 100.0, true, std::nullopt, "3/2+"}
        };
        add_element(std::move(au));
    }

    // 8. Uranium (Z=92, f-block actinide, nuclear/radioactive)
    {
        PeriodicElement u;
        u.atomic_number = 92;
        u.symbol = "U";
        u.name = "Uranium";
        u.period = 7;
        u.group = 3;
        u.block = 'f';
        u.standard_atomic_weight = 238.02891;
        u.electron_configuration = "[Rn] 5f3 6d1 7s2";
        u.electronegativity = 1.38;
        u.density_at_stp = 19100.0;
        u.melting_point_k = 1405.3;
        u.boiling_point_k = 4404.0;
        u.specific_heat_capacity = 116.0;
        u.thermal_conductivity = 27.5;
        u.electrical_conductivity = 3.5e6;
        u.isotopes = {
            {234, 234.040952, 0.0054, false, 7.749e12, "0+"}, // t_half ~ 2.45e5 yr
            {235, 235.043930, 0.7204, false, 2.221e16, "7/2-"}, // fissile, t_half ~ 7.04e8 yr
            {238, 238.050788, 99.2742, false, 1.41e17, "0+"}  // fertile, t_half ~ 4.468e9 yr
        };
        add_element(std::move(u));
    }
}

std::string PeriodicTableDatabase::to_json() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"elements\": [\n";

    size_t e_idx = 0;
    for (const auto& pair : elements_by_z_) {
        const auto& el = pair.second;
        ss << "    {\n";
        ss << "      \"atomic_number\": " << el.atomic_number << ",\n";
        ss << "      \"symbol\": \"" << el.symbol << "\",\n";
        ss << "      \"name\": \"" << el.name << "\",\n";
        ss << "      \"period\": " << el.period << ",\n";
        ss << "      \"group\": " << el.group << ",\n";
        ss << "      \"block\": \"" << el.block << "\",\n";
        ss << "      \"atomic_weight\": " << el.standard_atomic_weight << ",\n";
        ss << "      \"electron_configuration\": \"" << el.electron_configuration << "\",\n";
        if (el.electronegativity) ss << "      \"electronegativity\": " << *el.electronegativity << ",\n";
        if (el.density_at_stp) ss << "      \"density_at_stp\": " << *el.density_at_stp << ",\n";
        if (el.melting_point_k) ss << "      \"melting_point_k\": " << *el.melting_point_k << ",\n";
        if (el.boiling_point_k) ss << "      \"boiling_point_k\": " << *el.boiling_point_k << ",\n";
        if (el.thermal_conductivity) ss << "      \"thermal_conductivity\": " << *el.thermal_conductivity << ",\n";
        if (el.electrical_conductivity) ss << "      \"electrical_conductivity\": " << *el.electrical_conductivity << ",\n";
        ss << "      \"isotopes\": [\n";
        for (size_t iso_i = 0; iso_i < el.isotopes.size(); ++iso_i) {
            const auto& iso = el.isotopes[iso_i];
            ss << "        {\"A\": " << iso.mass_number << ", \"mass\": " << iso.atomic_mass
               << ", \"abundance\": " << iso.abundance_pct << ", \"stable\": " << (iso.is_stable ? "true" : "false") << "}";
            if (iso_i + 1 < el.isotopes.size()) ss << ",";
            ss << "\n";
        }
        ss << "      ]\n";
        ss << "    }";
        if (++e_idx < elements_by_z_.size()) ss << ",";
        ss << "\n";
    }

    ss << "  ]\n";
    ss << "}";
    return ss.str();
}

base::Result<PeriodicTableDatabase> PeriodicTableDatabase::from_json(std::string_view json_str) {
    std::string s(json_str);
    PeriodicTableDatabase db;

    std::regex elem_re(R"re(\{\s*"atomic_number"\s*:\s*([0-9]+)\s*,\s*"symbol"\s*:\s*"([^"]+)"\s*,\s*"name"\s*:\s*"([^"]+)"\s*,\s*"period"\s*:\s*([0-9]+)\s*,\s*"group"\s*:\s*([0-9]+)\s*,\s*"block"\s*:\s*"([^"]+)"\s*,\s*"atomic_weight"\s*:\s*([-+]?[0-9]*\.?[0-9]+(?:[eE][-+]?[0-9]+)?))re");
    auto it = std::sregex_iterator(s.begin(), s.end(), elem_re);
    auto end = std::sregex_iterator();

    for (; it != end; ++it) {
        PeriodicElement el;
        el.atomic_number = std::stoi((*it)[1].str());
        el.symbol = (*it)[2].str();
        el.name = (*it)[3].str();
        el.period = std::stoi((*it)[4].str());
        el.group = std::stoi((*it)[5].str());
        el.block = (*it)[6].str().empty() ? 's' : (*it)[6].str()[0];
        el.standard_atomic_weight = std::stod((*it)[7].str());
        db.add_element(el);
    }

    return db;
}

} // namespace shunya::core::domain
