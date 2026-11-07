#pragma once

#include <shunya/core/base/error.hpp>
#include <shunya/core/base/result.hpp>
#include <shunya/core/base/uuid.hpp>
#include <shunya/core/base/version.hpp>
#include <shunya/core/domain/material_curve.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace shunya::core::domain {

// MATRT-02: Isotope Definition and Nuclear Data
struct IsotopeDefinition {
    int atomic_number{1};                     // Z
    int mass_number{1};                       // A
    double atomic_mass_da{1.0};               // Daltons / amu
    double abundance_pct{100.0};              // 0.0 .. 100.0 %
    bool is_stable{true};
    std::optional<double> half_life_seconds;
    std::string decay_mode{"None"};           // "Alpha", "BetaMinus", "EC", "None"
    std::string spin_parity{"1/2+"};
    double thermal_neutron_cross_section_barns{0.0};

    bool operator==(const IsotopeDefinition& o) const noexcept {
        return atomic_number == o.atomic_number &&
               mass_number == o.mass_number &&
               atomic_mass_da == o.atomic_mass_da &&
               abundance_pct == o.abundance_pct &&
               is_stable == o.is_stable &&
               half_life_seconds == o.half_life_seconds &&
               decay_mode == o.decay_mode &&
               spin_parity == o.spin_parity &&
               thermal_neutron_cross_section_barns == o.thermal_neutron_cross_section_barns;
    }
};

// MATRT-01: Immutable Chemical Element Definition
struct ChemicalElementDefinition {
    int atomic_number{1};                     // Z (1 to 118)
    std::string symbol{"H"};
    std::string name{"Hydrogen"};
    int period{1};
    int group{1};
    char block{'s'};
    double standard_atomic_weight{1.008};
    std::string electron_configuration{"1s1"};
    std::vector<IsotopeDefinition> isotopes;

    [[nodiscard]] bool has_isotope(int mass_number) const noexcept;
    [[nodiscard]] const IsotopeDefinition* get_isotope(int mass_number) const noexcept;
};

// MATRT-03: Material Definition for Pure Elements, Compounds, Alloys, and Mixtures
enum class MaterialClassification : uint8_t {
    PureElement,
    ChemicalCompound,
    Alloy,
    Mixture,
    Composite
};

[[nodiscard]] constexpr std::string_view material_classification_to_string(MaterialClassification c) noexcept {
    switch (c) {
    case MaterialClassification::PureElement:      return "PureElement";
    case MaterialClassification::ChemicalCompound: return "ChemicalCompound";
    case MaterialClassification::Alloy:            return "Alloy";
    case MaterialClassification::Mixture:          return "Mixture";
    case MaterialClassification::Composite:        return "Composite";
    }
    return "PureElement";
}

[[nodiscard]] base::Result<MaterialClassification> material_classification_from_string(std::string_view s);

enum class FractionBasis : uint8_t {
    MassFraction,
    MoleFraction,
    VolumeFraction
};

[[nodiscard]] constexpr std::string_view fraction_basis_to_string(FractionBasis b) noexcept {
    switch (b) {
    case FractionBasis::MassFraction:   return "MassFraction";
    case FractionBasis::MoleFraction:   return "MoleFraction";
    case FractionBasis::VolumeFraction: return "VolumeFraction";
    }
    return "MassFraction";
}

[[nodiscard]] base::Result<FractionBasis> fraction_basis_from_string(std::string_view s);

struct MaterialConstituent {
    std::string identifier;                     // Symbol (e.g. "Fe", "Cr") or Material UUID
    FractionBasis basis{FractionBasis::MassFraction};
    double fraction{1.0};                       // In range (0, 1]

    bool operator==(const MaterialConstituent& o) const noexcept {
        return identifier == o.identifier && basis == o.basis && fraction == o.fraction;
    }
};

class MaterialDefinition {
public:
    MaterialDefinition();
    MaterialDefinition(base::Uuid id, std::string name, MaterialClassification classification);

    [[nodiscard]] const base::Uuid& id() const noexcept { return id_; }
    void set_id(const base::Uuid& id) noexcept { id_ = id; }

    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    void set_name(std::string name) { name_ = std::move(name); }

    [[nodiscard]] MaterialClassification classification() const noexcept { return classification_; }
    void set_classification(MaterialClassification c) noexcept { classification_ = c; }

    [[nodiscard]] const std::string& chemical_formula() const noexcept { return chemical_formula_; }
    void set_chemical_formula(std::string formula) { chemical_formula_ = std::move(formula); }

    [[nodiscard]] double molecular_weight() const noexcept { return molecular_weight_; }
    void set_molecular_weight(double mw) noexcept { molecular_weight_ = mw; }

    [[nodiscard]] double reference_density() const noexcept { return reference_density_; }
    void set_reference_density(double rho) noexcept { reference_density_ = rho; }

    void add_constituent(MaterialConstituent constituent);
    [[nodiscard]] const std::vector<MaterialConstituent>& constituents() const noexcept { return constituents_; }
    [[nodiscard]] base::ValidationReport validate_composition() const;

    // Scalar properties & curves
    void set_scalar_property(std::string name, double value);
    [[nodiscard]] std::optional<double> get_scalar_property(std::string_view name) const;
    [[nodiscard]] const std::unordered_map<std::string, double>& scalar_properties() const noexcept { return scalar_properties_; }

    void add_curve(MaterialCurve curve);
    [[nodiscard]] const MaterialCurve* get_curve(std::string_view name) const;
    [[nodiscard]] const std::unordered_map<std::string, MaterialCurve>& curves() const noexcept { return curves_; }

    [[nodiscard]] std::string to_json() const;
    static base::Result<MaterialDefinition> from_json(std::string_view json_str);

private:
    base::Uuid id_{base::Uuid::generate_random()};
    std::string name_{"Unnamed Material"};
    MaterialClassification classification_{MaterialClassification::PureElement};
    std::string chemical_formula_;
    double molecular_weight_{0.0};
    double reference_density_{1000.0};
    std::vector<MaterialConstituent> constituents_;
    std::unordered_map<std::string, double> scalar_properties_;
    std::unordered_map<std::string, MaterialCurve> curves_;
};

// MATRT-05: Material State
enum class MaterialPhase : uint8_t {
    Solid,
    Liquid,
    Gas,
    Supercritical,
    Plasma
};

[[nodiscard]] constexpr std::string_view material_phase_to_string(MaterialPhase p) noexcept {
    switch (p) {
    case MaterialPhase::Solid:         return "Solid";
    case MaterialPhase::Liquid:        return "Liquid";
    case MaterialPhase::Gas:           return "Gas";
    case MaterialPhase::Supercritical: return "Supercritical";
    case MaterialPhase::Plasma:        return "Plasma";
    }
    return "Solid";
}

[[nodiscard]] base::Result<MaterialPhase> material_phase_from_string(std::string_view s);

struct MaterialState {
    double temperature_k{293.15};
    double pressure_pa{101325.0};
    MaterialPhase primary_phase{MaterialPhase::Solid};
    std::unordered_map<std::string, double> phase_fractions; // e.g. "Solid": 0.8, "Liquid": 0.2
    double accumulated_plastic_strain{0.0};
    double internal_energy_j_kg{0.0};
    std::unordered_map<std::string, double> history_variables;
    uint64_t state_version{1};

    void mutate() noexcept { ++state_version; }
    void set_temperature(double t) noexcept { temperature_k = t; mutate(); }
    void set_pressure(double p) noexcept { pressure_pa = p; mutate(); }
    void set_phase_fraction(std::string phase_name, double frac) {
        phase_fractions[std::move(phase_name)] = frac;
        mutate();
    }
    [[nodiscard]] double get_phase_fraction(std::string_view phase_name) const noexcept;

    bool operator==(const MaterialState& o) const noexcept;
    [[nodiscard]] std::string to_json() const;
    static base::Result<MaterialState> from_json(std::string_view json_str);
};

// MATRT-04: Material Region Attached to Geometry
class MaterialRegion {
public:
    MaterialRegion();
    MaterialRegion(base::Uuid id, std::string name, base::Uuid material_id, std::string geometry_ref);

    [[nodiscard]] const base::Uuid& id() const noexcept { return id_; }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    void set_name(std::string name) { name_ = std::move(name); }

    [[nodiscard]] const base::Uuid& material_id() const noexcept { return material_id_; }
    void set_material_id(const base::Uuid& mid) noexcept { material_id_ = mid; }

    [[nodiscard]] const std::string& geometry_ref() const noexcept { return geometry_ref_; }
    void set_geometry_ref(std::string ref) { geometry_ref_ = std::move(ref); }

    [[nodiscard]] double volume_m3() const noexcept { return volume_m3_; }
    void set_volume_m3(double v) noexcept { volume_m3_ = v; }

    [[nodiscard]] const MaterialState& state() const noexcept { return state_; }
    MaterialState& state() noexcept { return state_; }
    void set_state(MaterialState s) noexcept { state_ = std::move(s); }

    [[nodiscard]] std::string to_json() const;
    static base::Result<MaterialRegion> from_json(std::string_view json_str);

private:
    base::Uuid id_{base::Uuid::generate_random()};
    std::string name_{"Region"};
    base::Uuid material_id_;
    std::string geometry_ref_;
    double volume_m3_{1.0};
    MaterialState state_;
};

// MATRT-08: Phase Transition and Latent-Heat Interface
struct PhaseTransitionModel {
    double solidus_temperature_k{1600.0};
    double liquidus_temperature_k{1650.0};
    double boiling_temperature_k{3000.0};
    double latent_heat_fusion_j_kg{2.5e5};
    double latent_heat_vaporization_j_kg{6.0e6};

    [[nodiscard]] double compute_liquid_fraction(double temperature_k) const noexcept;
    [[nodiscard]] double compute_apparent_heat_capacity(double temperature_k, double base_cp) const noexcept;
};

// MATRT-06, MATRT-07: Property Provider Interface
struct PropertyEvaluation {
    double value{0.0};
    double derivative_wrt_t{0.0};
    double derivative_wrt_p{0.0};
    bool in_valid_range{true};
};

class IPropertyProvider {
public:
    virtual ~IPropertyProvider() = default;
    [[nodiscard]] virtual base::Result<double> evaluate(std::string_view property_name, const MaterialState& state) const = 0;
    [[nodiscard]] virtual base::Result<PropertyEvaluation> evaluate_with_derivatives(std::string_view property_name, const MaterialState& state) const = 0;
    [[nodiscard]] virtual std::pair<double, double> temperature_validity_range(std::string_view property_name) const = 0;
    [[nodiscard]] virtual base::ValidationReport get_diagnostics(const MaterialState& state) const = 0;
};

// MATRT-10: Cache Invalidation Engine
class PropertyCache {
public:
    PropertyCache() = default;

    struct CacheKey {
        base::Uuid material_id;
        std::string property_name;
        int64_t discretized_t{0};
        int64_t discretized_p{0};
        uint64_t state_version{0};

        bool operator==(const CacheKey& o) const noexcept {
            return material_id == o.material_id &&
                   property_name == o.property_name &&
                   discretized_t == o.discretized_t &&
                   discretized_p == o.discretized_p &&
                   state_version == o.state_version;
        }
    };

    struct KeyHash {
        std::size_t operator()(const CacheKey& k) const noexcept {
            std::size_t h = std::hash<std::string>{}(k.property_name);
            h ^= std::hash<int64_t>{}(k.discretized_t) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<int64_t>{}(k.discretized_p) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<uint64_t>{}(k.state_version) + 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }
    };

    [[nodiscard]] std::optional<double> get(const base::Uuid& mat_id, std::string_view prop, const MaterialState& state) const;
    void put(const base::Uuid& mat_id, std::string_view prop, const MaterialState& state, double val);
    void invalidate() noexcept { cache_.clear(); }
    void invalidate_material(const base::Uuid& mat_id);
    [[nodiscard]] size_t size() const noexcept { return cache_.size(); }
    [[nodiscard]] size_t hits() const noexcept { return hits_; }
    [[nodiscard]] size_t misses() const noexcept { return misses_; }

private:
    mutable std::unordered_map<CacheKey, double, KeyHash> cache_;
    mutable size_t hits_{0};
    mutable size_t misses_{0};
};

// MATRT-12: Per-Region Diagnostics
class RegionDiagnosticsEngine {
public:
    [[nodiscard]] static base::ValidationReport diagnose_region(
        const MaterialRegion& region,
        const MaterialDefinition& def,
        const std::vector<std::string>& active_solver_domains);
};

} // namespace shunya::core::domain
