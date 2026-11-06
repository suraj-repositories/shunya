#include "shunya/core/domain/element_instance.hpp"

#include <algorithm>
#include <sstream>

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

base::Result<void> PortInstance::connect_to(const base::Uuid& target_instance, std::string target_port) {
    if (schema_.cardinality == PortCardinality::Single && !connections_.empty()) {
        return base::Result<void>::err({
            .code = 3201,
            .domain = base::ErrorDomain::Validation,
            .severity = base::Severity::Error,
            .message = "Port '" + name_ + "' has Single cardinality and is already connected",
            .details = ""
        });
    }

    PortConnectionTarget target{target_instance, std::move(target_port)};
    if (std::find(connections_.begin(), connections_.end(), target) == connections_.end()) {
        connections_.push_back(std::move(target));
    }
    return base::Result<void>::ok();
}

void PortInstance::disconnect_from(const base::Uuid& target_instance, std::string_view target_port) {
    connections_.erase(
        std::remove_if(connections_.begin(), connections_.end(), [&](const PortConnectionTarget& t) {
            return t.target_instance_id == target_instance && t.target_port_name == target_port;
        }),
        connections_.end()
    );
}

ElementInstance::ElementInstance(
    std::shared_ptr<const ElementDefinition> definition,
    std::string instance_name,
    base::Uuid id
) : id_(id),
    name_(std::move(instance_name)),
    definition_(std::move(definition)) {
    if (definition_) {
        ports_.reserve(definition_->ports().size());
        for (const auto& port_schema : definition_->ports()) {
            ports_.emplace_back(port_schema.name, port_schema);
        }
    }
}

base::Result<ParameterValue> ElementInstance::get_parameter(std::string_view param_name) const {
    auto override_it = parameter_overrides_.find(std::string(param_name));
    if (override_it != parameter_overrides_.end()) {
        return override_it->second;
    }

    if (!definition_) {
        return base::Result<ParameterValue>::err({
            .code = 3202,
            .domain = base::ErrorDomain::System,
            .severity = base::Severity::Error,
            .message = "Element definition is null for instance " + name_,
            .details = ""
        });
    }

    const auto* schema = definition_->get_parameter_schema(param_name);
    if (!schema) {
        return base::Result<ParameterValue>::err({
            .code = 3202,
            .domain = base::ErrorDomain::Validation,
            .severity = base::Severity::Error,
            .message = "Unknown parameter '" + std::string(param_name) + "' on element " + definition_->type_name(),
            .details = ""
        });
    }

    return schema->default_value;
}

base::Result<void> ElementInstance::set_parameter(std::string_view param_name, ParameterValue value) {
    if (!definition_) {
        return base::Result<void>::err({
            .code = 3202,
            .domain = base::ErrorDomain::System,
            .severity = base::Severity::Error,
            .message = "Element definition is null for instance " + name_,
            .details = ""
        });
    }

    const auto* schema = definition_->get_parameter_schema(param_name);
    if (!schema) {
        return base::Result<void>::err({
            .code = 3202,
            .domain = base::ErrorDomain::Validation,
            .severity = base::Severity::Error,
            .message = "Unknown parameter '" + std::string(param_name) + "'",
            .details = ""
        });
    }

    if (schema->is_read_only) {
        return base::Result<void>::err({
            .code = 3203,
            .domain = base::ErrorDomain::Validation,
            .severity = base::Severity::Error,
            .message = "Cannot modify read-only parameter '" + std::string(param_name) + "'",
            .details = ""
        });
    }

    auto val_res = schema->validate(value);
    if (val_res.is_err()) {
        return val_res;
    }

    parameter_overrides_[std::string(param_name)] = std::move(value);
    return base::Result<void>::ok();
}

bool ElementInstance::has_parameter_override(std::string_view param_name) const noexcept {
    return parameter_overrides_.find(std::string(param_name)) != parameter_overrides_.end();
}

void ElementInstance::reset_parameter_to_default(std::string_view param_name) {
    parameter_overrides_.erase(std::string(param_name));
}

PortInstance* ElementInstance::get_port(std::string_view port_name) noexcept {
    for (auto& p : ports_) {
        if (p.name() == port_name) return &p;
    }
    return nullptr;
}

const PortInstance* ElementInstance::get_port(std::string_view port_name) const noexcept {
    for (const auto& p : ports_) {
        if (p.name() == port_name) return &p;
    }
    return nullptr;
}

base::Result<void> ElementInstance::initialize() {
    if (!definition_) {
        state_ = LifecycleState::Error;
        return base::Result<void>::err({
            .code = 3205,
            .domain = base::ErrorDomain::System,
            .severity = base::Severity::Error,
            .message = "Cannot initialize element instance without definition",
            .details = ""
        });
    }

    // Validate all parameter overrides against definition schemas
    for (const auto& [param_name, val] : parameter_overrides_) {
        const auto* schema = definition_->get_parameter_schema(param_name);
        if (!schema) {
            state_ = LifecycleState::Error;
            return base::Result<void>::err({
                .code = 3202,
                .domain = base::ErrorDomain::Validation,
                .severity = base::Severity::Error,
                .message = "Unknown parameter override '" + param_name + "'",
                .details = ""
            });
        }
        auto val_res = schema->validate(val);
        if (val_res.is_err()) {
            state_ = LifecycleState::Error;
            return val_res;
        }
    }

    simulated_time_ = 0.0;
    state_ = LifecycleState::Configured;
    return base::Result<void>::ok();
}

base::Result<void> ElementInstance::update(double dt) {
    if (state_ == LifecycleState::Uninitialized) {
        auto init_res = initialize();
        if (init_res.is_err()) return init_res;
    }

    if (state_ == LifecycleState::Configured) {
        state_ = LifecycleState::Active;
    }

    if (state_ == LifecycleState::Paused) {
        return base::Result<void>::ok(); // Frozen
    }

    if (state_ == LifecycleState::Terminated || state_ == LifecycleState::Error) {
        return base::Result<void>::err({
            .code = 3204,
            .domain = base::ErrorDomain::System,
            .severity = base::Severity::Error,
            .message = "Cannot update element instance in state " + std::string(lifecycle_state_to_string(state_)),
            .details = ""
        });
    }

    simulated_time_ += dt;
    return base::Result<void>::ok();
}

base::Result<void> ElementInstance::reset() {
    simulated_time_ = 0.0;
    state_ = LifecycleState::Configured;
    return base::Result<void>::ok();
}

base::Result<void> ElementInstance::pause() {
    if (state_ == LifecycleState::Active) {
        state_ = LifecycleState::Paused;
    }
    return base::Result<void>::ok();
}

base::Result<void> ElementInstance::resume() {
    if (state_ == LifecycleState::Paused) {
        state_ = LifecycleState::Active;
    }
    return base::Result<void>::ok();
}

base::Result<void> ElementInstance::terminate() {
    state_ = LifecycleState::Terminated;
    return base::Result<void>::ok();
}

std::shared_ptr<ElementInstance> ElementInstance::duplicate(
    std::string new_name,
    const base::math::Vec3& position_offset
) const {
    std::string dup_name = new_name.empty() ? (name_ + " (Copy)") : std::move(new_name);
    auto dup = std::make_shared<ElementInstance>(definition_, std::move(dup_name));

    // Copy transforms
    auto xf = transform_;
    xf.position = xf.position + position_offset;
    dup->set_transform(xf);

    // Copy parameter overrides
    dup->parameter_overrides_ = parameter_overrides_;

    return dup;
}

std::string ElementInstance::to_json() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"id\": \"" << id_.to_string() << "\",\n";
    ss << "  \"name\": \"" << escape_json(name_) << "\",\n";
    if (definition_) {
        ss << "  \"definition_id\": \"" << definition_->id().to_string() << "\",\n";
        ss << "  \"definition_type\": \"" << escape_json(definition_->type_name()) << "\",\n";
    }
    ss << "  \"lifecycle_state\": \"" << lifecycle_state_to_string(state_) << "\",\n";
    ss << "  \"simulated_time\": " << simulated_time_ << ",\n";

    // Transform
    ss << "  \"transform\": {\n";
    ss << "    \"pos\": [" << transform_.position.x << ", " << transform_.position.y << ", " << transform_.position.z << "]\n";
    ss << "  },\n";

    // Parameter overrides
    ss << "  \"parameter_overrides\": {\n";
    std::size_t p_idx = 0;
    for (const auto& [k, v] : parameter_overrides_) {
        ss << "    \"" << escape_json(k) << "\": \"" << escape_json(v.to_string()) << "\"";
        if (++p_idx < parameter_overrides_.size()) ss << ",";
        ss << "\n";
    }
    ss << "  }\n";
    ss << "}\n";

    return ss.str();
}

std::vector<InspectorProperty> ElementInspector::inspect(const ElementInstance& instance) {
    std::vector<InspectorProperty> properties;
    auto def = instance.definition();
    if (!def) return properties;

    properties.reserve(def->parameters().size());

    for (const auto& schema : def->parameters()) {
        InspectorProperty prop;
        prop.name = schema.name;
        prop.display_label = schema.display_name.empty() ? schema.name : schema.display_name;
        prop.group = schema.type == ParameterType::Quantity ? "Physical Quantities" : "Parameters";
        prop.default_value_str = schema.default_value.to_string();
        prop.unit = schema.unit;
        prop.type = schema.type;
        prop.is_overridden = instance.has_parameter_override(schema.name);
        prop.is_read_only = schema.is_read_only;
        prop.min_value = schema.min_value;
        prop.max_value = schema.max_value;
        prop.enum_options = schema.enum_options;
        prop.contextual_help = schema.description;

        auto curr_val_res = instance.get_parameter(schema.name);
        if (curr_val_res.is_ok()) {
            prop.current_value_str = curr_val_res.value().to_string();
            auto val_check = schema.validate(curr_val_res.value());
            if (val_check.is_err()) {
                prop.is_valid = false;
                prop.validation_error = val_check.error().message;
            }
        } else {
            prop.is_valid = false;
            prop.validation_error = curr_val_res.error().message;
        }

        properties.push_back(std::move(prop));
    }

    return properties;
}

base::Result<void> ElementInspector::apply_change(
    ElementInstance& instance,
    std::string_view property_name,
    const ParameterValue& new_value
) {
    return instance.set_parameter(property_name, new_value);
}

void ElementRegistry::register_definition(std::shared_ptr<const ElementDefinition> def) {
    if (!def) return;
    definitions_by_id_[def->id()] = def;
    definitions_by_type_[def->type_name()] = def;
}

bool ElementRegistry::has_definition(const base::Uuid& id) const noexcept {
    return definitions_by_id_.find(id) != definitions_by_id_.end();
}

bool ElementRegistry::has_definition(std::string_view type_name) const noexcept {
    return definitions_by_type_.find(std::string(type_name)) != definitions_by_type_.end();
}

std::shared_ptr<const ElementDefinition> ElementRegistry::get_definition(const base::Uuid& id) const noexcept {
    auto it = definitions_by_id_.find(id);
    if (it != definitions_by_id_.end()) return it->second;
    return nullptr;
}

std::shared_ptr<const ElementDefinition> ElementRegistry::get_definition(std::string_view type_name) const noexcept {
    auto it = definitions_by_type_.find(std::string(type_name));
    if (it != definitions_by_type_.end()) return it->second;
    return nullptr;
}

std::vector<std::shared_ptr<const ElementDefinition>> ElementRegistry::list_definitions() const {
    std::vector<std::shared_ptr<const ElementDefinition>> defs;
    defs.reserve(definitions_by_id_.size());
    for (const auto& [_, d] : definitions_by_id_) {
        defs.push_back(d);
    }
    return defs;
}

std::vector<std::shared_ptr<const ElementDefinition>> ElementRegistry::list_by_category(ElementCategory cat) const {
    std::vector<std::shared_ptr<const ElementDefinition>> defs;
    for (const auto& [_, d] : definitions_by_id_) {
        if (d->category() == cat) {
            defs.push_back(d);
        }
    }
    return defs;
}

base::Result<std::shared_ptr<ElementInstance>> ElementRegistry::create_instance(
    std::string_view type_name,
    std::string instance_name
) {
    auto def = get_definition(type_name);
    if (!def) {
        return base::Result<std::shared_ptr<ElementInstance>>::err({
            .code = 3210,
            .domain = base::ErrorDomain::System,
            .severity = base::Severity::Error,
            .message = "Definition not found in registry: " + std::string(type_name),
            .details = ""
        });
    }

    auto inst = std::make_shared<ElementInstance>(def, std::move(instance_name));
    instances_[inst->id()] = inst;
    return inst;
}

std::shared_ptr<ElementInstance> ElementRegistry::get_instance(const base::Uuid& id) const noexcept {
    auto it = instances_.find(id);
    if (it != instances_.end()) return it->second;
    return nullptr;
}

bool ElementRegistry::remove_instance(const base::Uuid& id) {
    return instances_.erase(id) > 0;
}

void ElementRegistry::clear_instances() noexcept {
    instances_.clear();
}

base::Result<void> ElementRegistry::connect(
    const base::Uuid& src_instance_id,
    std::string_view src_port_name,
    const base::Uuid& dst_instance_id,
    std::string_view dst_port_name
) {
    auto src_inst = get_instance(src_instance_id);
    auto dst_inst = get_instance(dst_instance_id);
    if (!src_inst || !dst_inst) {
        return base::Result<void>::err({
            .code = 3211,
            .domain = base::ErrorDomain::Validation,
            .severity = base::Severity::Error,
            .message = "Source or destination element instance not found for connection",
            .details = ""
        });
    }

    auto* src_p = src_inst->get_port(src_port_name);
    auto* dst_p = dst_inst->get_port(dst_port_name);
    if (!src_p || !dst_p) {
        return base::Result<void>::err({
            .code = 3212,
            .domain = base::ErrorDomain::Validation,
            .severity = base::Severity::Error,
            .message = "Source port '" + std::string(src_port_name) + "' or destination port '" + std::string(dst_port_name) + "' not found",
            .details = ""
        });
    }

    if (!src_p->schema().can_connect_to(dst_p->schema())) {
        return base::Result<void>::err({
            .code = 3213,
            .domain = base::ErrorDomain::Validation,
            .severity = base::Severity::Error,
            .message = "Port contract violation: incompatible domains or directions between " +
                       std::string(src_port_name) + " and " + std::string(dst_port_name),
            .details = ""
        });
    }

    auto res1 = src_p->connect_to(dst_instance_id, std::string(dst_port_name));
    if (res1.is_err()) return res1;

    auto res2 = dst_p->connect_to(src_instance_id, std::string(src_port_name));
    if (res2.is_err()) {
        src_p->disconnect_from(dst_instance_id, dst_port_name);
        return res2;
    }

    return base::Result<void>::ok();
}

base::Result<void> ElementRegistry::disconnect(
    const base::Uuid& src_instance_id,
    std::string_view src_port_name,
    const base::Uuid& dst_instance_id,
    std::string_view dst_port_name
) {
    auto src_inst = get_instance(src_instance_id);
    auto dst_inst = get_instance(dst_instance_id);
    if (src_inst) {
        auto* src_p = src_inst->get_port(src_port_name);
        if (src_p) src_p->disconnect_from(dst_instance_id, dst_port_name);
    }
    if (dst_inst) {
        auto* dst_p = dst_inst->get_port(dst_port_name);
        if (dst_p) dst_p->disconnect_from(src_instance_id, src_port_name);
    }
    return base::Result<void>::ok();
}

} // namespace shunya::core::domain
