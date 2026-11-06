#pragma once

#include "shunya/core/base/version.hpp"
#include "shunya/core/base/result.hpp"
#include "shunya/core/base/error.hpp"

#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <functional>

namespace shunya::core::domain {

class IMigrationStep {
public:
    virtual ~IMigrationStep() = default;

    [[nodiscard]] virtual base::Version source_version() const noexcept = 0;
    [[nodiscard]] virtual base::Version target_version() const noexcept = 0;
    [[nodiscard]] virtual std::string_view description() const noexcept = 0;

    [[nodiscard]] virtual base::Result<std::string> migrate(std::string_view source_json) = 0;
};

struct MigrationPlan {
    base::Version from_version;
    base::Version to_version;
    std::vector<std::string> step_descriptions;
    bool is_valid = false;
};

class ProjectMigrator {
public:
    ProjectMigrator();

    void register_step(std::shared_ptr<IMigrationStep> step);

    [[nodiscard]] bool can_migrate(
        const base::Version& from,
        const base::Version& to
    ) const;

    [[nodiscard]] MigrationPlan create_plan(
        const base::Version& from,
        const base::Version& to
    ) const;

    [[nodiscard]] base::Result<std::string> preview_migration(
        std::string_view json_content,
        const base::Version& target_version
    ) const;

    [[nodiscard]] base::Result<void> migrate_file_with_backup(
        const std::string& file_path,
        const base::Version& target_version,
        bool keep_backup_on_success = true
    );

    [[nodiscard]] base::Result<void> rollback_from_backup(
        const std::string& file_path
    );

    [[nodiscard]] static base::Result<base::Version> extract_schema_version(
        std::string_view json_str
    );

private:
    std::vector<std::shared_ptr<IMigrationStep>> steps_;
};

// Standard migration step: v0.0.1 (legacy format) to v0.1.0 (current format)
class MigrationStep_0_0_1_to_0_1_0 : public IMigrationStep {
public:
    [[nodiscard]] base::Version source_version() const noexcept override {
        return {0, 0, 1, "alpha"};
    }
    [[nodiscard]] base::Version target_version() const noexcept override {
        return {0, 1, 0, "dev"};
    }
    [[nodiscard]] std::string_view description() const noexcept override {
        return "Upgrade legacy v0.0.1 schema to v0.1.0: normalize UUIDs, inject target_fidelity and schema_version";
    }

    [[nodiscard]] base::Result<std::string> migrate(std::string_view source_json) override;
};

} // namespace shunya::core::domain
