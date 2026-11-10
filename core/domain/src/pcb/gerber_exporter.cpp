#include "shunya/pcb/gerber_exporter.hpp"

#include <cmath>
#include <iomanip>
#include <map>
#include <sstream>

namespace shunya::pcb {

namespace {

long long to_gerber_coord(double mm) noexcept {
    // %FSLAX36Y36*% -> 6 decimal places in millimeters (1e6 scale)
    return std::llround(mm * 1000000.0);
}

std::string format_xy(const core::base::math::Vector2<double>& p) {
    return "X" + std::to_string(to_gerber_coord(p.x)) +
           "Y" + std::to_string(to_gerber_coord(p.y));
}

} // namespace

std::string GerberExporter::export_copper_layer(
    std::string_view layer_name,
    int layer_index,
    const std::vector<PcbTrackSegment>& tracks,
    const std::vector<DrcPad>& pads,
    const std::vector<PcbVia>& vias) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(4);
    out << "%TF.GenerationSoftware,Shunya,CAD,2.0*%\n";
    out << "%TF.FileFunction,Copper,L" << (layer_index + 1) << "," << layer_name << "*%\n";
    out << "%FSLAX36Y36*%\n";
    out << "%MOMM*%\n";
    out << "%LPD*%\n";

    std::map<std::string, int> aperture_ids;
    int next_d = 10;
    auto reg_circle = [&](double dia) {
        std::ostringstream key;
        key << "C," << std::fixed << std::setprecision(4) << dia;
        auto it = aperture_ids.find(key.str());
        if (it != aperture_ids.end()) return it->second;
        int d = next_d++;
        aperture_ids[key.str()] = d;
        out << "%ADD" << d << key.str() << "*%\n";
        return d;
    };
    auto reg_rect = [&](double w, double h) {
        std::ostringstream key;
        key << "R," << std::fixed << std::setprecision(4) << w << "X" << h;
        auto it = aperture_ids.find(key.str());
        if (it != aperture_ids.end()) return it->second;
        int d = next_d++;
        aperture_ids[key.str()] = d;
        out << "%ADD" << d << key.str() << "*%\n";
        return d;
    };

    for (const auto& t : tracks) {
        if (t.layer_index == layer_index) (void)reg_circle(t.width_mm);
    }
    for (const auto& p : pads) {
        if (p.layer_index == layer_index) (void)reg_rect(p.width_mm, p.height_mm);
    }
    for (const auto& v : vias) {
        if (v.spans_layer(layer_index)) (void)reg_circle(v.pad_diameter_mm);
    }

    out << "G01*\n";
    // Emit tracks
    for (const auto& t : tracks) {
        if (t.layer_index != layer_index) continue;
        const int d = reg_circle(t.width_mm);
        out << "D" << d << "*\n";
        out << format_xy(t.start_mm) << "D02*\n";
        out << format_xy(t.end_mm) << "D01*\n";
    }
    // Flash pads
    for (const auto& p : pads) {
        if (p.layer_index != layer_index) continue;
        const int d = reg_rect(p.width_mm, p.height_mm);
        out << "D" << d << "*\n";
        out << format_xy(p.center_mm) << "D03*\n";
    }
    // Flash vias
    for (const auto& v : vias) {
        if (!v.spans_layer(layer_index)) continue;
        const int d = reg_circle(v.pad_diameter_mm);
        out << "D" << d << "*\n";
        out << format_xy(v.center_mm) << "D03*\n";
    }

    out << "M02*\n";
    return out.str();
}

std::string GerberExporter::export_board_outline(const BoardOutline& outline) {
    std::ostringstream out;
    out << "%TF.GenerationSoftware,Shunya,CAD,2.0*%\n";
    out << "%TF.FileFunction,Profile,NP*%\n";
    out << "%FSLAX36Y36*%\n";
    out << "%MOMM*%\n";
    out << "%LPD*%\n";
    out << "%ADD10C,0.0500*%\n";
    out << "G01*\nD10*\n";
    const auto& poly = outline.outer_contour();
    if (!poly.empty()) {
        out << format_xy(poly.front()) << "D02*\n";
        for (size_t i = 1; i < poly.size(); ++i) {
            out << format_xy(poly[i]) << "D01*\n";
        }
        out << format_xy(poly.front()) << "D01*\n";
    }
    out << "M02*\n";
    return out.str();
}

} // namespace shunya::pcb
