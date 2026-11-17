#include "shunya/mat/shunyamat_serializer.hpp"

#include <fstream>
#include <iomanip>
#include <sstream>

namespace shunya::mat {

namespace {

std::string extract_json_string(std::string_view json, std::string_view key, std::string_view default_val = "") {
    const std::string pattern = "\"" + std::string(key) + "\":";
    auto pos = json.find(pattern);
    if (pos == std::string_view::npos) return std::string(default_val);
    pos += pattern.size();
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\r' || json[pos] == '\n')) {
        ++pos;
    }
    if (pos < json.size() && json[pos] == '\"') {
        ++pos;
        auto end_pos = json.find('\"', pos);
        if (end_pos != std::string_view::npos) {
            return std::string(json.substr(pos, end_pos - pos));
        }
    }
    return std::string(default_val);
}

double extract_json_double(std::string_view json, std::string_view key, double default_val = 0.0) {
    const std::string pattern = "\"" + std::string(key) + "\":";
    auto pos = json.find(pattern);
    if (pos == std::string_view::npos) return default_val;
    pos += pattern.size();
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\r' || json[pos] == '\n')) {
        ++pos;
    }
    if (pos >= json.size()) return default_val;
    size_t end_pos = pos;
    while (end_pos < json.size() &&
           (std::isdigit(json[end_pos]) || json[end_pos] == '.' || json[end_pos] == '-' ||
            json[end_pos] == '+' || json[end_pos] == 'e' || json[end_pos] == 'E')) {
        ++end_pos;
    }
    try {
        return std::stod(std::string(json.substr(pos, end_pos - pos)));
    } catch (...) {
        return default_val;
    }
}

} // namespace

std::string ShunyaMatSerializer::export_to_json(
    const MaterialPropertyBundle& bundle,
    const MaterialProvenanceMetadata& provenance) noexcept {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"format\": \"" << FORMAT_IDENTIFIER << "\",\n";
    ss << "  \"version\": \"" << FORMAT_VERSION << "\",\n";
    ss << "  \"material_id\": \"" << bundle.material_id << "\",\n";
    ss << "  \"material_name\": \"" << bundle.material_name << "\",\n";

    // Provenance
    ss << "  \"provenance\": {\n";
    ss << "    \"creator\": \"" << provenance.creator << "\",\n";
    ss << "    \"timestamp\": \"" << provenance.creation_timestamp_iso8601 << "\",\n";
    ss << "    \"standard_reference\": \"" << provenance.standard_reference << "\",\n";
    ss << "    \"notes\": \"" << provenance.notes << "\"\n";
    ss << "  },\n";

    // Chemical
    ss << "  \"chemical\": {\n";
    ss << "    \"chemical_formula\": \"" << bundle.chemical.chemical_formula << "\",\n";
    ss << "    \"molecular_weight_g_mol\": " << bundle.chemical.molecular_weight_g_mol << ",\n";
    ss << "    \"corrosion_resistance_tier\": " << static_cast<int>(bundle.chemical.corrosion_resistance) << "\n";
    ss << "  },\n";

    // Mechanical
    ss << "  \"mechanical\": {\n";
    ss << "    \"density_kg_m3\": " << bundle.mechanical.density_kg_m3 << ",\n";
    ss << "    \"young_modulus_pa\": " << bundle.mechanical.young_modulus_pa << ",\n";
    ss << "    \"shear_modulus_pa\": " << bundle.mechanical.shear_modulus_pa << ",\n";
    ss << "    \"bulk_modulus_pa\": " << bundle.mechanical.bulk_modulus_pa << ",\n";
    ss << "    \"poisson_ratio\": " << bundle.mechanical.poisson_ratio << ",\n";
    ss << "    \"yield_strength_pa\": " << bundle.mechanical.yield_strength_pa << ",\n";
    ss << "    \"ultimate_tensile_strength_pa\": " << bundle.mechanical.ultimate_tensile_strength_pa << ",\n";
    ss << "    \"fracture_toughness_k1c_mpa_m05\": " << bundle.mechanical.fracture_toughness_k1c_mpa_m05 << "\n";
    ss << "  },\n";

    // Thermal
    ss << "  \"thermal\": {\n";
    ss << "    \"melting_point_k\": " << bundle.thermal.melting_point_k << ",\n";
    ss << "    \"boiling_point_k\": " << bundle.thermal.boiling_point_k << ",\n";
    ss << "    \"base_thermal_conductivity_w_mk\": " << bundle.thermal.base_thermal_conductivity_w_mk << ",\n";
    ss << "    \"base_specific_heat_j_kgk\": " << bundle.thermal.base_specific_heat_j_kgk << ",\n";
    ss << "    \"linear_thermal_expansion_1_k\": " << bundle.thermal.linear_thermal_expansion_1_k << ",\n";
    ss << "    \"heat_of_fusion_j_kg\": " << bundle.thermal.heat_of_fusion_j_kg << "\n";
    ss << "  },\n";

    // Electrical
    ss << "  \"electrical\": {\n";
    ss << "    \"base_conductivity_s_m\": " << bundle.electrical.base_conductivity_s_m << ",\n";
    ss << "    \"base_resistivity_ohm_m\": " << bundle.electrical.base_resistivity_ohm_m << ",\n";
    ss << "    \"tcr_per_k\": " << bundle.electrical.tcr_per_k << ",\n";
    ss << "    \"reference_temp_k\": " << bundle.electrical.reference_temp_k << ",\n";
    ss << "    \"carrier_density_m3\": " << bundle.electrical.carrier_density_m3 << ",\n";
    ss << "    \"relative_permittivity\": " << bundle.electrical.relative_permittivity << ",\n";
    ss << "    \"breakdown_field_v_m\": " << bundle.electrical.breakdown_field_v_m << "\n";
    ss << "  },\n";

    // Environmental
    ss << "  \"environmental\": {\n";
    ss << "    \"pilling_bedworth_ratio\": " << bundle.environmental.pilling_bedworth_ratio << ",\n";
    ss << "    \"standard_reduction_potential_v\": " << bundle.environmental.standard_reduction_potential_v << ",\n";
    ss << "    \"saturation_moisture_content_pct\": " << bundle.environmental.saturation_moisture_content_pct << ",\n";
    ss << "    \"moisture_diffusion_coefficient_m2_s\": " << bundle.environmental.moisture_diffusion_coefficient_m2_s << "\n";
    ss << "  }\n";
    ss << "}\n";

    return ss.str();
}

core::base::Result<MaterialPropertyBundle> ShunyaMatSerializer::import_from_json(
    std::string_view json_str) noexcept {
    const std::string format = extract_json_string(json_str, "format");
    if (format != FORMAT_IDENTIFIER) {
        return core::base::Result<MaterialPropertyBundle>::err(
            core::base::Error{1, core::base::ErrorDomain::Materials, core::base::Severity::Error,
                              "Invalid .shunyamat package: missing or incorrect format identifier.", ""});
    }

    MaterialPropertyBundle b;
    b.material_id = extract_json_string(json_str, "material_id", "unnamed-material");
    b.material_name = extract_json_string(json_str, "material_name", "Unnamed Material");

    // Chemical
    b.chemical.chemical_formula = extract_json_string(json_str, "chemical_formula", "X");
    b.chemical.molecular_weight_g_mol = extract_json_double(json_str, "molecular_weight_g_mol", 1.0);
    b.chemical.corrosion_resistance = static_cast<CorrosionResistanceClass>(
        static_cast<int>(extract_json_double(json_str, "corrosion_resistance_tier", 2.0)));

    // Mechanical
    b.mechanical.density_kg_m3 = extract_json_double(json_str, "density_kg_m3", 1000.0);
    b.mechanical.young_modulus_pa = extract_json_double(json_str, "young_modulus_pa", 1e9);
    b.mechanical.shear_modulus_pa = extract_json_double(json_str, "shear_modulus_pa", 0.0);
    b.mechanical.bulk_modulus_pa = extract_json_double(json_str, "bulk_modulus_pa", 0.0);
    b.mechanical.poisson_ratio = extract_json_double(json_str, "poisson_ratio", 0.3);
    b.mechanical.yield_strength_pa = extract_json_double(json_str, "yield_strength_pa", 0.0);
    b.mechanical.ultimate_tensile_strength_pa = extract_json_double(json_str, "ultimate_tensile_strength_pa", 0.0);
    b.mechanical.fracture_toughness_k1c_mpa_m05 = extract_json_double(json_str, "fracture_toughness_k1c_mpa_m05", 0.0);

    // Thermal
    b.thermal.melting_point_k = extract_json_double(json_str, "melting_point_k", 1000.0);
    b.thermal.boiling_point_k = extract_json_double(json_str, "boiling_point_k", 2000.0);
    b.thermal.base_thermal_conductivity_w_mk = extract_json_double(json_str, "base_thermal_conductivity_w_mk", 1.0);
    b.thermal.base_specific_heat_j_kgk = extract_json_double(json_str, "base_specific_heat_j_kgk", 500.0);
    b.thermal.linear_thermal_expansion_1_k = extract_json_double(json_str, "linear_thermal_expansion_1_k", 1e-5);
    b.thermal.heat_of_fusion_j_kg = extract_json_double(json_str, "heat_of_fusion_j_kg", 0.0);

    // Electrical
    b.electrical.base_conductivity_s_m = extract_json_double(json_str, "base_conductivity_s_m", 1.0);
    b.electrical.base_resistivity_ohm_m = extract_json_double(json_str, "base_resistivity_ohm_m", 1.0);
    b.electrical.tcr_per_k = extract_json_double(json_str, "tcr_per_k", 0.0039);
    b.electrical.reference_temp_k = extract_json_double(json_str, "reference_temp_k", 293.15);
    b.electrical.carrier_density_m3 = extract_json_double(json_str, "carrier_density_m3", 0.0);
    b.electrical.relative_permittivity = extract_json_double(json_str, "relative_permittivity", 1.0);
    b.electrical.breakdown_field_v_m = extract_json_double(json_str, "breakdown_field_v_m", 0.0);

    // Environmental
    b.environmental.pilling_bedworth_ratio = extract_json_double(json_str, "pilling_bedworth_ratio", 1.0);
    b.environmental.standard_reduction_potential_v = extract_json_double(json_str, "standard_reduction_potential_v", 0.0);
    b.environmental.saturation_moisture_content_pct = extract_json_double(json_str, "saturation_moisture_content_pct", 0.0);
    b.environmental.moisture_diffusion_coefficient_m2_s = extract_json_double(json_str, "moisture_diffusion_coefficient_m2_s", 0.0);

    return core::base::Result<MaterialPropertyBundle>::ok(std::move(b));
}

core::base::Result<void> ShunyaMatSerializer::export_to_file(
    const MaterialPropertyBundle& bundle,
    const std::filesystem::path& file_path,
    const MaterialProvenanceMetadata& provenance) noexcept {
    try {
        std::ofstream ofs(file_path);
        if (!ofs.is_open()) {
            return core::base::Result<void>::err(
                core::base::Error{2, core::base::ErrorDomain::IO, core::base::Severity::Error,
                                  "Failed to open file for export: " + file_path.string(), ""});
        }
        ofs << export_to_json(bundle, provenance);
        return core::base::Result<void>::ok();
    } catch (const std::exception& ex) {
        return core::base::Result<void>::err(
            core::base::Error{3, core::base::ErrorDomain::IO, core::base::Severity::Error, ex.what(), ""});
    }
}

core::base::Result<MaterialPropertyBundle> ShunyaMatSerializer::import_from_file(
    const std::filesystem::path& file_path) noexcept {
    try {
        std::ifstream ifs(file_path);
        if (!ifs.is_open()) {
            return core::base::Result<MaterialPropertyBundle>::err(
                core::base::Error{4, core::base::ErrorDomain::IO, core::base::Severity::Error,
                                  "Failed to open file for import: " + file_path.string(), ""});
        }
        std::stringstream buffer;
        buffer << ifs.rdbuf();
        return import_from_json(buffer.str());
    } catch (const std::exception& ex) {
        return core::base::Result<MaterialPropertyBundle>::err(
            core::base::Error{5, core::base::ErrorDomain::IO, core::base::Severity::Error, ex.what(), ""});
    }
}

} // namespace shunya::mat
