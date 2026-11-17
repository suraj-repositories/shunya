#pragma once

#include "shunya/cad3d/solid_mesh.hpp"
#include "shunya/core/base/math.hpp"

#include <cmath>
#include <cstdint>
#include <map>
#include <numbers>
#include <string>
#include <variant>
#include <vector>

namespace shunya::cad3d {

using Vec2d = core::base::math::Vec2;

enum class PrimitiveType {
    Cube,
    Sphere,
    UvSphere,
    Icosphere,
    Cylinder,
    Cone,
    Torus,
    Plane,
    Circle,
    Grid,
    Capsule,
    HexBolt,
    Washer,
    Nut,
    Pin
};

enum class CapFillType {
    None,
    TriangleFan,
    Ngon
};

enum class CircleFillType {
    None,
    TriangleFan
};

enum class AlignmentMode {
    World,
    View,
    SurfaceNormal
};

// ============================================================================
// Parameter Structures
// ============================================================================

struct CubeParams {
    double width{1.0};
    double height{1.0};
    double depth{1.0};
    uint32_t subdivisions_x{1};
    uint32_t subdivisions_y{1};
    uint32_t subdivisions_z{1};
};

struct SphereParams {
    double radius{1.0};
    uint32_t subdivisions_u{24};
    uint32_t subdivisions_v{16};
};

struct UvSphereParams {
    double radius{1.0};
    uint32_t segments{32};
    uint32_t rings{16};
};

struct IcosphereParams {
    double radius{1.0};
    uint32_t subdivisions{2};
};

struct CylinderParams {
    double radius{1.0};
    double height{2.0};
    uint32_t radial_segments{32};
    uint32_t height_segments{1};
    CapFillType cap_fill{CapFillType::TriangleFan};
};

struct ConeParams {
    double radius{1.0};
    double height{2.0};
    uint32_t radial_segments{32};
    uint32_t height_segments{1};
    CapFillType cap_fill{CapFillType::TriangleFan};
};

struct TorusParams {
    double major_radius{1.0};
    double minor_radius{0.25};
    uint32_t major_segments{32};
    uint32_t minor_segments{16};
};

struct PlaneParams {
    double width{2.0};
    double height{2.0};
    uint32_t segments_x{1};
    uint32_t segments_y{1};
};

struct CircleParams {
    double radius{1.0};
    uint32_t segments{32};
    CircleFillType fill{CircleFillType::TriangleFan};
};

struct GridParams {
    double width{2.0};
    double height{2.0};
    uint32_t subdivisions_x{10};
    uint32_t subdivisions_y{10};
};

struct CapsuleParams {
    double radius{0.5};
    double cylinder_height{1.5};
    uint32_t radial_segments{32};
    uint32_t rings{8};
};

// Engineering Fastener Parameters (Task 8: PH4-05.01.003)
struct HexBoltParams {
    double nominal_diameter{6.0};   // d (e.g. M6)
    double pitch{1.0};              // P
    double total_length{25.0};       // L
    double shank_length{15.0};      // Ls
    double head_across_flats{10.0}; // s
    double head_height{4.0};        // k
    double washer_face_dia{9.5};    // dw
    double washer_face_height{0.4}; // c
};

struct WasherParams {
    double inner_diameter{6.4};     // d1 (e.g. for M6)
    double outer_diameter{12.0};    // d2
    double thickness{1.6};          // s
    double chamfer{0.2};            // c
};

struct NutParams {
    double nominal_diameter{6.0};   // d (e.g. M6 bore)
    double across_flats{10.0};      // s
    double height{5.0};             // m
    double chamfer{0.5};            // c
};

struct PinParams {
    double diameter{6.0};           // d
    double length{20.0};            // l
    double chamfer_length{0.8};     // c
    bool is_chamfered{true};
};

using PrimitiveParamsVariant = std::variant<
    CubeParams,
    SphereParams,
    UvSphereParams,
    IcosphereParams,
    CylinderParams,
    ConeParams,
    TorusParams,
    PlaneParams,
    CircleParams,
    GridParams,
    CapsuleParams,
    HexBoltParams,
    WasherParams,
    NutParams,
    PinParams
>;

// ============================================================================
// Analytical Properties (Task 10: PH4-05.01.005)
// ============================================================================

struct AnalyticalProperties {
    double volume{0.0};
    double surface_area{0.0};
    BBox3D bbox;
};

struct AnalyticalComparison {
    double analytical_volume{0.0};
    double discrete_volume{0.0};
    double volume_error_percent{0.0};
    double analytical_surface_area{0.0};
    double discrete_surface_area{0.0};
    double surface_area_error_percent{0.0};
};

// ============================================================================
// Topology & Mesh Quality Validation (Task 9: PH4-05.01.004)
// ============================================================================

struct MeshQualityReport {
    bool is_watertight{false};
    bool is_2_manifold{false};
    bool normals_valid{true};
    bool uvs_valid{true};
    int64_t euler_characteristic{0};
    size_t zero_normals_count{0};
    size_t invalid_uvs_count{0};
};

class ParametricMeshValidator {
public:
    [[nodiscard]] static MeshQualityReport validate_topology_and_attributes(
        const SolidMesh3D& mesh,
        bool closed_expected = true
    );
};

// ============================================================================
// Primitive Alignment (Task 1: PH4-05.01.006)
// ============================================================================

class PrimitiveAligner {
public:
    [[nodiscard]] static SolidMesh3D align_to_world(
        const SolidMesh3D& mesh,
        const Point3D& target_origin = {0.0, 0.0, 0.0}
    );

    [[nodiscard]] static SolidMesh3D align_to_view(
        const SolidMesh3D& mesh,
        const Point3D& target_origin,
        const Vec3d& view_direction,
        const Vec3d& view_up = {0.0, 1.0, 0.0}
    );

    [[nodiscard]] static SolidMesh3D align_to_surface_normal(
        const SolidMesh3D& mesh,
        const Point3D& surface_point,
        const Vec3d& surface_normal
    );
};

// ============================================================================
// Parametric Primitive Generator
// ============================================================================

class ParametricPrimitiveGenerator {
public:
    [[nodiscard]] static SolidMesh3D generate_cube(const CubeParams& params = {});
    [[nodiscard]] static SolidMesh3D generate_sphere(const SphereParams& params = {});
    [[nodiscard]] static SolidMesh3D generate_uv_sphere(const UvSphereParams& params = {});
    [[nodiscard]] static SolidMesh3D generate_icosphere(const IcosphereParams& params = {});
    [[nodiscard]] static SolidMesh3D generate_cylinder(const CylinderParams& params = {});
    [[nodiscard]] static SolidMesh3D generate_cone(const ConeParams& params = {});
    [[nodiscard]] static SolidMesh3D generate_torus(const TorusParams& params = {});
    [[nodiscard]] static SolidMesh3D generate_plane(const PlaneParams& params = {});
    [[nodiscard]] static SolidMesh3D generate_circle(const CircleParams& params = {});
    [[nodiscard]] static SolidMesh3D generate_grid(const GridParams& params = {});
    [[nodiscard]] static SolidMesh3D generate_capsule(const CapsuleParams& params = {});

    // Fasteners (PH4-05.01.003)
    [[nodiscard]] static SolidMesh3D generate_hex_bolt(const HexBoltParams& params = {});
    [[nodiscard]] static SolidMesh3D generate_washer(const WasherParams& params = {});
    [[nodiscard]] static SolidMesh3D generate_nut(const NutParams& params = {});
    [[nodiscard]] static SolidMesh3D generate_pin(const PinParams& params = {});

    // Analytical property calculations (PH4-05.01.005)
    [[nodiscard]] static AnalyticalProperties compute_analytical(const CubeParams& p);
    [[nodiscard]] static AnalyticalProperties compute_analytical(const SphereParams& p);
    [[nodiscard]] static AnalyticalProperties compute_analytical(const UvSphereParams& p);
    [[nodiscard]] static AnalyticalProperties compute_analytical(const IcosphereParams& p);
    [[nodiscard]] static AnalyticalProperties compute_analytical(const CylinderParams& p);
    [[nodiscard]] static AnalyticalProperties compute_analytical(const ConeParams& p);
    [[nodiscard]] static AnalyticalProperties compute_analytical(const TorusParams& p);
    [[nodiscard]] static AnalyticalProperties compute_analytical(const PlaneParams& p);
    [[nodiscard]] static AnalyticalProperties compute_analytical(const CircleParams& p);
    [[nodiscard]] static AnalyticalProperties compute_analytical(const GridParams& p);
    [[nodiscard]] static AnalyticalProperties compute_analytical(const CapsuleParams& p);
    [[nodiscard]] static AnalyticalProperties compute_analytical(const HexBoltParams& p);
    [[nodiscard]] static AnalyticalProperties compute_analytical(const WasherParams& p);
    [[nodiscard]] static AnalyticalProperties compute_analytical(const NutParams& p);
    [[nodiscard]] static AnalyticalProperties compute_analytical(const PinParams& p);

    [[nodiscard]] static AnalyticalComparison compare_analytical_vs_discrete(
        const SolidMesh3D& mesh,
        const AnalyticalProperties& analytical
    );

    // Standard Fastener Presets
    [[nodiscard]] static HexBoltParams iso4014_hex_bolt(const std::string& size_label, double length = 30.0);
    [[nodiscard]] static WasherParams din125_washer(const std::string& size_label);
    [[nodiscard]] static NutParams din934_nut(const std::string& size_label);
    [[nodiscard]] static PinParams iso8734_dowel_pin(double diameter, double length);
};

// ============================================================================
// Non-Destructive Parametric Primitive Instance (Task 7: PH4-05.01.002)
// ============================================================================

class ParametricPrimitive {
public:
    explicit ParametricPrimitive(PrimitiveType type);
    ParametricPrimitive(PrimitiveType type, PrimitiveParamsVariant params);

    [[nodiscard]] PrimitiveType type() const noexcept { return type_; }
    [[nodiscard]] const PrimitiveParamsVariant& params() const noexcept { return params_; }
    [[nodiscard]] PrimitiveParamsVariant& params() noexcept { is_dirty_ = true; return params_; }

    void set_params(PrimitiveParamsVariant new_params);

    // Generic parameter inspection and non-destructive editing
    [[nodiscard]] bool has_property(const std::string& key) const;
    [[nodiscard]] double get_double_property(const std::string& key, double default_val = 0.0) const;
    void set_double_property(const std::string& key, double val);
    [[nodiscard]] uint32_t get_uint_property(const std::string& key, uint32_t default_val = 0) const;
    void set_uint_property(const std::string& key, uint32_t val);

    [[nodiscard]] bool is_dirty() const noexcept { return is_dirty_; }
    void mark_dirty() noexcept { is_dirty_ = true; }

    /**
     * @brief Rebuilds the underlying SolidMesh3D deterministically if dirty.
     */
    const SolidMesh3D& mesh();

    /**
     * @brief Computes exact analytical properties from current parameters.
     */
    [[nodiscard]] AnalyticalProperties analytical_properties() const;

private:
    PrimitiveType type_{PrimitiveType::Cube};
    PrimitiveParamsVariant params_{CubeParams{}};
    SolidMesh3D cached_mesh_{};
    bool is_dirty_{true};
};

} // namespace shunya::cad3d
