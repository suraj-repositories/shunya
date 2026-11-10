#pragma once

#include "shunya/cad2d/bezier_spline.hpp"
#include "shunya/cad2d/block_manager.hpp"
#include "shunya/cad2d/construction_layer.hpp"
#include "shunya/cad2d/curve_2d.hpp"
#include "shunya/cad2d/layer_manager.hpp"

#include <string>
#include <string_view>

namespace shunya::cad2d {

enum class DxfVersion {
    R12,
    AC1015_2000
};

enum class DxfUnit : int {
    Unitless = 0,
    Inches = 1,
    Millimeters = 4,
    Centimeters = 5,
    Meters = 6
};

/**
 * @brief Returns scale factor to convert a value in `unit` to millimeters.
 */
[[nodiscard]] constexpr double dxf_unit_to_mm(DxfUnit unit) noexcept {
    switch (unit) {
        case DxfUnit::Inches:      return 25.4;
        case DxfUnit::Centimeters: return 10.0;
        case DxfUnit::Meters:      return 1000.0;
        case DxfUnit::Millimeters:
        case DxfUnit::Unitless:
        default:                   return 1.0;
    }
}

struct DxfDocument {
    DxfVersion version{DxfVersion::AC1015_2000};
    DxfUnit units{DxfUnit::Millimeters};
    SketchDocument2D sketch;
    LayerManager layers;
    BlockManager blocks;
};

/**
 * @brief Serializes 2D CAD documents, layers, and blocks to AutoCAD ASCII DXF R12 / 2000 format.
 */
class DxfWriter {
public:
    [[nodiscard]] static std::string write(
        const SketchDocument2D& sketch,
        const LayerManager& layers,
        const BlockManager& blocks,
        DxfVersion version = DxfVersion::AC1015_2000,
        DxfUnit units = DxfUnit::Millimeters
    );

    [[nodiscard]] static bool write_file(
        std::string_view file_path,
        const SketchDocument2D& sketch,
        const LayerManager& layers,
        const BlockManager& blocks,
        DxfVersion version = DxfVersion::AC1015_2000,
        DxfUnit units = DxfUnit::Millimeters
    );
};

/**
 * @brief Parses AutoCAD ASCII DXF R12 / 2000 files (TABLES/LAYER, BLOCKS, ENTITIES) into DxfDocument.
 */
class DxfParser {
public:
    [[nodiscard]] static bool parse(
        std::string_view dxf_content,
        DxfDocument& out_doc,
        DxfUnit target_unit = DxfUnit::Millimeters
    );

    [[nodiscard]] static bool parse_file(
        std::string_view file_path,
        DxfDocument& out_doc,
        DxfUnit target_unit = DxfUnit::Millimeters
    );
};

} // namespace shunya::cad2d
