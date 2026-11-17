#include "shunya/mat/material_mutation_propagator.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::mat {

void MaterialMutationPropagator::register_material(
    const core::base::Uuid& material_id, const MaterialPropertyBundle& bundle) {
    materials_[material_id] = bundle;
}

void MaterialMutationPropagator::register_object(const eng::PhysicalObject& object) {
    objects_[object.id] = object;

    // Track dependencies across all regions
    for (const auto& reg : object.regions) {
        if (!reg.material_id.is_nil()) {
            auto& vec = material_to_objects_[reg.material_id];
            if (std::find(vec.begin(), vec.end(), object.id) == vec.end()) {
                vec.push_back(object.id);
            }
        }
    }

    // Initial state calculation
    if (!object.regions.empty()) {
        const auto& mat_id = object.regions.front().material_id;
        auto it = materials_.find(mat_id);
        if (it != materials_.end()) {
            object_states_[object.id] = compute_physical_state(object, it->second);
        }
    }
}

void MaterialMutationPropagator::unregister_object(const core::base::Uuid& object_id) {
    objects_.erase(object_id);
    object_states_.erase(object_id);
    for (auto& [mat_id, vec] : material_to_objects_) {
        std::erase(vec, object_id);
    }
}

void MaterialMutationPropagator::add_listener(MutationListener listener) {
    listeners_.push_back(std::move(listener));
}

const ObjectPhysicalState* MaterialMutationPropagator::get_object_state(
    const core::base::Uuid& object_id) const noexcept {
    auto it = object_states_.find(object_id);
    return (it != object_states_.end()) ? &it->second : nullptr;
}

ObjectPhysicalState MaterialMutationPropagator::compute_physical_state(
    const eng::PhysicalObject& obj,
    const MaterialPropertyBundle& mat) const noexcept {
    ObjectPhysicalState state;
    state.object_id = obj.id;
    state.object_name = obj.name;

    const double vol = obj.geometry.compute_volume_m3();
    const double rho = mat.mechanical.density_kg_m3;
    state.effective_density_kg_m3 = rho;
    state.mass_kg = vol * rho;

    // Center of Gravity (CoG) in 3D:
    // Base object position offset by geometry local centroid
    state.center_of_gravity = obj.geometry.position;
    if (obj.geometry.kind == eng::ObjectShapeKind::Cone) {
        // Cone centroid is 1/4 h from base
        state.center_of_gravity.z += obj.geometry.dim_z_m * 0.25;
    } else {
        // Symmetric prisms/spheres/cylinders centered at mid-height
        state.center_of_gravity.z += obj.geometry.dim_z_m * 0.5;
    }

    // Moments of inertia for rectangular box or cylinder
    const double m = state.mass_kg;
    if (obj.geometry.kind == eng::ObjectShapeKind::Box) {
        const double w = obj.geometry.dim_x_m;
        const double d = obj.geometry.dim_y_m;
        const double h = obj.geometry.dim_z_m;
        state.moment_of_inertia.x = (1.0 / 12.0) * m * (d * d + h * h);
        state.moment_of_inertia.y = (1.0 / 12.0) * m * (w * w + h * h);
        state.moment_of_inertia.z = (1.0 / 12.0) * m * (w * w + d * d);
    } else if (obj.geometry.kind == eng::ObjectShapeKind::Cylinder) {
        const double r = obj.geometry.radius_m;
        const double h = obj.geometry.dim_z_m;
        state.moment_of_inertia.z = 0.5 * m * r * r;
        state.moment_of_inertia.x = (1.0 / 12.0) * m * (3.0 * r * r + h * h);
        state.moment_of_inertia.y = state.moment_of_inertia.x;
    } else {
        // General sphere / profile: I = 2/5 m r^2
        const double r = obj.geometry.radius_m;
        state.moment_of_inertia.x = 0.4 * m * r * r;
        state.moment_of_inertia.y = 0.4 * m * r * r;
        state.moment_of_inertia.z = 0.4 * m * r * r;
    }

    // Electrical resistance & conductance
    const double sigma = mat.electrical.base_conductivity_s_m;
    state.is_conductive = (sigma > 10.0); // > 10 S/m considered conductive
    const double area = std::max(1e-8, obj.geometry.compute_cross_section_area_m2());
    const double length = std::max(1e-4, obj.geometry.compute_characteristic_length_m());

    if (state.is_conductive && area > 0.0) {
        const double rho_elec = (sigma > 0.0) ? (1.0 / sigma) : 1e18;
        state.electrical_resistance_ohm = rho_elec * (length / area);
        state.electrical_conductance_siemens = 1.0 / state.electrical_resistance_ohm;
    } else {
        state.electrical_resistance_ohm = 1e12; // Gigaohm open-circuit limit
        state.electrical_conductance_siemens = 1e-12;
    }

    return state;
}

PropagationReport MaterialMutationPropagator::mutate_material_property(
    const core::base::Uuid& material_id,
    std::string_view property_name,
    double new_value) {
    ++current_tick_;

    PropagationReport report;
    report.propagation_tick = current_tick_;

    auto mat_it = materials_.find(material_id);
    if (mat_it == materials_.end()) {
        return report;
    }

    MaterialMutationEvent evt;
    evt.material_id = material_id;
    evt.property_name = std::string(property_name);
    evt.new_value = new_value;
    evt.mutation_tick = current_tick_;

    // Mutate the property in bundle
    auto& bundle = mat_it->second;
    if (property_name == "density" || property_name == "density_kg_m3") {
        evt.old_value = bundle.mechanical.density_kg_m3;
        bundle.mechanical.density_kg_m3 = new_value;
    } else if (property_name == "conductivity" || property_name == "base_conductivity_s_m") {
        evt.old_value = bundle.electrical.base_conductivity_s_m;
        bundle.electrical.base_conductivity_s_m = new_value;
        bundle.electrical.base_resistivity_ohm_m = (new_value > 0.0) ? (1.0 / new_value) : 1e18;
    } else if (property_name == "young_modulus" || property_name == "young_modulus_pa") {
        evt.old_value = bundle.mechanical.young_modulus_pa;
        bundle.mechanical.young_modulus_pa = new_value;
    }

    // Cascade to all dependent objects
    auto dep_it = material_to_objects_.find(material_id);
    if (dep_it != material_to_objects_.end()) {
        for (const auto& obj_id : dep_it->second) {
            auto obj_it = objects_.find(obj_id);
            if (obj_it != objects_.end()) {
                auto new_state = compute_physical_state(obj_it->second, bundle);
                object_states_[obj_id] = new_state;
                report.updated_objects.push_back(new_state);
            }
        }
    }
    report.affected_objects_count = report.updated_objects.size();

    // Notify listeners
    for (const auto& listener : listeners_) {
        listener(evt, report);
    }

    return report;
}

} // namespace shunya::mat
