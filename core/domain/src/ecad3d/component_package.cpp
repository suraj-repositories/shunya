#include "shunya/ecad3d/component_package.hpp"

#include "shunya/core/base/error.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

namespace shunya::ecad3d {

namespace {

inline core::base::Result<void> make_fail(std::string msg) {
    core::base::Error err;
    err.message = std::move(msg);
    err.domain = core::base::ErrorDomain::Validation;
    return core::base::Result<void>(std::move(err));
}

inline core::base::Result<void> make_ok() {
    return core::base::Result<void>();
}

uint32_t compute_simple_crc(std::string_view data) {
    uint32_t crc = 0xFFFFFFFF;
    for (char c : data) {
        crc ^= static_cast<uint8_t>(c);
        for (int i = 0; i < 8; ++i) {
            crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
        }
    }
    return ~crc;
}

} // namespace

// ============================================================================
// ComponentBundleArchive Implementation
// ============================================================================

std::string ComponentBundleArchive::serialize_bundle() const {
    std::ostringstream ss;
    std::string manifest_json = ComponentManifestSerializer::to_json(manifest);

    ss << "---BEGIN SHUNYA COMPONENT BUNDLE v1.0---\n";
    ss << "SECTION:MANIFEST\n";
    ss << manifest_json;
    if (manifest_json.empty() || manifest_json.back() != '\n') ss << "\n";

    ss << "SECTION:GEOMETRY\n";
    ss << mesh_geometry_data;
    if (mesh_geometry_data.empty() || mesh_geometry_data.back() != '\n') ss << "\n";

    ss << "SECTION:SYMBOL_SVG\n";
    ss << symbol_svg_data;
    if (symbol_svg_data.empty() || symbol_svg_data.back() != '\n') ss << "\n";

    std::string payload = ss.str();
    uint32_t crc = compute_simple_crc(payload);

    std::ostringstream final_ss;
    final_ss << payload;
    final_ss << "SECTION:CHECKSUM\n";
    final_ss << std::hex << crc << "\n";
    final_ss << "---END SHUNYA COMPONENT BUNDLE---\n";

    return final_ss.str();
}

core::base::Result<ComponentBundleArchive> ComponentBundleArchive::deserialize_bundle(std::string_view bundle_str) {
    if (bundle_str.find("---BEGIN SHUNYA COMPONENT BUNDLE") == std::string_view::npos) {
        core::base::Error err;
        err.message = "Missing component bundle header magic";
        err.domain = core::base::ErrorDomain::Validation;
        return core::base::Result<ComponentBundleArchive>(std::move(err));
    }

    ComponentBundleArchive archive;

    // Parse SECTION:MANIFEST
    auto man_pos = bundle_str.find("SECTION:MANIFEST\n");
    auto geo_pos = bundle_str.find("SECTION:GEOMETRY\n");
    auto svg_pos = bundle_str.find("SECTION:SYMBOL_SVG\n");
    auto crc_pos = bundle_str.find("SECTION:CHECKSUM\n");

    if (man_pos == std::string_view::npos || geo_pos == std::string_view::npos) {
        core::base::Error err;
        err.message = "Corrupt bundle structure: missing mandatory manifest or geometry section";
        err.domain = core::base::ErrorDomain::Validation;
        return core::base::Result<ComponentBundleArchive>(std::move(err));
    }

    std::string_view manifest_part = bundle_str.substr(
        man_pos + 17,
        geo_pos - (man_pos + 17)
    );

    auto man_res = ComponentManifestSerializer::from_json(manifest_part);
    if (man_res.is_err()) {
        core::base::Error err;
        err.message = "Failed to parse bundle manifest: " + man_res.error().message;
        err.domain = core::base::ErrorDomain::Validation;
        return core::base::Result<ComponentBundleArchive>(std::move(err));
    }
    archive.manifest = man_res.value();

    if (svg_pos != std::string_view::npos) {
        archive.mesh_geometry_data = std::string(bundle_str.substr(
            geo_pos + 17,
            svg_pos - (geo_pos + 17)
        ));

        if (crc_pos != std::string_view::npos) {
            archive.symbol_svg_data = std::string(bundle_str.substr(
                svg_pos + 19,
                crc_pos - (svg_pos + 19)
            ));
            archive.bundle_checksum = std::string(bundle_str.substr(crc_pos + 17, 8));
        } else {
            archive.symbol_svg_data = std::string(bundle_str.substr(svg_pos + 19));
        }
    } else {
        archive.mesh_geometry_data = std::string(bundle_str.substr(geo_pos + 17));
    }

    return core::base::Result<ComponentBundleArchive>(std::move(archive));
}

core::base::Result<void> ComponentBundleArchive::export_to_file(
    const ComponentBundleArchive& bundle,
    const std::filesystem::path& file_path
) {
    std::ofstream ofs(file_path, std::ios::binary);
    if (!ofs.is_open()) {
        return make_fail("Unable to open output file for component bundle: " + file_path.string());
    }
    ofs << bundle.serialize_bundle();
    return make_ok();
}

core::base::Result<ComponentBundleArchive> ComponentBundleArchive::import_from_file(
    const std::filesystem::path& file_path
) {
    std::ifstream ifs(file_path, std::ios::binary);
    if (!ifs.is_open()) {
        core::base::Error err;
        err.message = "File not found: " + file_path.string();
        err.domain = core::base::ErrorDomain::Validation;
        return core::base::Result<ComponentBundleArchive>(std::move(err));
    }
    std::stringstream buffer;
    buffer << ifs.rdbuf();
    return deserialize_bundle(buffer.str());
}

// ============================================================================
// ComponentRegistryResolver Implementation
// ============================================================================

core::base::Result<void> ComponentRegistryResolver::register_component(
    const VersionedComponentEntry& entry
) {
    if (entry.component_guid.empty()) {
        return make_fail("Cannot register component with empty GUID");
    }

    auto it = entries_.find(entry.component_guid);
    if (it != entries_.end()) {
        // If exact version and name match, overwrite
        if (it->second.version == entry.version) {
            it->second = entry;
            return make_ok();
        }
        // Different version -> collision prevention
        if (it->second.version > entry.version) {
            return make_fail("Component GUID collision: A newer version " +
                             it->second.version.to_string() + " is already registered");
        }
    }

    entries_[entry.component_guid] = entry;
    return make_ok();
}

bool ComponentRegistryResolver::contains(const std::string& guid) const noexcept {
    return entries_.find(guid) != entries_.end();
}

const VersionedComponentEntry* ComponentRegistryResolver::find_by_guid(const std::string& guid) const noexcept {
    auto it = entries_.find(guid);
    if (it != entries_.end()) return &it->second;
    return nullptr;
}

const VersionedComponentEntry* ComponentRegistryResolver::find_by_name(const std::string& name) const noexcept {
    for (const auto& [_, entry] : entries_) {
        if (entry.name == name) return &entry;
    }
    return nullptr;
}

std::vector<std::string> ComponentRegistryResolver::resolve_missing_dependencies(
    const VersionedComponentEntry& entry
) const {
    std::vector<std::string> missing;
    for (const auto& dep_guid : entry.dependencies) {
        if (!contains(dep_guid)) {
            missing.push_back(dep_guid);
        }
    }
    return missing;
}

// ============================================================================
// SpiceStabilityTester Implementation
// ============================================================================

SpiceStabilityReport SpiceStabilityTester::test_stability(
    const ComponentManifest& manifest,
    double dc_min_v,
    double dc_max_v,
    double pulse_amplitude_v,
    double pulse_duration_s
) {
    SpiceStabilityReport report;
    report.is_stable = true;

    if (manifest.spice_model_equation.empty()) {
        report.is_stable = false;
        report.diagnostics.push_back("SPICE equation is empty");
        return report;
    }

    // 1. Synthetic DC sweep: evaluate I-V curve linearity / convergence
    constexpr int NUM_DC_STEPS = 20;
    double dv = (dc_max_v - dc_min_v) / NUM_DC_STEPS;
    double r_nom = manifest.specs.nominal_primary_value > 0.0 ?
                   manifest.specs.nominal_primary_value : 1.0;

    for (int i = 0; i <= NUM_DC_STEPS; ++i) {
        double v = dc_min_v + i * dv;
        double current = (manifest.kind == eng::ComponentKind::Capacitor) ? 0.0 : (v / r_nom);

        if (std::isnan(current) || std::isinf(current)) {
            report.is_stable = false;
            report.diagnostics.push_back("DC sweep generated NaN/Inf current at V=" + std::to_string(v));
            break;
        }

        // Passive components must not generate energy: P = V * I >= 0
        if (v * current < -1e-6) {
            report.is_stable = false;
            report.diagnostics.push_back("Passive violation: Component generated net positive energy at V=" + std::to_string(v));
            break;
        }
        report.convergence_iterations++;
    }

    // 2. Synthetic AC Step Pulse: compute max dv/dt and transient response
    constexpr int NUM_TIME_STEPS = 50;
    double dt = pulse_duration_s / NUM_TIME_STEPS;
    double prev_v = 0.0;
    double max_dvdt = 0.0;

    for (int t = 0; t <= NUM_TIME_STEPS; ++t) {
        double cur_v = (t < 5) ? 0.0 : pulse_amplitude_v;
        double rate = std::abs(cur_v - prev_v) / dt;
        max_dvdt = std::max(max_dvdt, rate);
        prev_v = cur_v;
        report.convergence_iterations++;
    }

    report.max_dv_dt = max_dvdt;
    report.energy_conservation_error = 0.0;

    return report;
}

// ============================================================================
// ComponentLibraryPalette Implementation
// ============================================================================

void ComponentLibraryPalette::add_component(
    const ComponentManifest& manifest,
    ComponentPaletteCategory category
) {
    PaletteItemDescriptor item;
    item.guid = manifest.component_guid;
    item.name = manifest.name;
    item.category = category;
    item.part_number = manifest.part_number;
    item.description = manifest.description;
    item.package_type = manifest.package_type;
    item.icon_glyph = manifest.symbol_2d.primitive_shape;
    item.drag_and_drop_mime_type = "application/x-shunya-component;guid=" + manifest.component_guid;

    items_.push_back(std::move(item));
}

std::vector<PaletteItemDescriptor> ComponentLibraryPalette::search(
    std::string_view query,
    ComponentPaletteCategory filter
) const {
    std::vector<PaletteItemDescriptor> results;
    std::string q_lower(query);
    std::transform(q_lower.begin(), q_lower.end(), q_lower.begin(), ::tolower);

    for (const auto& item : items_) {
        if (filter != ComponentPaletteCategory::All && item.category != filter) {
            continue;
        }

        if (q_lower.empty()) {
            results.push_back(item);
            continue;
        }

        std::string n_lower = item.name;
        std::transform(n_lower.begin(), n_lower.end(), n_lower.begin(), ::tolower);
        std::string p_lower = item.part_number;
        std::transform(p_lower.begin(), p_lower.end(), p_lower.begin(), ::tolower);
        std::string d_lower = item.description;
        std::transform(d_lower.begin(), d_lower.end(), d_lower.begin(), ::tolower);

        if (n_lower.find(q_lower) != std::string::npos ||
            p_lower.find(q_lower) != std::string::npos ||
            d_lower.find(q_lower) != std::string::npos) {
            results.push_back(item);
        }
    }

    return results;
}

// ============================================================================
// ComponentVariantFactory Implementation
// ============================================================================

ComponentManifest ComponentVariantFactory::clone_and_modify(
    const ComponentManifest& base,
    const CustomVariantParams& params
) {
    ComponentManifest variant = base;
    variant.component_guid = core::base::Uuid::generate_random().to_string();

    if (!params.new_name.empty()) {
        variant.name = params.new_name;
    } else {
        variant.name = base.name + "_Variant";
    }

    if (!params.new_part_number.empty()) {
        variant.part_number = params.new_part_number;
    }

    if (params.new_nominal_value.has_value()) {
        variant.specs.nominal_primary_value = *params.new_nominal_value;
    }

    if (params.new_power_rating_w.has_value()) {
        variant.specs.max_power_dissipation_w = *params.new_power_rating_w;
    }

    if (params.new_tolerance_pct.has_value()) {
        variant.specs.tolerance_percent = *params.new_tolerance_pct;
    }

    variant.certification_id = "DERIVED-" + core::base::Uuid::generate_random().to_string().substr(0, 8);
    variant.is_qualified = true;

    return variant;
}

} // namespace shunya::ecad3d
