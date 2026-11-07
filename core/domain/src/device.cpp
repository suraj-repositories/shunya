#include "shunya/core/domain/device.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::core::domain::device {

// =============================================================================
// CadImportPipeline
// =============================================================================

base::Result<CadMesh> CadImportPipeline::import_mock_cad(
    const std::string& filepath, CadFormat format, const Vector3d& bounding_box) {
    if (bounding_box.x <= 0.0 || bounding_box.y <= 0.0 || bounding_box.z <= 0.0) {
        base::Error err;
        err.code = 1;
        err.domain = base::ErrorDomain::IO;
        err.severity = base::Severity::Error;
        err.message = "Invalid bounding box dimensions for CAD import";
        return base::Result<CadMesh>::err(err);
    }

    CadMesh mesh;
    mesh.source_filename = filepath;
    mesh.format = format;
    const double hx = bounding_box.x * 0.5;
    const double hy = bounding_box.y * 0.5;
    const double hz = bounding_box.z * 0.5;

    mesh.aabb_min = {-hx, -hy, -hz};
    mesh.aabb_max = {hx, hy, hz};
    mesh.estimated_volume = bounding_box.x * bounding_box.y * bounding_box.z;

    // 8 Bounding Box Vertices
    mesh.vertices = {
        {-hx, -hy, -hz}, {hx, -hy, -hz}, {hx, hy, -hz}, {-hx, hy, -hz},
        {-hx, -hy,  hz}, {hx, -hy,  hz}, {hx, hy,  hz}, {-hx, hy,  hz}
    };

    // 12 Triangles (6 faces)
    mesh.triangle_indices = {
        {0, 1, 2}, {0, 2, 3}, // Back
        {4, 6, 5}, {4, 7, 6}, // Front
        {4, 5, 1}, {4, 1, 0}, // Bottom
        {3, 2, 6}, {3, 6, 7}, // Top
        {0, 3, 7}, {0, 7, 4}, // Left
        {1, 5, 6}, {1, 6, 2}  // Right
    };

    return base::Result<CadMesh>::ok(std::move(mesh));
}

// =============================================================================
// DeviceSensor
// =============================================================================

DeviceSensor::DeviceSensor(std::string sensor_id, SensorType type, std::string target_part_or_net)
    : sensor_id_(std::move(sensor_id)), type_(type), target_(std::move(target_part_or_net)) {}

void DeviceSensor::record_scalar(double timestamp, double value, std::string unit) {
    history_.push_back(SensorReading{
        .timestamp = timestamp,
        .scalar_value = value,
        .vector_value = {0.0, 0.0, 0.0},
        .unit = std::move(unit)
    });
}

void DeviceSensor::record_vector(double timestamp, const Vector3d& vec, std::string unit) {
    history_.push_back(SensorReading{
        .timestamp = timestamp,
        .scalar_value = std::sqrt(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z),
        .vector_value = vec,
        .unit = std::move(unit)
    });
}

std::optional<SensorReading> DeviceSensor::latest_reading() const noexcept {
    if (history_.empty()) return std::nullopt;
    return history_.back();
}

// =============================================================================
// DeviceAssembly
// =============================================================================

DeviceAssembly::DeviceAssembly(std::string assembly_id)
    : assembly_id_(std::move(assembly_id)) {}

void DeviceAssembly::add_part(const DevicePart& part) {
    parts_.push_back(part);
}

const DevicePart* DeviceAssembly::get_part(const std::string& part_id) const {
    for (const auto& p : parts_) {
        if (p.part_id == part_id) return &p;
    }
    return nullptr;
}

DevicePart* DeviceAssembly::get_part_mut(const std::string& part_id) {
    for (auto& p : parts_) {
        if (p.part_id == part_id) return &p;
    }
    return nullptr;
}

void DeviceAssembly::add_conductor_path(const ConductorPath& path) {
    conductor_paths_.push_back(path);
}

void DeviceAssembly::add_contact_interface(const DeviceContactInterface& iface) {
    interfaces_.push_back(iface);
}

void DeviceAssembly::add_mate(const AssemblyMate& mate) {
    mates_.push_back(mate);
}

void DeviceAssembly::add_sensor(std::shared_ptr<DeviceSensor> sensor) {
    if (sensor) {
        sensors_.push_back(std::move(sensor));
    }
}

std::shared_ptr<DeviceSensor> DeviceAssembly::get_sensor(const std::string& sensor_id) const {
    for (const auto& s : sensors_) {
        if (s->sensor_id() == sensor_id) return s;
    }
    return nullptr;
}

DeviceDiagnosticsReport DeviceAssembly::run_diagnostics() const {
    DeviceDiagnosticsReport report;
    report.valid = true;

    // 1. Calculate cumulative mass and center of gravity
    double total_mass = 0.0;
    Vector3d weighted_com{0.0, 0.0, 0.0};

    for (const auto& part : parts_) {
        const double m = part.compute_mass();
        total_mass += m;
        weighted_com = weighted_com + (part.local_position * m);

        // Check for floating ports
        for (const auto& port : part.ports) {
            if (port.connected_net.empty()) {
                report.floating_ports.push_back(part.part_id + ":" + port.port_id);
            }
        }
    }

    report.total_mass = total_mass;
    if (total_mass > 1e-9) {
        report.center_of_mass = weighted_com * (1.0 / total_mass);
    }

    // 2. Electrical continuity check
    for (const auto& path : conductor_paths_) {
        if (path.waypoints.size() < 2 || path.length() < 1e-9) {
            report.has_continuity = false;
            report.open_circuit_nets.push_back(path.net_name);
            report.valid = false;
        }
    }

    return report;
}

std::vector<MeshLimitationIssue> DeviceAssembly::audit_geometric_fidelity(double solver_mesh_min_edge) const {
    std::vector<MeshLimitationIssue> issues;

    for (const auto& part : parts_) {
        // Audit primitive dimensions
        const double min_dim = std::min({part.primitive.dimensions.x, part.primitive.dimensions.y, part.primitive.dimensions.z});
        if (min_dim < solver_mesh_min_edge && part.primitive.compute_volume() > 0.0) {
            issues.push_back(MeshLimitationIssue{
                .part_id = part.part_id,
                .feature_name = "Primitive Thickness/Width",
                .feature_size = min_dim,
                .solver_mesh_resolution = solver_mesh_min_edge,
                .warning_message = "Part feature size is smaller than solver mesh resolution; feature will be under-resolved."
            });
        }

        // Audit layered stackups
        if (part.stackup.has_value()) {
            for (const auto& l : part.stackup->layers) {
                if (l.thickness < solver_mesh_min_edge) {
                    issues.push_back(MeshLimitationIssue{
                        .part_id = part.part_id,
                        .feature_name = "Layer:" + l.name,
                        .feature_size = l.thickness,
                        .solver_mesh_resolution = solver_mesh_min_edge,
                        .warning_message = "Layer thickness below solver 3D mesh resolution; recommend 1D/shell homogenization."
                    });
                }
            }
        }
    }

    // Audit traces
    for (const auto& path : conductor_paths_) {
        if (path.width < solver_mesh_min_edge || path.thickness < solver_mesh_min_edge) {
            issues.push_back(MeshLimitationIssue{
                .part_id = path.trace_id,
                .feature_name = "Trace:" + path.net_name,
                .feature_size = std::min(path.width, path.thickness),
                .solver_mesh_resolution = solver_mesh_min_edge,
                .warning_message = "Conductor cross-section below 3D volume mesh threshold; modeled via 1D equivalent circuit."
            });
        }
    }

    return issues;
}

void DeviceAssembly::add_variant(const DeviceVariant& variant) {
    variants_.push_back(variant);
}

} // namespace shunya::core::domain::device
