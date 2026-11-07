#include <shunya/core/domain/dataset_release.hpp>

#include <algorithm>
#include <iomanip>
#include <regex>
#include <sstream>

namespace shunya::core::domain {

namespace {

base::Error make_rel_error(std::string msg, int32_t code = 4201) {
    return base::Error{
        .code = code,
        .domain = base::ErrorDomain::Materials,
        .severity = base::Severity::Error,
        .message = std::move(msg),
        .details = ""
    };
}

// CRC32 table for immutable payload verification
uint32_t crc32_byte(uint32_t crc, uint8_t byte) {
    crc ^= byte;
    for (int i = 0; i < 8; ++i) {
        crc = (crc >> 1) ^ (0xEDB88320u * (crc & 1));
    }
    return crc;
}

std::string compute_crc32(std::string_view data) {
    uint32_t crc = 0xFFFFFFFFu;
    for (char c : data) {
        crc = crc32_byte(crc, static_cast<uint8_t>(c));
    }
    crc ^= 0xFFFFFFFFu;
    std::ostringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(8) << crc;
    return ss.str();
}

} // namespace

DatasetRelease::DatasetRelease(base::Uuid id,
                               base::Version version,
                               std::string release_tag,
                               std::string timestamp,
                               std::string release_notes,
                               std::string snapshot_json)
    : id_(id),
      version_(version),
      release_tag_(std::move(release_tag)),
      timestamp_(std::move(timestamp)),
      release_notes_(std::move(release_notes)),
      snapshot_json_(std::move(snapshot_json)),
      content_hash_(calculate_checksum(snapshot_json_)),
      is_locked_(true) {}

std::string DatasetRelease::calculate_checksum(std::string_view payload) {
    return compute_crc32(payload);
}

bool DatasetRelease::verify_integrity() const noexcept {
    return calculate_checksum(snapshot_json_) == content_hash_;
}

base::Result<PeriodicTableDatabase> DatasetRelease::unpack_database() const {
    if (!verify_integrity()) {
        return make_rel_error("Dataset release integrity check failed: content hash mismatch");
    }
    return PeriodicTableDatabase::from_json(snapshot_json_);
}

DatasetRelease DatasetRelease::create(base::Version version,
                                     std::string release_tag,
                                     const PeriodicTableDatabase& db,
                                     std::string release_notes) {
    std::string json_payload = db.to_json();
    return DatasetRelease(
        base::Uuid::generate_random(),
        version,
        std::move(release_tag),
        "2026-11-07T00:00:00Z",
        std::move(release_notes),
        std::move(json_payload)
    );
}

std::string DatasetRelease::to_json() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"id\": \"" << id_.to_string() << "\",\n";
    ss << "  \"version\": \"" << version_.as_string() << "\",\n";
    ss << "  \"release_tag\": \"" << release_tag_ << "\",\n";
    ss << "  \"timestamp\": \"" << timestamp_ << "\",\n";
    ss << "  \"release_notes\": \"" << release_notes_ << "\",\n";
    ss << "  \"content_hash\": \"" << content_hash_ << "\",\n";
    ss << "  \"is_locked\": " << (is_locked_ ? "true" : "false") << ",\n";
    ss << "  \"snapshot\": " << (snapshot_json_.empty() ? "{}" : snapshot_json_) << "\n";
    ss << "}";
    return ss.str();
}

base::Result<DatasetRelease> DatasetRelease::from_json(std::string_view json_str) {
    std::string s(json_str);

    auto extract_str = [&](const std::string& key) -> std::string {
        std::regex re("\"" + key + R"re("\s*:\s*"([^"]*)")re");
        std::smatch m;
        if (std::regex_search(s, m, re)) {
            return m[1].str();
        }
        return "";
    };

    std::string id_str = extract_str("id");
    auto id_opt = base::Uuid::from_string(id_str);
    base::Uuid id = id_opt.has_value() ? *id_opt : base::Uuid::generate_random();

    std::string ver_str = extract_str("version");
    base::Version ver{1, 0, 0};
    if (!ver_str.empty()) {
        auto v_res = base::Version::parse(ver_str);
        if (v_res.is_ok()) ver = v_res.value();
    }

    std::string tag = extract_str("release_tag");
    std::string time_str = extract_str("timestamp");
    std::string notes = extract_str("release_notes");

    // Extract snapshot block
    std::string snap = "{}";
    size_t snap_pos = s.find("\"snapshot\"");
    if (snap_pos != std::string::npos) {
        size_t obj_start = s.find('{', snap_pos);
        if (obj_start != std::string::npos) {
            size_t depth = 0;
            size_t end_pos = std::string::npos;
            for (size_t i = obj_start; i < s.size(); ++i) {
                if (s[i] == '{') depth++;
                else if (s[i] == '}') {
                    depth--;
                    if (depth == 0) {
                        end_pos = i;
                        break;
                    }
                }
            }
            if (end_pos != std::string::npos) {
                snap = s.substr(obj_start, end_pos - obj_start + 1);
            }
        }
    }

    return DatasetRelease(id, ver, tag, time_str, notes, snap);
}

std::string DatasetDiffReport::generate_summary() const {
    std::ostringstream ss;
    ss << "Dataset Release Diff Report (" << old_version.as_string() << " -> " << new_version.as_string() << ")\n";
    ss << "==================================================\n";
    ss << "Added Elements (" << added_elements.size() << "): ";
    for (size_t i = 0; i < added_elements.size(); ++i) {
        ss << added_elements[i] << (i + 1 < added_elements.size() ? ", " : "");
    }
    ss << "\n";

    ss << "Removed Elements (" << removed_elements.size() << "): ";
    for (size_t i = 0; i < removed_elements.size(); ++i) {
        ss << removed_elements[i] << (i + 1 < removed_elements.size() ? ", " : "");
    }
    ss << "\n";

    ss << "Modified Properties (" << modified_properties.size() << "):\n";
    for (const auto& diff : modified_properties) {
        ss << "  - " << diff.element_symbol << "." << diff.property_name << ": "
           << diff.old_value << " -> " << diff.new_value;
        if (!diff.new_source.empty()) ss << " (Source: " << diff.new_source << ")";
        ss << "\n";
    }

    ss << "Impacted Physical Domains: ";
    size_t d_idx = 0;
    for (const auto& d : impacted_domains) {
        ss << d << (++d_idx < impacted_domains.size() ? ", " : "");
    }
    ss << "\n";
    return ss.str();
}

DatasetDiffReport compute_dataset_diff(const PeriodicTableDatabase& old_db,
                                      const PeriodicTableDatabase& new_db,
                                      base::Version old_ver,
                                      base::Version new_ver) {
    DatasetDiffReport report;
    report.old_version = old_ver;
    report.new_version = new_ver;

    // Check added and modified elements
    for (const auto& [z, new_el] : new_db.elements()) {
        const auto* old_el = old_db.get_element(z);
        if (!old_el) {
            report.added_elements.push_back(new_el.symbol);
            report.impacted_domains.insert("Materials");
            continue;
        }

        // Compare standard atomic weight
        if (std::abs(new_el.standard_atomic_weight - old_el->standard_atomic_weight) > 1e-5) {
            PropertyDiff pd;
            pd.element_symbol = new_el.symbol;
            pd.property_name = "standard_atomic_weight";
            pd.old_value = std::to_string(old_el->standard_atomic_weight);
            pd.new_value = std::to_string(new_el.standard_atomic_weight);
            pd.impacted_domains = {"Materials", "Nuclear"};
            report.modified_properties.push_back(pd);
            report.impacted_domains.insert("Materials");
            report.impacted_domains.insert("Nuclear");
        }

        // Compare density
        if (new_el.density_at_stp != old_el->density_at_stp) {
            PropertyDiff pd;
            pd.element_symbol = new_el.symbol;
            pd.property_name = "density";
            pd.old_value = old_el->density_at_stp ? std::to_string(*old_el->density_at_stp) : "none";
            pd.new_value = new_el.density_at_stp ? std::to_string(*new_el.density_at_stp) : "none";
            pd.impacted_domains = {"Mechanical", "Fluid"};
            report.modified_properties.push_back(pd);
            report.impacted_domains.insert("Mechanical");
        }

        // Compare thermal conductivity
        if (new_el.thermal_conductivity != old_el->thermal_conductivity) {
            PropertyDiff pd;
            pd.element_symbol = new_el.symbol;
            pd.property_name = "thermal_conductivity";
            pd.old_value = old_el->thermal_conductivity ? std::to_string(*old_el->thermal_conductivity) : "none";
            pd.new_value = new_el.thermal_conductivity ? std::to_string(*new_el.thermal_conductivity) : "none";
            pd.impacted_domains = {"Thermal"};
            report.modified_properties.push_back(pd);
            report.impacted_domains.insert("Thermal");
        }

        // Compare electrical conductivity
        if (new_el.electrical_conductivity != old_el->electrical_conductivity) {
            PropertyDiff pd;
            pd.element_symbol = new_el.symbol;
            pd.property_name = "electrical_conductivity";
            pd.old_value = old_el->electrical_conductivity ? std::to_string(*old_el->electrical_conductivity) : "none";
            pd.new_value = new_el.electrical_conductivity ? std::to_string(*new_el.electrical_conductivity) : "none";
            pd.impacted_domains = {"Electrical"};
            report.modified_properties.push_back(pd);
            report.impacted_domains.insert("Electrical");
        }

        // Compare melting point
        if (new_el.melting_point_k != old_el->melting_point_k) {
            PropertyDiff pd;
            pd.element_symbol = new_el.symbol;
            pd.property_name = "melting_point";
            pd.old_value = old_el->melting_point_k ? std::to_string(*old_el->melting_point_k) : "none";
            pd.new_value = new_el.melting_point_k ? std::to_string(*new_el.melting_point_k) : "none";
            pd.impacted_domains = {"Thermal", "PhaseTransition"};
            report.modified_properties.push_back(pd);
            report.impacted_domains.insert("Thermal");
        }
    }

    // Check removed elements
    for (const auto& [z, old_el] : old_db.elements()) {
        if (!new_db.has_element(z)) {
            report.removed_elements.push_back(old_el.symbol);
            report.impacted_domains.insert("Materials");
        }
    }

    return report;
}

bool MissingDataReport::is_fully_satisfied(std::string_view domain_name) const noexcept {
    auto it = domain_coverage_pct.find(std::string(domain_name));
    if (it != domain_coverage_pct.end()) {
        return it->second >= 99.9;
    }
    return false;
}

std::string MissingDataReport::generate_report_text() const {
    std::ostringstream ss;
    ss << "Missing Data Report for " << element_name << " (" << element_symbol << ")\n";
    ss << "Overall Completeness: " << std::fixed << std::setprecision(1) << overall_completeness_pct << "%\n";
    ss << "Domain Coverage:\n";
    for (const auto& [d, cov] : domain_coverage_pct) {
        ss << "  - " << d << ": " << cov << "%\n";
        auto m_it = missing_properties_by_domain.find(d);
        if (m_it != missing_properties_by_domain.end() && !m_it->second.empty()) {
            ss << "    Missing: ";
            for (size_t i = 0; i < m_it->second.size(); ++i) {
                ss << m_it->second[i] << (i + 1 < m_it->second.size() ? ", " : "");
            }
            ss << "\n";
        }
    }
    return ss.str();
}

std::vector<DomainRequirement> standard_domain_requirements() {
    return {
        {
            "Structural/Mechanical",
            {"density", "youngs_modulus", "poissons_ratio", "yield_strength"}
        },
        {
            "Thermal",
            {"thermal_conductivity", "specific_heat_capacity", "melting_point"}
        },
        {
            "Electrical",
            {"electrical_conductivity", "electronegativity"}
        },
        {
            "Nuclear",
            {"atomic_mass", "isotopes"}
        }
    };
}

MissingDataReport evaluate_missing_data(const PeriodicElement& elem,
                                       const std::vector<DomainRequirement>& requirements) {
    MissingDataReport rep;
    rep.element_symbol = elem.symbol;
    rep.element_name = elem.name;

    auto has_prop = [&](const std::string& prop) -> bool {
        if (prop == "density") return elem.density_at_stp.has_value();
        if (prop == "melting_point") return elem.melting_point_k.has_value();
        if (prop == "boiling_point") return elem.boiling_point_k.has_value();
        if (prop == "thermal_conductivity") return elem.thermal_conductivity.has_value();
        if (prop == "specific_heat_capacity") return elem.specific_heat_capacity.has_value();
        if (prop == "electrical_conductivity") return elem.electrical_conductivity.has_value();
        if (prop == "electronegativity") return elem.electronegativity.has_value();
        if (prop == "atomic_mass") return elem.standard_atomic_weight > 0.0;
        if (prop == "isotopes") return !elem.isotopes.empty();
        return elem.audited_fields.find(prop) != elem.audited_fields.end();
    };

    size_t total_props = 0;
    size_t present_props = 0;

    for (const auto& req : requirements) {
        size_t d_total = req.required_properties.size();
        size_t d_present = 0;
        std::vector<std::string> missing;

        for (const auto& prop : req.required_properties) {
            total_props++;
            if (has_prop(prop)) {
                d_present++;
                present_props++;
            } else {
                missing.push_back(prop);
            }
        }

        double cov = (d_total > 0) ? (100.0 * static_cast<double>(d_present) / static_cast<double>(d_total)) : 100.0;
        rep.domain_coverage_pct[req.domain_name] = cov;
        rep.missing_properties_by_domain[req.domain_name] = std::move(missing);
    }

    rep.overall_completeness_pct = (total_props > 0)
        ? (100.0 * static_cast<double>(present_props) / static_cast<double>(total_props))
        : 100.0;

    return rep;
}

} // namespace shunya::core::domain
