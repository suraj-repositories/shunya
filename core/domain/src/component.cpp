#include "shunya/core/domain/component.hpp"

#include <algorithm>
#include <sstream>
#include <regex>

namespace shunya::core::domain {

namespace {

base::Error make_cmp_error(std::string msg, int32_t code = 1001) {
    return base::Error{
        .code = code,
        .domain = base::ErrorDomain::System,
        .severity = base::Severity::Error,
        .message = std::move(msg),
        .details = ""
    };
}

} // namespace

base::Result<EncapsulationLevel> encapsulation_level_from_string(std::string_view s) {
    if (s == "Public")          return EncapsulationLevel::Public;
    if (s == "Protected")       return EncapsulationLevel::Protected;
    if (s == "InternalPrivate") return EncapsulationLevel::InternalPrivate;
    return make_cmp_error(std::string("Unknown EncapsulationLevel: ") + std::string(s));
}

ComponentDefinition::ComponentDefinition()
    : id_(base::Uuid::generate_random()),
      type_name_("GenericComponent"),
      display_name_("Generic Component") {
    // Default variant
    ComponentVariant def_var;
    def_var.variant_id = "default";
    def_var.display_name = "Default Functional Model";
    def_var.fidelity = FidelityTier::L1_Functional;
    def_var.implementation_type = "Analytical";
    variants_.push_back(std::move(def_var));
}

ComponentDefinition::ComponentDefinition(std::string type_name, std::string display_name, base::Version version)
    : id_(base::Uuid::generate_random()),
      type_name_(std::move(type_name)),
      display_name_(std::move(display_name)),
      version_(version) {
    ComponentVariant def_var;
    def_var.variant_id = "default";
    def_var.display_name = "Default Functional Model";
    def_var.fidelity = FidelityTier::L1_Functional;
    def_var.implementation_type = "Analytical";
    variants_.push_back(std::move(def_var));
}

ComponentDefinition::ComponentDefinition(base::Uuid id, std::string type_name, std::string display_name, base::Version version)
    : id_(id),
      type_name_(std::move(type_name)),
      display_name_(std::move(display_name)),
      version_(version) {
    ComponentVariant def_var;
    def_var.variant_id = "default";
    def_var.display_name = "Default Functional Model";
    def_var.fidelity = FidelityTier::L1_Functional;
    def_var.implementation_type = "Analytical";
    variants_.push_back(std::move(def_var));
}

void ComponentDefinition::add_tag(std::string tag) {
    if (!has_tag(tag)) {
        tags_.push_back(std::move(tag));
    }
}

bool ComponentDefinition::has_tag(std::string_view tag) const noexcept {
    return std::any_of(tags_.begin(), tags_.end(), [tag](const std::string& t) {
        return t == tag;
    });
}

base::Result<void> ComponentDefinition::add_port(ComponentPort port) {
    if (port.schema.name.empty()) {
        return make_cmp_error("Port name cannot be empty", 1002);
    }
    if (ports_.contains(port.schema.name)) {
        return make_cmp_error("Port already exists: " + port.schema.name, 1003);
    }
    std::string key = port.schema.name;
    ports_.emplace(std::move(key), std::move(port));
    return base::Result<void>();
}

bool ComponentDefinition::remove_port(std::string_view port_name) {
    auto it = ports_.find(std::string(port_name));
    if (it != ports_.end()) {
        ports_.erase(it);
        return true;
    }
    return false;
}

const ComponentPort* ComponentDefinition::get_port(std::string_view port_name) const noexcept {
    auto it = ports_.find(std::string(port_name));
    if (it != ports_.end()) {
        return &it->second;
    }
    return nullptr;
}

bool ComponentDefinition::has_port(std::string_view port_name) const noexcept {
    return ports_.contains(std::string(port_name));
}

std::vector<ComponentPort> ComponentDefinition::public_ports() const {
    std::vector<ComponentPort> res;
    for (const auto& pair : ports_) {
        if (pair.second.encapsulation == EncapsulationLevel::Public) {
            res.push_back(pair.second);
        }
    }
    return res;
}

base::Result<void> ComponentDefinition::add_parameter(ParameterSchema param) {
    if (param.name.empty()) {
        return make_cmp_error("Parameter name cannot be empty", 1004);
    }
    if (parameters_.contains(param.name)) {
        return make_cmp_error("Parameter already exists: " + param.name, 1005);
    }
    std::string key = param.name;
    parameters_.emplace(std::move(key), std::move(param));
    return base::Result<void>();
}

bool ComponentDefinition::remove_parameter(std::string_view param_name) {
    auto it = parameters_.find(std::string(param_name));
    if (it != parameters_.end()) {
        parameters_.erase(it);
        return true;
    }
    return false;
}

const ParameterSchema* ComponentDefinition::get_parameter(std::string_view param_name) const noexcept {
    auto it = parameters_.find(std::string(param_name));
    if (it != parameters_.end()) {
        return &it->second;
    }
    return nullptr;
}

bool ComponentDefinition::has_parameter(std::string_view param_name) const noexcept {
    return parameters_.contains(std::string(param_name));
}

base::Result<void> ComponentDefinition::add_child(ChildComponentNode child) {
    if (child.instance_name.empty()) {
        return make_cmp_error("Child instance name cannot be empty", 1006);
    }
    if (has_child(child.instance_name)) {
        return make_cmp_error("Child component already exists: " + child.instance_name, 1007);
    }
    children_.push_back(std::move(child));
    return base::Result<void>();
}

bool ComponentDefinition::remove_child(std::string_view instance_name) {
    auto it = std::remove_if(children_.begin(), children_.end(), [instance_name](const ChildComponentNode& c) {
        return c.instance_name == instance_name;
    });
    if (it != children_.end()) {
        children_.erase(it, children_.end());
        // Remove related connections
        std::erase_if(connections_, [instance_name](const InternalConnection& conn) {
            return conn.from_child == instance_name || conn.to_child == instance_name;
        });
        return true;
    }
    return false;
}

const ChildComponentNode* ComponentDefinition::get_child(std::string_view instance_name) const noexcept {
    for (const auto& c : children_) {
        if (c.instance_name == instance_name) {
            return &c;
        }
    }
    return nullptr;
}

bool ComponentDefinition::has_child(std::string_view instance_name) const noexcept {
    return get_child(instance_name) != nullptr;
}

base::Result<void> ComponentDefinition::add_connection(InternalConnection conn) {
    if (conn.from_port.empty() || conn.to_port.empty()) {
        return make_cmp_error("Connection ports cannot be empty", 1008);
    }
    // Validation of child existence
    if (!conn.from_child.empty() && !has_child(conn.from_child)) {
        return make_cmp_error("Source child not found: " + conn.from_child, 1009);
    }
    if (!conn.to_child.empty() && !has_child(conn.to_child)) {
        return make_cmp_error("Target child not found: " + conn.to_child, 1010);
    }
    // Self-loop prevention on exact same endpoint
    if (conn.from_child == conn.to_child && conn.from_port == conn.to_port) {
        return make_cmp_error("Cannot connect port to itself", 1011);
    }
    connections_.push_back(std::move(conn));
    return base::Result<void>();
}

bool ComponentDefinition::remove_connection(const base::Uuid& conn_id) {
    auto it = std::remove_if(connections_.begin(), connections_.end(), [&conn_id](const InternalConnection& c) {
        return c.id == conn_id;
    });
    if (it != connections_.end()) {
        connections_.erase(it, connections_.end());
        return true;
    }
    return false;
}

base::Result<void> ComponentDefinition::add_variant(ComponentVariant variant) {
    if (variant.variant_id.empty()) {
        return make_cmp_error("Variant ID cannot be empty", 1012);
    }
    for (const auto& v : variants_) {
        if (v.variant_id == variant.variant_id) {
            return make_cmp_error("Variant ID already exists: " + variant.variant_id, 1013);
        }
    }
    variants_.push_back(std::move(variant));
    return base::Result<void>();
}

bool ComponentDefinition::set_active_variant(std::string_view variant_id) {
    for (const auto& v : variants_) {
        if (v.variant_id == variant_id) {
            active_variant_id_ = std::string(variant_id);
            return true;
        }
    }
    return false;
}

const ComponentVariant* ComponentDefinition::get_variant(std::string_view variant_id) const noexcept {
    for (const auto& v : variants_) {
        if (v.variant_id == variant_id) {
            return &v;
        }
    }
    return nullptr;
}

const ComponentVariant& ComponentDefinition::active_variant() const noexcept {
    const auto* v = get_variant(active_variant_id_);
    if (v != nullptr) return *v;
    return variants_.front();
}

base::ValidationReport ComponentDefinition::validate() const {
    base::ValidationReport rep;

    if (id_.is_nil()) {
        rep.add_error("CMP_NIL_ID", "ComponentDefinition UUID is nil");
    }
    if (type_name_.empty()) {
        rep.add_error("CMP_EMPTY_TYPE_NAME", "Component type name is empty");
    }
    if (variants_.empty()) {
        rep.add_error("CMP_NO_VARIANTS", "Component must have at least one model variant");
    }
    if (get_variant(active_variant_id_) == nullptr) {
        rep.add_warning("CMP_INVALID_ACTIVE_VARIANT", "Active variant ID not found in variants list");
    }

    // Verify parent port bindings
    for (const auto& pair : ports_) {
        const auto& port = pair.second;
        if (!port.internal_binding_target.empty()) {
            // format: "child_name.port_name"
            auto dot_pos = port.internal_binding_target.find('.');
            if (dot_pos == std::string::npos) {
                rep.add_error("CMP_INVALID_BINDING", "Port internal binding format must be 'child.port': " + port.schema.name);
            } else {
                std::string child_name = port.internal_binding_target.substr(0, dot_pos);
                if (!has_child(child_name)) {
                    rep.add_error("CMP_UNBOUND_PORT", "Port bound to nonexistent child: " + child_name);
                }
            }
        }
    }

    // Verify internal connections
    for (const auto& conn : connections_) {
        if (conn.from_child.empty()) {
            // Parent boundary port
            if (!has_port(conn.from_port)) {
                rep.add_error("CMP_INVALID_CONN_PORT", "Boundary source port not found on component: " + conn.from_port);
            }
        } else if (!has_child(conn.from_child)) {
            rep.add_error("CMP_INVALID_CONN_CHILD", "Source child not found: " + conn.from_child);
        }

        if (conn.to_child.empty()) {
            // Parent boundary port
            if (!has_port(conn.to_port)) {
                rep.add_error("CMP_INVALID_CONN_PORT", "Boundary target port not found on component: " + conn.to_port);
            }
        } else if (!has_child(conn.to_child)) {
            rep.add_error("CMP_INVALID_CONN_CHILD", "Target child not found: " + conn.to_child);
        }
    }

    return rep;
}

std::string ComponentDefinition::to_json() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"id\": \"" << id_.to_string() << "\",\n";
    ss << "  \"type_name\": \"" << type_name_ << "\",\n";
    ss << "  \"display_name\": \"" << display_name_ << "\",\n";
    ss << "  \"description\": \"" << description_ << "\",\n";
    ss << "  \"author\": \"" << author_ << "\",\n";
    ss << "  \"license\": \"" << license_ << "\",\n";
    ss << "  \"version\": \"" << version_.as_string() << "\",\n";
    ss << "  \"active_variant_id\": \"" << active_variant_id_ << "\",\n";

    // Ports
    ss << "  \"ports\": [\n";
    size_t p_i = 0;
    for (const auto& pair : ports_) {
        const auto& p = pair.second;
        ss << "    {\n";
        ss << "      \"name\": \"" << p.schema.name << "\",\n";
        ss << "      \"domain\": \"" << port_domain_to_string(p.schema.domain) << "\",\n";
        ss << "      \"direction\": \"" << port_direction_to_string(p.schema.direction) << "\",\n";
        ss << "      \"encapsulation\": \"" << encapsulation_level_to_string(p.encapsulation) << "\",\n";
        ss << "      \"internal_binding\": \"" << p.internal_binding_target << "\"\n";
        ss << "    }";
        if (++p_i < ports_.size()) ss << ",";
        ss << "\n";
    }
    ss << "  ],\n";

    // Variants
    ss << "  \"variants\": [\n";
    size_t v_i = 0;
    for (const auto& v : variants_) {
        ss << "    {\n";
        ss << "      \"variant_id\": \"" << v.variant_id << "\",\n";
        ss << "      \"display_name\": \"" << v.display_name << "\",\n";
        ss << "      \"fidelity\": \"" << fidelity_tier_to_string(v.fidelity) << "\",\n";
        ss << "      \"implementation_type\": \"" << v.implementation_type << "\",\n";
        ss << "      \"model_expression\": \"" << v.model_expression << "\"\n";
        ss << "    }";
        if (++v_i < variants_.size()) ss << ",";
        ss << "\n";
    }
    ss << "  ],\n";

    // Children
    ss << "  \"children\": [\n";
    for (size_t c_i = 0; c_i < children_.size(); ++c_i) {
        const auto& c = children_[c_i];
        ss << "    {\n";
        ss << "      \"instance_name\": \"" << c.instance_name << "\",\n";
        ss << "      \"definition_id\": \"" << c.definition_id.to_string() << "\",\n";
        ss << "      \"type_name\": \"" << c.type_name << "\"\n";
        ss << "    }";
        if (c_i + 1 < children_.size()) ss << ",";
        ss << "\n";
    }
    ss << "  ],\n";

    // Connections
    ss << "  \"connections\": [\n";
    for (size_t cn_i = 0; cn_i < connections_.size(); ++cn_i) {
        const auto& cn = connections_[cn_i];
        ss << "    {\n";
        ss << "      \"id\": \"" << cn.id.to_string() << "\",\n";
        ss << "      \"from_child\": \"" << cn.from_child << "\",\n";
        ss << "      \"from_port\": \"" << cn.from_port << "\",\n";
        ss << "      \"to_child\": \"" << cn.to_child << "\",\n";
        ss << "      \"to_port\": \"" << cn.to_port << "\"\n";
        ss << "    }";
        if (cn_i + 1 < connections_.size()) ss << ",";
        ss << "\n";
    }
    ss << "  ]\n";
    ss << "}";
    return ss.str();
}

base::Result<ComponentDefinition> ComponentDefinition::from_json(std::string_view json_str) {
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
    auto id_res = base::Uuid::from_string(id_str);
    base::Uuid id = id_res.has_value() ? *id_res : base::Uuid::generate_random();

    std::string type_name = extract_str("type_name");
    std::string display_name = extract_str("display_name");
    std::string ver_str = extract_str("version");
    base::Version version{1, 0, 0};
    if (!ver_str.empty()) {
        auto v_res = base::Version::parse(ver_str);
        if (v_res.is_ok()) version = v_res.value();
    }

    ComponentDefinition def(id, type_name, display_name, version);
    def.set_description(extract_str("description"));
    def.set_author(extract_str("author"));
    def.set_license(extract_str("license"));

    // Parse ports
    size_t ports_pos = s.find("\"ports\"");
    if (ports_pos != std::string::npos) {
        std::regex port_re(R"re(\{\s*"name"\s*:\s*"([^"]+)"\s*,\s*"domain"\s*:\s*"([^"]+)"\s*,\s*"direction"\s*:\s*"([^"]+)"\s*,\s*"encapsulation"\s*:\s*"([^"]+)"(?:,\s*"internal_binding"\s*:\s*"([^"]*)")?\s*\})re");
        auto it = std::sregex_iterator(s.begin() + ports_pos, s.end(), port_re);
        auto end = std::sregex_iterator();
        for (; it != end; ++it) {
            ComponentPort cp;
            cp.schema.name = (*it)[1].str();
            auto dom_opt = port_domain_from_string((*it)[2].str());
            if (dom_opt.has_value()) cp.schema.domain = *dom_opt;
            if ((*it)[3].str() == "Input") cp.schema.direction = PortDirection::Input;
            else if ((*it)[3].str() == "Output") cp.schema.direction = PortDirection::Output;
            else cp.schema.direction = PortDirection::Bidirectional;

            auto enc_res = encapsulation_level_from_string((*it)[4].str());
            if (enc_res.is_ok()) cp.encapsulation = enc_res.value();
            if ((*it).size() > 5) cp.internal_binding_target = (*it)[5].str();
            (void)def.add_port(std::move(cp));
        }
    }

    // Parse children
    size_t child_pos = s.find("\"children\"");
    if (child_pos != std::string::npos) {
        std::regex ch_re(R"re(\{\s*"instance_name"\s*:\s*"([^"]+)"\s*,\s*"definition_id"\s*:\s*"([^"]+)"\s*,\s*"type_name"\s*:\s*"([^"]+)"\s*\})re");
        auto it = std::sregex_iterator(s.begin() + child_pos, s.end(), ch_re);
        auto end = std::sregex_iterator();
        for (; it != end; ++it) {
            ChildComponentNode ch;
            ch.instance_name = (*it)[1].str();
            auto cid_opt = base::Uuid::from_string((*it)[2].str());
            ch.definition_id = cid_opt.value_or(base::Uuid::generate_random());
            ch.type_name = (*it)[3].str();
            (void)def.add_child(std::move(ch));
        }
    }

    // Parse connections
    size_t conn_pos = s.find("\"connections\"");
    if (conn_pos != std::string::npos) {
        std::regex cn_re(R"re(\{\s*"id"\s*:\s*"([^"]+)"\s*,\s*"from_child"\s*:\s*"([^"]*)"\s*,\s*"from_port"\s*:\s*"([^"]+)"\s*,\s*"to_child"\s*:\s*"([^"]*)"\s*,\s*"to_port"\s*:\s*"([^"]+)"\s*\})re");
        auto it = std::sregex_iterator(s.begin() + conn_pos, s.end(), cn_re);
        auto end = std::sregex_iterator();
        for (; it != end; ++it) {
            InternalConnection cn;
            auto cid_opt = base::Uuid::from_string((*it)[1].str());
            cn.id = cid_opt.value_or(base::Uuid::generate_random());
            cn.from_child = (*it)[2].str();
            cn.from_port = (*it)[3].str();
            cn.to_child = (*it)[4].str();
            cn.to_port = (*it)[5].str();
            (void)def.add_connection(std::move(cn));
        }
    }

    std::string act_var = extract_str("active_variant_id");
    if (!act_var.empty()) {
        (void)def.set_active_variant(act_var);
    }

    return def;
}

} // namespace shunya::core::domain
