#pragma once

#include "shunya/core/domain/data_exchange.hpp"
#include "shunya/pcb/board_outline.hpp"
#include "shunya/pcb/board_stackup.hpp"
#include "shunya/pcb/drc_engine.hpp"
#include "shunya/pcb/push_shove_engine.hpp"
#include "shunya/pcb/step_mesh_assembler.hpp"

#include <string>
#include <vector>

namespace shunya::pcb {

struct Pcb3DAssemblyResult {
    core::domain::TriangleMesh watertight_mesh;
    double expected_substrate_volume_mm3{0.0};
    double mesh_volume_mm3{0.0};
    core::base::math::Vector3<double> bbox_min_mm;
    core::base::math::Vector3<double> bbox_max_mm;
};

/**
 * @brief Assembles 3D PCB solid CAD models and exports Wavefront OBJ and ISO-10303-21 STEP ASCII.
 */
class Pcb3DExporter {
public:
    [[nodiscard]] static Pcb3DAssemblyResult assemble_3d_model(
        const BoardOutline& outline,
        const BoardStackupManager& stackup,
        const std::vector<PcbTrackSegment>& tracks,
        const std::vector<DrcPad>& pads,
        const std::vector<PcbComponent3DBody>& bodies = {});

    [[nodiscard]] static std::string export_obj(const Pcb3DAssemblyResult& assembly);
    [[nodiscard]] static std::string export_step_ap214(
        std::string_view product_name,
        const Pcb3DAssemblyResult& assembly);
};

} // namespace shunya::pcb
