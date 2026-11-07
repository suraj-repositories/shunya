#include "shunya/core/domain/component_runtime.hpp"

#include <algorithm>
#include <sstream>
#include <regex>

namespace shunya::core::domain {

namespace {

base::Error make_cmprt_error(std::string msg, int32_t code = 1020) {
    return base::Error{
        .code = code,
        .domain = base::ErrorDomain::System,
        .severity = base::Severity::Error,
        .message = std::move(msg),
        .details = ""
    };
}

// Simple CRC32 table & computation for component packaging
uint32_t compute_crc32_buffer(const void* data, size_t length) {
    const auto* bytes = static_cast<const uint8_t*>(data);
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; ++i) {
        crc ^= bytes[i];
        for (int k = 0; k < 8; ++k) {
            crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
        }
    }
    return ~crc;
}

} // namespace

// =========================================================================
// ComponentRuntimeInstance (CMP-07)
// =========================================================================

ComponentRuntimeInstance::ComponentRuntimeInstance() = default;

ComponentRuntimeInstance::ComponentRuntimeInstance(std::shared_ptr<const ComponentDefinition> definition,
                                                   std::string instance_name)
    : instance_name_(std::move(instance_name)),
      definition_(std::move(definition)) {
    if (definition_) {
        // Initialize default parameters from definition
        for (const auto& pair : definition_->parameters()) {
            parameters_[pair.first] = pair.second.default_value;
        }
    }
}

void ComponentRuntimeInstance::set_definition(std::shared_ptr<const ComponentDefinition> def) {
    definition_ = std::move(def);
    if (definition_) {
        for (const auto& pair : definition_->parameters()) {
            if (!parameters_.contains(pair.first)) {
                parameters_[pair.first] = pair.second.default_value;
            }
        }
    }
}

void ComponentRuntimeInstance::set_parameter(std::string name, ParameterValue val) {
    parameters_[std::move(name)] = std::move(val);
}

std::optional<ParameterValue> ComponentRuntimeInstance::get_parameter(std::string_view name) const {
    auto it = parameters_.find(std::string(name));
    if (it != parameters_.end()) {
        return it->second;
    }
    return std::nullopt;
}

void ComponentRuntimeInstance::set_input_value(std::string port_name, double val) {
    input_values_[std::move(port_name)] = val;
}

double ComponentRuntimeInstance::get_input_value(std::string_view port_name) const noexcept {
    auto it = input_values_.find(std::string(port_name));
    if (it != input_values_.end()) return it->second;
    return 0.0;
}

void ComponentRuntimeInstance::set_output_value(std::string port_name, double val) {
    output_values_[std::move(port_name)] = val;
}

double ComponentRuntimeInstance::get_output_value(std::string_view port_name) const noexcept {
    auto it = output_values_.find(std::string(port_name));
    if (it != output_values_.end()) return it->second;
    return 0.0;
}

void ComponentRuntimeInstance::set_state_variable(std::string var_name, double val) {
    state_vars_[std::move(var_name)] = val;
}

double ComponentRuntimeInstance::get_state_variable(std::string_view var_name) const noexcept {
    auto it = state_vars_.find(std::string(var_name));
    if (it != state_vars_.end()) return it->second;
    return 0.0;
}

base::Result<void> ComponentRuntimeInstance::initialize() {
    simulated_time_ = 0.0;
    step_count_ = 0;
    lifecycle_state_ = RuntimeLifecycleState::Ready;
    return base::Result<void>();
}

base::Result<void> ComponentRuntimeInstance::step(double dt) {
    if (dt <= 0.0) {
        return make_cmprt_error("Time step dt must be positive", 1021);
    }
    if (lifecycle_state_ == RuntimeLifecycleState::Uninitialized) {
        auto init_res = initialize();
        if (!init_res.is_ok()) return init_res;
    }

    lifecycle_state_ = RuntimeLifecycleState::Running;

    if (custom_step_) {
        auto res = custom_step_(dt, simulated_time_, input_values_, state_vars_, output_values_);
        if (!res.is_ok()) {
            lifecycle_state_ = RuntimeLifecycleState::ErrorTerminated;
            return res;
        }
    }

    simulated_time_ += dt;
    ++step_count_;
    return base::Result<void>();
}

base::Result<void> ComponentRuntimeInstance::reset() {
    simulated_time_ = 0.0;
    step_count_ = 0;
    state_vars_.clear();
    input_values_.clear();
    output_values_.clear();
    lifecycle_state_ = RuntimeLifecycleState::Ready;
    return base::Result<void>();
}

std::string ComponentRuntimeInstance::serialize_state_json() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"instance_id\": \"" << instance_id_.to_string() << "\",\n";
    ss << "  \"instance_name\": \"" << instance_name_ << "\",\n";
    ss << "  \"lifecycle_state\": \"" << runtime_lifecycle_state_to_string(lifecycle_state_) << "\",\n";
    ss << "  \"simulated_time\": " << simulated_time_ << ",\n";
    ss << "  \"step_count\": " << step_count_ << ",\n";

    // State Variables
    ss << "  \"state_variables\": {\n";
    size_t sv_i = 0;
    for (const auto& pair : state_vars_) {
        ss << "    \"" << pair.first << "\": " << pair.second;
        if (++sv_i < state_vars_.size()) ss << ",";
        ss << "\n";
    }
    ss << "  },\n";

    // Output Values
    ss << "  \"output_values\": {\n";
    size_t ov_i = 0;
    for (const auto& pair : output_values_) {
        ss << "    \"" << pair.first << "\": " << pair.second;
        if (++ov_i < output_values_.size()) ss << ",";
        ss << "\n";
    }
    ss << "  }\n";
    ss << "}";
    return ss.str();
}

base::Result<void> ComponentRuntimeInstance::restore_state_json(std::string_view json_str) {
    std::string s(json_str);

    auto extract_num = [&](const std::string& key) -> std::optional<double> {
        std::regex re("\"" + key + R"re("\s*:\s*([-+]?[0-9]*\.?[0-9]+(?:[eE][-+]?[0-9]+)?))re");
        std::smatch m;
        if (std::regex_search(s, m, re)) {
            try { return std::stod(m[1].str()); } catch (...) { return std::nullopt; }
        }
        return std::nullopt;
    };

    simulated_time_ = extract_num("simulated_time").value_or(0.0);
    auto sc_opt = extract_num("step_count");
    if (sc_opt) step_count_ = static_cast<uint64_t>(*sc_opt);

    // Parse state variables block
    size_t sv_pos = s.find("\"state_variables\"");
    if (sv_pos != std::string::npos) {
        size_t b_start = s.find('{', sv_pos);
        size_t b_end = s.find('}', b_start);
        if (b_start != std::string::npos && b_end != std::string::npos) {
            std::string sub = s.substr(b_start, b_end - b_start + 1);
            std::regex pair_re(R"re("([^"]+)"\s*:\s*([-+]?[0-9]*\.?[0-9]+(?:[eE][-+]?[0-9]+)?))re");
            auto it = std::sregex_iterator(sub.begin(), sub.end(), pair_re);
            auto end = std::sregex_iterator();
            for (; it != end; ++it) {
                state_vars_[(*it)[1].str()] = std::stod((*it)[2].str());
            }
        }
    }

    lifecycle_state_ = RuntimeLifecycleState::Ready;
    return base::Result<void>();
}

// =========================================================================
// ComponentBuilder (CMP-06)
// =========================================================================

ComponentBuilder::ComponentBuilder(std::string type_name, std::string display_name)
    : comp_(base::Uuid::generate_random(), std::move(type_name), std::move(display_name)) {}

ComponentBuilder& ComponentBuilder::set_id(base::Uuid id) {
    comp_.set_id(id);
    return *this;
}

ComponentBuilder& ComponentBuilder::set_version(base::Version version) {
    comp_.set_version(version);
    return *this;
}

ComponentBuilder& ComponentBuilder::set_description(std::string desc) {
    comp_.set_description(std::move(desc));
    return *this;
}

ComponentBuilder& ComponentBuilder::set_author(std::string author) {
    comp_.set_author(std::move(author));
    return *this;
}

ComponentBuilder& ComponentBuilder::set_license(std::string license) {
    comp_.set_license(std::move(license));
    return *this;
}

ComponentBuilder& ComponentBuilder::add_tag(std::string tag) {
    comp_.add_tag(std::move(tag));
    return *this;
}

ComponentBuilder& ComponentBuilder::set_symbol_svg(std::string svg) {
    comp_.set_symbol_svg(std::move(svg));
    return *this;
}

ComponentBuilder& ComponentBuilder::set_geometry_ref(std::string ref) {
    comp_.set_geometry_model_ref(std::move(ref));
    return *this;
}

ComponentBuilder& ComponentBuilder::add_electrical_port(std::string name, PortDirection dir, EncapsulationLevel enc) {
    ComponentPort p;
    p.schema.name = std::move(name);
    p.schema.domain = PortDomain::Electrical;
    p.schema.direction = dir;
    p.encapsulation = enc;
    (void)comp_.add_port(std::move(p));
    return *this;
}

ComponentBuilder& ComponentBuilder::add_thermal_port(std::string name, PortDirection dir, EncapsulationLevel enc) {
    ComponentPort p;
    p.schema.name = std::move(name);
    p.schema.domain = PortDomain::Thermal;
    p.schema.direction = dir;
    p.encapsulation = enc;
    (void)comp_.add_port(std::move(p));
    return *this;
}

ComponentBuilder& ComponentBuilder::add_mechanical_port(std::string name, bool is_rotational, PortDirection dir, EncapsulationLevel enc) {
    ComponentPort p;
    p.schema.name = std::move(name);
    p.schema.domain = is_rotational ? PortDomain::MechanicalRotational : PortDomain::MechanicalTranslational;
    p.schema.direction = dir;
    p.encapsulation = enc;
    (void)comp_.add_port(std::move(p));
    return *this;
}

ComponentBuilder& ComponentBuilder::add_fluid_port(std::string name, PortDirection dir, EncapsulationLevel enc) {
    ComponentPort p;
    p.schema.name = std::move(name);
    p.schema.domain = PortDomain::Fluid;
    p.schema.direction = dir;
    p.encapsulation = enc;
    (void)comp_.add_port(std::move(p));
    return *this;
}

ComponentBuilder& ComponentBuilder::add_signal_port(std::string name, PortDirection dir, EncapsulationLevel enc) {
    ComponentPort p;
    p.schema.name = std::move(name);
    p.schema.domain = PortDomain::Signal;
    p.schema.direction = dir;
    p.encapsulation = enc;
    (void)comp_.add_port(std::move(p));
    return *this;
}

ComponentBuilder& ComponentBuilder::add_port(ComponentPort port) {
    (void)comp_.add_port(std::move(port));
    return *this;
}

ComponentBuilder& ComponentBuilder::add_numeric_param(std::string name, double default_val, std::string unit,
                                                     std::optional<double> min_val, std::optional<double> max_val) {
    ParameterSchema p;
    p.name = std::move(name);
    p.type = ParameterType::Double;
    p.default_value = default_val;
    p.unit = std::move(unit);
    p.min_value = min_val;
    p.max_value = max_val;
    (void)comp_.add_parameter(std::move(p));
    return *this;
}

ComponentBuilder& ComponentBuilder::add_string_param(std::string name, std::string default_val) {
    ParameterSchema p;
    p.name = std::move(name);
    p.type = ParameterType::String;
    p.default_value = std::move(default_val);
    (void)comp_.add_parameter(std::move(p));
    return *this;
}

ComponentBuilder& ComponentBuilder::add_bool_param(std::string name, bool default_val) {
    ParameterSchema p;
    p.name = std::move(name);
    p.type = ParameterType::Bool;
    p.default_value = default_val;
    (void)comp_.add_parameter(std::move(p));
    return *this;
}

ComponentBuilder& ComponentBuilder::add_parameter(ParameterSchema schema) {
    (void)comp_.add_parameter(std::move(schema));
    return *this;
}

ComponentBuilder& ComponentBuilder::add_variant(ComponentVariant variant) {
    (void)comp_.add_variant(std::move(variant));
    return *this;
}

ComponentBuilder& ComponentBuilder::add_child(std::string instance_name, base::Uuid def_id, std::string type_name) {
    ChildComponentNode c;
    c.instance_name = std::move(instance_name);
    c.definition_id = def_id;
    c.type_name = std::move(type_name);
    (void)comp_.add_child(std::move(c));
    return *this;
}

ComponentBuilder& ComponentBuilder::connect(std::string from_child, std::string from_port,
                                           std::string to_child, std::string to_port) {
    InternalConnection cn;
    cn.from_child = std::move(from_child);
    cn.from_port = std::move(from_port);
    cn.to_child = std::move(to_child);
    cn.to_port = std::move(to_port);
    (void)comp_.add_connection(std::move(cn));
    return *this;
}

base::Result<ComponentDefinition> ComponentBuilder::build() {
    auto rep = comp_.validate();
    if (rep.has_errors()) {
        std::string err = "Component validation failed during build";
        if (!rep.issues().empty()) {
            err += ": " + rep.issues().front().message;
        }
        return make_cmprt_error(err);
    }
    return comp_;
}

// =========================================================================
// ComponentPackage (CMP-08)
// =========================================================================

ComponentPackage::ComponentPackage() = default;

ComponentPackage::ComponentPackage(std::string package_id, std::string name, base::Version version)
    : package_id_(std::move(package_id)),
      name_(std::move(name)),
      version_(version) {}

void ComponentPackage::add_component(ComponentDefinition comp) {
    components_.push_back(std::move(comp));
}

const ComponentDefinition* ComponentPackage::get_component(std::string_view type_name) const noexcept {
    for (const auto& c : components_) {
        if (c.type_name() == type_name) {
            return &c;
        }
    }
    return nullptr;
}

void ComponentPackage::add_dependency_lock(DependencyLockEntry entry) {
    dependencies_.push_back(std::move(entry));
}

void ComponentPackage::add_asset(AssetEntry asset) {
    assets_.push_back(std::move(asset));
}

base::ValidationReport ComponentPackage::validate_package() const {
    base::ValidationReport rep;
    if (package_id_.empty()) {
        rep.add_error("PKG_EMPTY_ID", "Package ID cannot be empty");
    }
    if (name_.empty()) {
        rep.add_error("PKG_EMPTY_NAME", "Package display name cannot be empty");
    }
    if (components_.empty()) {
        rep.add_warning("PKG_NO_COMPONENTS", "Package does not contain any component definitions");
    }
    for (const auto& c : components_) {
        auto c_rep = c.validate();
        for (const auto& issue : c_rep.issues()) {
            if (issue.severity >= base::Severity::Error) {
                rep.add_error("PKG_CMP_ERR", c.type_name() + ": " + issue.message);
            }
        }
    }
    return rep;
}

uint32_t ComponentPackage::compute_package_crc32() const {
    std::string serialized = serialize_manifest_json();
    return compute_crc32_buffer(serialized.data(), serialized.size());
}

std::string ComponentPackage::serialize_manifest_json() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"package_id\": \"" << package_id_ << "\",\n";
    ss << "  \"name\": \"" << name_ << "\",\n";
    ss << "  \"version\": \"" << version_.as_string() << "\",\n";
    ss << "  \"author\": \"" << author_ << "\",\n";
    ss << "  \"description\": \"" << description_ << "\",\n";

    // Dependencies
    ss << "  \"dependencies\": [\n";
    for (size_t d_i = 0; d_i < dependencies_.size(); ++d_i) {
        const auto& d = dependencies_[d_i];
        ss << "    {\n";
        ss << "      \"package_name\": \"" << d.package_name << "\",\n";
        ss << "      \"required_version\": \"" << d.required_version.as_string() << "\",\n";
        ss << "      \"content_hash\": \"" << d.content_hash_sha256 << "\"\n";
        ss << "    }";
        if (d_i + 1 < dependencies_.size()) ss << ",";
        ss << "\n";
    }
    ss << "  ],\n";

    // Components
    ss << "  \"components\": [\n";
    for (size_t c_i = 0; c_i < components_.size(); ++c_i) {
        ss << components_[c_i].to_json();
        if (c_i + 1 < components_.size()) ss << ",";
        ss << "\n";
    }
    ss << "  ]\n";
    ss << "}";
    return ss.str();
}

base::Result<ComponentPackage> ComponentPackage::deserialize_manifest_json(std::string_view json_str) {
    std::string s(json_str);

    auto extract_str = [&](const std::string& key) -> std::string {
        std::regex re("\"" + key + R"re("\s*:\s*"([^"]*)")re");
        std::smatch m;
        if (std::regex_search(s, m, re)) {
            return m[1].str();
        }
        return "";
    };

    std::string pid = extract_str("package_id");
    std::string name = extract_str("name");
    std::string ver_str = extract_str("version");
    base::Version ver{1, 0, 0};
    if (!ver_str.empty()) {
        auto v_res = base::Version::parse(ver_str);
        if (v_res.is_ok()) ver = v_res.value();
    }

    ComponentPackage pkg(pid, name, ver);
    pkg.set_author(extract_str("author"));
    pkg.set_description(extract_str("description"));

    // Parse dependencies
    size_t dep_pos = s.find("\"dependencies\"");
    if (dep_pos != std::string::npos) {
        std::regex dep_re(R"re(\{\s*"package_name"\s*:\s*"([^"]+)"\s*,\s*"required_version"\s*:\s*"([^"]+)"\s*,\s*"content_hash"\s*:\s*"([^"]*)"\s*\})re");
        auto it = std::sregex_iterator(s.begin() + dep_pos, s.end(), dep_re);
        auto end = std::sregex_iterator();
        for (; it != end; ++it) {
            DependencyLockEntry entry;
            entry.package_name = (*it)[1].str();
            auto v_res = base::Version::parse((*it)[2].str());
            if (v_res.is_ok()) entry.required_version = v_res.value();
            entry.content_hash_sha256 = (*it)[3].str();
            pkg.add_dependency_lock(std::move(entry));
        }
    }

    return pkg;
}

} // namespace shunya::core::domain
