#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/mat/property_models/material_property_bundle.hpp"

#include <filesystem>
#include <string>
#include <string_view>

namespace shunya::mat {

struct MaterialProvenanceMetadata {
    std::string creator{"Shunya Engineering Lab"};
    std::string creation_timestamp_iso8601{"2026-11-17T00:00:00Z"};
    std::string standard_reference{"NIST / ISO / ASTM Standards"};
    std::string notes;
    std::string checksum_sha256;
};

/**
 * @brief Portable human-readable material package (.shunyamat) serializer:
 *        Exports and imports comprehensive multi-domain property bundles
 *        with full epistemic provenance and SI constraints.
 * (Roadmap task PH4-03.04.006)
 */
class ShunyaMatSerializer {
public:
    static constexpr std::string_view FORMAT_IDENTIFIER = "shunyamat";
    static constexpr std::string_view FORMAT_VERSION = "4.0.0";

    /**
     * @brief Exports a MaterialPropertyBundle to a human-readable JSON string.
     */
    [[nodiscard]] static std::string export_to_json(
        const MaterialPropertyBundle& bundle,
        const MaterialProvenanceMetadata& provenance = {}) noexcept;

    /**
     * @brief Imports and reconstructs a MaterialPropertyBundle from a JSON string.
     */
    [[nodiscard]] static core::base::Result<MaterialPropertyBundle> import_from_json(
        std::string_view json_str) noexcept;

    /**
     * @brief Exports bundle to a file on disk (usually with extension .shunyamat).
     */
    [[nodiscard]] static core::base::Result<void> export_to_file(
        const MaterialPropertyBundle& bundle,
        const std::filesystem::path& file_path,
        const MaterialProvenanceMetadata& provenance = {}) noexcept;

    /**
     * @brief Imports bundle from a file on disk.
     */
    [[nodiscard]] static core::base::Result<MaterialPropertyBundle> import_from_file(
        const std::filesystem::path& file_path) noexcept;
};

} // namespace shunya::mat
