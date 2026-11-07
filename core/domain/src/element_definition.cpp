#include "shunya/core/domain/element_definition.hpp"

#include <sstream>
#include <algorithm>
#include <regex>

namespace shunya::core::domain {

namespace {

std::string escape_json(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else out += c;
    }
    return out;
}

} // namespace

std::optional<ElementCategory> element_category_from_string(std::string_view s) noexcept {
    if (s == "PhysicalObject") return ElementCategory::PhysicalObject;
    if (s == "Material")       return ElementCategory::Material;
    if (s == "Field")          return ElementCategory::Field;
    if (s == "Source")         return ElementCategory::Source;
    if (s == "Sensor")         return ElementCategory::Sensor;
    if (s == "Actuator")       return ElementCategory::Actuator;
    if (s == "Device")         return ElementCategory::Device;
    if (s == "LogicBlock")     return ElementCategory::LogicBlock;
    if (s == "AbstractNode")   return ElementCategory::AbstractNode;
    return std::nullopt;
}

std::optional<PortDomain> port_domain_from_string(std::string_view s) noexcept {
    if (s == "Electrical")              return PortDomain::Electrical;
    if (s == "MechanicalTranslational") return PortDomain::MechanicalTranslational;
    if (s == "MechanicalRotational")    return PortDomain::MechanicalRotational;
    if (s == "Thermal")                 return PortDomain::Thermal;
    if (s == "Fluid")                   return PortDomain::Fluid;
    if (s == "Signal")                  return PortDomain::Signal;
    if (s == "Optical")                 return PortDomain::Optical;
    return std::nullopt;
}

std::string ParameterValue::to_string() const {
    return std::visit([](auto&& arg) -> std::string {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, double>) {
            return std::to_string(arg);
        } else if constexpr (std::is_same_v<T, int64_t>) {
            return std::to_string(arg);
        } else if constexpr (std::is_same_v<T, bool>) {
            return arg ? "true" : "false";
        } else if constexpr (std::is_same_v<T, std::string>) {
            return arg;
        } else if constexpr (std::is_same_v<T, base::units::Quantity>) {
            return std::to_string(arg.value) + " [" + arg.dimension.symbol() + "]";
        } else if constexpr (std::is_same_v<T, base::math::Vec3>) {
            return "(" + std::to_string(arg.x) + ", " + std::to_string(arg.y) + ", " + std::to_string(arg.z) + ")";
        }
        return "";
    }, storage_);
}

base::Result<void> ParameterSchema::validate(const ParameterValue& value) const {
    // Type validation
    if (type == ParameterType::Double) {
        if (value.type() != ParameterType::Double && value.type() != ParameterType::Int64) {
            return base::Result<void>::err({
                .code = 3101,
                .domain = base::ErrorDomain::Validation,
                .severity = base::Severity::Error,
                .message = "Parameter '" + name + "' expected Double, got " + std::string(parameter_type_to_string(value.type())),
                .details = ""
            });
        }
        double d = (value.type() == ParameterType::Double) ? value.as_double() : static_cast<double>(value.as_int64());
        if (min_value.has_value() && d < min_value.value()) {
            return base::Result<void>::err({
                .code = 3102,
                .domain = base::ErrorDomain::Validation,
                .severity = base::Severity::Error,
                .message = "Parameter '" + name + "' value " + std::to_string(d) + " is below minimum allowed " + std::to_string(min_value.value()),
                .details = ""
            });
        }
        if (max_value.has_value() && d > max_value.value()) {
            return base::Result<void>::err({
                .code = 3103,
                .domain = base::ErrorDomain::Validation,
                .severity = base::Severity::Error,
                .message = "Parameter '" + name + "' value " + std::to_string(d) + " exceeds maximum allowed " + std::to_string(max_value.value()),
                .details = ""
            });
        }
    } else if (type == ParameterType::Int64) {
        if (value.type() != ParameterType::Int64) {
            return base::Result<void>::err({
                .code = 3101,
                .domain = base::ErrorDomain::Validation,
                .severity = base::Severity::Error,
                .message = "Parameter '" + name + "' expected Int64, got " + std::string(parameter_type_to_string(value.type())),
                .details = ""
            });
        }
        int64_t val = value.as_int64();
        if (min_value.has_value() && static_cast<double>(val) < min_value.value()) {
            return base::Result<void>::err({
                .code = 3102,
                .domain = base::ErrorDomain::Validation,
                .severity = base::Severity::Error,
                .message = "Parameter '" + name + "' value is below minimum allowed",
                .details = ""
            });
        }
        if (max_value.has_value() && static_cast<double>(val) > max_value.value()) {
            return base::Result<void>::err({
                .code = 3103,
                .domain = base::ErrorDomain::Validation,
                .severity = base::Severity::Error,
                .message = "Parameter '" + name + "' value exceeds maximum allowed",
                .details = ""
            });
        }
    } else if (type == ParameterType::Enum) {
        if (value.type() != ParameterType::String) {
            return base::Result<void>::err({
                .code = 3101,
                .domain = base::ErrorDomain::Validation,
                .severity = base::Severity::Error,
                .message = "Parameter '" + name + "' expected String for Enum option",
                .details = ""
            });
        }
        const auto& s = value.as_string();
        if (std::find(enum_options.begin(), enum_options.end(), s) == enum_options.end()) {
            return base::Result<void>::err({
                .code = 3104,
                .domain = base::ErrorDomain::Validation,
                .severity = base::Severity::Error,
                .message = "Parameter '" + name + "' value '" + s + "' is not a valid enum choice",
                .details = ""
            });
        }
    } else if (type == ParameterType::Quantity) {
        if (value.type() != ParameterType::Quantity && value.type() != ParameterType::Double && value.type() != ParameterType::Int64) {
            return base::Result<void>::err({
                .code = 3101,
                .domain = base::ErrorDomain::Validation,
                .severity = base::Severity::Error,
                .message = "Parameter '" + name + "' expected Quantity or Double, got " + std::string(parameter_type_to_string(value.type())),
                .details = ""
            });
        }
    } else if (type != value.type()) {
        return base::Result<void>::err({
            .code = 3101,
            .domain = base::ErrorDomain::Validation,
            .severity = base::Severity::Error,
            .message = "Parameter '" + name + "' type mismatch: expected " + std::string(parameter_type_to_string(type)) + ", got " + std::string(parameter_type_to_string(value.type())),
            .details = ""
        });
    }

    return base::Result<void>::ok();
}

bool PortSchema::can_connect_to(const PortSchema& other) const noexcept {
    // 1. Domains must match identically
    if (domain != other.domain) return false;

    // 2. Direction check
    if (direction == PortDirection::Input && other.direction == PortDirection::Input) {
        return false; // Cannot connect two inputs together
    }
    if (direction == PortDirection::Output && other.direction == PortDirection::Output) {
        return false; // Cannot connect two outputs together
    }

    return true;
}

ElementDefinition::ElementDefinition(
    std::string type_name,
    std::string display_name,
    ElementCategory category,
    base::Version version,
    base::Uuid id
) : id_(id),
    type_name_(std::move(type_name)),
    display_name_(std::move(display_name)),
    category_(category),
    version_(version) {}

void ElementDefinition::add_parameter(ParameterSchema schema) {
    auto it = std::find_if(parameters_.begin(), parameters_.end(), [&](const ParameterSchema& p) {
        return p.name == schema.name;
    });
    if (it != parameters_.end()) {
        *it = std::move(schema);
    } else {
        parameters_.push_back(std::move(schema));
    }
}

bool ElementDefinition::has_parameter(std::string_view param_name) const noexcept {
    return get_parameter_schema(param_name) != nullptr;
}

const ParameterSchema* ElementDefinition::get_parameter_schema(std::string_view param_name) const noexcept {
    for (const auto& p : parameters_) {
        if (p.name == param_name) return &p;
    }
    return nullptr;
}

void ElementDefinition::add_port(PortSchema schema) {
    auto it = std::find_if(ports_.begin(), ports_.end(), [&](const PortSchema& p) {
        return p.name == schema.name;
    });
    if (it != ports_.end()) {
        *it = std::move(schema);
    } else {
        ports_.push_back(std::move(schema));
    }
}

bool ElementDefinition::has_port(std::string_view port_name) const noexcept {
    return get_port_schema(port_name) != nullptr;
}

const PortSchema* ElementDefinition::get_port_schema(std::string_view port_name) const noexcept {
    for (const auto& p : ports_) {
        if (p.name == port_name) return &p;
    }
    return nullptr;
}

std::optional<std::string_view> ElementDefinition::get_metadata(std::string_view key) const noexcept {
    auto it = metadata_.find(std::string(key));
    if (it != metadata_.end()) return it->second;
    return std::nullopt;
}

std::string ElementDefinition::to_json() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"id\": \"" << id_.to_string() << "\",\n";
    ss << "  \"type_name\": \"" << escape_json(type_name_) << "\",\n";
    ss << "  \"display_name\": \"" << escape_json(display_name_) << "\",\n";
    ss << "  \"category\": \"" << element_category_to_string(category_) << "\",\n";
    ss << "  \"version\": \"" << version_.as_string() << "\",\n";
    ss << "  \"description\": \"" << escape_json(description_) << "\",\n";
    ss << "  \"author\": \"" << escape_json(author_) << "\",\n";

    // Parameters
    ss << "  \"parameters\": [\n";
    for (std::size_t i = 0; i < parameters_.size(); ++i) {
        const auto& p = parameters_[i];
        ss << "    {\n";
        ss << "      \"name\": \"" << escape_json(p.name) << "\",\n";
        ss << "      \"display_name\": \"" << escape_json(p.display_name) << "\",\n";
        ss << "      \"type\": \"" << parameter_type_to_string(p.type) << "\",\n";
        ss << "      \"unit\": \"" << escape_json(p.unit) << "\",\n";
        ss << "      \"is_read_only\": " << (p.is_read_only ? "true" : "false") << ",\n";
        ss << "      \"is_visible\": " << (p.is_visible ? "true" : "false") << "\n";
        ss << "    }";
        if (i + 1 < parameters_.size()) ss << ",";
        ss << "\n";
    }
    ss << "  ],\n";

    // Ports
    ss << "  \"ports\": [\n";
    for (std::size_t i = 0; i < ports_.size(); ++i) {
        const auto& pt = ports_[i];
        ss << "    {\n";
        ss << "      \"name\": \"" << escape_json(pt.name) << "\",\n";
        ss << "      \"domain\": \"" << port_domain_to_string(pt.domain) << "\",\n";
        ss << "      \"direction\": \"" << port_direction_to_string(pt.direction) << "\",\n";
        ss << "      \"unit\": \"" << escape_json(pt.expected_unit) << "\"\n";
        ss << "    }";
        if (i + 1 < ports_.size()) ss << ",";
        ss << "\n";
    }
    ss << "  ]\n";
    ss << "}\n";

    return ss.str();
}

base::Result<std::shared_ptr<ElementDefinition>> ElementDefinition::from_json(std::string_view json_str) {
    if (json_str.empty()) {
        return base::Result<std::shared_ptr<ElementDefinition>>::err({
            .code = 3110,
            .domain = base::ErrorDomain::IO,
            .severity = base::Severity::Error,
            .message = "Empty element definition JSON",
            .details = ""
        });
    }

    std::string s(json_str);
    static const std::regex id_re(R"re("id"\s*:\s*"([0-9a-fA-F-]+)")re");
    static const std::regex type_re(R"re("type_name"\s*:\s*"([^"]*)")re");
    static const std::regex name_re(R"re("display_name"\s*:\s*"([^"]*)")re");
    static const std::regex cat_re(R"re("category"\s*:\s*"([^"]*)")re");
    static const std::regex ver_re(R"re("version"\s*:\s*"([^"]*)")re");

    std::smatch id_m, type_m, name_m, cat_m, ver_m;
    if (!std::regex_search(s, id_m, id_re) || !std::regex_search(s, type_m, type_re)) {
        return base::Result<std::shared_ptr<ElementDefinition>>::err({
            .code = 3111,
            .domain = base::ErrorDomain::Validation,
            .severity = base::Severity::Error,
            .message = "Element definition missing required id or type_name",
            .details = ""
        });
    }

    auto uid_opt = base::Uuid::from_string(id_m[1].str());
    base::Uuid id = uid_opt.has_value() ? uid_opt.value() : base::Uuid::generate_random();

    std::string type_name = type_m[1].str();
    std::string display_name = std::regex_search(s, name_m, name_re) ? name_m[1].str() : type_name;

    ElementCategory category = ElementCategory::PhysicalObject;
    if (std::regex_search(s, cat_m, cat_re)) {
        auto cat_opt = element_category_from_string(cat_m[1].str());
        if (cat_opt.has_value()) category = cat_opt.value();
    }

    base::Version ver{1, 0, 0, "final"};
    if (std::regex_search(s, ver_m, ver_re)) {
        auto ver_res = base::Version::parse(ver_m[1].str());
        if (ver_res.is_ok()) ver = ver_res.value();
    }

    auto def = std::make_shared<ElementDefinition>(type_name, display_name, category, ver, id);

    // Extract parameters
    static const std::regex param_re(R"re(\{\s*"name"\s*:\s*"([^"]*)"\s*,\s*"display_name"\s*:\s*"([^"]*)"\s*,\s*"type"\s*:\s*"([^"]*)"\s*,\s*"unit"\s*:\s*"([^"]*)")re");
    auto p_begin = std::sregex_iterator(s.begin(), s.end(), param_re);
    auto p_end = std::sregex_iterator();
    for (auto it = p_begin; it != p_end; ++it) {
        std::smatch m = *it;
        ParameterSchema ps;
        ps.name = m[1].str();
        ps.display_name = m[2].str();
        std::string t_str = m[3].str();
        if (t_str == "Int64") ps.type = ParameterType::Int64;
        else if (t_str == "Bool") ps.type = ParameterType::Bool;
        else if (t_str == "String") ps.type = ParameterType::String;
        else if (t_str == "Quantity") ps.type = ParameterType::Quantity;
        else if (t_str == "Vec3") ps.type = ParameterType::Vec3;
        else if (t_str == "Enum") ps.type = ParameterType::Enum;
        else ps.type = ParameterType::Double;
        ps.unit = m[4].str();
        def->add_parameter(std::move(ps));
    }

    // Extract ports
    static const std::regex port_re(R"re(\{\s*"name"\s*:\s*"([^"]*)"\s*,\s*"domain"\s*:\s*"([^"]*)"\s*,\s*"direction"\s*:\s*"([^"]*)"\s*,\s*"unit"\s*:\s*"([^"]*)")re");
    auto pt_begin = std::sregex_iterator(s.begin(), s.end(), port_re);
    for (auto it = pt_begin; it != p_end; ++it) {
        std::smatch m = *it;
        PortSchema pts;
        pts.name = m[1].str();
        auto dom_opt = port_domain_from_string(m[2].str());
        pts.domain = dom_opt.has_value() ? dom_opt.value() : PortDomain::Electrical;
        std::string dir_str = m[3].str();
        if (dir_str == "Input") pts.direction = PortDirection::Input;
        else if (dir_str == "Output") pts.direction = PortDirection::Output;
        else pts.direction = PortDirection::Bidirectional;
        pts.expected_unit = m[4].str();
        def->add_port(std::move(pts));
    }

    return def;
}

} // namespace shunya::core::domain
