#pragma once

#include "shunya/cad3d/half_edge_mesh.hpp"
#include "shunya/cad3d/mesh_decimator.hpp"
#include "shunya/cad3d/solid_mesh.hpp"
#include "shunya/core/base/math.hpp"

#include <cmath>
#include <cstdint>
#include <memory>
#include <numbers>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace shunya::rig3d {

using namespace cad3d;

enum class ModifierType {
    Mirror,
    Array,
    Solidify,
    Subsurf,
    Decimate,
    Custom
};

/**
 * @brief Abstract base class for non-destructive mesh modifier stages.
 */
class ModifierStage {
public:
    virtual ~ModifierStage() = default;

    [[nodiscard]] virtual ModifierType type() const noexcept = 0;
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;

    [[nodiscard]] bool is_enabled() const noexcept { return enabled_; }
    void set_enabled(bool enabled) noexcept { enabled_ = enabled; }

    [[nodiscard]] virtual SolidMesh3D evaluate(const SolidMesh3D& input) const = 0;
    [[nodiscard]] virtual HalfEdgeMesh evaluate(const HalfEdgeMesh& input) const;

protected:
    bool enabled_{true};
};

// =============================================================================
// Mirror Modifier (PH4-11.01.002)
// =============================================================================
struct MirrorModifierConfig {
    bool mirror_x{true};
    bool mirror_y{false};
    bool mirror_z{false};
    bool clip{true};                  ///< Clamp vertices crossing the mirror plane
    bool merge{true};                 ///< Weld vertices within threshold of mirror plane
    double merge_threshold{0.005};    ///< Merge distance tolerance
    double mirror_plane_offset{0.0};  ///< Plane offset coordinate
};

class MirrorModifier : public ModifierStage {
public:
    explicit MirrorModifier(MirrorModifierConfig config = {}) : config_(config) {}

    [[nodiscard]] ModifierType type() const noexcept override { return ModifierType::Mirror; }
    [[nodiscard]] std::string_view name() const noexcept override { return "Mirror"; }

    [[nodiscard]] const MirrorModifierConfig& config() const noexcept { return config_; }
    void set_config(const MirrorModifierConfig& cfg) noexcept { config_ = cfg; }

    [[nodiscard]] SolidMesh3D evaluate(const SolidMesh3D& input) const override;

private:
    MirrorModifierConfig config_;
};

// =============================================================================
// Array Modifier (PH4-11.01.003)
// =============================================================================
struct ArrayModifierConfig {
    uint32_t count{2};                          ///< Total number of copies (including original)
    Vec3d constant_offset{0.0, 0.0, 0.0};       ///< Fixed translation per copy
    Vec3d relative_offset{1.0, 0.0, 0.0};       ///< Offset proportional to mesh bounding box
    bool use_relative_offset{true};             ///< Enable relative offset
    bool use_constant_offset{false};            ///< Enable constant offset
    bool use_circular{false};                   ///< Circular radial array around axis
    double circular_angle_rad{0.0};             ///< Total angular span or angle step
    Vec3d circular_axis{0.0, 0.0, 1.0};         ///< Rotation axis for circular array
    Point3D circular_center{0.0, 0.0, 0.0};     ///< Center of rotation
    bool merge_endpoints{false};                ///< Merge first and last copies if full 360 loop
    double merge_threshold{0.005};
};

class ArrayModifier : public ModifierStage {
public:
    explicit ArrayModifier(ArrayModifierConfig config = {}) : config_(config) {}

    [[nodiscard]] ModifierType type() const noexcept override { return ModifierType::Array; }
    [[nodiscard]] std::string_view name() const noexcept override { return "Array"; }

    [[nodiscard]] const ArrayModifierConfig& config() const noexcept { return config_; }
    void set_config(const ArrayModifierConfig& cfg) noexcept { config_ = cfg; }

    [[nodiscard]] SolidMesh3D evaluate(const SolidMesh3D& input) const override;

private:
    ArrayModifierConfig config_;
};

// =============================================================================
// Solidify Modifier (PH4-11.01.004)
// =============================================================================
struct SolidifyModifierConfig {
    double thickness{0.05};           ///< Skin shell thickness
    double offset{-1.0};              ///< -1.0 = inward, 0.0 = centered, +1.0 = outward
    bool fill_rim{true};              ///< Seal boundary edges with side rim quads
};

class SolidifyModifier : public ModifierStage {
public:
    explicit SolidifyModifier(SolidifyModifierConfig config = {}) : config_(config) {}

    [[nodiscard]] ModifierType type() const noexcept override { return ModifierType::Solidify; }
    [[nodiscard]] std::string_view name() const noexcept override { return "Solidify"; }

    [[nodiscard]] const SolidifyModifierConfig& config() const noexcept { return config_; }
    void set_config(const SolidifyModifierConfig& cfg) noexcept { config_ = cfg; }

    [[nodiscard]] SolidMesh3D evaluate(const SolidMesh3D& input) const override;

private:
    SolidifyModifierConfig config_;
};

// =============================================================================
// Subdivision Surface (Subsurf) Modifier (PH4-11.01.005)
// =============================================================================
struct SubsurfModifierConfig {
    uint32_t levels{1};               ///< Subdivision iterations (1..4)
    double edge_crease{0.0};          ///< Edge sharpness weight [0.0 = smooth, 1.0 = sharp]
};

class SubsurfModifier : public ModifierStage {
public:
    explicit SubsurfModifier(SubsurfModifierConfig config = {}) : config_(config) {}

    [[nodiscard]] ModifierType type() const noexcept override { return ModifierType::Subsurf; }
    [[nodiscard]] std::string_view name() const noexcept override { return "Subdivision Surface"; }

    [[nodiscard]] const SubsurfModifierConfig& config() const noexcept { return config_; }
    void set_config(const SubsurfModifierConfig& cfg) noexcept { config_ = cfg; }

    [[nodiscard]] SolidMesh3D evaluate(const SolidMesh3D& input) const override;

private:
    SubsurfModifierConfig config_;
};

// =============================================================================
// Decimate Modifier (PH4-11.01.006)
// =============================================================================
class DecimateModifier : public ModifierStage {
public:
    explicit DecimateModifier(DecimationParams params = {}) : params_(params) {}

    [[nodiscard]] ModifierType type() const noexcept override { return ModifierType::Decimate; }
    [[nodiscard]] std::string_view name() const noexcept override { return "Decimate"; }

    [[nodiscard]] const DecimationParams& params() const noexcept { return params_; }
    void set_params(const DecimationParams& p) noexcept { params_ = p; }

    [[nodiscard]] SolidMesh3D evaluate(const SolidMesh3D& input) const override;

private:
    DecimationParams params_;
};

// =============================================================================
// Modifier Stack Pipeline (PH4-11.01.001 & PH4-11.01.007)
// =============================================================================
class ModifierStack {
public:
    ModifierStack() = default;

    void add_modifier(std::shared_ptr<ModifierStage> stage);
    bool remove_modifier(size_t index);
    bool move_up(size_t index);
    bool move_down(size_t index);
    void clear() noexcept;

    [[nodiscard]] size_t size() const noexcept { return stages_.size(); }
    [[nodiscard]] bool empty() const noexcept { return stages_.empty(); }
    [[nodiscard]] std::shared_ptr<ModifierStage> get(size_t index) const;

    [[nodiscard]] SolidMesh3D evaluate(const SolidMesh3D& base_mesh) const;
    [[nodiscard]] HalfEdgeMesh evaluate(const HalfEdgeMesh& base_mesh) const;

    /// PH4-11.01.007: Apply Modifier Operation (bakes permanently into base mesh)
    bool apply_modifier(size_t index, SolidMesh3D& inout_mesh);
    bool apply_modifier(size_t index, HalfEdgeMesh& inout_mesh);
    void apply_all(SolidMesh3D& inout_mesh);
    void apply_all(HalfEdgeMesh& inout_mesh);

private:
    std::vector<std::shared_ptr<ModifierStage>> stages_;
};

} // namespace shunya::rig3d
