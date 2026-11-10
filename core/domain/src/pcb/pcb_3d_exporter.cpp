#include "shunya/pcb/pcb_3d_exporter.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace shunya::pcb {

namespace {

void append_box_mesh(core::domain::TriangleMesh& mesh,
                     double x0, double y0, double z0,
                     double x1, double y1, double z1) {
    const uint32_t b = static_cast<uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back({x0, y0, z0}); // 0
    mesh.vertices.push_back({x1, y0, z0}); // 1
    mesh.vertices.push_back({x1, y1, z0}); // 2
    mesh.vertices.push_back({x0, y1, z0}); // 3
    mesh.vertices.push_back({x0, y0, z1}); // 4
    mesh.vertices.push_back({x1, y0, z1}); // 5
    mesh.vertices.push_back({x1, y1, z1}); // 6
    mesh.vertices.push_back({x0, y1, z1}); // 7
    for (int i = 0; i < 8; ++i) mesh.normals.push_back({0.0, 0.0, (i < 4) ? -1.0 : 1.0});

    // Bottom (-Z)
    mesh.faces.push_back({b + 0, b + 2, b + 1});
    mesh.faces.push_back({b + 0, b + 3, b + 2});
    // Top (+Z)
    mesh.faces.push_back({b + 4, b + 5, b + 6});
    mesh.faces.push_back({b + 4, b + 6, b + 7});
    // Sides
    mesh.faces.push_back({b + 0, b + 1, b + 5});
    mesh.faces.push_back({b + 0, b + 5, b + 4});
    mesh.faces.push_back({b + 1, b + 2, b + 6});
    mesh.faces.push_back({b + 1, b + 6, b + 5});
    mesh.faces.push_back({b + 2, b + 3, b + 7});
    mesh.faces.push_back({b + 2, b + 7, b + 6});
    mesh.faces.push_back({b + 3, b + 0, b + 4});
    mesh.faces.push_back({b + 3, b + 4, b + 7});
}

} // namespace

Pcb3DAssemblyResult Pcb3DExporter::assemble_3d_model(
    const BoardOutline& outline,
    const BoardStackupManager& stackup,
    const std::vector<PcbTrackSegment>& tracks,
    const std::vector<DrcPad>& pads,
    const std::vector<PcbComponent3DBody>& bodies) {
    Pcb3DAssemblyResult out;
    const double h_board = stackup.total_thickness_mm();

    double min_x = 0.0, max_x = 100.0, min_y = 0.0, max_y = 80.0;
    const auto& contour = outline.outer_contour();
    if (!contour.empty()) {
        min_x = max_x = contour.front().x;
        min_y = max_y = contour.front().y;
        for (const auto& pt : contour) {
            min_x = std::min(min_x, pt.x);
            max_x = std::max(max_x, pt.x);
            min_y = std::min(min_y, pt.y);
            max_y = std::max(max_y, pt.y);
        }
    }

    // Core dielectric + copper slab [0, h_board]
    append_box_mesh(out.watertight_mesh, min_x, min_y, 0.0, max_x, max_y, h_board);
    out.expected_substrate_volume_mm3 = (max_x - min_x) * (max_y - min_y) * h_board;

    // Raised SMD copper pads & component 3D bodies
    constexpr double kCuReliefH = 0.035;
    for (const auto& p : pads) {
        const double z0 = (p.layer_index == 0) ? h_board : -kCuReliefH;
        append_box_mesh(out.watertight_mesh,
                        p.center_mm.x - 0.5 * p.width_mm,
                        p.center_mm.y - 0.5 * p.height_mm, z0,
                        p.center_mm.x + 0.5 * p.width_mm,
                        p.center_mm.y + 0.5 * p.height_mm, z0 + kCuReliefH);
    }
    (void)tracks;

    for (const auto& body : bodies) {
        const double z0 = body.on_top ? h_board : -body.height_mm;
        append_box_mesh(out.watertight_mesh,
                        body.center_mm.x - 0.5 * body.size_x_mm,
                        body.center_mm.y - 0.5 * body.size_y_mm, z0,
                        body.center_mm.x + 0.5 * body.size_x_mm,
                        body.center_mm.y + 0.5 * body.size_y_mm, z0 + body.height_mm);
    }

    out.mesh_volume_mm3 = out.watertight_mesh.compute_volume();
    out.bbox_min_mm = out.watertight_mesh.bounding_box_min();
    out.bbox_max_mm = out.watertight_mesh.bounding_box_max();
    return out;
}

std::string Pcb3DExporter::export_obj(const Pcb3DAssemblyResult& assembly) {
    return core::domain::ObjMeshIO::export_obj(assembly.watertight_mesh);
}

std::string Pcb3DExporter::export_step_ap214(
    std::string_view product_name,
    const Pcb3DAssemblyResult& assembly) {
    std::ostringstream out;
    out << "ISO-10303-21;\n"
        << "HEADER;\n"
        << "FILE_DESCRIPTION(('Shunya EDA 3D PCB BRep Assembly'),'2;1');\n"
        << "FILE_NAME('" << product_name << ".step','2026-11-10T00:00:00',('Shunya'),('Shunya'),'','','');\n"
        << "FILE_SCHEMA(('AUTOMOTIVE_DESIGN { 1 0 10303 214 1 1 1 1 }'));\n"
        << "ENDSEC;\n"
        << "DATA;\n";

    int eid = 1;
    out << "#" << eid++ << "=APPLICATION_CONTEXT('automotive design');\n";
    out << "#" << eid++ << "=PRODUCT('" << product_name << "','" << product_name << "','',(#1));\n";
    for (const auto& v : assembly.watertight_mesh.vertices) {
        out << "#" << eid++ << "=CARTESIAN_POINT('',(" << v.x << "," << v.y << "," << v.z << "));\n";
    }
    out << "ENDSEC;\n"
        << "END-ISO-10303-21;\n";
    return out.str();
}

} // namespace shunya::pcb
