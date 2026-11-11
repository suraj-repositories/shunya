#include "shunya/matdb/element_database.hpp"

#include "shunya/matdb/elements_118_catalog.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <fstream>
#include <sstream>

namespace shunya::matdb {

namespace {

std::string trim_copy(std::string_view sv) {
    size_t b = 0;
    while (b < sv.size() && std::isspace(static_cast<unsigned char>(sv[b]))) ++b;
    size_t e = sv.size();
    while (e > b && std::isspace(static_cast<unsigned char>(sv[e - 1]))) --e;
    return std::string(sv.substr(b, e - b));
}

std::string to_lower_copy(std::string_view sv) {
    std::string out(sv);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

std::optional<double> parse_double_opt(std::string_view sv) {
    const std::string s = trim_copy(sv);
    if (s.empty()) return std::nullopt;
    const std::string low = to_lower_copy(s);
    if (low == "inf" || low == "infinity" || low == "nan") return std::nullopt;
    double val = 0.0;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), val);
    if (ec == std::errc{} && ptr == s.data() + s.size() && std::isfinite(val)) {
        return val;
    }
    return std::nullopt;
}

std::optional<int> parse_int_opt(std::string_view sv) {
    const std::string s = trim_copy(sv);
    if (s.empty()) return std::nullopt;
    int val = 0;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), val);
    if (ec == std::errc{} && ptr == s.data() + s.size()) {
        return val;
    }
    return std::nullopt;
}

std::string normalize_space_group(std::string_view raw, bool& normalized) {
    std::string s = trim_copy(raw);
    normalized = false;
    if (s.find('\n') != std::string::npos || s.find('\r') != std::string::npos || s.find('_') != std::string::npos) {
        std::string cleaned;
        cleaned.reserve(s.size());
        for (char c : s) {
            if (c == '\r' || c == '\n') continue;
            if (c == '_') {
                cleaned.push_back('-');
                normalized = true;
            } else {
                cleaned.push_back(c);
            }
        }
        if (cleaned != s) normalized = true;
        return cleaned;
    }
    return s;
}

core::base::Error make_csv_error(int32_t code, std::string msg, std::string details = {}) {
    return core::base::Error{
        .code = code,
        .domain = core::base::ErrorDomain::Materials,
        .severity = core::base::Severity::Error,
        .message = std::move(msg),
        .details = std::move(details)
    };
}

double compare_key(const ElementRecord& r, ElementSortField field, bool& has_val) {
    has_val = true;
    auto opt_val = [&](const std::optional<double>& o) -> double {
        if (!o.has_value()) {
            has_val = false;
            return 0.0;
        }
        return *o;
    };
    switch (field) {
    case ElementSortField::AtomicNumber:           return static_cast<double>(r.atomic_number);
    case ElementSortField::AtomicWeight:           return opt_val(r.atomic_weight);
    case ElementSortField::Density:                return opt_val(r.density);
    case ElementSortField::MeltingPoint:           return opt_val(r.melting_point);
    case ElementSortField::BoilingPoint:           return opt_val(r.boiling_point);
    case ElementSortField::SpecificHeat:           return opt_val(r.specific_heat);
    case ElementSortField::ThermalConductivity:    return opt_val(r.thermal_conductivity);
    case ElementSortField::ElectricalConductivity: return opt_val(r.effective_conductivity());
    case ElementSortField::YoungModulus:           return opt_val(r.young_modulus);
    case ElementSortField::Electronegativity:      return opt_val(r.electronegativity);
    case ElementSortField::Name:
    case ElementSortField::Symbol:
        return 0.0;
    }
    return 0.0;
}

} // namespace

std::optional<double> ElementRecord::effective_resistivity() const noexcept {
    if (resistivity.has_value() && *resistivity > 0.0) return resistivity;
    if (electrical_conductivity.has_value() && *electrical_conductivity > 0.0) {
        return 1.0 / *electrical_conductivity;
    }
    return std::nullopt;
}

std::optional<double> ElementRecord::effective_conductivity() const noexcept {
    if (electrical_conductivity.has_value() && *electrical_conductivity > 0.0) return electrical_conductivity;
    if (resistivity.has_value() && *resistivity > 0.0) {
        return 1.0 / *resistivity;
    }
    return std::nullopt;
}

PropertyProvenance ElementRecord::provenance_of(std::string_view column_name) const noexcept {
    auto it = field_provenance.find(std::string(column_name));
    if (it != field_provenance.end()) return it->second;
    return PropertyProvenance::MissingUnsupported;
}

ElementDatabase& ElementDatabase::global() {
    static ElementDatabase instance = []() {
        ElementDatabase db;
        (void)db.load_default();
        return db;
    }();
    return instance;
}

std::vector<std::vector<std::string>> ElementDatabase::parse_rfc4180_csv(std::string_view text) {
    std::vector<std::vector<std::string>> rows;
    std::vector<std::string> current_row;
    std::string field;
    bool in_quotes = false;

    const size_t n = text.size();
    for (size_t i = 0; i < n; ++i) {
        const char c = text[i];
        if (in_quotes) {
            if (c == '"') {
                if (i + 1 < n && text[i + 1] == '"') {
                    field.push_back('"');
                    ++i;
                } else {
                    in_quotes = false;
                }
            } else {
                field.push_back(c);
            }
        } else {
            if (c == '"') {
                in_quotes = true;
            } else if (c == ',') {
                current_row.push_back(std::move(field));
                field.clear();
            } else if (c == '\r') {
                if (i + 1 < n && text[i + 1] == '\n') ++i;
                current_row.push_back(std::move(field));
                field.clear();
                rows.push_back(std::move(current_row));
                current_row.clear();
            } else if (c == '\n') {
                current_row.push_back(std::move(field));
                field.clear();
                rows.push_back(std::move(current_row));
                current_row.clear();
            } else {
                field.push_back(c);
            }
        }
    }

    if (!field.empty() || !current_row.empty()) {
        current_row.push_back(std::move(field));
        rows.push_back(std::move(current_row));
    }
    return rows;
}

core::base::Result<void> ElementDatabase::load_from_csv(const std::filesystem::path& csv_path) {
    std::ifstream ifs(csv_path, std::ios::binary);
    if (!ifs.is_open()) {
        return make_csv_error(4101, "Failed to open chemical element CSV file", csv_path.string());
    }
    std::ostringstream ss;
    ss << ifs.rdbuf();
    auto res = load_from_csv_text(ss.str(), csv_path.string());
    if (res.is_ok()) {
        source_path_ = csv_path;
    }
    return res;
}

core::base::Result<void> ElementDatabase::load_default() {
    std::vector<std::filesystem::path> candidates = {
        std::filesystem::path(R"(C:\Users\Shubham\Desktop\test\chemistry\elements.csv)"),
        std::filesystem::current_path() / "data" / "elements.csv",
        std::filesystem::current_path() / ".." / "data" / "elements.csv",
        std::filesystem::current_path() / ".." / ".." / "data" / "elements.csv"
    };
#ifdef SHUNYA_SOURCE_DIR
    candidates.push_back(std::filesystem::path(SHUNYA_SOURCE_DIR) / "data" / "elements.csv");
#endif
    for (const auto& p : candidates) {
        std::error_code ec;
        if (std::filesystem::exists(p, ec) && std::filesystem::is_regular_file(p, ec)) {
            auto res = load_from_csv(p);
            if (res.is_ok()) return res;
        }
    }
    return make_csv_error(4102, "No valid elements.csv file found in default search paths");
}

core::base::Result<void> ElementDatabase::load_from_csv_text(std::string_view csv_text, std::string source_label) {
    if (csv_text.empty()) {
        return make_csv_error(4103, "Element CSV text is empty", std::move(source_label));
    }

    // Strip UTF-8 BOM if present
    if (csv_text.size() >= 3 &&
        static_cast<unsigned char>(csv_text[0]) == 0xEF &&
        static_cast<unsigned char>(csv_text[1]) == 0xBB &&
        static_cast<unsigned char>(csv_text[2]) == 0xBF) {
        csv_text.remove_prefix(3);
    }

    auto rows = parse_rfc4180_csv(csv_text);
    if (rows.size() < 2) {
        return make_csv_error(4104, "Element CSV missing header or data rows", std::move(source_label));
    }

    const auto& header = rows[0];
    std::unordered_map<std::string, size_t> col_idx;
    for (size_t i = 0; i < header.size(); ++i) {
        col_idx[trim_copy(header[i])] = i;
    }

    auto require_col = [&](const char* name) -> bool {
        return col_idx.find(name) != col_idx.end();
    };
    if (!require_col("Name") || !require_col("Symbol") || !require_col("Atomic_Number")) {
        return make_csv_error(4105, "Element CSV missing mandatory Name/Symbol/Atomic_Number columns");
    }

    std::vector<ElementRecord> parsed;
    std::unordered_map<int, size_t> by_z;
    std::unordered_map<std::string, size_t> by_sym;
    parsed.reserve(rows.size() - 1);

    for (size_t r_idx = 1; r_idx < rows.size(); ++r_idx) {
        const auto& row = rows[r_idx];
        if (row.empty() || (row.size() == 1 && trim_copy(row[0]).empty())) continue;

        auto cell = [&](std::string_view col_name) -> std::string {
            auto it = col_idx.find(std::string(col_name));
            if (it == col_idx.end() || it->second >= row.size()) return {};
            return row[it->second];
        };

        auto z_opt = parse_int_opt(cell("Atomic_Number"));
        if (!z_opt.has_value() || *z_opt < 1 || *z_opt > 118) {
            return make_csv_error(4106, "Invalid Atomic_Number in CSV row " + std::to_string(r_idx));
        }

        ElementRecord rec;
        rec.atomic_number = *z_opt;
        rec.name = trim_copy(cell("Name"));
        rec.symbol = trim_copy(cell("Symbol"));
        if (rec.name.empty() || rec.symbol.empty()) {
            return make_csv_error(4107, "Empty Name or Symbol at Z=" + std::to_string(rec.atomic_number));
        }

        // Preserve all raw CSV columns verbatim
        for (size_t c = 0; c < header.size(); ++c) {
            const std::string key = trim_copy(header[c]);
            const std::string val = (c < row.size()) ? row[c] : std::string{};
            rec.raw_csv_fields[key] = val;
            rec.field_provenance[key] = trim_copy(val).empty()
                ? PropertyProvenance::MissingUnsupported
                : PropertyProvenance::CsvPrimary;
        }

        auto assign_opt_double = [&](const char* col, std::optional<double>& dst) {
            const std::string raw = cell(col);
            dst = parse_double_opt(raw);
            if (!trim_copy(raw).empty() && !dst.has_value()) {
                rec.field_provenance[col] = PropertyProvenance::MissingUnsupported;
            }
        };
        auto assign_opt_int = [&](const char* col, std::optional<int>& dst) {
            dst = parse_int_opt(cell(col));
        };

        // Cross-check IUPAC 2021 catalog for modern symbols/names (Z=112..118) & electron configuration
        if (const auto* iupac = Elements118Catalog::find_by_z(rec.atomic_number)) {
            rec.iupac_symbol = iupac->symbol;
            rec.iupac_name = iupac->name;
            rec.electron_configuration_iupac = iupac->electron_configuration;
            if (rec.symbol != iupac->symbol) {
                rec.normalization_notes.push_back(
                    "CSV uses legacy symbol '" + rec.symbol + "' (IUPAC modern symbol: '" + iupac->symbol + "').");
            }
        } else {
            rec.iupac_symbol = rec.symbol;
            rec.iupac_name = rec.name;
        }

        rec.alternate_names = trim_copy(cell("Alternate_Names"));
        rec.block = trim_copy(cell("Block"));
        assign_opt_int("Group", rec.group);
        rec.period = parse_int_opt(cell("Period")).value_or(0);
        rec.graph_period = parse_int_opt(cell("Graph.Period")).value_or(rec.period);
        rec.graph_group = parse_int_opt(cell("Graph.Group")).value_or(rec.group.value_or(0));
        rec.phase = trim_copy(cell("Phase"));
        rec.color = trim_copy(cell("Color"));
        rec.gas_phase = trim_copy(cell("Gas_phase"));

        assign_opt_double("Atomic_Weight", rec.atomic_weight);
        if (!rec.atomic_weight.has_value()) {
            if (const auto* iupac = Elements118Catalog::find_by_z(rec.atomic_number)) {
                rec.atomic_weight = iupac->atomic_weight_amu;
                rec.field_provenance["Atomic_Weight"] = PropertyProvenance::NistReferenceFallback;
                rec.normalization_notes.push_back(
                    "Missing Atomic_Weight in CSV; populated IUPAC 2021 standard value (" +
                    std::to_string(iupac->atomic_weight_amu) + ").");
            }
        }

        assign_opt_int("Valence", rec.valence);
        assign_opt_double("Electronegativity", rec.electronegativity);
        assign_opt_double("ElectronAffinity", rec.electron_affinity);
        rec.electron_configuration = trim_copy(cell("Electron_Configuration"));
        if (!rec.electron_configuration_iupac.empty() &&
            rec.electron_configuration != rec.electron_configuration_iupac) {
            rec.normalization_notes.push_back(
                "CSV Electron_Configuration '" + rec.electron_configuration +
                "' differs from IUPAC ground-state '" + rec.electron_configuration_iupac + "'.");
        }
        rec.quantum_numbers = trim_copy(cell("Quantum_Numbers"));
        assign_opt_double("Atomic_Radius", rec.atomic_radius);
        assign_opt_double("Covalent_Radius", rec.covalent_radius);
        assign_opt_double("Van_der_Waals_Radius", rec.van_der_waals_radius);

        assign_opt_double("Density", rec.density);
        assign_opt_double("Density_Liquid", rec.density_liquid);
        assign_opt_double("Molar_Volume", rec.molar_volume);
        assign_opt_double("Melting_Point", rec.melting_point);
        assign_opt_double("Boiling_Point", rec.boiling_point);
        assign_opt_double("Absolute_Melting_Point", rec.absolute_melting_point);
        assign_opt_double("Absolute_Boiling_Point", rec.absolute_boiling_point);
        assign_opt_double("Critical_Pressure", rec.critical_pressure);
        assign_opt_double("Critical_Temperature", rec.critical_temperature);
        assign_opt_double("Heat_of_Fusion", rec.heat_of_fusion);
        assign_opt_double("Heat_of_Vaporization", rec.heat_of_vaporization);
        assign_opt_double("Heat_of_Combustion", rec.heat_of_combustion);
        assign_opt_double("Specific_Heat", rec.specific_heat);

        // Normalize Excel-mangled Adiabatic_Index ("07-May" = 7/5, "05-Mar" = 5/3)
        const std::string raw_gamma = trim_copy(cell("Adiabatic_Index"));
        if (!raw_gamma.empty()) {
            if (auto g_num = parse_double_opt(raw_gamma)) {
                rec.adiabatic_index = *g_num;
            } else if (raw_gamma == "07-May" || raw_gamma == "7-May") {
                rec.adiabatic_index = 7.0 / 5.0;
                rec.field_provenance["Adiabatic_Index"] = PropertyProvenance::CsvNormalized;
                rec.normalization_notes.push_back("Normalized Excel-mangled Adiabatic_Index '07-May' -> 1.4 (7/5).");
            } else if (raw_gamma == "05-Mar" || raw_gamma == "5-Mar") {
                rec.adiabatic_index = 5.0 / 3.0;
                rec.field_provenance["Adiabatic_Index"] = PropertyProvenance::CsvNormalized;
                rec.normalization_notes.push_back("Normalized Excel-mangled Adiabatic_Index '05-Mar' -> 1.6667 (5/3).");
            } else {
                rec.field_provenance["Adiabatic_Index"] = PropertyProvenance::MissingUnsupported;
                rec.normalization_notes.push_back("Unparseable Adiabatic_Index '" + raw_gamma + "' marked Unsupported.");
            }
        }

        assign_opt_double("Thermal_Conductivity", rec.thermal_conductivity);
        assign_opt_double("Thermal_Expansion", rec.thermal_expansion);
        assign_opt_double("Brinell_Hardness", rec.brinell_hardness);
        assign_opt_double("Mohs_Hardness", rec.mohs_hardness);
        assign_opt_double("Vickers_Hardness", rec.vickers_hardness);
        assign_opt_double("Bulk_Modulus", rec.bulk_modulus);
        assign_opt_double("Shear_Modulus", rec.shear_modulus);
        assign_opt_double("Young_Modulus", rec.young_modulus);
        assign_opt_double("Poisson_Ratio", rec.poisson_ratio);
        assign_opt_double("Refractive_Index", rec.refractive_index);
        assign_opt_double("Speed_of_Sound", rec.speed_of_sound);

        rec.electrical_type = trim_copy(cell("Electrical_Type"));
        assign_opt_double("Electrical_Conductivity", rec.electrical_conductivity);
        assign_opt_double("Resistivity", rec.resistivity);
        assign_opt_double("Superconducting_Point", rec.superconducting_point);
        rec.magnetic_type = trim_copy(cell("Magnetic_Type"));
        assign_opt_double("Curie_Point", rec.curie_point);
        assign_opt_double("Neel_Point", rec.neel_point);
        assign_opt_double("Mass_Magnetic_Susceptibility", rec.mass_magnetic_susceptibility);
        assign_opt_double("Molar_Magnetic_Susceptibility", rec.molar_magnetic_susceptibility);
        assign_opt_double("Volume_Magnetic_Susceptibility", rec.volume_magnetic_susceptibility);

        assign_opt_double("Autoignition_Point", rec.autoignition_point);
        assign_opt_double("Flashpoint", rec.flashpoint);
        rec.dot_hazard_class = trim_copy(cell("DOT_Hazard_Class"));
        rec.dot_numbers = trim_copy(cell("DOT_Numbers"));
        rec.eu_number = trim_copy(cell("EU_Number"));
        assign_opt_int("NFPA_Fire_Rating", rec.nfpa_fire_rating);
        assign_opt_int("NFPA_Health_Rating", rec.nfpa_health_rating);
        assign_opt_int("NFPA_Reactivity_Rating", rec.nfpa_reactivity_rating);
        rec.rtecs_number = trim_copy(cell("RTECS_Number"));
        rec.cas_number = trim_copy(cell("CAS_Number"));
        rec.cid_number = trim_copy(cell("CID_Number"));
        rec.gmelin_number = trim_copy(cell("Gmelin_Number"));
        rec.nsc_number = trim_copy(cell("NSC_Number"));

        assign_opt_double("Percent_in_Universe", rec.percent_in_universe);
        assign_opt_double("Percent_in_Sun", rec.percent_in_sun);
        assign_opt_double("Percent_in_Meteorites", rec.percent_in_meteorites);
        assign_opt_double("Percent_in_Earths_Crust", rec.percent_in_earths_crust);
        assign_opt_double("Percent_in_Oceans", rec.percent_in_oceans);
        assign_opt_double("Percent_in_Humans", rec.percent_in_humans);

        bool sg_norm = false;
        rec.space_group_name = normalize_space_group(cell("Space_Group_Name"), sg_norm);
        if (sg_norm) {
            rec.field_provenance["Space_Group_Name"] = PropertyProvenance::CsvNormalized;
            rec.normalization_notes.push_back("Normalized multiline Space_Group_Name -> '" + rec.space_group_name + "'.");
        }
        assign_opt_int("Space_Group_Number", rec.space_group_number);

        rec.half_life_raw = trim_copy(cell("HalfLife"));
        rec.lifetime_raw = trim_copy(cell("Lifetime"));
        rec.decay_mode = trim_copy(cell("Decay_Mode"));
        if (!rec.half_life_raw.empty() && to_lower_copy(rec.half_life_raw) != "inf") {
            rec.is_radioactive_in_csv = true;
            rec.half_life_seconds = parse_double_opt(rec.half_life_raw);
        }
        assign_opt_double("Neutron_Cross_Section", rec.neutron_cross_section);
        assign_opt_double("Neutron_Mass_Absorption", rec.neutron_mass_absorption);

        const size_t idx = parsed.size();
        by_z[rec.atomic_number] = idx;
        by_sym[to_lower_copy(rec.symbol)] = idx;
        if (!rec.iupac_symbol.empty()) {
            by_sym[to_lower_copy(rec.iupac_symbol)] = idx;
        }
        parsed.push_back(std::move(rec));
    }

    if (parsed.empty()) {
        return make_csv_error(4108, "No element rows parsed from CSV");
    }

    columns_.clear();
    columns_.reserve(header.size());
    for (const auto& h : header) {
        columns_.push_back(trim_copy(h));
    }
    elements_ = std::move(parsed);
    index_by_z_ = std::move(by_z);
    index_by_symbol_lower_ = std::move(by_sym);
    source_path_ = std::move(source_label);
    return core::base::Result<void>::ok();
}

const ElementRecord* ElementDatabase::find_by_z(int atomic_number) const noexcept {
    auto it = index_by_z_.find(atomic_number);
    if (it == index_by_z_.end()) return nullptr;
    return &elements_[it->second];
}

const ElementRecord* ElementDatabase::find_by_symbol(std::string_view symbol) const noexcept {
    auto it = index_by_symbol_lower_.find(to_lower_copy(trim_copy(symbol)));
    if (it == index_by_symbol_lower_.end()) return nullptr;
    return &elements_[it->second];
}

const ElementRecord* ElementDatabase::find_by_name(std::string_view name) const noexcept {
    const std::string target = to_lower_copy(trim_copy(name));
    for (const auto& el : elements_) {
        if (to_lower_copy(el.name) == target ||
            to_lower_copy(el.iupac_name) == target ||
            to_lower_copy(el.alternate_names) == target) {
            return &el;
        }
    }
    return nullptr;
}

std::vector<const ElementRecord*> ElementDatabase::query(const ElementQuery& q) const {
    const std::string needle = to_lower_copy(trim_copy(q.search_text));
    const std::string phase_f = to_lower_copy(trim_copy(q.phase_filter));
    const std::string block_f = to_lower_copy(trim_copy(q.block_filter));
    const std::string elec_f = to_lower_copy(trim_copy(q.electrical_type_filter));
    const std::string mag_f = to_lower_copy(trim_copy(q.magnetic_type_filter));
    const std::string req_prop = trim_copy(q.require_property);

    std::vector<const ElementRecord*> out;
    out.reserve(elements_.size());

    for (const auto& el : elements_) {
        if (!needle.empty()) {
            const bool match =
                to_lower_copy(el.name).find(needle) != std::string::npos ||
                to_lower_copy(el.symbol).find(needle) != std::string::npos ||
                to_lower_copy(el.iupac_name).find(needle) != std::string::npos ||
                to_lower_copy(el.iupac_symbol).find(needle) != std::string::npos ||
                to_lower_copy(el.alternate_names).find(needle) != std::string::npos ||
                to_lower_copy(el.cas_number).find(needle) != std::string::npos ||
                std::to_string(el.atomic_number) == needle;
            if (!match) continue;
        }
        if (!phase_f.empty() && to_lower_copy(el.phase) != phase_f) continue;
        if (!block_f.empty() && to_lower_copy(el.block) != block_f) continue;
        if (!elec_f.empty() && to_lower_copy(el.electrical_type) != elec_f) continue;
        if (!mag_f.empty() && to_lower_copy(el.magnetic_type) != mag_f) continue;
        if (q.group_filter.has_value() && el.group != q.group_filter) continue;
        if (q.period_filter.has_value() && el.period != *q.period_filter) continue;
        if (!req_prop.empty()) {
            auto it = el.raw_csv_fields.find(req_prop);
            if (it == el.raw_csv_fields.end() || trim_copy(it->second).empty()) continue;
        }
        out.push_back(&el);
    }

    std::stable_sort(out.begin(), out.end(), [&](const ElementRecord* a, const ElementRecord* b) {
        if (q.sort_by == ElementSortField::Name) {
            return q.descending ? (a->name > b->name) : (a->name < b->name);
        }
        if (q.sort_by == ElementSortField::Symbol) {
            return q.descending ? (a->symbol > b->symbol) : (a->symbol < b->symbol);
        }
        bool ha = false, hb = false;
        const double va = compare_key(*a, q.sort_by, ha);
        const double vb = compare_key(*b, q.sort_by, hb);
        if (ha != hb) return ha; // present values always come before missing values
        if (!ha && !hb) return a->atomic_number < b->atomic_number;
        return q.descending ? (va > vb) : (va < vb);
    });

    return out;
}

void ElementDatabase::populate_periodic_database(core::domain::PeriodicTableDatabase& db) const {
    for (const auto& rec : elements_) {
        core::domain::PeriodicElement elem;
        elem.atomic_number = rec.atomic_number;
        elem.symbol = rec.iupac_symbol.empty() ? rec.symbol : rec.iupac_symbol;
        elem.name = rec.iupac_name.empty() ? rec.name : rec.iupac_name;
        elem.period = rec.period;
        elem.group = rec.group.value_or(rec.graph_group);
        elem.block = rec.block.empty() ? 's' : rec.block[0];
        elem.standard_atomic_weight = rec.atomic_weight.value_or(1.0);
        elem.electron_configuration = rec.electron_configuration_iupac.empty()
            ? rec.electron_configuration
            : rec.electron_configuration_iupac;
        elem.electronegativity = rec.electronegativity;
        elem.density_at_stp = rec.density;
        elem.melting_point_k = rec.melting_point;
        elem.boiling_point_k = rec.boiling_point;
        elem.specific_heat_capacity = rec.specific_heat;
        elem.thermal_conductivity = rec.thermal_conductivity;
        elem.electrical_conductivity = rec.effective_conductivity();
        (void)db.add_element(std::move(elem));
    }
}

core::base::ValidationReport ElementDatabase::audit_dataset() const {
    core::base::ValidationReport report;
    if (elements_.size() != 118) {
        report.add_error("ElementDatabase", "Expected 118 elements, loaded " + std::to_string(elements_.size()));
    }
    for (const auto& el : elements_) {
        for (const auto& note : el.normalization_notes) {
            report.add_warning(el.symbol + " (Z=" + std::to_string(el.atomic_number) + ")", note);
        }
    }
    return report;
}

} // namespace shunya::matdb
