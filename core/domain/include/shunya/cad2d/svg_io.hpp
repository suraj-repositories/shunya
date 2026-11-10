#pragma once

#include "shunya/cad2d/bezier_spline.hpp"
#include "shunya/cad2d/block_manager.hpp"
#include "shunya/cad2d/construction_layer.hpp"
#include "shunya/cad2d/curve_2d.hpp"
#include "shunya/cad2d/layer_manager.hpp"

#include <string>
#include <string_view>

namespace shunya::cad2d {

enum class SvgLengthUnit {
    Millimeters,
    Inches,
    Centimeters,
    Pixels
};

struct SvgExportOptions {
    SvgLengthUnit unit{SvgLengthUnit::Millimeters};
    double padding_mm{5.0};
    bool flip_y{true};
    bool include_blocks{true};
};

struct SvgDocument {
    SvgLengthUnit unit{SvgLengthUnit::Millimeters};
    double width_mm{100.0};
    double height_mm{100.0};
    SketchDocument2D sketch;
    LayerManager layers;
};

/**
 * @brief SVG 1.1 exchange providing 1:1 physical scale export and path/layer-preserving import.
 */
class SvgExchange {
public:
    [[nodiscard]] static std::string export_svg(
        const SketchDocument2D& sketch,
        const LayerManager& layers,
        const BlockManager* blocks = nullptr,
        const SvgExportOptions& options = {}
    );

    [[nodiscard]] static bool export_svg_file(
        std::string_view file_path,
        const SketchDocument2D& sketch,
        const LayerManager& layers,
        const BlockManager* blocks = nullptr,
        const SvgExportOptions& options = {}
    );

    [[nodiscard]] static bool import_svg(
        std::string_view svg_xml,
        SvgDocument& out_doc
    );

    [[nodiscard]] static bool import_svg_file(
        std::string_view file_path,
        SvgDocument& out_doc
    );
};

} // namespace shunya::cad2d
