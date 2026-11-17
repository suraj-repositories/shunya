#include "shunya/ecad3d/component_manifest.hpp"

#include "shunya/core/base/error.hpp"
#include "shunya/core/base/uuid.hpp"

#include <iomanip>
#include <sstream>

namespace shunya::ecad3d {

namespace {

inline core::base::Result<void> make_fail(std::string msg) {
    core::base::Error err;
    err.message = std::move(msg);
    err.domain = core::base::ErrorDomain::Validation;
    return core::base::Result<void>(std::move(err));
}

inline core::base::Result<void> make_ok() {
    return core::base::Result<void>();
}

std::string extract_str(std::string_view json, std::string_view key, std::string_view def_val = "") {
    std::string pattern = "\"" + std::string(key) + "\":";
    auto pos = json.find(pattern);
    if (pos == std::string_view::npos) return std::string(def_val);
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
    return std::string(def_val);
}

double extract_num(std::string_view json, std::string_view key, double def_val = 0.0) {
    std::string pattern = "\"" + std::string(key) + "\":";
    auto pos = json.find(pattern);
    if (pos == std::string_view::npos) return def_val;
    pos += pattern.size();
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\r' || json[pos] == '\n')) {
        ++pos;
    }
    if (pos >= json.size()) return def_val;
    size_t end_pos = pos;
    while (end_pos < json.size() &&
           (std::isdigit(json[end_pos]) || json[end_pos] == '.' || json[end_pos] == '-' ||
            json[end_pos] == '+' || json[end_pos] == 'e' || json[end_pos] == 'E')) {
        ++end_pos;
    }
    try {
        return std::stod(std::string(json.substr(pos, end_pos - pos)));
    } catch (...) {
        return def_val;
    }
}

bool extract_bool(std::string_view json, std::string_view key, bool def_val = false) {
    std::string pattern = "\"" + std::string(key) + "\":";
    auto pos = json.find(pattern);
    if (pos == std::string_view::npos) return def_val;
    pos += pattern.size();
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\r' || json[pos] == '\n')) {
        ++pos;
    }
    if (json.substr(pos, 4) == "true") return true;
    if (json.substr(pos, 5) == "false") return false;
    return def_val;
}

} // namespace

ComponentManifest ComponentManifestSerializer::create_manifest(
    const ComponentAuthoringData& authoring_data,
    const SchematicSymbol2D& symbol_2d,
    const PinTerminalMapper& mapper,
    const ComponentQualificationInspector::QualificationCertificate& cert
) {
    ComponentManifest m;
    m.component_guid = core::base::Uuid::generate_random().to_string();
    m.name = authoring_data.name;
    m.kind = authoring_data.kind;
    m.manufacturer = authoring_data.metadata.manufacturer;
    m.part_number = authoring_data.metadata.part_number;
    m.package_type = authoring_data.metadata.package_type;
    m.description = authoring_data.metadata.description;

    m.specs = authoring_data.specs;
    m.spice_model_equation = authoring_data.spice_model.netlist_template;

    m.material_dependencies = authoring_data.selected_material_names;
    m.terminals = authoring_data.terminals;
    m.vertex_count = authoring_data.mesh_geometry.vertices.size();
    m.triangle_count = authoring_data.mesh_geometry.triangles.size();

    m.symbol_2d = symbol_2d;
    m.pin_mappings = mapper.mappings();
    m.mounting_orientation = mapper.mounting_orientation();

    m.is_qualified = cert.is_qualified;
    m.certification_id = cert.certification_id;

    return m;
}

std::string ComponentManifestSerializer::to_json(const ComponentManifest& m, bool /*pretty*/) {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"schema\": \"" << m.schema_identifier << "\",\n";
    ss << "  \"version\": \"" << m.schema_version << "\",\n";
    ss << "  \"guid\": \"" << m.component_guid << "\",\n";
    ss << "  \"name\": \"" << m.name << "\",\n";
    ss << "  \"kind\": " << static_cast<int>(m.kind) << ",\n";
    ss << "  \"manufacturer\": \"" << m.manufacturer << "\",\n";
    ss << "  \"part_number\": \"" << m.part_number << "\",\n";
    ss << "  \"package_type\": \"" << m.package_type << "\",\n";
    ss << "  \"description\": \"" << m.description << "\",\n";

    // Nominal specs
    ss << "  \"specs\": {\n";
    ss << "    \"nominal_value\": " << m.specs.nominal_primary_value << ",\n";
    ss << "    \"max_voltage\": " << m.specs.max_voltage_v << ",\n";
    ss << "    \"max_current\": " << m.specs.max_current_a << ",\n";
    ss << "    \"power_rating\": " << m.specs.max_power_dissipation_w << ",\n";
    ss << "    \"tolerance_percent\": " << m.specs.tolerance_percent << "\n";
    ss << "  },\n";

    ss << "  \"spice_equation\": \"" << m.spice_model_equation << "\",\n";

    // Geometry summary
    ss << "  \"geometry\": {\n";
    ss << "    \"vertices\": " << m.vertex_count << ",\n";
    ss << "    \"triangles\": " << m.triangle_count << "\n";
    ss << "  },\n";

    // Materials
    ss << "  \"materials\": [";
    for (size_t i = 0; i < m.material_dependencies.size(); ++i) {
        ss << "\"" << m.material_dependencies[i] << "\"";
        if (i + 1 < m.material_dependencies.size()) ss << ", ";
    }
    ss << "],\n";

    // Terminals
    ss << "  \"terminals\": [";
    for (size_t i = 0; i < m.terminals.size(); ++i) {
        const auto& t = m.terminals[i];
        ss << "{\"id\": \"" << t.terminal_id << "\", \"pin\": " << t.pin_number
           << ", \"polarity\": " << static_cast<int>(t.polarity) << "}";
        if (i + 1 < m.terminals.size()) ss << ", ";
    }
    ss << "],\n";

    // Symbol
    ss << "  \"symbol\": {\n";
    ss << "    \"id\": \"" << m.symbol_2d.symbol_id << "\",\n";
    ss << "    \"standard\": \"" << m.symbol_2d.standard << "\",\n";
    ss << "    \"pins_count\": " << m.symbol_2d.pins.size() << "\n";
    ss << "  },\n";

    // Qualification
    ss << "  \"qualification\": {\n";
    ss << "    \"is_qualified\": " << (m.is_qualified ? "true" : "false") << ",\n";
    ss << "    \"certification_id\": \"" << m.certification_id << "\"\n";
    ss << "  }\n";
    ss << "}\n";

    return ss.str();
}

core::base::Result<void> ComponentManifestSerializer::validate_schema(std::string_view json_str) {
    if (json_str.empty()) {
        return make_fail("Empty component manifest JSON");
    }

    auto schema = extract_str(json_str, "schema");
    if (schema != SCHEMA_IDENTIFIER) {
        return make_fail("Invalid schema identifier '" + schema + "', expected '" + std::string(SCHEMA_IDENTIFIER) + "'");
    }

    auto version = extract_str(json_str, "version");
    if (version.empty()) {
        return make_fail("Manifest missing 'version' tag");
    }

    auto guid = extract_str(json_str, "guid");
    if (guid.empty()) {
        return make_fail("Manifest missing 'guid' field");
    }

    auto name = extract_str(json_str, "name");
    if (name.empty()) {
        return make_fail("Manifest missing component 'name' field");
    }

    return make_ok();
}

core::base::Result<ComponentManifest> ComponentManifestSerializer::from_json(std::string_view json_str) {
    auto valid_res = validate_schema(json_str);
    if (valid_res.is_err()) {
        core::base::Error err;
        err.message = valid_res.error().message;
        err.domain = core::base::ErrorDomain::Validation;
        return core::base::Result<ComponentManifest>(std::move(err));
    }

    ComponentManifest m;
    m.schema_identifier = extract_str(json_str, "schema", SCHEMA_IDENTIFIER);
    m.schema_version = extract_str(json_str, "version", CURRENT_VERSION);
    m.component_guid = extract_str(json_str, "guid");
    m.name = extract_str(json_str, "name");
    m.kind = static_cast<eng::ComponentKind>(static_cast<int>(extract_num(json_str, "kind", 0.0)));
    m.manufacturer = extract_str(json_str, "manufacturer");
    m.part_number = extract_str(json_str, "part_number");
    m.package_type = extract_str(json_str, "package_type");
    m.description = extract_str(json_str, "description");
    m.spice_model_equation = extract_str(json_str, "spice_equation");

    m.specs.nominal_primary_value = extract_num(json_str, "nominal_value", 0.0);
    m.specs.max_voltage_v = extract_num(json_str, "max_voltage", 0.0);
    m.specs.max_current_a = extract_num(json_str, "max_current", 0.0);
    m.specs.max_power_dissipation_w = extract_num(json_str, "power_rating", 0.0);
    m.specs.tolerance_percent = extract_num(json_str, "tolerance_percent", 0.0);

    m.vertex_count = static_cast<size_t>(extract_num(json_str, "vertices", 0.0));
    m.triangle_count = static_cast<size_t>(extract_num(json_str, "triangles", 0.0));

    m.is_qualified = extract_bool(json_str, "is_qualified", false);
    m.certification_id = extract_str(json_str, "certification_id");

    return core::base::Result<ComponentManifest>(std::move(m));
}

} // namespace shunya::ecad3d
