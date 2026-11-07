#pragma once

#include <shunya/core/base/error.hpp>
#include <shunya/core/base/result.hpp>
#include <shunya/core/base/uuid.hpp>
#include <shunya/core/base/version.hpp>
#include <shunya/core/domain/periodic_table.hpp>

#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace shunya::core::domain {

// MATDB-07: Dataset Releases and Immutable Version Snapshots
class DatasetRelease {
public:
    DatasetRelease() = default;
    DatasetRelease(base::Uuid id,
                   base::Version version,
                   std::string release_tag,
                   std::string timestamp,
                   std::string release_notes,
                   std::string snapshot_json);

    [[nodiscard]] const base::Uuid& id() const noexcept { return id_; }
    [[nodiscard]] const base::Version& version() const noexcept { return version_; }
    [[nodiscard]] const std::string& release_tag() const noexcept { return release_tag_; }
    [[nodiscard]] const std::string& timestamp() const noexcept { return timestamp_; }
    [[nodiscard]] const std::string& release_notes() const noexcept { return release_notes_; }
    [[nodiscard]] const std::string& content_hash() const noexcept { return content_hash_; }
    [[nodiscard]] const std::string& snapshot_json() const noexcept { return snapshot_json_; }
    [[nodiscard]] bool is_locked() const noexcept { return is_locked_; }

    void lock() noexcept { is_locked_ = true; }
    [[nodiscard]] bool verify_integrity() const noexcept;

    [[nodiscard]] base::Result<PeriodicTableDatabase> unpack_database() const;

    [[nodiscard]] std::string to_json() const;
    static base::Result<DatasetRelease> from_json(std::string_view json_str);

    static DatasetRelease create(base::Version version,
                                 std::string release_tag,
                                 const PeriodicTableDatabase& db,
                                 std::string release_notes = "");

private:
    base::Uuid id_{base::Uuid::generate_random()};
    base::Version version_{1, 0, 0};
    std::string release_tag_{"REL-1.0.0"};
    std::string timestamp_{"2026-11-07T00:00:00Z"};
    std::string release_notes_;
    std::string snapshot_json_;
    std::string content_hash_;
    bool is_locked_{true};

    static std::string calculate_checksum(std::string_view payload);
};

// MATDB-08: Property Difference in Release Updates
struct PropertyDiff {
    std::string element_symbol;
    std::string property_name;
    std::string old_value;
    std::string new_value;
    std::string old_source;
    std::string new_source;
    std::vector<std::string> impacted_domains;

    bool operator==(const PropertyDiff& o) const noexcept {
        return element_symbol == o.element_symbol &&
               property_name == o.property_name &&
               old_value == o.old_value &&
               new_value == o.new_value &&
               old_source == o.old_source &&
               new_source == o.new_source &&
               impacted_domains == o.impacted_domains;
    }
};

// MATDB-08: Dataset Diff Report
struct DatasetDiffReport {
    base::Version old_version{1, 0, 0};
    base::Version new_version{1, 1, 0};
    std::vector<std::string> added_elements;
    std::vector<std::string> removed_elements;
    std::vector<PropertyDiff> modified_properties;
    std::unordered_set<std::string> impacted_domains;

    [[nodiscard]] bool has_changes() const noexcept {
        return !added_elements.empty() || !removed_elements.empty() || !modified_properties.empty();
    }

    [[nodiscard]] std::string generate_summary() const;
};

[[nodiscard]] DatasetDiffReport compute_dataset_diff(const PeriodicTableDatabase& old_db,
                                                    const PeriodicTableDatabase& new_db,
                                                    base::Version old_ver,
                                                    base::Version new_ver);

// MATDB-09: Missing-Data Reports by Property and Domain Model Requirement
struct DomainRequirement {
    std::string domain_name;
    std::vector<std::string> required_properties;
};

struct MissingDataReport {
    std::string element_symbol;
    std::string element_name;
    double overall_completeness_pct{0.0};
    std::unordered_map<std::string, double> domain_coverage_pct;
    std::unordered_map<std::string, std::vector<std::string>> missing_properties_by_domain;

    [[nodiscard]] bool is_fully_satisfied(std::string_view domain_name) const noexcept;
    [[nodiscard]] std::string generate_report_text() const;
};

[[nodiscard]] std::vector<DomainRequirement> standard_domain_requirements();

[[nodiscard]] MissingDataReport evaluate_missing_data(const PeriodicElement& elem,
                                                     const std::vector<DomainRequirement>& requirements);

} // namespace shunya::core::domain
