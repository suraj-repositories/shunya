#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/integ/fmi_wrapper.hpp"

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace shunya::integ {

/**
 * @brief FMI 2.0/3.0 Co-Simulation FMU Importer & modelDescription.xml parser with dynamic library & native FMI C-API table binding.
 */
class FmuImporter {
public:
    FmuImporter() = default;
    ~FmuImporter();

    FmuImporter(const FmuImporter&) = delete;
    FmuImporter& operator=(const FmuImporter&) = delete;

    [[nodiscard]] static core::base::Result<FmiModelDescription> parse_model_description_xml(
        std::string_view xml_content);

    [[nodiscard]] core::base::Result<void> load_with_api(
        const FmiModelDescription& desc,
        Fmi2CoSimulationApi api,
        std::string_view instance_name = "shunya_fmu_instance");

    [[nodiscard]] core::base::Result<void> load_shared_library(
        const FmiModelDescription& desc,
        const std::string& shared_lib_path,
        std::string_view instance_name = "shunya_fmu_instance");

    [[nodiscard]] core::base::Result<void> initialize(double start_time_s = 0.0, double stop_time_s = 10.0);
    [[nodiscard]] core::base::Result<void> set_real(std::string_view var_name, double value);
    [[nodiscard]] core::base::Result<double> get_real(std::string_view var_name) const;
    [[nodiscard]] core::base::Result<void> do_step(double current_time_s, double dt_s);
    void unload();

    [[nodiscard]] bool is_loaded() const noexcept { return component_ != nullptr; }
    [[nodiscard]] const FmiModelDescription& description() const noexcept { return desc_; }

private:
    const FmiScalarVariable* find_variable(std::string_view name) const noexcept;

    FmiModelDescription desc_;
    Fmi2CoSimulationApi api_{};
    Fmi2Component component_{nullptr};
    void* lib_handle_{nullptr};
    std::unordered_map<std::string, size_t> var_index_;
};

} // namespace shunya::integ
