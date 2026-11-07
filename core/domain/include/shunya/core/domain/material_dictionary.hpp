#pragma once

#include <shunya/core/base/error.hpp>
#include <shunya/core/base/result.hpp>
#include <shunya/core/base/units.hpp>
#include <shunya/core/base/version.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace shunya::core::domain {

// MATDB-01: Field Data Types
enum class FieldDataType : uint8_t {
    Numeric,
    Text,
    Boolean,
    Quantity,
    Vector,
    Tensor,
    Curve,
    Composite
};

[[nodiscard]] constexpr std::string_view field_data_type_to_string(FieldDataType t) noexcept {
    switch (t) {
    case FieldDataType::Numeric:   return "Numeric";
    case FieldDataType::Text:      return "Text";
    case FieldDataType::Boolean:   return "Boolean";
    case FieldDataType::Quantity:  return "Quantity";
    case FieldDataType::Vector:    return "Vector";
    case FieldDataType::Tensor:    return "Tensor";
    case FieldDataType::Curve:     return "Curve";
    case FieldDataType::Composite: return "Composite";
    }
    return "Numeric";
}

[[nodiscard]] base::Result<FieldDataType> field_data_type_from_string(std::string_view s);

// MATDB-01: Field Applicability
enum class FieldApplicability : uint8_t {
    Universal,
    SolidOnly,
    FluidOnly,
    GasOnly,
    PlasmaOnly,
    Semiconductor,
    Conductor,
    Nuclear
};

[[nodiscard]] constexpr std::string_view field_applicability_to_string(FieldApplicability a) noexcept {
    switch (a) {
    case FieldApplicability::Universal:     return "Universal";
    case FieldApplicability::SolidOnly:     return "SolidOnly";
    case FieldApplicability::FluidOnly:     return "FluidOnly";
    case FieldApplicability::GasOnly:       return "GasOnly";
    case FieldApplicability::PlasmaOnly:    return "PlasmaOnly";
    case FieldApplicability::Semiconductor: return "Semiconductor";
    case FieldApplicability::Conductor:     return "Conductor";
    case FieldApplicability::Nuclear:       return "Nuclear";
    }
    return "Universal";
}

[[nodiscard]] base::Result<FieldApplicability> field_applicability_from_string(std::string_view s);

// MATDB-03: Provenance & Licences
enum class LicenceType : uint8_t {
    PublicDomain,
    CC0,
    OpenDataCommons_ODbL,
    CC_BY_4_0,
    ProprietaryCommercial,
    AcademicOnly,
    Unknown
};

[[nodiscard]] constexpr std::string_view licence_type_to_string(LicenceType l) noexcept {
    switch (l) {
    case LicenceType::PublicDomain:          return "PublicDomain";
    case LicenceType::CC0:                   return "CC0";
    case LicenceType::OpenDataCommons_ODbL:  return "OpenDataCommons_ODbL";
    case LicenceType::CC_BY_4_0:             return "CC_BY_4_0";
    case LicenceType::ProprietaryCommercial: return "ProprietaryCommercial";
    case LicenceType::AcademicOnly:          return "AcademicOnly";
    case LicenceType::Unknown:               return "Unknown";
    }
    return "Unknown";
}

[[nodiscard]] base::Result<LicenceType> licence_type_from_string(std::string_view s);

struct FieldProvenance {
    std::string source_name;
    std::string provider_id;
    std::string citation_or_doi;
    int year{2026};
    LicenceType licence{LicenceType::PublicDomain};
    bool can_redistribute{true};
    double uncertainty{0.0};
    double confidence_score{1.0};

    bool operator==(const FieldProvenance& o) const noexcept {
        return source_name == o.source_name &&
               provider_id == o.provider_id &&
               citation_or_doi == o.citation_or_doi &&
               year == o.year &&
               licence == o.licence &&
               can_redistribute == o.can_redistribute &&
               uncertainty == o.uncertainty &&
               confidence_score == o.confidence_score;
    }
};

// MATDB-04: Audited Value preserving raw inputs and normalized units
struct AuditedValue {
    std::string field_name;
    std::string raw_value;
    std::string raw_unit;
    double normalized_value{0.0};
    std::string canonical_unit;
    double conversion_factor{1.0};
    FieldProvenance provenance;
    std::string audit_timestamp{"2026-11-07T00:00:00Z"};

    bool operator==(const AuditedValue& o) const noexcept {
        return field_name == o.field_name &&
               raw_value == o.raw_value &&
               raw_unit == o.raw_unit &&
               normalized_value == o.normalized_value &&
               canonical_unit == o.canonical_unit &&
               conversion_factor == o.conversion_factor &&
               provenance == o.provenance &&
               audit_timestamp == o.audit_timestamp;
    }
};

// MATDB-01: Field Definition Schema
struct FieldDefinition {
    std::string canonical_name;
    std::vector<std::string> aliases;
    FieldDataType data_type{FieldDataType::Numeric};
    std::string canonical_unit;
    base::units::Dimension dimension{base::units::Dimensionless};
    FieldApplicability applicability{FieldApplicability::Universal};
    std::string description;
    bool is_required{false};
    std::string default_value;
    std::optional<double> min_value;
    std::optional<double> max_value;

    [[nodiscard]] bool matches(std::string_view name_or_alias) const noexcept;
};

// MATDB-01: Versioned Data Dictionary
class DataDictionary {
public:
    DataDictionary();
    explicit DataDictionary(base::Version version);

    [[nodiscard]] const base::Version& version() const noexcept { return version_; }
    void set_version(base::Version v) noexcept { version_ = v; }

    base::Result<void> register_field(FieldDefinition field);
    [[nodiscard]] bool has_field(std::string_view name_or_alias) const noexcept;
    [[nodiscard]] const FieldDefinition* get_field(std::string_view name_or_alias) const noexcept;
    [[nodiscard]] size_t field_count() const noexcept { return fields_.size(); }
    [[nodiscard]] const std::unordered_map<std::string, FieldDefinition>& fields() const noexcept { return fields_; }

    void seed_default_dictionary();

    [[nodiscard]] base::ValidationReport validate_audited_value(const AuditedValue& val) const;
    [[nodiscard]] base::Result<AuditedValue> audit_and_normalize(std::string_view field_name,
                                                                 double raw_num,
                                                                 std::string_view raw_unit,
                                                                 FieldProvenance prov) const;

    [[nodiscard]] std::string to_json() const;
    static base::Result<DataDictionary> from_json(std::string_view json_str);

private:
    base::Version version_{1, 0, 0};
    std::unordered_map<std::string, FieldDefinition> fields_;
    std::unordered_map<std::string, std::string> alias_to_canonical_;
};

} // namespace shunya::core::domain
