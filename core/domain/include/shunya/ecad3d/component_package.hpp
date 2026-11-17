#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/core/base/uuid.hpp"
#include "shunya/ecad3d/component_manifest.hpp"

#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace shunya::ecad3d {

/**
 * @brief Standalone Component Bundle (.shunyacomp archive).
 * (Roadmap task PH4-06.04.002)
 */
struct ComponentBundleArchive {
    ComponentManifest manifest;
    std::string mesh_geometry_data;
    std::string symbol_svg_data;
    std::map<std::string, std::string> custom_material_definitions;
    std::string bundle_checksum;

    [[nodiscard]] std::string serialize_bundle() const;
    [[nodiscard]] static core::base::Result<ComponentBundleArchive> deserialize_bundle(std::string_view bundle_str);
    [[nodiscard]] static core::base::Result<void> export_to_file(const ComponentBundleArchive& bundle, const std::filesystem::path& file_path);
    [[nodiscard]] static core::base::Result<ComponentBundleArchive> import_from_file(const std::filesystem::path& file_path);
};

/**
 * @brief Semantic Versioning structure.
 * (Roadmap task PH4-06.04.003)
 */
struct SemanticVersion {
    int major{1};
    int minor{0};
    int patch{0};

    [[nodiscard]] std::string to_string() const {
        return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
    }
    auto operator<=>(const SemanticVersion&) const = default;
};

/**
 * @brief Registered catalog component entry with GUID and dependencies.
 * (Roadmap task PH4-06.04.003)
 */
struct VersionedComponentEntry {
    std::string component_guid;
    SemanticVersion version;
    std::string name;
    std::vector<std::string> required_materials;
    std::vector<std::string> dependencies;
    ComponentManifest manifest;
};

class ComponentRegistryResolver {
public:
    core::base::Result<void> register_component(const VersionedComponentEntry& entry);
    [[nodiscard]] bool contains(const std::string& guid) const noexcept;
    [[nodiscard]] const VersionedComponentEntry* find_by_guid(const std::string& guid) const noexcept;
    [[nodiscard]] const VersionedComponentEntry* find_by_name(const std::string& name) const noexcept;
    [[nodiscard]] std::vector<std::string> resolve_missing_dependencies(const VersionedComponentEntry& entry) const;
    [[nodiscard]] size_t size() const noexcept { return entries_.size(); }

private:
    std::map<std::string, VersionedComponentEntry> entries_;
};

/**
 * @brief Automated SPICE model numerical stability tester.
 * (Roadmap task PH4-06.04.004)
 */
struct SpiceStabilityReport {
    bool is_stable{false};
    double max_dv_dt{0.0};
    int convergence_iterations{0};
    double energy_conservation_error{0.0};
    std::vector<std::string> diagnostics;
};

class SpiceStabilityTester {
public:
    [[nodiscard]] static SpiceStabilityReport test_stability(
        const ComponentManifest& manifest,
        double dc_min_v = -10.0,
        double dc_max_v = 10.0,
        double pulse_amplitude_v = 5.0,
        double pulse_duration_s = 1.0e-5
    );
};

/**
 * @brief Component Library Palette model with search and category filtering.
 * (Roadmap task PH4-06.04.005)
 */
enum class ComponentPaletteCategory {
    All,
    Passive,
    Semiconductor,
    Optoelectronics,
    Electromechanical,
    Power
};

struct PaletteItemDescriptor {
    std::string guid;
    std::string name;
    ComponentPaletteCategory category{ComponentPaletteCategory::Passive};
    std::string part_number;
    std::string description;
    std::string package_type;
    std::string drag_and_drop_mime_type{"application/x-shunya-component"};
    std::string icon_glyph;
};

class ComponentLibraryPalette {
public:
    void add_component(const ComponentManifest& manifest, ComponentPaletteCategory category);
    [[nodiscard]] std::vector<PaletteItemDescriptor> search(
        std::string_view query,
        ComponentPaletteCategory filter = ComponentPaletteCategory::All
    ) const;
    [[nodiscard]] size_t total_items() const noexcept { return items_.size(); }

private:
    std::vector<PaletteItemDescriptor> items_;
};

/**
 * @brief Component Clone-and-Modify factory for custom variants.
 * (Roadmap task PH4-06.04.006)
 */
class ComponentVariantFactory {
public:
    struct CustomVariantParams {
        std::string new_name;
        std::string new_part_number;
        std::optional<double> new_nominal_value;
        std::optional<double> new_power_rating_w;
        std::optional<double> new_tolerance_pct;
    };

    [[nodiscard]] static ComponentManifest clone_and_modify(
        const ComponentManifest& base,
        const CustomVariantParams& params
    );
};

} // namespace shunya::ecad3d
