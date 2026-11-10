#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/integ/fmi_wrapper.hpp"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace shunya::integ {

struct FmuArchiveEntry {
    std::string relative_path;
    std::vector<uint8_t> content;
};

/**
 * @brief FMI 2.0 Co-Simulation FMU Exporter generating modelDescription.xml, C-API wrapper source, and PKZIP .fmu archives.
 */
class FmuExporter {
public:
    [[nodiscard]] static core::base::Result<std::string> generate_model_description_xml(
        const FmiModelDescription& desc);

    [[nodiscard]] static std::string generate_c_wrapper_source(
        const FmiModelDescription& desc);

    [[nodiscard]] static core::base::Result<std::vector<uint8_t>> build_stored_zip_archive(
        const std::vector<FmuArchiveEntry>& entries);

    [[nodiscard]] static core::base::Result<std::vector<uint8_t>> export_fmu_bytes(
        const FmiModelDescription& desc);

    /**
     * @brief Extracts a named entry from an uncompressed stored PKZIP (.fmu) archive buffer.
     */
    [[nodiscard]] static core::base::Result<std::string> extract_entry_text_from_zip(
        const std::vector<uint8_t>& zip_bytes,
        const std::string& relative_path);
};

} // namespace shunya::integ
