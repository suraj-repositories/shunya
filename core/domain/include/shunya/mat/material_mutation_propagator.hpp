#pragma once

#include "shunya/core/base/math.hpp"
#include "shunya/core/base/uuid.hpp"
#include "shunya/eng/physical_object.hpp"
#include "shunya/mat/property_models/material_property_bundle.hpp"

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace shunya::mat {

struct MaterialMutationEvent {
    core::base::Uuid material_id;
    std::string property_name; // e.g. "density", "conductivity", "young_modulus"
    double old_value{0.0};
    double new_value{0.0};
    uint64_t mutation_tick{0};
};

struct ObjectPhysicalState {
    core::base::Uuid object_id;
    std::string object_name;
    double mass_kg{0.0};
    double effective_density_kg_m3{0.0};
    core::base::math::Vector3<double> center_of_gravity{0.0, 0.0, 0.0};
    core::base::math::Vector3<double> moment_of_inertia{0.0, 0.0, 0.0}; // Ixx, Iyy, Izz
    double electrical_resistance_ohm{0.0};
    double electrical_conductance_siemens{0.0};
    bool is_conductive{false};
};

struct PropagationReport {
    uint64_t propagation_tick{0};
    size_t affected_objects_count{0};
    std::vector<ObjectPhysicalState> updated_objects;
};

/**
 * @brief Reactive material mutation propagation engine:
 *        Whenever any material property changes, immediately evaluates dependencies,
 *        propagates mutations, and recalculates mass, density, 3D Center of Gravity (CoG),
 *        moments of inertia, and electrical parameters across all registered 3D objects.
 * (Roadmap task PH4-03.04.003)
 */
class MaterialMutationPropagator {
public:
    using MutationListener = std::function<void(const MaterialMutationEvent&, const PropagationReport&)>;

    MaterialMutationPropagator() = default;

    // Registers a material-to-object dependency
    void register_object(const eng::PhysicalObject& object);
    void unregister_object(const core::base::Uuid& object_id);

    // Binds a material bundle definition
    void register_material(const core::base::Uuid& material_id, const MaterialPropertyBundle& bundle);

    // Attaches reactive listeners
    void add_listener(MutationListener listener);

    /**
     * @brief Mutates a material property and triggers immediate reactive cascade propagation.
     */
    [[nodiscard]] PropagationReport mutate_material_property(
        const core::base::Uuid& material_id,
        std::string_view property_name,
        double new_value);

    [[nodiscard]] const ObjectPhysicalState* get_object_state(const core::base::Uuid& object_id) const noexcept;
    [[nodiscard]] size_t registered_objects_count() const noexcept { return object_states_.size(); }

private:
    [[nodiscard]] ObjectPhysicalState compute_physical_state(
        const eng::PhysicalObject& obj,
        const MaterialPropertyBundle& mat) const noexcept;

    std::unordered_map<core::base::Uuid, MaterialPropertyBundle> materials_;
    std::unordered_map<core::base::Uuid, eng::PhysicalObject> objects_;
    std::unordered_map<core::base::Uuid, ObjectPhysicalState> object_states_;
    std::unordered_map<core::base::Uuid, std::vector<core::base::Uuid>> material_to_objects_;
    std::vector<MutationListener> listeners_;
    uint64_t current_tick_{0};
};

} // namespace shunya::mat
