#include "shunya/eng/engineering_pipeline.hpp"

#include <algorithm>
#include <cctype>
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

void EngineeringWorkspaceState::notify_change(
    DependencyChangeKind kind,
    const core::base::Uuid& target_id,
    std::string summary) {
    ++version_;
    if (project_ && kind != DependencyChangeKind::ProjectReset) {
        project_->mark_dirty();
    }
    WorkspaceChangeEvent ev{kind, target_id, version_, std::move(summary)};
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

std::string EngineeringWorkspaceState::to_bundle_json() const {
    std::ostringstream os;
    os << std::setprecision(12);
    os << "{\n";
    os << "  \"bundle_version\": \"3.0\",\n";
    if (project_) {
        os << "  \"project_manifest\": " << project_->manifest().to_json() << ",\n";
    } else {
        os << "  \"project_manifest\": {},\n";
    }
    os << "  \"environment\": {\n"
       << "    \"temperature_k\": " << env_.temperature_k << ",\n"
       << "    \"pressure_pa\": " << env_.pressure_pa << ",\n"
       << "    \"gravity_m_s2\": " << env_.gravity_m_s2 << ",\n"
       << "    \"convection_h_w_m2k\": " << env_.convection_h_w_m2k << ",\n"
       << "    \"oxidizing_atmosphere\": " << (env_.oxidizing_atmosphere ? "true" : "false") << "\n"
       << "  },\n";
    os << "  \"materials_registry\": " << materials_.to_json() << ",\n";
    os << "  \"mixtures\": " << serialize_array(mixtures_, "  ") << ",\n";
    os << "  \"objects\": " << serialize_array(object_manager_.objects(), "  ") << ",\n";
    os << "  \"batteries\": " << serialize_array(batteries_, "  ") << ",\n";
    os << "  \"components\": " << serialize_array(component_library_.components(), "  ") << ",\n";
    os << "  \"circuits\": " << serialize_array(circuits_, "  ") << ",\n";
    os << "  \"active_circuit_idx\": " << active_circuit_idx_ << "\n";
    os << "}\n";
    return os.str();
}

core::base::Result<void> EngineeringWorkspaceState::from_bundle_json(std::string_view json_str) {
    const std::string s(json_str);
    if (s.find("\"bundle_version\"") == std::string::npos) {
        return make_eng_error(4901, "Invalid Shunya v3.0 project bundle JSON");
    }

    std::shared_ptr<core::domain::Project> next_proj;
    if (auto proj_blk = extract_json_block(json_str, "project_manifest", '{', '}')) {
        auto man_res = core::domain::ProjectManifest::from_json(*proj_blk);
        if (man_res.is_ok()) {
            next_proj = std::make_shared<core::domain::Project>(man_res.value());
        }
    }
    if (!next_proj) {
        auto res = core::domain::Project::create_new("Loaded_Engineering_Project", "Shunya Engineer");
        if (res.is_ok()) next_proj = res.value();
    }

    AmbientEnvironmentConfig next_env;
    if (auto env_blk = extract_json_block(json_str, "environment", '{', '}')) {
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

    core::domain::MaterialRegistry next_mats;
    if (auto mat_blk = extract_json_block(json_str, "materials_registry", '{', '}')) {
        auto mr = core::domain::MaterialRegistry::from_json(*mat_blk);
        if (mr.is_err()) return mr.error();
        next_mats = std::move(mr.value());
    }

    std::vector<mat::MixtureExperiment> next_mix;
    if (auto arr = extract_json_block(json_str, "mixtures", '[', ']')) {
        for (auto item : split_json_object_array(*arr)) {
            auto r = mat::MixtureExperiment::from_json(item);
            if (r.is_ok()) next_mix.push_back(std::move(r.value()));
        }
    }

    PhysicalObjectManager next_objs;
    if (auto arr = extract_json_block(json_str, "objects", '[', ']')) {
        for (auto item : split_json_object_array(*arr)) {
            auto r = PhysicalObject::from_json(item);
            if (r.is_ok()) {
                (void)next_objs.add_object(std::move(r.value()), next_mats);
            }
        }
    }

    std::vector<BatteryDesignSpec> next_bats;
    if (auto arr = extract_json_block(json_str, "batteries", '[', ']')) {
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
    if (auto arr = extract_json_block(json_str, "components", '[', ']')) {
        for (auto item : split_json_object_array(*arr)) {
            auto r = GeneratedComponentDefinition::from_json(item);
            if (r.is_ok()) {
                (void)next_comps.register_component(std::move(r.value()));
            }
        }
    }

    std::vector<InteractiveCircuitDocument> next_circs;
    if (auto arr = extract_json_block(json_str, "circuits", '[', ']')) {
        for (auto item : split_json_object_array(*arr)) {
            auto r = InteractiveCircuitDocument::from_json(item);
            if (r.is_ok()) next_circs.push_back(std::move(r.value()));
        }
    }
    if (next_circs.empty()) {
        next_circs.emplace_back("Main_Circuit");
    }

    size_t next_idx = 0;
    std::smatch idx_m;
    if (std::regex_search(s, idx_m, std::regex("\"active_circuit_idx\"\\s*:\\s*([0-9]+)"))) {
        next_idx = static_cast<size_t>(std::stoull(idx_m[1].str()));
    }
    if (next_idx >= next_circs.size()) next_idx = 0;

    // Commit loaded state atomically
    project_ = std::move(next_proj);
    env_ = next_env;
    materials_ = std::move(next_mats);
    mixtures_ = std::move(next_mix);
    object_manager_ = std::move(next_objs);
    batteries_ = std::move(next_bats);
    component_library_ = std::move(next_comps);
    component_library_.recalculate_all(object_manager_, materials_);
    circuits_ = std::move(next_circs);
    for (auto& c : circuits_) {
        c.sync_from_component_library(component_library_);
        c.resolve_connectivity();
    }
    active_circuit_idx_ = next_idx;
    if (project_) project_->clear_dirty();
    notify_change(DependencyChangeKind::ProjectReset,
                  project_ ? project_->manifest().id : core::base::Uuid{},
                  "Loaded Shunya v3.0 project bundle");
    return core::base::Result<void>::ok();
}

core::base::Result<void> EngineeringWorkspaceState::save_to_file(const std::filesystem::path& path) {
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
        os << to_bundle_json();
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
    std::ifstream is(path, std::ios::binary);
    if (!is.is_open()) {
        return make_eng_error(4905, "Failed to open project file: " + path.string());
    }
    std::ostringstream ss;
    ss << is.rdbuf();
    auto res = from_bundle_json(ss.str());
    if (res.is_ok() && project_) {
        project_->set_file_path(path.string());
        project_->clear_dirty();
    }
    return res;
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

} // namespace shunya::eng
