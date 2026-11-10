#include "tests/test_framework.hpp"

#include "shunya/cad2d/dxf_io.hpp"
#include "shunya/cad2d/svg_io.hpp"
#include "shunya/core/base/math.hpp"

#include <cmath>
#include <memory>
#include <string>
#include <vector>

using namespace shunya::cad2d;
using shunya::core::base::math::PI;

namespace {

bool is_approx(double a, double b, double eps = 1e-6) {
    return std::abs(a - b) <= eps;
}

} // namespace

TEST_CASE("DXF R12/2000 Round-Trip: 20 mechanical test drawings with layers, blocks, and curves") {
    for (int i = 0; i < 20; ++i) {
        SketchDocument2D sketch;
        LayerManager layers;
        BlockManager blocks;

        layers.add_layer("Outline", "0", {"#61afef", 0.35, LinePattern::Solid});
        layers.add_layer("Centerlines", "0", {"#e06c75", 0.18, LinePattern::Center});

        const double w = 40.0 + static_cast<double>(i) * 5.0;
        const double h = 25.0 + static_cast<double>(i) * 3.0;
        const double r = 4.0 + static_cast<double>(i) * 0.25;

        // Closed outer contour
        sketch.add_entity(
            std::make_shared<Polyline2D>(
                std::vector<Point2D>{{0.0, 0.0}, {w, 0.0}, {w, h}, {0.0, h}}, true),
            false,
            "Outline"
        );
        // Center bore circle
        sketch.add_entity(std::make_shared<Circle2D>(Point2D{w * 0.5, h * 0.5}, r), false, "Outline");
        // Arc slot & centerline & cubic spline
        sketch.add_entity(std::make_shared<Arc2D>(Point2D{10.0, 10.0}, 5.0, 0.0, PI * 0.5), false, "Outline");
        sketch.add_entity(std::make_shared<LineSegment2D>(Point2D{-5.0, h * 0.5}, Point2D{w + 5.0, h * 0.5}), true, "Centerlines");
        sketch.add_entity(
            std::make_shared<BezierCubic2D>(Point2D{0.0, 0.0}, Point2D{10.0, h}, Point2D{w - 10.0, 0.0}, Point2D{w, h}),
            false,
            "Outline"
        );

        // Add reusable fastener block & place 2 instances
        BlockDefinition hole_blk;
        hole_blk.name = "M4_Hole";
        hole_blk.items.push_back({std::make_shared<Circle2D>(Point2D{0.0, 0.0}, 2.1), "0", false});
        REQUIRE(blocks.create_block(hole_blk));
        REQUIRE(!blocks.place_instance("M4_Hole", Point2D{5.0, 5.0}, 0.0, 1.0, 1.0, "Outline").empty());
        REQUIRE(!blocks.place_instance("M4_Hole", Point2D{w - 5.0, h - 5.0}, 0.0, 1.0, 1.0, "Outline").empty());

        const DxfVersion ver = (i % 2 == 0) ? DxfVersion::AC1015_2000 : DxfVersion::R12;
        const std::string dxf = DxfWriter::write(sketch, layers, blocks, ver, DxfUnit::Millimeters);
        REQUIRE(dxf.find("ENTITIES") != std::string::npos);
        REQUIRE(dxf.find("EOF") != std::string::npos);

        DxfDocument imported;
        REQUIRE(DxfParser::parse(dxf, imported, DxfUnit::Millimeters));
        REQUIRE(imported.version == ver);
        REQUIRE(imported.sketch.entities().size() == 5);
        REQUIRE(imported.layers.find_layer("Outline") != nullptr);
        REQUIRE(imported.layers.find_layer("Centerlines") != nullptr);
        REQUIRE(imported.layers.find_layer("Centerlines")->style.pattern == LinePattern::Center);
        REQUIRE(imported.blocks.find_block("M4_Hole") != nullptr);
        REQUIRE(imported.blocks.instance_count() == 2);

        // Verify exact coordinate tolerance on circle & spline endpoints
        const auto& c_orig = static_cast<const Circle2D&>(*sketch.entities()[1].curve);
        const auto& c_imp = static_cast<const Circle2D&>(*imported.sketch.entities()[1].curve);
        REQUIRE(is_approx(c_orig.center.x, c_imp.center.x, 1e-6));
        REQUIRE(is_approx(c_orig.center.y, c_imp.center.y, 1e-6));
        REQUIRE(is_approx(c_orig.radius, c_imp.radius, 1e-6));

        const auto& bez_imp = static_cast<const BezierCubic2D&>(*imported.sketch.entities()[4].curve);
        REQUIRE(is_approx(bez_imp.p3.x, w, 1e-6));
        REQUIRE(is_approx(bez_imp.p3.y, h, 1e-6));
    }
}

TEST_CASE("DXFParser converts Inches ($INSUNITS=1) cleanly to target Millimeters") {
    SketchDocument2D sketch;
    LayerManager layers;
    BlockManager blocks;
    sketch.add_entity(std::make_shared<LineSegment2D>(Point2D{0.0, 0.0}, Point2D{2.0, 1.0}), false, "0");

    const std::string dxf_in = DxfWriter::write(sketch, layers, blocks, DxfVersion::AC1015_2000, DxfUnit::Inches);
    DxfDocument doc_mm;
    REQUIRE(DxfParser::parse(dxf_in, doc_mm, DxfUnit::Millimeters));
    REQUIRE(doc_mm.sketch.entities().size() == 1);
    const auto& seg = static_cast<const LineSegment2D&>(*doc_mm.sketch.entities()[0].curve);
    REQUIRE(is_approx(seg.p1.x, 50.8, 1e-6));
    REQUIRE(is_approx(seg.p1.y, 25.4, 1e-6));
}

TEST_CASE("SVG 1.1 Export & Import preserves 1:1 physical mm/in scale, layers, and Bezier loops") {
    SketchDocument2D sketch;
    LayerManager layers;
    layers.add_layer("Profile", "0", {"#98c379", 0.30, LinePattern::Solid});
    layers.add_layer("Guides", "0", {"#e5c07b", 0.15, LinePattern::Dashed});
    layers.set_visible("Guides", false);

    // Closed Bezier + polyline + arc + circle
    sketch.add_entity(
        std::make_shared<Polyline2D>(std::vector<Point2D>{{0.0, 0.0}, {100.0, 0.0}, {100.0, 50.0}, {0.0, 50.0}}, true),
        false,
        "Profile"
    );
    sketch.add_entity(
        std::make_shared<BezierCubic2D>(Point2D{10.0, 10.0}, Point2D{40.0, 45.0}, Point2D{60.0, 5.0}, Point2D{90.0, 40.0}),
        false,
        "Profile"
    );
    sketch.add_entity(std::make_shared<Circle2D>(Point2D{50.0, 25.0}, 12.5), false, "Profile");
    sketch.add_entity(std::make_shared<LineSegment2D>(Point2D{0.0, 25.0}, Point2D{100.0, 25.0}), true, "Guides");

    SvgExportOptions opts;
    opts.unit = SvgLengthUnit::Millimeters;
    opts.padding_mm = 5.0;
    const std::string svg_mm = SvgExchange::export_svg(sketch, layers, nullptr, opts);

    REQUIRE(svg_mm.find("width=\"110.000000mm\"") != std::string::npos);
    REQUIRE(svg_mm.find("height=\"60.000000mm\"") != std::string::npos);
    REQUIRE(svg_mm.find("data-layer=\"Profile\"") != std::string::npos);
    REQUIRE(svg_mm.find("display=\"none\"") != std::string::npos);

    SvgDocument imp;
    REQUIRE(SvgExchange::import_svg(svg_mm, imp));
    REQUIRE(is_approx(imp.width_mm, 110.0, 1e-4));
    REQUIRE(is_approx(imp.height_mm, 60.0, 1e-4));
    REQUIRE(imp.sketch.entities().size() == 4);
    REQUIRE(imp.layers.find_layer("Guides") != nullptr);
    REQUIRE(!imp.layers.is_effectively_visible("Guides"));

    // Verify cubic Bezier & Inches export scaling round-trip
    opts.unit = SvgLengthUnit::Inches;
    const std::string svg_in = SvgExchange::export_svg(sketch, layers, nullptr, opts);
    REQUIRE(svg_in.find("in\"") != std::string::npos);
    SvgDocument imp_from_in;
    REQUIRE(SvgExchange::import_svg(svg_in, imp_from_in));
    REQUIRE(is_approx(imp_from_in.width_mm, 110.0, 1e-3));
    REQUIRE(imp_from_in.sketch.entities().size() == 4);

    const Circle2D* circ_in = nullptr;
    const BezierCubic2D* bez_in = nullptr;
    for (const auto& e : imp_from_in.sketch.entities()) {
        if (e.curve && e.curve->type() == CurveType2D::Circle) {
            circ_in = static_cast<const Circle2D*>(e.curve.get());
            REQUIRE(e.layer == "Profile");
        } else if (e.curve && e.curve->type() == CurveType2D::BezierCubic) {
            bez_in = static_cast<const BezierCubic2D*>(e.curve.get());
            REQUIRE(e.layer == "Profile");
        }
    }
    REQUIRE(circ_in != nullptr);
    REQUIRE(bez_in != nullptr);
    REQUIRE(is_approx(circ_in->radius, 12.5, 1e-4));
    REQUIRE(is_approx(bez_in->p3.x, 90.0, 1e-3));
    REQUIRE(is_approx(bez_in->p3.y, 40.0, 1e-3));
}
