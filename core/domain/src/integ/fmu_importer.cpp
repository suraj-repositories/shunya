#include "shunya/integ/fmu_importer.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdint>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace shunya::integ {

namespace {

std::string extract_xml_attr(std::string_view tag, std::string_view attr_name) {
    const std::string needle = std::string(attr_name) + "=";
    size_t pos = tag.find(needle);
    while (pos != std::string_view::npos) {
        if (pos > 0 && !std::isspace(static_cast<unsigned char>(tag[pos - 1]))) {
            pos = tag.find(needle, pos + needle.size());
            continue;
        }
        size_t q_start = pos + needle.size();
        if (q_start >= tag.size()) return {};
        const char quote = tag[q_start];
        if (quote != '"' && quote != '\'') {
            pos = tag.find(needle, pos + needle.size());
            continue;
        }
        size_t q_end = tag.find(quote, q_start + 1);
        if (q_end == std::string_view::npos) return {};
        return std::string(tag.substr(q_start + 1, q_end - q_start - 1));
    }
    return {};
}

FmiCausality parse_causality(std::string_view s) noexcept {
    if (s == "parameter") return FmiCausality::Parameter;
    if (s == "calculatedParameter") return FmiCausality::CalculatedParameter;
    if (s == "input") return FmiCausality::Input;
    if (s == "output") return FmiCausality::Output;
    if (s == "independent") return FmiCausality::Independent;
    return FmiCausality::Local;
}

FmiVariability parse_variability(std::string_view s) noexcept {
    if (s == "constant") return FmiVariability::Constant;
    if (s == "fixed") return FmiVariability::Fixed;
    if (s == "tunable") return FmiVariability::Tunable;
    if (s == "discrete") return FmiVariability::Discrete;
    return FmiVariability::Continuous;
}

template <typename FnPtr>
FnPtr load_symbol(void* lib, const char* symbol) noexcept {
#ifdef _WIN32
    auto addr = reinterpret_cast<uintptr_t>(
        ::GetProcAddress(static_cast<HMODULE>(lib), symbol));
    return reinterpret_cast<FnPtr>(addr);
#else
    auto addr = reinterpret_cast<uintptr_t>(::dlsym(lib, symbol));
    return reinterpret_cast<FnPtr>(addr);
#endif
}

} // namespace

FmuImporter::~FmuImporter() {
    unload();
}

core::base::Result<FmiModelDescription> FmuImporter::parse_model_description_xml(
    std::string_view xml_content) {
    const size_t root_pos = xml_content.find("<fmiModelDescription");
    if (root_pos == std::string_view::npos) {
        return core::base::Error{
            451, core::base::ErrorDomain::IO, core::base::Severity::Error,
            "Invalid FMI modelDescription.xml: missing <fmiModelDescription>", ""
        };
    }
    const size_t root_end = xml_content.find('>', root_pos);
    if (root_end == std::string_view::npos) {
        return core::base::Error{
            452, core::base::ErrorDomain::IO, core::base::Severity::Error,
            "Malformed <fmiModelDescription> tag", ""
        };
    }

    FmiModelDescription desc;
    const std::string_view root_tag = xml_content.substr(root_pos, root_end - root_pos + 1);
    desc.fmi_version = extract_xml_attr(root_tag, "fmiVersion");
    desc.model_name = extract_xml_attr(root_tag, "modelName");
    desc.guid = extract_xml_attr(root_tag, "guid");

    if (desc.model_name.empty() || desc.guid.empty()) {
        return core::base::Error{
            453, core::base::ErrorDomain::Validation, core::base::Severity::Error,
            "FMI modelDescription missing required modelName or guid", ""
        };
    }

    const size_t cs_pos = xml_content.find("<CoSimulation");
    if (cs_pos != std::string_view::npos) {
        const size_t cs_end = xml_content.find('>', cs_pos);
        if (cs_end != std::string_view::npos) {
            desc.model_identifier = extract_xml_attr(
                xml_content.substr(cs_pos, cs_end - cs_pos + 1), "modelIdentifier");
            desc.supports_co_simulation = true;
        }
    }
    if (desc.model_identifier.empty()) {
        desc.model_identifier = desc.model_name;
    }

    const size_t exp_pos = xml_content.find("<DefaultExperiment");
    if (exp_pos != std::string_view::npos) {
        const size_t exp_end = xml_content.find('>', exp_pos);
        if (exp_end != std::string_view::npos) {
            const auto tag = xml_content.substr(exp_pos, exp_end - exp_pos + 1);
            const auto s_start = extract_xml_attr(tag, "startTime");
            const auto s_stop = extract_xml_attr(tag, "stopTime");
            const auto s_step = extract_xml_attr(tag, "stepSize");
            if (!s_start.empty()) desc.default_start_time = std::stod(s_start);
            if (!s_stop.empty()) desc.default_stop_time = std::stod(s_stop);
            if (!s_step.empty()) desc.default_step_size = std::stod(s_step);
        }
    }

    size_t search_pos = 0;
    while ((search_pos = xml_content.find("<ScalarVariable", search_pos)) != std::string_view::npos) {
        const size_t sv_tag_end = xml_content.find('>', search_pos);
        if (sv_tag_end == std::string_view::npos) break;

        const auto sv_tag = xml_content.substr(search_pos, sv_tag_end - search_pos + 1);
        FmiScalarVariable sv;
        sv.name = extract_xml_attr(sv_tag, "name");
        const auto vr_str = extract_xml_attr(sv_tag, "valueReference");
        if (!vr_str.empty()) {
            sv.value_reference = static_cast<uint32_t>(std::stoul(vr_str));
        }
        sv.causality = parse_causality(extract_xml_attr(sv_tag, "causality"));
        sv.variability = parse_variability(extract_xml_attr(sv_tag, "variability"));
        sv.description = extract_xml_attr(sv_tag, "description");

        const size_t sv_close = xml_content.find("</ScalarVariable>", sv_tag_end);
        const size_t next_sv = xml_content.find("<ScalarVariable", sv_tag_end + 1);
        const size_t block_end = (sv_close != std::string_view::npos &&
                                  (next_sv == std::string_view::npos || sv_close < next_sv))
                                     ? sv_close
                                     : sv_tag_end;
        if (block_end > sv_tag_end) {
            const auto inner = xml_content.substr(sv_tag_end, block_end - sv_tag_end);
            const size_t real_pos = inner.find("<Real");
            if (real_pos != std::string_view::npos) {
                const size_t real_end = inner.find('>', real_pos);
                const auto real_tag = inner.substr(real_pos, real_end - real_pos + 1);
                const auto start_s = extract_xml_attr(real_tag, "start");
                if (!start_s.empty()) {
                    sv.start_value = std::stod(start_s);
                    sv.has_start = true;
                }
                sv.unit = extract_xml_attr(real_tag, "unit");
            }
        }
        if (!sv.name.empty()) {
            desc.variables.push_back(std::move(sv));
        }
        search_pos = sv_tag_end + 1;
    }

    return desc;
}

core::base::Result<void> FmuImporter::load_with_api(
    const FmiModelDescription& desc,
    Fmi2CoSimulationApi api,
    std::string_view instance_name) {
    unload();
    if (!api.instantiate || !api.do_step || !api.get_real || !api.set_real) {
        return core::base::Error{
            454, core::base::ErrorDomain::Validation, core::base::Severity::Error,
            "Incomplete FMI 2.0 CoSimulation API callback table", ""
        };
    }

    desc_ = desc;
    api_ = api;
    var_index_.clear();
    for (size_t i = 0; i < desc_.variables.size(); ++i) {
        var_index_[desc_.variables[i].name] = i;
    }

    const std::string inst(instance_name);
    component_ = api_.instantiate(inst.c_str(), desc_.guid.c_str(), "");
    if (!component_) {
        return core::base::Error{
            455, core::base::ErrorDomain::Simulation, core::base::Severity::Error,
            "FMU instantiation failed", desc_.model_name
        };
    }
    return core::base::Result<void>::ok();
}

core::base::Result<void> FmuImporter::load_shared_library(
    const FmiModelDescription& desc,
    const std::string& shared_lib_path,
    std::string_view instance_name) {
    unload();
#ifdef _WIN32
    HMODULE mod = ::LoadLibraryA(shared_lib_path.c_str());
    void* lib = static_cast<void*>(mod);
#else
    void* lib = ::dlopen(shared_lib_path.c_str(), RTLD_LAZY);
#endif
    if (!lib) {
        return core::base::Error{
            456, core::base::ErrorDomain::IO, core::base::Severity::Error,
            "Failed to load compiled FMU shared library", shared_lib_path
        };
    }
    lib_handle_ = lib;

    Fmi2CoSimulationApi api;
    api.instantiate = load_symbol<decltype(api.instantiate)>(lib, "fmi2Instantiate");
    api.setup_experiment = load_symbol<decltype(api.setup_experiment)>(lib, "fmi2SetupExperiment");
    api.enter_initialization = load_symbol<decltype(api.enter_initialization)>(lib, "fmi2EnterInitializationMode");
    api.exit_initialization = load_symbol<decltype(api.exit_initialization)>(lib, "fmi2ExitInitializationMode");
    api.set_real = load_symbol<decltype(api.set_real)>(lib, "fmi2SetReal");
    api.get_real = load_symbol<decltype(api.get_real)>(lib, "fmi2GetReal");
    api.do_step = load_symbol<decltype(api.do_step)>(lib, "fmi2DoStep");
    api.terminate = load_symbol<decltype(api.terminate)>(lib, "fmi2Terminate");
    api.free_instance = load_symbol<decltype(api.free_instance)>(lib, "fmi2FreeInstance");

    return load_with_api(desc, api, instance_name);
}

core::base::Result<void> FmuImporter::initialize(double start_time_s, double stop_time_s) {
    if (!component_) {
        return core::base::Error{
            457, core::base::ErrorDomain::Simulation, core::base::Severity::Error,
            "No FMU component loaded", ""
        };
    }
    if (api_.setup_experiment) {
        const int st = api_.setup_experiment(component_, 0, 0.0, start_time_s, 1, stop_time_s);
        if (st >= static_cast<int>(FmiStatus::Error)) {
            return core::base::Error{458, core::base::ErrorDomain::Simulation, core::base::Severity::Error, "fmi2SetupExperiment failed", ""};
        }
    }
    if (api_.enter_initialization && api_.enter_initialization(component_) >= static_cast<int>(FmiStatus::Error)) {
        return core::base::Error{459, core::base::ErrorDomain::Simulation, core::base::Severity::Error, "fmi2EnterInitializationMode failed", ""};
    }
    for (const auto& v : desc_.variables) {
        if (v.has_start &&
            (v.causality == FmiCausality::Parameter || v.causality == FmiCausality::Input)) {
            const Fmi2ValueReference vr = v.value_reference;
            api_.set_real(component_, &vr, 1, &v.start_value);
        }
    }
    if (api_.exit_initialization && api_.exit_initialization(component_) >= static_cast<int>(FmiStatus::Error)) {
        return core::base::Error{460, core::base::ErrorDomain::Simulation, core::base::Severity::Error, "fmi2ExitInitializationMode failed", ""};
    }
    return core::base::Result<void>::ok();
}

const FmiScalarVariable* FmuImporter::find_variable(std::string_view name) const noexcept {
    const auto it = var_index_.find(std::string(name));
    if (it == var_index_.end()) return nullptr;
    return &desc_.variables[it->second];
}

core::base::Result<void> FmuImporter::set_real(std::string_view var_name, double value) {
    if (!component_) {
        return core::base::Error{461, core::base::ErrorDomain::Simulation, core::base::Severity::Error, "No FMU loaded", ""};
    }
    const auto* sv = find_variable(var_name);
    if (!sv) {
        return core::base::Error{462, core::base::ErrorDomain::Validation, core::base::Severity::Error, "Unknown FMU variable", std::string(var_name)};
    }
    const Fmi2ValueReference vr = sv->value_reference;
    if (api_.set_real(component_, &vr, 1, &value) >= static_cast<int>(FmiStatus::Error)) {
        return core::base::Error{463, core::base::ErrorDomain::Simulation, core::base::Severity::Error, "fmi2SetReal failed", std::string(var_name)};
    }
    return core::base::Result<void>::ok();
}

core::base::Result<double> FmuImporter::get_real(std::string_view var_name) const {
    if (!component_) {
        return core::base::Error{464, core::base::ErrorDomain::Simulation, core::base::Severity::Error, "No FMU loaded", ""};
    }
    const auto* sv = find_variable(var_name);
    if (!sv) {
        return core::base::Error{465, core::base::ErrorDomain::Validation, core::base::Severity::Error, "Unknown FMU variable", std::string(var_name)};
    }
    const Fmi2ValueReference vr = sv->value_reference;
    double val = 0.0;
    if (api_.get_real(component_, &vr, 1, &val) >= static_cast<int>(FmiStatus::Error)) {
        return core::base::Error{466, core::base::ErrorDomain::Simulation, core::base::Severity::Error, "fmi2GetReal failed", std::string(var_name)};
    }
    return val;
}

core::base::Result<void> FmuImporter::do_step(double current_time_s, double dt_s) {
    if (!component_ || dt_s <= 0.0) {
        return core::base::Error{467, core::base::ErrorDomain::Validation, core::base::Severity::Error, "Invalid FMU state or step size", ""};
    }
    if (api_.do_step(component_, current_time_s, dt_s, 1) >= static_cast<int>(FmiStatus::Error)) {
        return core::base::Error{468, core::base::ErrorDomain::Simulation, core::base::Severity::Error, "fmi2DoStep failed", ""};
    }
    return core::base::Result<void>::ok();
}

void FmuImporter::unload() {
    if (component_) {
        if (api_.terminate) api_.terminate(component_);
        if (api_.free_instance) api_.free_instance(component_);
        component_ = nullptr;
    }
    if (lib_handle_) {
#ifdef _WIN32
        ::FreeLibrary(static_cast<HMODULE>(lib_handle_));
#else
        ::dlclose(lib_handle_);
#endif
        lib_handle_ = nullptr;
    }
    api_ = {};
}

} // namespace shunya::integ
