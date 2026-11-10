#pragma once

#include "shunya/cad3d/solid_mesh.hpp"

#include <string>
#include <string_view>
#include <unordered_map>

namespace shunya::cad3d {

struct PbrMaterialParams {
    std::string name{"default_pbr"};
    Vec3d albedo{0.8, 0.8, 0.8};
    double metallic{0.0};
    double roughness{0.5};
    double ao{1.0};
    Vec3d emissive{0.0, 0.0, 0.0};
};

/**
 * @brief Curated engineering & scientific PBR material presets (metals & dielectrics).
 */
class PbrMaterialLibrary {
public:
    static PbrMaterialLibrary create_default() {
        PbrMaterialLibrary lib;
        lib.register_material({"copper",      {0.955, 0.637, 0.538}, 1.0, 0.25, 1.0, {0, 0, 0}});
        lib.register_material({"aluminum",    {0.913, 0.921, 0.925}, 1.0, 0.30, 1.0, {0, 0, 0}});
        lib.register_material({"gold",        {1.000, 0.766, 0.336}, 1.0, 0.18, 1.0, {0, 0, 0}});
        lib.register_material({"steel",       {0.560, 0.570, 0.580}, 1.0, 0.40, 1.0, {0, 0, 0}});
        lib.register_material({"abs_plastic", {0.150, 0.450, 0.850}, 0.0, 0.45, 1.0, {0, 0, 0}});
        lib.register_material({"fr4_pcb",     {0.050, 0.320, 0.140}, 0.0, 0.55, 1.0, {0, 0, 0}});
        return lib;
    }

    void register_material(PbrMaterialParams mat) {
        auto key = mat.name;
        materials_[std::move(key)] = std::move(mat);
    }

    [[nodiscard]] const PbrMaterialParams* find(std::string_view name) const noexcept {
        auto it = materials_.find(std::string(name));
        return (it != materials_.end()) ? &it->second : nullptr;
    }

    [[nodiscard]] size_t count() const noexcept { return materials_.size(); }

private:
    std::unordered_map<std::string, PbrMaterialParams> materials_;
};

} // namespace shunya::cad3d
