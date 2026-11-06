#pragma once

#include <shunya/core/base/error.hpp>
#include <shunya/core/base/result.hpp>
#include <shunya/core/base/uuid.hpp>
#include <shunya/core/base/version.hpp>
#include <shunya/core/domain/material_curve.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace shunya::core::domain {

enum class MaterialState {
    Solid,
    Liquid,
    Gas,
    Plasma
};

[[nodiscard]] constexpr std::string_view material_state_to_string(MaterialState s) noexcept {
    switch (s) {
    case MaterialState::Solid:  return "Solid";
    case MaterialState::Liquid: return "Liquid";
    case MaterialState::Gas:    return "Gas";
    case MaterialState::Plasma: return "Plasma";
    }
    return "Solid";
}

[[nodiscard]] base::Result<MaterialState> material_state_from_string(std::string_view str);

// MAT-02: Mechanical Properties
struct MechanicalProperties {
    std::optional<double> youngs_modulus_pa;       // E (Pa)
    std::optional<double> poissons_ratio;           // nu (-1.0 to 0.5)
    std::optional<double> shear_modulus_pa;         // G (Pa)
    std::optional<double> bulk_modulus_pa;          // K (Pa)
    std::optional<double> yield_strength_pa;        // sigma_y (Pa)
    std::optional<double> ultimate_strength_pa;     // sigma_u (Pa)
    std::optional<double> elongation_at_break;      // fraction (e.g. 0.15 = 15%)
    std::optional<double> tangent_modulus_pa;       // plastic hardening modulus (Pa)
    std::optional<double> fracture_toughness;       // K_1c (Pa * m^0.5)

    // Derived helpers
    [[nodiscard]] double effective_shear_modulus() const noexcept;
    [[nodiscard]] double effective_bulk_modulus() const noexcept;

    bool operator==(const MechanicalProperties& other) const noexcept;
};

// MAT-03: Thermal Properties
struct ThermalProperties {
    std::optional<double> thermal_conductivity;     // k (W / (m * K))
    std::optional<double> specific_heat_capacity;   // cp (J / (kg * K))
    std::optional<double> thermal_expansion_coeff;  // alpha (1 / K)
    std::optional<double> emissivity;               // 0.0 .. 1.0
    std::optional<double> melting_point_k;          // K
    std::optional<double> boiling_point_k;          // K
    std::optional<double> latent_heat_fusion;       // J / kg
    std::optional<double> latent_heat_vaporization;  // J / kg

    bool operator==(const ThermalProperties& other) const noexcept;
};

// MAT-04: Electrical & Electromagnetic Properties
struct ElectricalProperties {
    std::optional<double> electrical_conductivity;   // sigma (S / m)
    std::optional<double> electrical_resistivity;    // rho_e (Ohm * m)
    std::optional<double> relative_permittivity;    // eps_r (>= 1.0)
    std::optional<double> relative_permeability;    // mu_r (>= 0.0)
    std::optional<double> dielectric_breakdown_strength; // V / m
    std::optional<double> loss_tangent;             // tan(delta)
    std::optional<double> temp_coeff_resistance;    // 1 / K

    [[nodiscard]] double effective_resistivity() const noexcept;

    bool operator==(const ElectricalProperties& other) const noexcept;
};

// MAT-05: Fluid & Chemical Properties
struct FluidProperties {
    std::optional<double> dynamic_viscosity;        // mu (Pa * s)
    std::optional<double> kinematic_viscosity;      // nu (m^2 / s)
    std::optional<double> diffusivity;              // D (m^2 / s)
    std::optional<double> surface_tension;          // N / m
    std::optional<double> molecular_weight;         // kg / mol (or g/mol)
    std::string chemical_formula;                   // e.g. "H2O", "Cu", "Si"

    bool operator==(const FluidProperties& other) const noexcept;
};

// MAT-01: Material schema & model
class Material {
public:
    Material();
    Material(base::Uuid id, std::string name, MaterialState state = MaterialState::Solid, double density_kg_m3 = 0.0);

    // MAT-01 Attributes
    [[nodiscard]] const base::Uuid& id() const noexcept { return id_; }
    void set_id(const base::Uuid& id) noexcept { id_ = id; }

    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    void set_name(std::string name) { name_ = std::move(name); }

    [[nodiscard]] const std::string& description() const noexcept { return description_; }
    void set_description(std::string desc) { description_ = std::move(desc); }

    [[nodiscard]] const std::string& composition() const noexcept { return composition_; }
    void set_composition(std::string comp) { composition_ = std::move(comp); }

    [[nodiscard]] double density() const noexcept { return density_kg_m3_; }
    void set_density(double rho) noexcept { density_kg_m3_ = rho; }

    [[nodiscard]] MaterialState state() const noexcept { return state_; }
    void set_state(MaterialState s) noexcept { state_ = s; }

    [[nodiscard]] const std::string& provenance() const noexcept { return provenance_; }
    void set_provenance(std::string prov) { provenance_ = std::move(prov); }

    [[nodiscard]] const std::string& source() const noexcept { return source_; }
    void set_source(std::string src) { source_ = std::move(src); }

    [[nodiscard]] const base::Version& version() const noexcept { return version_; }
    void set_version(const base::Version& v) noexcept { version_ = v; }

    // MAT-02: Mechanical
    [[nodiscard]] const MechanicalProperties& mechanical() const noexcept { return mechanical_; }
    MechanicalProperties& mechanical() noexcept { return mechanical_; }
    void set_mechanical(MechanicalProperties mech) noexcept { mechanical_ = mech; }

    // MAT-03: Thermal
    [[nodiscard]] const ThermalProperties& thermal() const noexcept { return thermal_; }
    ThermalProperties& thermal() noexcept { return thermal_; }
    void set_thermal(ThermalProperties therm) noexcept { thermal_ = therm; }

    // MAT-04: Electrical
    [[nodiscard]] const ElectricalProperties& electrical() const noexcept { return electrical_; }
    ElectricalProperties& electrical() noexcept { return electrical_; }
    void set_electrical(ElectricalProperties elec) noexcept { electrical_ = elec; }

    // MAT-05: Fluid / Chemical
    [[nodiscard]] const FluidProperties& fluid() const noexcept { return fluid_; }
    FluidProperties& fluid() noexcept { return fluid_; }
    void set_fluid(FluidProperties fl) { fluid_ = std::move(fl); }

    // MAT-07: Material Curves
    void add_curve(MaterialCurve curve);
    [[nodiscard]] const MaterialCurve* get_curve(std::string_view property_name) const noexcept;
    [[nodiscard]] bool has_curve(std::string_view property_name) const noexcept;
    void remove_curve(std::string_view property_name);
    [[nodiscard]] const std::unordered_map<std::string, MaterialCurve>& curves() const noexcept { return curves_; }

    // Property Evaluation with curve fallback to scalar
    [[nodiscard]] base::Result<double> evaluate_thermal_conductivity(double temperature_k) const;
    [[nodiscard]] base::Result<double> evaluate_specific_heat(double temperature_k) const;
    [[nodiscard]] base::Result<double> evaluate_electrical_resistivity(double temperature_k) const;
    [[nodiscard]] base::Result<double> evaluate_stress(double strain) const;

    // MAT-08: Validation
    [[nodiscard]] base::ValidationReport validate() const;

    // JSON Serialization
    [[nodiscard]] std::string to_json() const;
    static base::Result<Material> from_json(std::string_view json_str);

    // Standard Preset Materials
    static Material structural_steel();
    static Material aluminium_6061();
    static Material copper();
    static Material silicon();
    static Material air_standard();
    static Material water_liquid();

private:
    base::Uuid id_;
    std::string name_;
    std::string description_;
    std::string composition_;
    double density_kg_m3_{0.0};
    MaterialState state_{MaterialState::Solid};
    std::string provenance_;
    std::string source_;
    base::Version version_{1, 0, 0};

    MechanicalProperties mechanical_;
    ThermalProperties thermal_;
    ElectricalProperties electrical_;
    FluidProperties fluid_;

    std::unordered_map<std::string, MaterialCurve> curves_;
};

} // namespace shunya::core::domain
