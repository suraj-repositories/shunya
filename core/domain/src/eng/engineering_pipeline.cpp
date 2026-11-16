#include "shunya/eng/engineering_pipeline.hpp"
#include "shunya/core/domain/migration.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <regex>
#include <sstream>

namespace shunya::eng {

namespace {

core::base::Error make_eng_error(int32_t code, std::string msg, std::string details = {}) {
    return core::base::Error{
        .code = code,
        .domain = core::base::ErrorDomain::Validation,
        .severity = core::base::Severity::Error,
        .message = std::move(msg),
        .details = std::move(details)
    };
}

uint32_t compute_crc32(const uint8_t* data, size_t length) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int k = 0; k < 8; ++k) {
            crc = (crc >> 1) ^ (0xEDB88320u & (-(crc & 1)));
        }
    }
    return ~crc;
}

// Finds the substring after `"key":` and extracts either a balanced {...} object or [...] array.
std::optional<std::string_view> extract_json_block(std::string_view json,
                                                   std::string_view key,
                                                   char open_ch,
                                                   char close_ch) {
    const std::string needle = "\"" + std::string(key) + "\"";
    size_t pos = json.find(needle);
    while (pos != std::string_view::npos) {
        size_t p = pos + needle.size();
        while (p < json.size() && std::isspace(static_cast<unsigned char>(json[p]))) ++p;
        if (p < json.size() && json[p] == ':') {
            ++p;
            while (p < json.size() && std::isspace(static_cast<unsigned char>(json[p]))) ++p;
            if (p < json.size() && json[p] == open_ch) {
                int depth = 0;
                bool in_str = false;
                bool esc = false;
                const size_t start = p;
                for (; p < json.size(); ++p) {
                    const char c = json[p];
                    if (esc) { esc = false; continue; }
                    if (in_str) {
                        if (c == '\\') esc = true;
                        else if (c == '"') in_str = false;
                        continue;
                    }
                    if (c == '"') in_str = true;
                    else if (c == open_ch) ++depth;
                    else if (c == close_ch) {
                        --depth;
                        if (depth == 0) {
                            return json.substr(start, p - start + 1);
                        }
                    }
                }
            }
        }
        pos = json.find(needle, pos + needle.size());
    }
    return std::nullopt;
}

std::vector<std::string_view> split_json_object_array(std::string_view arr) {
    std::vector<std::string_view> items;
    if (arr.size() < 2 || arr.front() != '[') return items;
    int depth = 0;
    bool in_str = false;
    bool esc = false;
    size_t obj_start = std::string_view::npos;
    for (size_t i = 1; i + 1 < arr.size(); ++i) {
        const char c = arr[i];
        if (esc) { esc = false; continue; }
        if (in_str) {
            if (c == '\\') esc = true;
            else if (c == '"') in_str = false;
            continue;
        }
        if (c == '"') {
            in_str = true;
        } else if (c == '{') {
            if (depth == 0) obj_start = i;
            ++depth;
        } else if (c == '}') {
            --depth;
            if (depth == 0 && obj_start != std::string_view::npos) {
                items.push_back(arr.substr(obj_start, i - obj_start + 1));
                obj_start = std::string_view::npos;
            }
        }
    }
    return items;
}

template <typename T>
std::string serialize_array(const std::vector<T>& vec, const std::string& indent) {
    if (vec.empty()) return "[]";
    std::ostringstream os;
    os << "[\n";
    for (size_t i = 0; i < vec.size(); ++i) {
        os << indent << "  " << vec[i].to_json();
        if (i + 1 < vec.size()) os << ",";
        os << "\n";
    }
    os << indent << "]";
    return os.str();
}

} // namespace

EngineeringWorkspaceState::EngineeringWorkspaceState() {
    auto res = core::domain::Project::create_new("Untitled_Material_Engineering_Project", "Shunya Engineer");
    if (res.is_ok()) {
        project_ = res.value();
        project_->clear_dirty();
    }
    circuits_.emplace_back("Main_Circuit");
}

void EngineeringWorkspaceState::reset_project(std::shared_ptr<core::domain::Project> project) {
    project_ = std::move(project);
    mixtures_.clear();
    materials_ = core::domain::MaterialRegistry{};
    object_manager_.clear();
    batteries_.clear();
    component_library_.clear();
    circuits_.clear();
    circuits_.emplace_back("Main_Circuit");
    active_circuit_idx_ = 0;
    physics_runtime_ = PhysicsSimulationRuntime{};
    scene_graph_.clear();
    command_stack_.clear();
    legacy_circuit_.clear();
    env_ = AmbientEnvironmentConfig{};
    notify_change(DependencyChangeKind::ProjectReset,
                  project_ ? project_->manifest().id : core::base::Uuid{},
                  "Project reset");
}

InteractiveCircuitDocument& EngineeringWorkspaceState::active_circuit_doc() {
    if (circuits_.empty()) {
        circuits_.emplace_back("Main_Circuit");
        active_circuit_idx_ = 0;
    }
    if (active_circuit_idx_ >= circuits_.size()) {
        active_circuit_idx_ = circuits_.size() - 1;
    }
    return circuits_[active_circuit_idx_];
}

const InteractiveCircuitDocument& EngineeringWorkspaceState::active_circuit_doc() const {
    static const InteractiveCircuitDocument kFallback("Empty_Circuit");
    if (circuits_.empty()) return kFallback;
    const size_t idx = std::min(active_circuit_idx_, circuits_.size() - 1);
    return circuits_[idx];
}

void EngineeringWorkspaceState::set_active_circuit_index(size_t idx) {
    if (!circuits_.empty() && idx < circuits_.size()) {
        active_circuit_idx_ = idx;
    }
}

void EngineeringWorkspaceState::set_environment(const AmbientEnvironmentConfig& env) {
    env_ = env;
    if (project_) project_->mark_dirty();
    notify_change(DependencyChangeKind::EnvironmentUpdated, {}, "Ambient environment updated");
}

uint64_t EngineeringWorkspaceState::subscribe(ChangeCallback cb) {
    const uint64_t token = next_token_++;
    listeners_[token] = std::move(cb);
    return token;
}

void EngineeringWorkspaceState::unsubscribe(uint64_t token) {
    listeners_.erase(token);
}

uint64_t EngineeringWorkspaceState::tick_for(DependencyChangeKind kind) const noexcept {
    switch (kind) {
        case DependencyChangeKind::ProjectReset: return ticks_.project;
        case DependencyChangeKind::ElementDatabaseUpdated: return ticks_.element_database;
        case DependencyChangeKind::MaterialUpdated: return ticks_.materials;
        case DependencyChangeKind::MixtureUpdated: return ticks_.mixtures;
        case DependencyChangeKind::ObjectUpdated:
        case DependencyChangeKind::AssemblyUpdated: return ticks_.objects;
        case DependencyChangeKind::ComponentUpdated: return ticks_.components;
        case DependencyChangeKind::BatteryUpdated: return ticks_.batteries;
        case DependencyChangeKind::CircuitUpdated: return ticks_.circuits;
        case DependencyChangeKind::EnvironmentUpdated: return ticks_.environment;
        case DependencyChangeKind::SimulationStepped: return ticks_.simulation;
    }
    return ticks_.global;
}

bool EngineeringWorkspaceState::is_dirty_since(DependencyChangeKind kind, uint64_t last_seen_tick) const noexcept {
    return tick_for(kind) > last_seen_tick;
}

bool EngineeringWorkspaceState::is_any_dirty_since(uint64_t last_seen_global_tick) const noexcept {
    return ticks_.global > last_seen_global_tick;
}

void EngineeringWorkspaceState::touch(DependencyChangeKind kind, const core::base::Uuid& target_id, std::string summary) {
    notify_change(kind, target_id, std::move(summary));
}

std::vector<StateMutationEntry> EngineeringWorkspaceState::recent_mutations(size_t limit) const {
    if (mutation_history_.size() <= limit) {
        return mutation_history_;
    }
    return std::vector<StateMutationEntry>(
        mutation_history_.end() - limit, mutation_history_.end());
}

void EngineeringWorkspaceState::notify_change(
    DependencyChangeKind kind,
    const core::base::Uuid& target_id,
    std::string summary) {
    ++ticks_.global;
    switch (kind) {
        case DependencyChangeKind::ProjectReset: ticks_.project = ticks_.global; break;
        case DependencyChangeKind::ElementDatabaseUpdated: ticks_.element_database = ticks_.global; break;
        case DependencyChangeKind::MaterialUpdated: ticks_.materials = ticks_.global; break;
        case DependencyChangeKind::MixtureUpdated: ticks_.mixtures = ticks_.global; break;
        case DependencyChangeKind::ObjectUpdated:
        case DependencyChangeKind::AssemblyUpdated: ticks_.objects = ticks_.global; break;
        case DependencyChangeKind::ComponentUpdated: ticks_.components = ticks_.global; break;
        case DependencyChangeKind::BatteryUpdated: ticks_.batteries = ticks_.global; break;
        case DependencyChangeKind::CircuitUpdated: ticks_.circuits = ticks_.global; break;
        case DependencyChangeKind::EnvironmentUpdated: ticks_.environment = ticks_.global; break;
        case DependencyChangeKind::SimulationStepped: ticks_.simulation = ticks_.global; break;
    }

    if (mutation_history_.size() >= 256) {
        mutation_history_.erase(mutation_history_.begin());
    }
    mutation_history_.push_back(StateMutationEntry{
        .tick = ticks_.global,
        .kind = kind,
        .target_id = target_id,
        .summary = summary
    });

    if (project_ && kind != DependencyChangeKind::ProjectReset) {
        project_->mark_dirty();
    }
    WorkspaceChangeEvent ev{kind, target_id, ticks_.global, std::move(summary)};
    std::vector<uint64_t> tokens;
    tokens.reserve(listeners_.size());
    for (const auto& [tok, _] : listeners_) {
        tokens.push_back(tok);
    }
    for (uint64_t tok : tokens) {
        auto it = listeners_.find(tok);
        if (it != listeners_.end() && it->second) {
            it->second(ev);
        }
    }
}

void EngineeringWorkspaceState::register_material_dependent_invalidator(DependentInvalidator fn) {
    if (fn) {
        material_invalidators_.push_back(std::move(fn));
    }
}

void EngineeringWorkspaceState::notify_material_modified(
    const core::base::Uuid& material_id,
    std::string summary) {
    object_manager_.recalculate_for_material(material_id, materials_);
    component_library_.recalculate_all(object_manager_, materials_);
    for (auto& c : circuits_) {
        c.sync_from_component_library(component_library_);
    }
    for (const auto& inv : material_invalidators_) {
        if (inv) {
            inv(material_id);
        }
    }
    notify_change(DependencyChangeKind::MaterialUpdated, material_id, std::move(summary));
}

void EngineeringWorkspaceState::notify_object_modified(
    const core::base::Uuid& object_id,
    std::string summary) {
    if (auto* obj = object_manager_.get_object_mut(object_id)) {
        auto eval = PhysicalObjectManager::evaluate_properties(*obj, materials_);
        if (eval.is_ok()) {
            obj->computed = eval.value();
        }
        component_library_.recalculate_for_object(*obj, materials_);
        for (auto& c : circuits_) {
            c.sync_from_component_library(component_library_);
        }
    }
    notify_change(DependencyChangeKind::ObjectUpdated, object_id, std::move(summary));
}

std::string EngineeringWorkspaceState::to_bundle_json(const Shunya4SaveOptions& options) const {
    std::ostringstream os;
    os << std::setprecision(12);
    os << "{\n";
    os << "  \"bundle_version\": \"4.0\",\n";
    os << "  \"format\": \"shunya4\",\n";
    os << "  \"is_differential\": " << (options.incremental ? "true" : "false") << ",\n";
    os << "  \"base_tick\": " << options.base_tick << ",\n";
    os << "  \"ticks\": {\n"
       << "    \"global\": " << ticks_.global << ",\n"
       << "    \"materials\": " << ticks_.materials << ",\n"
       << "    \"objects\": " << ticks_.objects << ",\n"
       << "    \"components\": " << ticks_.components << ",\n"
       << "    \"circuits\": " << ticks_.circuits << ",\n"
       << "    \"simulation\": " << ticks_.simulation << "\n"
       << "  },\n";

    if (project_) {
        os << "  \"project_manifest\": " << project_->manifest().to_json() << ",\n";
    } else {
        os << "  \"project_manifest\": {},\n";
    }

    const bool diff = options.incremental && options.base_tick > 0;

    if (!diff || ticks_.simulation > options.base_tick) {
        os << "  \"environment\": {\n"
           << "    \"temperature_k\": " << env_.temperature_k << ",\n"
           << "    \"pressure_pa\": " << env_.pressure_pa << ",\n"
           << "    \"gravity_m_s2\": " << env_.gravity_m_s2 << ",\n"
           << "    \"convection_h_w_m2k\": " << env_.convection_h_w_m2k << ",\n"
           << "    \"oxidizing_atmosphere\": " << (env_.oxidizing_atmosphere ? "true" : "false") << "\n"
           << "  },\n";
    }

    if (!diff || ticks_.materials > options.base_tick) {
        os << "  \"materials_registry\": " << materials_.to_json() << ",\n";
        os << "  \"mixtures\": " << serialize_array(mixtures_, "  ") << ",\n";
    }

    if (!diff || ticks_.objects > options.base_tick) {
        os << "  \"objects\": " << serialize_array(object_manager_.objects(), "  ") << ",\n";
    }

    if (!diff || ticks_.components > options.base_tick) {
        os << "  \"batteries\": " << serialize_array(batteries_, "  ") << ",\n";
        os << "  \"components\": " << serialize_array(component_library_.components(), "  ") << ",\n";
    }

    if (!diff || ticks_.circuits > options.base_tick) {
        os << "  \"circuits\": " << serialize_array(circuits_, "  ") << ",\n";
        os << "  \"active_circuit_idx\": " << active_circuit_idx_ << ",\n";
    }

    os << "  \"checkpoints_count\": " << checkpoints_.size() << "\n";
    os << "}\n";
    return os.str();
}

std::vector<uint8_t> EngineeringWorkspaceState::to_shunya4_binary(const Shunya4SaveOptions& options) const {
    const std::string json = to_bundle_json(options);
    const uint32_t crc = compute_crc32(reinterpret_cast<const uint8_t*>(json.data()), json.size());

    Shunya4BinaryHeader hdr{};
    std::memcpy(hdr.magic, "SHUNYA4\0", 8);
    hdr.version = 4;
    hdr.flags = (options.incremental ? 0x01 : 0x00);
    hdr.base_tick = options.base_tick;
    hdr.current_tick = ticks_.global;
    hdr.checksum = crc;
    hdr.reserved = 0;
    hdr.payload_size = json.size();

    std::vector<uint8_t> out(sizeof(Shunya4BinaryHeader) + json.size());
    std::memcpy(out.data(), &hdr, sizeof(Shunya4BinaryHeader));
    std::memcpy(out.data() + sizeof(Shunya4BinaryHeader), json.data(), json.size());
    return out;
}

core::base::Result<void> EngineeringWorkspaceState::from_shunya4_binary(const uint8_t* data, size_t size) {
    if (!data || size < sizeof(Shunya4BinaryHeader)) {
        return make_eng_error(4914, "Invalid .shunya4 binary container: truncated header");
    }

    Shunya4BinaryHeader hdr{};
    std::memcpy(&hdr, data, sizeof(Shunya4BinaryHeader));

    if (std::memcmp(hdr.magic, "SHUNYA4\0", 8) != 0) {
        return make_eng_error(4915, "Invalid .shunya4 binary container: magic mismatch");
    }
    if (hdr.version != 4) {
        return make_eng_error(4916, "Unsupported .shunya4 binary version: " + std::to_string(hdr.version));
    }
    if (sizeof(Shunya4BinaryHeader) + hdr.payload_size > size) {
        return make_eng_error(4917, "Invalid .shunya4 binary container: payload truncated");
    }

    const uint8_t* payload = data + sizeof(Shunya4BinaryHeader);
    const uint32_t computed_crc = compute_crc32(payload, hdr.payload_size);
    if (computed_crc != hdr.checksum) {
        return make_eng_error(4918, "CRC32 checksum mismatch in .shunya4 container: corrupted data");
    }

    std::string_view json_view(reinterpret_cast<const char*>(payload), hdr.payload_size);
    return from_bundle_json(json_view);
}

core::base::Result<void> EngineeringWorkspaceState::from_shunya4_binary(const std::vector<uint8_t>& binary_data) {
    return from_shunya4_binary(binary_data.data(), binary_data.size());
}

core::base::Result<void> EngineeringWorkspaceState::from_bundle_json(std::string_view json_str) {
    const std::string s(json_str);
    if (s.find("\"bundle_version\"") == std::string::npos) {
        // Legacy Phase 1/2 project file fallback & schema migration (v0.0.1 / v0.1.0 -> v3.0)
        if (s.find("\"schema_version\"") != std::string::npos) {
            std::string working = s;
            core::domain::ProjectMigrator migrator;
            auto ver_res = core::domain::ProjectMigrator::extract_schema_version(working);
            if (ver_res.is_ok()) {
                const auto target = core::base::Version::parse("0.1.0-dev").value();
                if (migrator.can_migrate(ver_res.value(), target)) {
                    auto mig = migrator.preview_migration(working, target);
                    if (mig.is_ok()) working = mig.value();
                }
            }
            auto man_res = core::domain::ProjectManifest::from_json(working);
            if (man_res.is_ok()) {
                materials_ = core::domain::MaterialRegistry{};
                mixtures_.clear();
                object_manager_ = PhysicalObjectManager{};
                batteries_.clear();
                component_library_ = ComponentLibraryManager{};
                circuits_.clear();
                circuits_.emplace_back("Main_Circuit");
                active_circuit_idx_ = 0;
                project_ = std::make_shared<core::domain::Project>(man_res.value());
                project_->clear_dirty();
                notify_change(DependencyChangeKind::ProjectReset,
                              project_->manifest().id,
                              "Migrated legacy project schema into Shunya v3.0 workspace");
                return core::base::Result<void>::ok();
            }
        }
        return make_eng_error(4901, "Invalid Shunya project bundle JSON");
    }

    const bool is_diff = (s.find("\"is_differential\": true") != std::string::npos ||
                          s.find("\"is_differential\":true") != std::string::npos ||
                          s.find("\"is_differential\" : true") != std::string::npos);

    std::shared_ptr<core::domain::Project> next_proj;
    if (auto proj_blk = extract_json_block(json_str, "project_manifest", '{', '}')) {
        auto man_res = core::domain::ProjectManifest::from_json(*proj_blk);
        if (man_res.is_ok()) {
            next_proj = std::make_shared<core::domain::Project>(man_res.value());
        }
    }
    if (!next_proj && !is_diff) {
        auto res = core::domain::Project::create_new("Loaded_Engineering_Project", "Shunya Engineer");
        if (res.is_ok()) next_proj = res.value();
    }

    AmbientEnvironmentConfig next_env = env_;
    bool has_env = false;
    if (auto env_blk = extract_json_block(json_str, "environment", '{', '}')) {
        has_env = true;
        const std::string es(*env_blk);
        std::smatch m;
        if (std::regex_search(es, m, std::regex("\"temperature_k\"\\s*:\\s*([-+0-9.eE]+)"))) {
            next_env.temperature_k = std::stod(m[1].str());
        }
        if (std::regex_search(es, m, std::regex("\"pressure_pa\"\\s*:\\s*([-+0-9.eE]+)"))) {
            next_env.pressure_pa = std::stod(m[1].str());
        }
        if (std::regex_search(es, m, std::regex("\"gravity_m_s2\"\\s*:\\s*([-+0-9.eE]+)"))) {
            next_env.gravity_m_s2 = std::stod(m[1].str());
        }
        if (std::regex_search(es, m, std::regex("\"convection_h_w_m2k\"\\s*:\\s*([-+0-9.eE]+)"))) {
            next_env.convection_h_w_m2k = std::stod(m[1].str());
        }
        if (std::regex_search(es, m, std::regex("\"oxidizing_atmosphere\"\\s*:\\s*(true|false)"))) {
            next_env.oxidizing_atmosphere = (m[1].str() == "true");
        }
    }

    core::domain::MaterialRegistry next_mats = materials_;
    bool has_mats = false;
    if (auto mat_blk = extract_json_block(json_str, "materials_registry", '{', '}')) {
        has_mats = true;
        auto mr = core::domain::MaterialRegistry::from_json(*mat_blk);
        if (mr.is_err()) return mr.error();
        next_mats = std::move(mr.value());
    }

    std::vector<mat::MixtureExperiment> next_mix = mixtures_;
    bool has_mix = false;
    if (auto arr = extract_json_block(json_str, "mixtures", '[', ']')) {
        has_mix = true;
        next_mix.clear();
        for (auto item : split_json_object_array(*arr)) {
            auto r = mat::MixtureExperiment::from_json(item);
            if (r.is_ok()) next_mix.push_back(std::move(r.value()));
        }
    }

    PhysicalObjectManager next_objs;
    bool has_objs = false;
    if (auto arr = extract_json_block(json_str, "objects", '[', ']')) {
        has_objs = true;
        for (auto item : split_json_object_array(*arr)) {
            auto r = PhysicalObject::from_json(item);
            if (r.is_ok()) {
                (void)next_objs.add_object(std::move(r.value()), next_mats);
            }
        }
    } else if (is_diff) {
        next_objs = object_manager_;
    }

    std::vector<BatteryDesignSpec> next_bats = batteries_;
    bool has_bats = false;
    if (auto arr = extract_json_block(json_str, "batteries", '[', ']')) {
        has_bats = true;
        next_bats.clear();
        for (auto item : split_json_object_array(*arr)) {
            auto r = BatteryDesignSpec::from_json(item);
            if (r.is_ok()) {
                BatteryDesignSpec b = std::move(r.value());
                b.derived = BatteryEngineer::evaluate_and_validate(b, next_mats);
                next_bats.push_back(std::move(b));
            }
        }
    }

    ComponentLibraryManager next_comps;
    bool has_comps = false;
    if (auto arr = extract_json_block(json_str, "components", '[', ']')) {
        has_comps = true;
        for (auto item : split_json_object_array(*arr)) {
            auto r = GeneratedComponentDefinition::from_json(item);
            if (r.is_ok()) {
                (void)next_comps.register_component(std::move(r.value()));
            }
        }
    } else if (is_diff) {
        next_comps = component_library_;
    }

    std::vector<InteractiveCircuitDocument> next_circs;
    bool has_circs = false;
    if (auto arr = extract_json_block(json_str, "circuits", '[', ']')) {
        has_circs = true;
        for (auto item : split_json_object_array(*arr)) {
            auto r = InteractiveCircuitDocument::from_json(item);
            if (r.is_ok()) next_circs.push_back(std::move(r.value()));
        }
    } else if (is_diff) {
        next_circs = circuits_;
    }
    if (next_circs.empty()) {
        next_circs.emplace_back("Main_Circuit");
    }

    size_t next_idx = active_circuit_idx_;
    std::smatch idx_m;
    if (std::regex_search(s, idx_m, std::regex("\"active_circuit_idx\"\\s*:\\s*([0-9]+)"))) {
        next_idx = static_cast<size_t>(std::stoull(idx_m[1].str()));
    }
    if (next_idx >= next_circs.size()) next_idx = 0;

    if (auto ticks_blk = extract_json_block(json_str, "ticks", '{', '}')) {
        const std::string ts(*ticks_blk);
        std::smatch tm;
        if (std::regex_search(ts, tm, std::regex("\"global\"\\s*:\\s*([0-9]+)"))) {
            ticks_.global = std::stoull(tm[1].str());
        }
        if (std::regex_search(ts, tm, std::regex("\"materials\"\\s*:\\s*([0-9]+)"))) {
            ticks_.materials = std::stoull(tm[1].str());
        }
        if (std::regex_search(ts, tm, std::regex("\"objects\"\\s*:\\s*([0-9]+)"))) {
            ticks_.objects = std::stoull(tm[1].str());
        }
        if (std::regex_search(ts, tm, std::regex("\"components\"\\s*:\\s*([0-9]+)"))) {
            ticks_.components = std::stoull(tm[1].str());
        }
        if (std::regex_search(ts, tm, std::regex("\"circuits\"\\s*:\\s*([0-9]+)"))) {
            ticks_.circuits = std::stoull(tm[1].str());
        }
        if (std::regex_search(ts, tm, std::regex("\"simulation\"\\s*:\\s*([0-9]+)"))) {
            ticks_.simulation = std::stoull(tm[1].str());
        }
    }

    // Commit loaded state atomically
    if (next_proj || !is_diff) project_ = std::move(next_proj);
    if (has_env || !is_diff) env_ = next_env;
    if (has_mats || !is_diff) materials_ = std::move(next_mats);
    if (has_mix || !is_diff) mixtures_ = std::move(next_mix);
    if (has_objs || !is_diff) object_manager_ = std::move(next_objs);
    if (has_bats || !is_diff) batteries_ = std::move(next_bats);
    if (has_comps || !is_diff) {
        component_library_ = std::move(next_comps);
        component_library_.recalculate_all(object_manager_, materials_);
    }
    if (has_circs || !is_diff) {
        circuits_ = std::move(next_circs);
        for (auto& c : circuits_) {
            c.sync_from_component_library(component_library_);
            c.resolve_connectivity();
        }
        active_circuit_idx_ = next_idx;
    }

    if (project_) project_->clear_dirty();
    notify_change(DependencyChangeKind::ProjectReset,
                  project_ ? project_->manifest().id : core::base::Uuid{},
                  is_diff ? "Applied differential Shunya v4.0 update" : "Loaded Shunya project bundle");
    return core::base::Result<void>::ok();
}

core::base::Result<void> EngineeringWorkspaceState::save_to_file(
    const std::filesystem::path& path,
    const Shunya4SaveOptions& options) {
    std::error_code ec;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), ec);
    }
    const std::filesystem::path tmp_path = path.string() + ".tmp";
    {
        std::ofstream os(tmp_path, std::ios::binary | std::ios::trunc);
        if (!os.is_open()) {
            return make_eng_error(4902, "Failed to open project file for writing: " + path.string());
        }
        if (options.binary_format || path.extension() == ".shunya4bin") {
            auto bin = to_shunya4_binary(options);
            os.write(reinterpret_cast<const char*>(bin.data()), static_cast<std::streamsize>(bin.size()));
        } else {
            os << to_bundle_json(options);
        }
        if (!os.good()) {
            return make_eng_error(4903, "Failed while writing project bundle to: " + path.string());
        }
    }
    std::filesystem::rename(tmp_path, path, ec);
    if (ec) {
        std::filesystem::copy_file(tmp_path, path, std::filesystem::copy_options::overwrite_existing, ec);
        std::filesystem::remove(tmp_path, ec);
        if (ec) {
            return make_eng_error(4904, "Failed to finalize project bundle save: " + ec.message());
        }
    }
    if (project_) {
        project_->set_file_path(path.string());
        project_->clear_dirty();
    }
    return core::base::Result<void>::ok();
}

core::base::Result<void> EngineeringWorkspaceState::load_from_file(const std::filesystem::path& path) {
    return load_from_file_with_progress(path, nullptr, {});
}

core::base::Result<void> EngineeringWorkspaceState::load_from_file_with_progress(
    const std::filesystem::path& path,
    LoadProgressCallback progress_cb,
    core::base::CancellationToken cancel_token) {

    auto report = [&](double frac, std::string stage) {
        if (progress_cb) {
            progress_cb(ProjectLoadProgress{.fraction = frac, .stage = std::move(stage)});
        }
    };

    if (cancel_token.is_cancelled()) {
        return core::base::Result<void>::err(core::base::Error{
            .code = 4920,
            .domain = core::base::ErrorDomain::System,
            .severity = core::base::Severity::Warning,
            .message = "Project load cancelled",
            .details = "Operation aborted before file read"
        });
    }

    report(0.10, "Opening and reading file container");

    std::ifstream is(path, std::ios::binary);
    if (!is.is_open()) {
        return make_eng_error(4905, "Failed to open project file: " + path.string());
    }
    std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>());

    if (cancel_token.is_cancelled()) {
        return core::base::Result<void>::err(core::base::Error{
            .code = 4920,
            .domain = core::base::ErrorDomain::System,
            .severity = core::base::Severity::Warning,
            .message = "Project load cancelled",
            .details = "Operation aborted after reading file buffer"
        });
    }

    report(0.30, "Validating container integrity & schema");

    std::string_view json_view;
    if (buffer.size() >= sizeof(Shunya4BinaryHeader) && std::memcmp(buffer.data(), "SHUNYA4\0", 8) == 0) {
        Shunya4BinaryHeader hdr{};
        std::memcpy(&hdr, buffer.data(), sizeof(Shunya4BinaryHeader));
        if (hdr.version != 4) {
            return make_eng_error(4916, "Unsupported .shunya4 binary version: " + std::to_string(hdr.version));
        }
        if (sizeof(Shunya4BinaryHeader) + hdr.payload_size > buffer.size()) {
            return make_eng_error(4917, "Invalid .shunya4 binary container: payload truncated");
        }
        const uint8_t* payload = buffer.data() + sizeof(Shunya4BinaryHeader);
        const uint32_t computed_crc = compute_crc32(payload, hdr.payload_size);
        if (computed_crc != hdr.checksum) {
            return make_eng_error(4918, "CRC32 checksum mismatch in .shunya4 container: corrupted data");
        }
        json_view = std::string_view(reinterpret_cast<const char*>(payload), hdr.payload_size);
    } else {
        json_view = std::string_view(reinterpret_cast<const char*>(buffer.data()), buffer.size());
    }

    if (cancel_token.is_cancelled()) {
        return core::base::Result<void>::err(core::base::Error{
            .code = 4920,
            .domain = core::base::ErrorDomain::System,
            .severity = core::base::Severity::Warning,
            .message = "Project load cancelled",
            .details = "Operation aborted before schema parsing"
        });
    }

    report(0.60, "Migrating schema & parsing domain entities");
    auto parse_res = from_bundle_json(json_view);
    if (parse_res.is_err()) {
        return parse_res;
    }

    if (cancel_token.is_cancelled()) {
        return core::base::Result<void>::err(core::base::Error{
            .code = 4920,
            .domain = core::base::ErrorDomain::System,
            .severity = core::base::Severity::Warning,
            .message = "Project load cancelled",
            .details = "Operation aborted before finalizing"
        });
    }

    report(0.85, "Resolving circuit schematics & 3D connectivity");
    if (project_) {
        project_->set_file_path(path.string());
        project_->clear_dirty();
    }

    report(1.00, "Project loaded successfully");
    return core::base::Result<void>::ok();
}

std::future<core::base::Result<void>> EngineeringWorkspaceState::load_from_file_async(
    const std::filesystem::path& path,
    LoadProgressCallback progress_cb,
    core::base::CancellationToken cancel_token) {
    return std::async(std::launch::async, [this, path, progress_cb = std::move(progress_cb), cancel_token]() mutable {
        return load_from_file_with_progress(path, std::move(progress_cb), cancel_token);
    });
}

core::base::Result<void> EngineeringWorkspaceState::seed_lithium_battery_workflow() {
    if (!matdb::ElementDatabase::global().is_loaded()) {
        auto lr = matdb::ElementDatabase::global().load_default();
        if (lr.is_err()) return lr.error();
    }

    auto proj_res = core::domain::Project::create_new("LiFePO4_Battery_Demo", "Shunya Phase 3");
    if (proj_res.is_ok()) {
        reset_project(proj_res.value());
    }

    auto promote_exp = [&](mat::MixtureExperiment exp, core::base::Uuid& out_mat_id) -> core::base::Result<void> {
        auto mres = mat::MaterialMixingLab::promote_to_material(exp);
        if (mres.is_err()) return mres.error();
        core::domain::Material mat = mres.value();
        out_mat_id = mat.id();
        auto rreg = materials_.register_material(std::move(mat));
        if (rreg.is_err()) return rreg.error();
        mixtures_.push_back(std::move(exp));
        return core::base::Result<void>::ok();
    };

    // 1. Cathode: LiFePO4 stoichiometry mixture (ChemicalCompound / PhysicalMixture)
    core::base::Uuid cathode_id, anode_id, cu_id, al_id, ni_cr_id, separator_id, electrolyte_id, casing_id;

    {
        mat::MixtureExperiment exp;
        exp.name = "LiFePO4 Cathode Active";
        exp.scenario = mat::FormationScenario::ChemicalCompound;
        exp.inputs = {
            {"Li", mat::QuantityInputBasis::MoleCount, 1.0, 0.0, false},
            {"Fe", mat::QuantityInputBasis::MoleCount, 1.0, 0.0, false},
            {"P",  mat::QuantityInputBasis::MoleCount, 1.0, 0.0, false},
            {"O",  mat::QuantityInputBasis::MoleCount, 4.0, 0.0, false}
        };
        auto r = promote_exp(std::move(exp), cathode_id);
        if (r.is_err()) return r.error();
    }
    // 2. Anode: Carbon (Graphite) with trace Silicon enhancement
    {
        mat::MixtureExperiment exp;
        exp.name = "Graphite-Si Anode";
        exp.scenario = mat::FormationScenario::CompositeMaterial;
        exp.inputs = {
            {"C",  mat::QuantityInputBasis::MassFractionPct, 95.0, 0.0, true},
            {"Si", mat::QuantityInputBasis::MassFractionPct, 5.0,  0.0, false}
        };
        auto r = promote_exp(std::move(exp), anode_id);
        if (r.is_err()) return r.error();
    }
    // 3. Copper Conductor (Element Cu)
    {
        mat::MixtureExperiment exp;
        exp.name = "OFHC Copper Conductor";
        exp.scenario = mat::FormationScenario::PhysicalMixture;
        exp.inputs = {{"Cu", mat::QuantityInputBasis::MassFractionPct, 100.0, 0.0, false}};
        auto r = promote_exp(std::move(exp), cu_id);
        if (r.is_err()) return r.error();
    }
    // 4. Aluminum Collector & Housing
    {
        mat::MixtureExperiment exp;
        exp.name = "Aluminum 1100 Foil";
        exp.scenario = mat::FormationScenario::Alloy;
        exp.inputs = {{"Al", mat::QuantityInputBasis::MassFractionPct, 100.0, 0.0, false}};
        auto r = promote_exp(std::move(exp), al_id);
        if (r.is_err()) return r.error();
    }
    // 5. Nichrome Heating / Resistor Alloy (80% Ni / 20% Cr)
    {
        mat::MixtureExperiment exp;
        exp.name = "Nichrome 80/20 Resistor Alloy";
        exp.scenario = mat::FormationScenario::Alloy;
        exp.inputs = {
            {"Ni", mat::QuantityInputBasis::MassFractionPct, 80.0, 0.0, false},
            {"Cr", mat::QuantityInputBasis::MassFractionPct, 20.0, 0.0, false}
        };
        auto r = promote_exp(std::move(exp), ni_cr_id);
        if (r.is_err()) return r.error();
    }

    // 6. Polymer Separator & LiPF6 Carbonate Electrolyte (explicit insulating/ionic definitions)
    {
        core::domain::Material sep(core::base::Uuid::generate_random(), "PP/PE Porous Separator",
                                   core::domain::MaterialPhysicalState::Solid, 920.0);
        sep.set_provenance("Scientific Reference: Polyolefin microporous separator");
        core::domain::ThermalProperties tp;
        tp.thermal_conductivity = 0.25;
        tp.specific_heat_capacity = 1900.0;
        tp.melting_point_k = 408.15;
        sep.set_thermal(tp);
        core::domain::ElectricalProperties ep;
        ep.electrical_conductivity = 1e-12;
        ep.electrical_resistivity = 1e12;
        ep.relative_permittivity = 2.3;
        sep.set_electrical(ep);
        separator_id = sep.id();
        auto r = materials_.register_material(std::move(sep));
        if (r.is_err()) return r.error();
    }
    {
        core::domain::Material elec(core::base::Uuid::generate_random(), "LiPF6 in EC/DMC Electrolyte",
                                    core::domain::MaterialPhysicalState::Liquid, 1210.0);
        elec.set_provenance("Scientific Reference: 1M LiPF6 EC:DMC ionic solution");
        core::domain::ThermalProperties tp;
        tp.thermal_conductivity = 0.45;
        tp.specific_heat_capacity = 1600.0;
        tp.boiling_point_k = 395.0;
        elec.set_thermal(tp);
        core::domain::ElectricalProperties ep;
        // Ionic conductivity ~1 S/m, non-electron-shorting separator fills pores
        ep.electrical_conductivity = 1.0;
        ep.electrical_resistivity = 1.0;
        ep.relative_permittivity = 30.0;
        elec.set_electrical(ep);
        electrolyte_id = elec.id();
        auto r = materials_.register_material(std::move(elec));
        if (r.is_err()) return r.error();
    }
    {
        casing_id = al_id;
    }

    // 7. Create Physical Objects: Copper Wire & Load Resistor Body
    PhysicalObject wire_obj;
    wire_obj.name = "Copper_Bus_Wire";
    wire_obj.category = "Conductor";
    wire_obj.geometry.kind = ObjectShapeKind::Cylinder;
    wire_obj.geometry.dim_y_m = 0.25; // length 25 cm
    wire_obj.geometry.radius_m = 0.0015; // 1.5 mm radius
    wire_obj.regions = {{"whole", "Copper Core", cu_id, 1.0, 0.0, true}};
    auto wire_id_res = object_manager_.add_object(wire_obj, materials_);
    if (wire_id_res.is_err()) return wire_id_res.error();
    const core::base::Uuid wire_obj_id = wire_id_res.value();

    PhysicalObject res_obj;
    res_obj.name = "Nichrome_Load_Element";
    res_obj.category = "Resistor";
    res_obj.geometry.kind = ObjectShapeKind::Box;
    res_obj.geometry.dim_x_m = 0.0002;
    res_obj.geometry.dim_y_m = 0.0002;
    res_obj.geometry.dim_z_m = 0.20;
    res_obj.geometry.position = {0.15, 0.0, 0.0};
    res_obj.regions = {{"whole", "Nichrome Core", ni_cr_id, 1.0, 0.0, true}};
    auto res_id_res = object_manager_.add_object(res_obj, materials_);
    if (res_id_res.is_err()) return res_id_res.error();
    const core::base::Uuid res_obj_id = res_id_res.value();

    // 8. Engineer Battery Cell & Generate Battery Component
    BatteryDesignSpec bat;
    bat.name = "LiFePO4_Pouch_Cell_3V3";
    bat.chemistry_label = "LiFePO4 / Graphite";
    bat.materials.anode_material_id = anode_id;
    bat.materials.cathode_material_id = cathode_id;
    bat.materials.electrolyte_material_id = electrolyte_id;
    bat.materials.separator_material_id = separator_id;
    bat.materials.anode_collector_material_id = cu_id;
    bat.materials.cathode_collector_material_id = al_id;
    bat.materials.casing_material_id = casing_id;
    bat.materials.positive_terminal_material_id = al_id;
    bat.materials.negative_terminal_material_id = cu_id;

    auto bat_comp_res = BatteryEngineer::generate_component(bat, materials_, &object_manager_);
    if (bat_comp_res.is_err()) return bat_comp_res.error();
    batteries_.push_back(bat);
    const GeneratedComponentDefinition bat_comp = bat_comp_res.value();
    auto rbc = component_library_.register_component(bat_comp);
    if (rbc.is_err()) return rbc.error();

    // Re-acquire pointers AFTER all add_object calls so vector reallocation cannot invalidate them
    const PhysicalObject* stored_wire = object_manager_.get_object(wire_obj_id);
    const PhysicalObject* stored_res = object_manager_.get_object(res_obj_id);
    if (!stored_wire || !stored_res) {
        return make_eng_error(4910, "Internal error: seeded object missing after registration");
    }

    // 9. Generate Wire & Resistor Components from Objects
    ComponentTypeConfig wire_cfg;
    wire_cfg.conductor_length_m = 0.25;
    auto wire_comp_res = ComponentGenerator::generate(
        "Copper_Bus_Wire_Comp",
        *stored_wire,
        ComponentKind::ConductorWire,
        wire_cfg,
        {
            {"A", core::domain::ElectricalPinPolarity::PassiveBiDirectional, {-0.125, 0.0, 0.0}, "whole"},
            {"B", core::domain::ElectricalPinPolarity::PassiveBiDirectional, {0.125, 0.0, 0.0}, "whole"}
        },
        materials_);
    if (wire_comp_res.is_err()) return wire_comp_res.error();
    (void)component_library_.register_component(wire_comp_res.value());

    ComponentTypeConfig res_cfg;
    res_cfg.conductor_length_m = 0.20;
    auto res_comp_res = ComponentGenerator::generate(
        "Nichrome_Load_Comp",
        *stored_res,
        ComponentKind::Resistor,
        res_cfg,
        {
            {"T1", core::domain::ElectricalPinPolarity::PassiveBiDirectional, {-0.1, 0.0, 0.0}, "whole"},
            {"T2", core::domain::ElectricalPinPolarity::PassiveBiDirectional, {0.1, 0.0, 0.0}, "whole"}
        },
        materials_);
    if (res_comp_res.is_err()) return res_comp_res.error();
    const GeneratedComponentDefinition load_comp = res_comp_res.value();
    (void)component_library_.register_component(load_comp);

    // 10. Build Closed Battery -> Switch -> Load Resistor -> Ground Circuit
    InteractiveCircuitDocument doc("LiFePO4_Load_Test_Circuit");
    PlacedCircuitComponent p_bat;
    p_bat.id = "BAT1";
    p_bat.label = "LiFePO4 Cell";
    p_bat.kind = CircuitSymbolKind::Battery;
    p_bat.generated_component_id = bat_comp.id;
    p_bat.x_mm = 0.0;
    p_bat.y_mm = 0.0;
    p_bat.primary_value = bat_comp.config.nominal_voltage_v;
    p_bat.secondary_value = bat_comp.config.internal_resistance_ohm;
    p_bat.tertiary_value = bat_comp.config.capacity_ah;
    doc.add_component(p_bat);

    PlacedCircuitComponent p_sw;
    p_sw.id = "SW1";
    p_sw.label = "Main Switch";
    p_sw.kind = CircuitSymbolKind::Switch;
    p_sw.x_mm = 40.0;
    p_sw.y_mm = 0.0;
    p_sw.switch_closed = true;
    p_sw.primary_value = 1e-3;
    doc.add_component(p_sw);

    PlacedCircuitComponent p_r;
    p_r.id = "R1";
    p_r.label = "Load Resistor";
    p_r.kind = CircuitSymbolKind::Resistor;
    p_r.generated_component_id = load_comp.id;
    p_r.x_mm = 90.0;
    p_r.y_mm = 0.0;
    p_r.primary_value = std::max(1.0, load_comp.derived.primary_value);
    doc.add_component(p_r);

    PlacedCircuitComponent p_gnd;
    p_gnd.id = "GND1";
    p_gnd.label = "Ground";
    p_gnd.kind = CircuitSymbolKind::Ground;
    p_gnd.x_mm = 0.0;
    p_gnd.y_mm = 30.0;
    doc.add_component(p_gnd);

    // Connect terminals without crossing intermediate pins: route top bus at y = -15 mm
    const auto bat_terms = p_bat.terminal_world_positions();
    const auto sw_terms = p_sw.terminal_world_positions();
    const auto r_terms = p_r.terminal_world_positions();
    const auto gnd_terms = p_gnd.terminal_world_positions();

    // Wire 1: BAT+ -> SW.1 (via y = -15 to avoid BAT- at +10,0)
    doc.add_wire({"W1", {bat_terms[0].second, {bat_terms[0].second.x, -15.0}, {sw_terms[0].second.x, -15.0}, sw_terms[0].second}, {}});
    // Wire 2: SW.2 -> R1.1
    doc.add_wire({"W2", {sw_terms[1].second, r_terms[0].second}, {}});
    // Wire 3: R1.2 -> GND
    doc.add_wire({"W3", {r_terms[1].second, {r_terms[1].second.x, 25.0}, gnd_terms[0].second}, {}});
    // Wire 4: BAT- -> GND
    doc.add_wire({"W4", {bat_terms[1].second, {bat_terms[1].second.x, 25.0}, gnd_terms[0].second}, {}});

    doc.resolve_connectivity();
    circuits_.clear();
    circuits_.push_back(std::move(doc));
    active_circuit_idx_ = 0;

    physics_runtime_.initialize_from_objects(
        object_manager_, env_.temperature_k, env_.gravity_m_s2, env_.convection_h_w_m2k);

    if (project_) project_->clear_dirty();
    notify_change(DependencyChangeKind::ProjectReset,
                  project_ ? project_->manifest().id : core::base::Uuid{},
                  "Seeded complete LiFePO4 battery -> circuit workflow");
    return core::base::Result<void>::ok();
}

core::base::ValidationReport EngineeringWorkspaceState::validate_workspace() const {
    core::base::ValidationReport report = materials_.validate_registry();
    if (!project_) {
        report.add_error("project", "No active project manifest loaded");
    }
    if (env_.temperature_k <= 0.0) {
        report.add_error("environment.temperature_k", "Ambient temperature must be strictly positive (K)");
    }
    if (env_.pressure_pa < 0.0) {
        report.add_error("environment.pressure_pa", "Ambient pressure cannot be negative (Pa)");
    }
    for (const auto& obj : object_manager_.objects()) {
        if (!obj.computed.valid) {
            report.add_warning("object." + obj.name, "Object properties invalid or uncomputed");
        }
    }
    for (const auto& c : circuits_) {
        if (c.components().empty()) continue;
        const auto issues = c.validate();
        for (const auto& iss : issues) {
            if (iss.severity == core::domain::CircuitDiagnosticSeverity::Error) {
                report.add_error("circuit." + c.name(), iss.message);
            } else if (iss.severity == core::domain::CircuitDiagnosticSeverity::Warning) {
                report.add_warning("circuit." + c.name(), iss.message);
            }
        }
    }
    return report;
}

namespace {
class SnapshotTransactionCommand : public core::domain::ICommand {
public:
    SnapshotTransactionCommand(
        EngineeringWorkspaceState* state,
        std::string name,
        std::string before_snapshot,
        std::string after_snapshot)
        : state_(state),
          name_(std::move(name)),
          before_snapshot_(std::move(before_snapshot)),
          after_snapshot_(std::move(after_snapshot)) {}

    core::base::Result<void> execute() override {
        return core::base::Result<void>::ok();
    }

    core::base::Result<void> undo() override {
        if (!state_) return make_eng_error(4912, "Null workspace state in undo");
        return state_->from_bundle_json(before_snapshot_);
    }

    core::base::Result<void> redo() override {
        if (!state_) return make_eng_error(4913, "Null workspace state in redo");
        return state_->from_bundle_json(after_snapshot_);
    }

    [[nodiscard]] std::string_view name() const noexcept override { return name_; }

private:
    EngineeringWorkspaceState* state_{nullptr};
    std::string name_;
    std::string before_snapshot_;
    std::string after_snapshot_;
};
} // namespace

uint64_t EngineeringWorkspaceState::create_checkpoint(std::string name) {
    WorkspaceCheckpoint cp;
    cp.id = next_checkpoint_id_++;
    cp.name = std::move(name);
    cp.tick = ticks_.global;
    cp.bundle_snapshot = to_bundle_json();
    checkpoints_.push_back(cp);
    return cp.id;
}

core::base::Result<void> EngineeringWorkspaceState::restore_checkpoint(uint64_t checkpoint_id) {
    for (const auto& cp : checkpoints_) {
        if (cp.id == checkpoint_id) {
            auto res = from_bundle_json(cp.bundle_snapshot);
            if (res.is_ok()) {
                notify_change(DependencyChangeKind::ProjectReset,
                              project_ ? project_->manifest().id : core::base::Uuid{},
                              "Restored workspace checkpoint: " + cp.name);
            }
            return res;
        }
    }
    return make_eng_error(4910, "Checkpoint ID not found: " + std::to_string(checkpoint_id));
}

std::vector<WorkspaceCheckpoint> EngineeringWorkspaceState::checkpoints() const {
    return checkpoints_;
}

std::optional<WorkspaceCheckpoint> EngineeringWorkspaceState::find_checkpoint(uint64_t id) const {
    for (const auto& cp : checkpoints_) {
        if (cp.id == id) return cp;
    }
    return std::nullopt;
}

std::optional<WorkspaceCheckpoint> EngineeringWorkspaceState::find_checkpoint(std::string_view name) const {
    for (const auto& cp : checkpoints_) {
        if (cp.name == name) return cp;
    }
    return std::nullopt;
}

bool EngineeringWorkspaceState::delete_checkpoint(uint64_t id) {
    auto it = std::remove_if(checkpoints_.begin(), checkpoints_.end(),
                             [id](const WorkspaceCheckpoint& cp) { return cp.id == id; });
    if (it != checkpoints_.end()) {
        checkpoints_.erase(it, checkpoints_.end());
        return true;
    }
    return false;
}

core::base::Result<void> EngineeringWorkspaceState::execute_transaction(
    std::string name,
    std::function<core::base::Result<void>()> action) {
    if (!action) {
        return make_eng_error(4911, "Transaction action is empty");
    }
    const std::string before = to_bundle_json();
    auto action_res = action();
    if (action_res.is_err()) {
        (void)from_bundle_json(before);
        return action_res;
    }
    const std::string after = to_bundle_json();
    auto cmd = std::make_unique<SnapshotTransactionCommand>(this, std::move(name), before, after);
    return command_stack_.push_and_execute(std::move(cmd));
}

core::base::Result<void> EngineeringWorkspaceState::undo() {
    return command_stack_.undo();
}

core::base::Result<void> EngineeringWorkspaceState::redo() {
    return command_stack_.redo();
}

EntityRelationshipGraph EngineeringWorkspaceState::build_entity_graph() const {
    EntityRelationshipGraph graph;
    graph.rebuild_from_workspace(*this);
    return graph;
}

GarbageCollectionReport EngineeringWorkspaceState::collect_garbage_and_prune_orphans(bool prune_unreferenced_materials) {
    GarbageCollectionReport rep;

    // 1. Gather all referenced material UUIDs
    std::unordered_set<core::base::Uuid> referenced_mats;
    for (const auto& obj : object_manager_.objects()) {
        for (const auto& reg : obj.regions) {
            if (materials_.has_material(reg.material_id)) {
                referenced_mats.insert(reg.material_id);
            }
        }
    }
    for (const auto& bat : batteries_) {
        if (materials_.has_material(bat.materials.cathode_material_id)) referenced_mats.insert(bat.materials.cathode_material_id);
        if (materials_.has_material(bat.materials.anode_material_id)) referenced_mats.insert(bat.materials.anode_material_id);
        if (materials_.has_material(bat.materials.electrolyte_material_id)) referenced_mats.insert(bat.materials.electrolyte_material_id);
        if (materials_.has_material(bat.materials.separator_material_id)) referenced_mats.insert(bat.materials.separator_material_id);
        if (materials_.has_material(bat.materials.anode_collector_material_id)) referenced_mats.insert(bat.materials.anode_collector_material_id);
        if (materials_.has_material(bat.materials.cathode_collector_material_id)) referenced_mats.insert(bat.materials.cathode_collector_material_id);
        if (materials_.has_material(bat.materials.casing_material_id)) referenced_mats.insert(bat.materials.casing_material_id);
    }

    // Find or register fallback material from already-referenced materials if possible
    core::base::Uuid fallback_mat_id;
    for (const auto& ref_id : referenced_mats) {
        fallback_mat_id = ref_id;
        break;
    }
    if (fallback_mat_id.is_nil()) {
        if (const auto* cu = materials_.get_material("Copper")) {
            fallback_mat_id = cu->id();
        } else {
            core::domain::Material fallback(core::base::Uuid::generate_random(), "DefaultMaterial", core::domain::MaterialPhysicalState::Solid, 2700.0);
            fallback_mat_id = fallback.id();
            (void)materials_.register_material(std::move(fallback));
        }
        referenced_mats.insert(fallback_mat_id);
    }

    for (auto& obj : object_manager_.objects_mut()) {
        for (auto& reg : obj.regions) {
            if (!materials_.has_material(reg.material_id)) {
                reg.material_id = fallback_mat_id;
                ++rep.repaired_regions;
                rep.actions_taken.push_back("Repaired region '" + reg.region_id + "' in object '" + obj.name + "' with fallback material");
            }
        }
    }

    // 2. Prune unreferenced materials if requested
    if (prune_unreferenced_materials) {
        std::vector<core::base::Uuid> to_prune;
        for (const auto& [mid, m] : materials_.materials()) {
            if (referenced_mats.find(mid) == referenced_mats.end() && mid != fallback_mat_id) {
                to_prune.push_back(mid);
            }
        }
        for (const auto& mid : to_prune) {
            (void)materials_.unregister_material(mid);
            ++rep.pruned_materials;
            rep.actions_taken.push_back("Pruned unreferenced material UUID: " + mid.to_string());
        }
    }

    // 3. Prune components whose source_object_id is missing
    std::vector<core::base::Uuid> comp_ids_to_remove;
    for (const auto& comp : component_library_.components()) {
        if (!object_manager_.get_object(comp.source_object_id)) {
            comp_ids_to_remove.push_back(comp.id);
        }
    }
    for (const auto& cid : comp_ids_to_remove) {
        (void)component_library_.remove_component(cid);
        ++rep.pruned_components;
        rep.actions_taken.push_back("Pruned component UUID " + cid.to_string() + " with missing source object");
    }

    // 4. Prune placed circuit component references if generated_component_id does not exist
    for (auto& circ : circuits_) {
        for (auto& inst : circ.components_mut()) {
            if (inst.generated_component_id && !component_library_.get_component(*inst.generated_component_id)) {
                inst.generated_component_id = std::nullopt;
                ++rep.pruned_circuit_instances;
                rep.actions_taken.push_back("Reset missing generated_component_id on circuit component " + inst.id);
            }
        }
        // Prune dangling wires
        auto& wires = circ.wires_mut();
        auto wire_it = std::remove_if(wires.begin(), wires.end(), [](const CircuitWire& w) {
            return w.points_mm.size() < 2;
        });
        if (wire_it != wires.end()) {
            const size_t pruned = static_cast<size_t>(std::distance(wire_it, wires.end()));
            rep.pruned_dangling_wires += pruned;
            wires.erase(wire_it, wires.end());
            rep.actions_taken.push_back("Pruned " + std::to_string(pruned) + " dangling wires in circuit " + circ.name());
        }
    }

    if (rep.repaired_regions > 0 || rep.pruned_materials > 0 || rep.pruned_components > 0 || rep.pruned_circuit_instances > 0) {
        notify_change(DependencyChangeKind::ProjectReset, {}, "Garbage collection pruned orphaned assets");
    }

    return rep;
}

core::base::Result<CorruptionRecoveryReport> EngineeringWorkspaceState::recover_from_corrupted_bundle(std::string_view corrupted_data) {
    if (corrupted_data.empty()) {
        return make_eng_error(4925, "Cannot recover from empty project bundle");
    }

    CorruptionRecoveryReport rep;
    rep.recovered = true;

    // Check if binary header exists
    std::string_view working_view = corrupted_data;
    if (working_view.size() >= sizeof(Shunya4BinaryHeader) && std::memcmp(working_view.data(), "SHUNYA4\0", 8) == 0) {
        rep.repairs_applied.push_back("Detected binary container header; extracting payload");
        working_view = working_view.substr(sizeof(Shunya4BinaryHeader));
    }

    std::string repaired_json(working_view);

    // If JSON is truncated, count brackets and auto-close
    int open_braces = 0, open_brackets = 0;
    bool in_string = false;
    bool escaped = false;
    for (char c : repaired_json) {
        if (escaped) { escaped = false; continue; }
        if (c == '\\') { escaped = true; continue; }
        if (c == '"') { in_string = !in_string; continue; }
        if (!in_string) {
            if (c == '{') ++open_braces;
            else if (c == '}') --open_braces;
            else if (c == '[') ++open_brackets;
            else if (c == ']') --open_brackets;
        }
    }

    if (in_string) {
        repaired_json += "\"";
        rep.repairs_applied.push_back("Closed dangling open string in corrupted JSON");
    }
    while (open_brackets > 0) {
        repaired_json += "]";
        --open_brackets;
        rep.repairs_applied.push_back("Closed dangling JSON array bracket");
    }
    while (open_braces > 0) {
        repaired_json += "}";
        --open_braces;
        rep.repairs_applied.push_back("Closed dangling JSON object brace");
    }

    // Try standard load with repaired JSON first
    auto standard_res = from_bundle_json(repaired_json);
    if (standard_res.is_ok()) {
        rep.salvaged_materials = materials_.material_count();
        rep.salvaged_objects = object_manager_.count();
        rep.salvaged_components = component_library_.components().size();
        rep.salvaged_circuits = circuits_.size();
        rep.gc_report = collect_garbage_and_prune_orphans();
        rep.repairs_applied.push_back("Full workspace state successfully salvaged from repaired JSON bundle");
        return core::base::Result<CorruptionRecoveryReport>::ok(std::move(rep));
    }

    // Heuristic multi-section recovery: salvage whatever sections we can find
    rep.repairs_applied.push_back("Applying component-wise heuristic section salvaging");

    // Salvage project manifest
    if (auto proj_blk = extract_json_block(repaired_json, "project_manifest", '{', '}')) {
        auto man_res = core::domain::ProjectManifest::from_json(*proj_blk);
        if (man_res.is_ok()) {
            project_ = std::make_shared<core::domain::Project>(man_res.value());
            rep.repairs_applied.push_back("Salvaged project manifest: " + man_res.value().name);
        }
    }
    if (!project_) {
        auto res = core::domain::Project::create_new("Recovered_Engineering_Project", "Shunya Auto-Recovery");
        if (res.is_ok()) project_ = res.value();
        rep.repairs_applied.push_back("Generated default project manifest for recovered workspace");
    }

    // Salvage environment
    if (auto env_blk = extract_json_block(repaired_json, "environment", '{', '}')) {
        const std::string es(*env_blk);
        std::smatch m;
        if (std::regex_search(es, m, std::regex("\"temperature_k\"\\s*:\\s*([-+0-9.eE]+)"))) {
            double tk = std::stod(m[1].str());
            if (tk > 0.0 && tk < 100000.0) env_.temperature_k = tk;
        }
        if (std::regex_search(es, m, std::regex("\"pressure_pa\"\\s*:\\s*([-+0-9.eE]+)"))) {
            double p = std::stod(m[1].str());
            if (p >= 0.0) env_.pressure_pa = p;
        }
    }

    // Salvage materials
    if (auto mat_blk = extract_json_block(repaired_json, "materials_registry", '{', '}')) {
        auto mr = core::domain::MaterialRegistry::from_json(*mat_blk);
        if (mr.is_ok()) {
            materials_ = std::move(mr.value());
            rep.salvaged_materials = materials_.material_count();
            rep.repairs_applied.push_back("Salvaged " + std::to_string(rep.salvaged_materials) + " material definitions");
        }
    }

    // Salvage objects
    if (auto arr = extract_json_block(repaired_json, "objects", '[', ']')) {
        for (auto item : split_json_object_array(*arr)) {
            auto r = PhysicalObject::from_json(item);
            if (r.is_ok()) {
                (void)object_manager_.add_object(std::move(r.value()), materials_);
                ++rep.salvaged_objects;
            }
        }
        rep.repairs_applied.push_back("Salvaged " + std::to_string(rep.salvaged_objects) + " physical objects");
    }

    // Salvage components
    if (auto arr = extract_json_block(repaired_json, "components", '[', ']')) {
        for (auto item : split_json_object_array(*arr)) {
            auto r = GeneratedComponentDefinition::from_json(item);
            if (r.is_ok()) {
                (void)component_library_.register_component(std::move(r.value()));
                ++rep.salvaged_components;
            }
        }
        rep.repairs_applied.push_back("Salvaged " + std::to_string(rep.salvaged_components) + " component definitions");
    }

    // Salvage circuits
    if (auto arr = extract_json_block(repaired_json, "circuits", '[', ']')) {
        for (auto item : split_json_object_array(*arr)) {
            auto r = InteractiveCircuitDocument::from_json(item);
            if (r.is_ok()) {
                circuits_.push_back(std::move(r.value()));
                ++rep.salvaged_circuits;
            }
        }
        rep.repairs_applied.push_back("Salvaged " + std::to_string(rep.salvaged_circuits) + " circuit schematics");
    }
    if (circuits_.empty()) {
        circuits_.emplace_back("Main_Circuit");
    }

    // Post-recovery pruning of orphaned references
    rep.gc_report = collect_garbage_and_prune_orphans();

    notify_change(DependencyChangeKind::ProjectReset,
                  project_ ? project_->manifest().id : core::base::Uuid{},
                  "Recovered workspace from corrupted project file");

    return core::base::Result<CorruptionRecoveryReport>::ok(std::move(rep));
}

core::base::Result<CorruptionRecoveryReport> EngineeringWorkspaceState::recover_from_corrupted_file(const std::filesystem::path& path) {
    std::ifstream is(path, std::ios::binary);
    if (!is.is_open()) {
        return make_eng_error(4905, "Failed to open project file for recovery: " + path.string());
    }
    std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>());
    std::string_view data_view(reinterpret_cast<const char*>(buffer.data()), buffer.size());
    return recover_from_corrupted_bundle(data_view);
}

} // namespace shunya::eng
