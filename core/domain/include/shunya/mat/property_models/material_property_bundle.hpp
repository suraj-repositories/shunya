#pragma once

#include "shunya/mat/property_models/chemical_property_model.hpp"
#include "shunya/mat/property_models/electrical_property_model.hpp"
#include "shunya/mat/property_models/thermal_property_model.hpp"
#include "shunya/mat/property_models/mechanical_property_model.hpp"
#include "shunya/mat/property_models/environmental_property_model.hpp"

#include <string>
#include <vector>

namespace shunya::mat {

struct BundleValidationResult {
    bool is_valid = true;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

struct MaterialPropertyBundle {
    std::string material_id;
    std::string material_name;

    ChemicalPropertyModel chemical;
    ElectricalPropertyModel electrical;
    ThermalPropertyModel thermal;
    MechanicalPropertyModel mechanical;
    EnvironmentalPropertyModel environmental;

    // Validate SI unit constraints and physical consistency across domains
    [[nodiscard]] BundleValidationResult validate() const;

    // Computes Lorenz number L = k / (sigma * T) [W*Ohm/K^2] at given temperature
    [[nodiscard]] double lorenz_number(double temp_k = 293.15) const;

    // Formatted multi-domain summary
    [[nodiscard]] std::string summary() const;
};

} // namespace shunya::mat
