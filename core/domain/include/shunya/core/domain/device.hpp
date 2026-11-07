#pragma once

#include "shunya/core/base/math.hpp"
#include "shunya/core/base/uuid.hpp"
#include "shunya/core/base/error.hpp"
#include "shunya/core/base/result.hpp"

#include <vector>
#include <string>
#include <memory>
#include <optional>
#include <variant>
#include <unordered_map>
#include <functional>
#include <cmath>

namespace shunya::core::domain::device {

using Vector3d = base::math::Vector3<double>;
using Quaterniond = base::math::Quaternion<double>;

// =============================================================================
// DEV-02: Geometry Primitives & CAD Importer Abstraction
// =============================================================================

enum class CadFormat {
    Obj,
    Stl,
    Step
};

struct GeometricPrimitive {
    enum class Type {
        Box,
        Cylinder,
        Sphere,
        Extrusion
    } type = Type::Box;

    Vector3d dimensions{1.0, 1.0, 1.0}; // Box (width, height, depth) or Cylinder (radius, height, radius)
    double radius = 0.5;

    [[nodiscard]] double compute_volume() const noexcept {
        switch (type) {
            case Type::Box:
                return dimensions.x * dimensions.y * dimensions.z;
            case Type::Cylinder:
                return base::math::PI * radius * radius * dimensions.y;
            case Type::Sphere:
                return (4.0 / 3.0) * base::math::PI * radius * radius * radius;
            case Type::Extrusion:
                return dimensions.x * dimensions.y * dimensions.z;
        }
        return 0.0;
    }
};

struct CadMesh {
    std::string source_filename;
    CadFormat format = CadFormat::Obj;
    std::vector<Vector3d> vertices;
    std::vector<std::array<std::size_t, 3>> triangle_indices;
    Vector3d aabb_min{0.0, 0.0, 0.0};
    Vector3d aabb_max{0.0, 0.0, 0.0};
    double estimated_volume = 1e-3;

    [[nodiscard]] bool is_valid() const noexcept {
        return !vertices.empty() && !triangle_indices.empty();
    }
};

class CadImportPipeline {
public:
    [[nodiscard]] static base::Result<CadMesh> import_mock_cad(
        const std::string& filepath, CadFormat format, const Vector3d& bounding_box);
};

// =============================================================================
// DEV-04: Layered & Composite Construction
// =============================================================================

enum class LayerType {
    Substrate,
    Conductor,
    Dielectric,
    Semiconductor,
    Coating,
    Enclosure
};

struct DeviceLayer {
    std::string name;
    LayerType type = LayerType::Substrate;
    double thickness = 1e-3;         // meters
    double thermal_conductivity = 1.0; // W / (m * K)
    double electrical_conductivity = 1e-6; // S / m
    double relative_permittivity = 1.0;
    double density = 1000.0;         // kg / m^3
};

struct CompositeStackup {
    std::string stackup_id;
    std::vector<DeviceLayer> layers;

    [[nodiscard]] double total_thickness() const noexcept {
        double t = 0.0;
        for (const auto& l : layers) t += l.thickness;
        return t;
    }

    [[nodiscard]] double effective_thermal_conductivity() const noexcept {
        // Series thermal resistance: R_th = sum(t_i / k_i) -> k_eff = total_t / R_th
        double r_sum = 0.0;
        for (const auto& l : layers) {
            const double k = std::max(l.thermal_conductivity, 1e-6);
            r_sum += l.thickness / k;
        }
        const double tot_t = total_thickness();
        return (r_sum > 1e-12) ? (tot_t / r_sum) : 0.0;
    }

    [[nodiscard]] double areal_capacitance() const noexcept {
        // Capacitance per unit area: eps_0 / sum(t_i / eps_r_i) for dielectric layers
        constexpr double EPS_0 = 8.8541878128e-12;
        double sum_t_eps = 0.0;
        for (const auto& l : layers) {
            if (l.type == LayerType::Dielectric || l.type == LayerType::Substrate) {
                const double eps_r = std::max(l.relative_permittivity, 1.0);
                sum_t_eps += l.thickness / eps_r;
            }
        }
        return (sum_t_eps > 1e-12) ? (EPS_0 / sum_t_eps) : 0.0;
    }
};

// =============================================================================
// DEV-05: Conductor Paths & Trace Connectivity
// =============================================================================

struct ConductorPath {
    std::string trace_id;
    std::string net_name;
    std::vector<Vector3d> waypoints;
    double width = 0.5e-3;       // 0.5 mm
    double thickness = 35e-6;    // 1 oz copper = 35 um
    double resistivity = 1.68e-8; // Copper resistivity (ohm * m)
    double ampacity = 1.5;       // Amperes max current

    [[nodiscard]] double length() const noexcept {
        double len = 0.0;
        for (std::size_t i = 1; i < waypoints.size(); ++i) {
            const Vector3d diff = waypoints[i] - waypoints[i - 1];
            len += std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
        }
        return len;
    }

    [[nodiscard]] double resistance() const noexcept {
        const double l = length();
        const double area = std::max(width * thickness, 1e-12);
        return resistivity * (l / area);
    }
};

// =============================================================================
// DEV-06: Interface Models (Electrical, Thermal, Mechanical)
// =============================================================================

enum class ContactDomain {
    Electrical,
    Thermal,
    Mechanical
};

struct DeviceContactInterface {
    std::string interface_id;
    ContactDomain domain = ContactDomain::Electrical;
    std::string part_a_id;
    std::string part_b_id;
    double contact_resistance = 1e-3;   // Ohms (electrical)
    double thermal_interface_r = 0.05;  // K / W (thermal TIM)
    double friction_coefficient = 0.4;  // Mechanical friction
    double contact_area = 1e-4;         // m^2
};

// =============================================================================
// DEV-08: Assembly Mates & Attachment Points
// =============================================================================

enum class MateType {
    Coincident,
    Concentric,
    Distance,
    Fixed
};

struct AssemblyMate {
    std::string mate_id;
    MateType type = MateType::Fixed;
    std::string part_a_id;
    std::string part_b_id;
    Vector3d offset{0.0, 0.0, 0.0};
    double target_distance = 0.0;
    bool is_satisfied = true;
};

// =============================================================================
// DEV-01 & DEV-03: Device Part & Hierarchy
// =============================================================================

struct DevicePort {
    std::string port_id;
    std::string domain; // "electrical", "thermal", "mechanical"
    Vector3d local_position{0.0, 0.0, 0.0};
    Vector3d normal{0.0, 1.0, 0.0};
    std::string connected_net;
};

struct DevicePart {
    std::string part_id;
    std::string name;
    Vector3d local_position{0.0, 0.0, 0.0};
    Quaterniond local_rotation{1.0, 0.0, 0.0, 0.0};
    double density = 2700.0; // kg / m^3
    GeometricPrimitive primitive;
    std::optional<CadMesh> mesh;
    std::optional<CompositeStackup> stackup;
    std::vector<DevicePort> ports;
    std::vector<std::string> child_part_ids;

    [[nodiscard]] double compute_volume() const noexcept {
        if (mesh && mesh->is_valid()) {
            return mesh->estimated_volume;
        }
        return primitive.compute_volume();
    }

    [[nodiscard]] double compute_mass() const noexcept {
        return compute_volume() * density;
    }
};

// =============================================================================
// DEV-09: Sensors & Non-Invasive Probes
// =============================================================================

enum class SensorType {
    Voltage,
    Current,
    Temperature,
    StrainGauge,
    Accelerometer,
    MagneticField
};

struct SensorReading {
    double timestamp = 0.0;
    double scalar_value = 0.0;
    Vector3d vector_value{0.0, 0.0, 0.0};
    std::string unit;
};

class DeviceSensor {
public:
    DeviceSensor(std::string sensor_id, SensorType type, std::string target_part_or_net);

    [[nodiscard]] const std::string& sensor_id() const noexcept { return sensor_id_; }
    [[nodiscard]] SensorType type() const noexcept { return type_; }
    [[nodiscard]] const std::string& target() const noexcept { return target_; }

    void record_scalar(double timestamp, double value, std::string unit);
    void record_vector(double timestamp, const Vector3d& vec, std::string unit);

    [[nodiscard]] const std::vector<SensorReading>& readings() const noexcept { return history_; }
    [[nodiscard]] std::optional<SensorReading> latest_reading() const noexcept;

private:
    std::string sensor_id_;
    SensorType type_;
    std::string target_;
    std::vector<SensorReading> history_;
};

// =============================================================================
// DEV-10: Diagnostics & Integrity Checks
// =============================================================================

struct DeviceDiagnosticsReport {
    bool has_continuity = true;
    std::vector<std::string> open_circuit_nets;
    std::vector<std::string> floating_ports;
    double total_mass = 0.0;
    Vector3d center_of_mass{0.0, 0.0, 0.0};
    bool valid = true;
};

// =============================================================================
// DEV-11: Variants & Parameter Sweeps
// =============================================================================

struct ParameterOverride {
    std::string parameter_name;
    double value = 0.0;
};

struct DeviceVariant {
    std::string variant_id;
    std::string label;
    std::unordered_map<std::string, double> parameter_overrides;
};

// =============================================================================
// DEV-12: Geometric Fidelity & Solver Limitation Auditor
// =============================================================================

struct MeshLimitationIssue {
    std::string part_id;
    std::string feature_name;
    double feature_size = 0.0;
    double solver_mesh_resolution = 0.0;
    std::string warning_message;
};

// =============================================================================
// DEV-01: Device Assembly Container
// =============================================================================

class DeviceAssembly {
public:
    explicit DeviceAssembly(std::string assembly_id);

    [[nodiscard]] const std::string& assembly_id() const noexcept { return assembly_id_; }

    void add_part(const DevicePart& part);
    [[nodiscard]] const DevicePart* get_part(const std::string& part_id) const;
    [[nodiscard]] DevicePart* get_part_mut(const std::string& part_id);
    [[nodiscard]] const std::vector<DevicePart>& parts() const noexcept { return parts_; }

    void add_conductor_path(const ConductorPath& path);
    [[nodiscard]] const std::vector<ConductorPath>& conductor_paths() const noexcept { return conductor_paths_; }

    void add_contact_interface(const DeviceContactInterface& iface);
    [[nodiscard]] const std::vector<DeviceContactInterface>& contact_interfaces() const noexcept { return interfaces_; }

    void add_mate(const AssemblyMate& mate);
    [[nodiscard]] const std::vector<AssemblyMate>& mates() const noexcept { return mates_; }

    void add_sensor(std::shared_ptr<DeviceSensor> sensor);
    [[nodiscard]] std::shared_ptr<DeviceSensor> get_sensor(const std::string& sensor_id) const;

    [[nodiscard]] DeviceDiagnosticsReport run_diagnostics() const;

    [[nodiscard]] std::vector<MeshLimitationIssue> audit_geometric_fidelity(double solver_mesh_min_edge) const;

    void add_variant(const DeviceVariant& variant);
    [[nodiscard]] const std::vector<DeviceVariant>& variants() const noexcept { return variants_; }

private:
    std::string assembly_id_;
    std::vector<DevicePart> parts_;
    std::vector<ConductorPath> conductor_paths_;
    std::vector<DeviceContactInterface> interfaces_;
    std::vector<AssemblyMate> mates_;
    std::vector<std::shared_ptr<DeviceSensor>> sensors_;
    std::vector<DeviceVariant> variants_;
};

} // namespace shunya::core::domain::device
