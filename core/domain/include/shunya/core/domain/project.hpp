#pragma once

#include "shunya/core/base/uuid.hpp"
#include "shunya/core/base/version.hpp"
#include "shunya/core/base/result.hpp"
#include "shunya/core/base/error.hpp"

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <memory>
#include <optional>

namespace shunya::core::domain {

enum class FidelityTier : uint8_t {
    L0_Visual = 0,
    L1_Functional = 1,
    L2_EngineeringApproximation = 2,
    L3_HighFidelity = 3,
    L4_ResearchCertified = 4
};

[[nodiscard]] constexpr std::string_view fidelity_tier_to_string(FidelityTier tier) noexcept {
    switch (tier) {
        case FidelityTier::L0_Visual: return "L0_Visual";
        case FidelityTier::L1_Functional: return "L1_Functional";
        case FidelityTier::L2_EngineeringApproximation: return "L2_EngineeringApproximation";
        case FidelityTier::L3_HighFidelity: return "L3_HighFidelity";
        case FidelityTier::L4_ResearchCertified: return "L4_ResearchCertified";
    }
    return "L2_EngineeringApproximation";
}

[[nodiscard]] std::optional<FidelityTier> fidelity_tier_from_string(std::string_view s) noexcept;

struct ProjectDependency {
    std::string name;
    std::string version;
    std::string source_uri;
};

struct ProjectManifest {
    base::Uuid id;
    std::string name;
    std::string description;
    std::string author;
    std::string created_at;
    std::string updated_at;
    base::Version schema_version{0, 1, 0, "dev"};
    FidelityTier target_fidelity = FidelityTier::L2_EngineeringApproximation;
    std::vector<ProjectDependency> dependencies;
    std::unordered_map<std::string, std::string> metadata;

    [[nodiscard]] std::string to_json() const;
    [[nodiscard]] static base::Result<ProjectManifest> from_json(std::string_view json_str);
};

class Project {
public:
    explicit Project(ProjectManifest manifest, std::string file_path = "");

    [[nodiscard]] static base::Result<std::shared_ptr<Project>> create_new(std::string name, std::string author = "Unknown");
    [[nodiscard]] static base::Result<std::shared_ptr<Project>> open_from_file(const std::string& file_path);

    [[nodiscard]] base::Result<void> save_to_file(const std::string& file_path = "", bool atomic = true);

    [[nodiscard]] const ProjectManifest& manifest() const noexcept { return manifest_; }
    ProjectManifest& manifest() noexcept { mark_dirty(); return manifest_; }

    [[nodiscard]] bool is_dirty() const noexcept { return is_dirty_; }
    void mark_dirty() noexcept { is_dirty_ = true; }
    void clear_dirty() noexcept { is_dirty_ = false; }

    [[nodiscard]] bool is_read_only() const noexcept { return is_read_only_; }
    void set_read_only(bool ro) noexcept { is_read_only_ = ro; }

    [[nodiscard]] const std::string& file_path() const noexcept { return file_path_; }
    void set_file_path(std::string path) { file_path_ = std::move(path); }

private:
    ProjectManifest manifest_;
    std::string file_path_;
    bool is_dirty_ = false;
    bool is_read_only_ = false;
};

} // namespace shunya::core::domain
