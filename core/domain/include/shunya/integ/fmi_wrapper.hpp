#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace shunya::integ {

enum class FmiStatus : int {
    OK = 0,
    Warning = 1,
    Discard = 2,
    Error = 3,
    Fatal = 4,
    Pending = 5
};

enum class FmiCausality {
    Parameter,
    CalculatedParameter,
    Input,
    Output,
    Local,
    Independent
};

enum class FmiVariability {
    Constant,
    Fixed,
    Tunable,
    Discrete,
    Continuous
};

struct FmiScalarVariable {
    std::string name;
    uint32_t value_reference{0};
    FmiCausality causality{FmiCausality::Local};
    FmiVariability variability{FmiVariability::Continuous};
    double start_value{0.0};
    bool has_start{false};
    std::string unit;
    std::string description;
};

struct FmiModelDescription {
    std::string fmi_version{"2.0"};
    std::string model_name;
    std::string guid;
    std::string model_identifier;
    bool supports_co_simulation{true};
    double default_start_time{0.0};
    double default_stop_time{10.0};
    double default_step_size{0.01};
    std::vector<FmiScalarVariable> variables;
};

using Fmi2Component = void*;
using Fmi2ValueReference = uint32_t;

struct Fmi2CoSimulationApi {
    Fmi2Component (*instantiate)(const char* instance_name, const char* guid, const char* resource_location) = nullptr;
    int (*setup_experiment)(Fmi2Component c, int tolerance_defined, double tolerance, double start_time, int stop_defined, double stop_time) = nullptr;
    int (*enter_initialization)(Fmi2Component c) = nullptr;
    int (*exit_initialization)(Fmi2Component c) = nullptr;
    int (*set_real)(Fmi2Component c, const Fmi2ValueReference* vr, size_t nvr, const double* value) = nullptr;
    int (*get_real)(Fmi2Component c, const Fmi2ValueReference* vr, size_t nvr, double* value) = nullptr;
    int (*do_step)(Fmi2Component c, double current_time, double step_size, int no_set_state) = nullptr;
    int (*terminate)(Fmi2Component c) = nullptr;
    void (*free_instance)(Fmi2Component c) = nullptr;
};

} // namespace shunya::integ
