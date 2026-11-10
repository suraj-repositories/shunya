#include "shunya/cad2d/dxf_io.hpp"

#include "shunya/core/base/math.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <unordered_map>
#include <utility>
#include <vector>

namespace shunya::cad2d {

namespace {

std::string trim_copy(std::string_view sv) {
    size_t start = 0;
    while (start < sv.size() && (sv[start] == ' ' || sv[start] == '\t' || sv[start] == '\r' || sv[start] == '\n')) {
        ++start;
    }
    size_t end = sv.size();
    while (end > start && (sv[end - 1] == ' ' || sv[end - 1] == '\t' || sv[end - 1] == '\r' || sv[end - 1] == '\n')) {
        --end;
    }
    return std::string(sv.substr(start, end - start));
}

std::string upper_copy(std::string_view sv) {
    std::string s(sv);
    for (char& ch : s) {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
    return s;
}

std::string dxf_linetype_name(LinePattern p) {
    switch (p) {
        case LinePattern::Solid:   return "CONTINUOUS";
        case LinePattern::Dashed:  return "DASHED";
        case LinePattern::Dotted:  return "DOTTED";
        case LinePattern::DashDot: return "DASHDOT";
        case LinePattern::Center:  return "CENTER";
    }
    return "CONTINUOUS";
}

LinePattern parse_linetype_name(std::string_view name) {
    const std::string up = upper_copy(name);
    if (up.find("DASHED") != std::string::npos) return LinePattern::Dashed;
    if (up.find("DOTTED") != std::string::npos) return LinePattern::Dotted;
    if (up.find("DASHDOT") != std::string::npos) return LinePattern::DashDot;
    if (up.find("CENTER") != std::string::npos) return LinePattern::Center;
    return LinePattern::Solid;
}

int hex_to_aci(std::string_view hex) {
    if (hex == "#e06c75" || hex == "#ff0000") return 1;
    if (hex == "#e5c07b" || hex == "#ffff00") return 2;
    if (hex == "#98c379" || hex == "#00ff00") return 3;
    if (hex == "#56b6c2" || hex == "#00ffff") return 4;
    if (hex == "#61afef" || hex == "#0000ff") return 5;
    if (hex == "#c678dd" || hex == "#ff00ff") return 6;
    return 7; // white/black default
}

std::string aci_to_hex(int aci) {
    switch (std::abs(aci)) {
        case 1: return "#e06c75";
        case 2: return "#e5c07b";
        case 3: return "#98c379";
        case 4: return "#56b6c2";
        case 5: return "#61afef";
        case 6: return "#c678dd";
        default: return "#abb2bf";
    }
}

void write_pair(std::ostringstream& out, int code, std::string_view val) {
    out << code << "\n" << val << "\n";
}

void write_pair_int(std::ostringstream& out, int code, int val) {
    out << code << "\n" << val << "\n";
}

void write_pair_double(std::ostringstream& out, int code, double val) {
    out << code << "\n" << std::setprecision(12) << val << "\n";
}

void write_curve_entity(
    std::ostringstream& out,
    const Curve2D& curve,
    std::string_view layer,
    DxfVersion version
) {
    const double rad_to_deg = 180.0 / core::base::math::PI;
    switch (curve.type()) {
        case CurveType2D::LineSegment: {
            const auto& seg = static_cast<const LineSegment2D&>(curve);
            write_pair(out, 0, "LINE");
            write_pair(out, 8, layer);
            write_pair_double(out, 10, seg.p0.x);
            write_pair_double(out, 20, seg.p0.y);
            write_pair_double(out, 30, 0.0);
            write_pair_double(out, 11, seg.p1.x);
            write_pair_double(out, 21, seg.p1.y);
            write_pair_double(out, 31, 0.0);
            break;
        }
        case CurveType2D::Circle: {
            const auto& c = static_cast<const Circle2D&>(curve);
            write_pair(out, 0, "CIRCLE");
            write_pair(out, 8, layer);
            write_pair_double(out, 10, c.center.x);
            write_pair_double(out, 20, c.center.y);
            write_pair_double(out, 30, 0.0);
            write_pair_double(out, 40, c.radius);
            break;
        }
        case CurveType2D::Arc: {
            const auto& arc = static_cast<const Arc2D&>(curve);
            double a0 = arc.start_angle;
            double a1 = arc.start_angle + arc.sweep_angle;
            if (arc.sweep_angle < 0.0) {
                std::swap(a0, a1); // DXF ARC is always CCW from start_angle to end_angle
            }
            write_pair(out, 0, "ARC");
            write_pair(out, 8, layer);
            write_pair_double(out, 10, arc.center.x);
            write_pair_double(out, 20, arc.center.y);
            write_pair_double(out, 30, 0.0);
            write_pair_double(out, 40, arc.radius);
            write_pair_double(out, 50, a0 * rad_to_deg);
            write_pair_double(out, 51, a1 * rad_to_deg);
            break;
        }
        case CurveType2D::Polyline: {
            const auto& poly = static_cast<const Polyline2D&>(curve);
            if (version == DxfVersion::AC1015_2000) {
                write_pair(out, 0, "LWPOLYLINE");
                write_pair(out, 8, layer);
                write_pair_int(out, 90, static_cast<int>(poly.points.size()));
                write_pair_int(out, 70, poly.closed ? 1 : 0);
                for (const auto& pt : poly.points) {
                    write_pair_double(out, 10, pt.x);
                    write_pair_double(out, 20, pt.y);
                }
            } else {
                write_pair(out, 0, "POLYLINE");
                write_pair(out, 8, layer);
                write_pair_int(out, 66, 1);
                write_pair_int(out, 70, poly.closed ? 1 : 0);
                write_pair_double(out, 10, 0.0);
                write_pair_double(out, 20, 0.0);
                write_pair_double(out, 30, 0.0);
                for (const auto& pt : poly.points) {
                    write_pair(out, 0, "VERTEX");
                    write_pair(out, 8, layer);
                    write_pair_double(out, 10, pt.x);
                    write_pair_double(out, 20, pt.y);
                    write_pair_double(out, 30, 0.0);
                }
                write_pair(out, 0, "SEQEND");
                write_pair(out, 8, layer);
            }
            break;
        }
        case CurveType2D::BezierCubic: {
            const auto& bez = static_cast<const BezierCubic2D&>(curve);
            write_pair(out, 0, "SPLINE");
            write_pair(out, 8, layer);
            write_pair_int(out, 70, bez.is_closed() ? 1 : 8);
            write_pair_int(out, 71, 3); // cubic degree
            write_pair_int(out, 72, 8); // 8 clamped knots
            write_pair_int(out, 73, 4); // 4 control points
            write_pair_int(out, 74, 0);
            for (int k = 0; k < 4; ++k) write_pair_double(out, 40, 0.0);
            for (int k = 0; k < 4; ++k) write_pair_double(out, 40, 1.0);
            const Point2D cps[4] = {bez.p0, bez.p1, bez.p2, bez.p3};
            for (const auto& cp : cps) {
                write_pair_double(out, 10, cp.x);
                write_pair_double(out, 20, cp.y);
                write_pair_double(out, 30, 0.0);
            }
            break;
        }
        case CurveType2D::Ellipse: {
            const auto& ell = static_cast<const Ellipse2D&>(curve);
            write_pair(out, 0, "ELLIPSE");
            write_pair(out, 8, layer);
            write_pair_double(out, 10, ell.center.x);
            write_pair_double(out, 20, ell.center.y);
            write_pair_double(out, 30, 0.0);
            write_pair_double(out, 11, ell.rx * std::cos(ell.rotation));
            write_pair_double(out, 21, ell.rx * std::sin(ell.rotation));
            write_pair_double(out, 31, 0.0);
            write_pair_double(out, 40, ell.rx > 1e-12 ? (ell.ry / ell.rx) : 1.0);
            write_pair_double(out, 41, 0.0);
            write_pair_double(out, 42, 2.0 * core::base::math::PI);
            break;
        }
    }
}

void write_insert(std::ostringstream& out, const BlockReference& ref) {
    const double rad_to_deg = 180.0 / core::base::math::PI;
    write_pair(out, 0, "INSERT");
    write_pair(out, 8, ref.layer);
    write_pair(out, 2, ref.block_name);
    write_pair_double(out, 10, ref.insertion_point.x);
    write_pair_double(out, 20, ref.insertion_point.y);
    write_pair_double(out, 30, 0.0);
    write_pair_double(out, 41, ref.scale_x);
    write_pair_double(out, 42, ref.scale_y);
    write_pair_double(out, 43, 1.0);
    write_pair_double(out, 50, ref.rotation_rad * rad_to_deg);
}

struct GroupPair {
    int code{0};
    std::string value;
};

std::vector<GroupPair> tokenize_dxf(std::string_view text) {
    std::vector<std::string_view> lines;
    size_t pos = 0;
    while (pos <= text.size()) {
        size_t nl = text.find('\n', pos);
        if (nl == std::string_view::npos) {
            if (pos < text.size()) lines.push_back(text.substr(pos));
            break;
        }
        lines.push_back(text.substr(pos, nl - pos));
        pos = nl + 1;
    }

    std::vector<GroupPair> pairs;
    pairs.reserve(lines.size() / 2);
    for (size_t i = 0; i + 1 < lines.size(); i += 2) {
        const std::string code_str = trim_copy(lines[i]);
        if (code_str.empty()) continue;
        char* end_ptr = nullptr;
        const long c = std::strtol(code_str.c_str(), &end_ptr, 10);
        if (end_ptr == code_str.c_str()) continue;
        pairs.push_back({static_cast<int>(c), trim_copy(lines[i + 1])});
    }
    return pairs;
}

double to_double(const std::string& s, double fallback = 0.0) {
    char* end_ptr = nullptr;
    const double v = std::strtod(s.c_str(), &end_ptr);
    return (end_ptr != s.c_str()) ? v : fallback;
}

int to_int(const std::string& s, int fallback = 0) {
    char* end_ptr = nullptr;
    const long v = std::strtol(s.c_str(), &end_ptr, 10);
    return (end_ptr != s.c_str()) ? static_cast<int>(v) : fallback;
}

struct ParsedEntityResult {
    std::shared_ptr<Curve2D> curve;
    BlockReference insert_ref;
    bool is_insert{false};
    std::string layer{"0"};
};

ParsedEntityResult parse_single_entity(
    const std::string& etype,
    const std::vector<GroupPair>& fields,
    double scale_to_target,
    size_t& idx,
    const std::vector<GroupPair>& all_pairs
) {
    ParsedEntityResult res;
    res.layer = "0";
    const double deg_to_rad = core::base::math::PI / 180.0;

    if (etype == "LINE") {
        Point2D p0{}, p1{};
        for (const auto& f : fields) {
            if (f.code == 8) res.layer = f.value;
            else if (f.code == 10) p0.x = to_double(f.value) * scale_to_target;
            else if (f.code == 20) p0.y = to_double(f.value) * scale_to_target;
            else if (f.code == 11) p1.x = to_double(f.value) * scale_to_target;
            else if (f.code == 21) p1.y = to_double(f.value) * scale_to_target;
        }
        res.curve = std::make_shared<LineSegment2D>(p0, p1);
        return res;
    }

    if (etype == "CIRCLE") {
        Point2D center{};
        double r = 1.0;
        for (const auto& f : fields) {
            if (f.code == 8) res.layer = f.value;
            else if (f.code == 10) center.x = to_double(f.value) * scale_to_target;
            else if (f.code == 20) center.y = to_double(f.value) * scale_to_target;
            else if (f.code == 40) r = to_double(f.value) * scale_to_target;
        }
        res.curve = std::make_shared<Circle2D>(center, r);
        return res;
    }

    if (etype == "ARC") {
        Point2D center{};
        double r = 1.0;
        double a0_deg = 0.0;
        double a1_deg = 180.0;
        for (const auto& f : fields) {
            if (f.code == 8) res.layer = f.value;
            else if (f.code == 10) center.x = to_double(f.value) * scale_to_target;
            else if (f.code == 20) center.y = to_double(f.value) * scale_to_target;
            else if (f.code == 40) r = to_double(f.value) * scale_to_target;
            else if (f.code == 50) a0_deg = to_double(f.value);
            else if (f.code == 51) a1_deg = to_double(f.value);
        }
        double sweep_deg = a1_deg - a0_deg;
        while (sweep_deg <= 0.0) sweep_deg += 360.0;
        res.curve = std::make_shared<Arc2D>(center, r, a0_deg * deg_to_rad, sweep_deg * deg_to_rad);
        return res;
    }

    if (etype == "LWPOLYLINE") {
        std::vector<Point2D> pts;
        bool closed = false;
        double cur_x = 0.0;
        bool has_x = false;
        for (const auto& f : fields) {
            if (f.code == 8) res.layer = f.value;
            else if (f.code == 70) closed = (to_int(f.value) & 1) != 0;
            else if (f.code == 10) {
                cur_x = to_double(f.value) * scale_to_target;
                has_x = true;
            } else if (f.code == 20 && has_x) {
                pts.push_back({cur_x, to_double(f.value) * scale_to_target});
                has_x = false;
            }
        }
        if (pts.size() >= 2) {
            res.curve = std::make_shared<Polyline2D>(std::move(pts), closed);
        }
        return res;
    }

    if (etype == "POLYLINE") {
        bool closed = false;
        for (const auto& f : fields) {
            if (f.code == 8) res.layer = f.value;
            else if (f.code == 70) closed = (to_int(f.value) & 1) != 0;
        }
        std::vector<Point2D> pts;
        while (idx < all_pairs.size()) {
            if (all_pairs[idx].code == 0) {
                const std::string sub = upper_copy(all_pairs[idx].value);
                ++idx;
                if (sub == "SEQEND") {
                    while (idx < all_pairs.size() && all_pairs[idx].code != 0) ++idx;
                    break;
                }
                if (sub == "VERTEX") {
                    Point2D pt{};
                    while (idx < all_pairs.size() && all_pairs[idx].code != 0) {
                        if (all_pairs[idx].code == 10) pt.x = to_double(all_pairs[idx].value) * scale_to_target;
                        else if (all_pairs[idx].code == 20) pt.y = to_double(all_pairs[idx].value) * scale_to_target;
                        ++idx;
                    }
                    pts.push_back(pt);
                    continue;
                }
                --idx;
                break;
            }
            ++idx;
        }
        if (pts.size() >= 2) {
            res.curve = std::make_shared<Polyline2D>(std::move(pts), closed);
        }
        return res;
    }

    if (etype == "SPLINE") {
        std::vector<Point2D> cps;
        double cur_x = 0.0;
        bool has_x = false;
        for (const auto& f : fields) {
            if (f.code == 8) res.layer = f.value;
            else if (f.code == 10) {
                cur_x = to_double(f.value) * scale_to_target;
                has_x = true;
            } else if (f.code == 20 && has_x) {
                cps.push_back({cur_x, to_double(f.value) * scale_to_target});
                has_x = false;
            }
        }
        if (cps.size() >= 4) {
            res.curve = std::make_shared<BezierCubic2D>(cps[0], cps[1], cps[2], cps[3]);
        } else if (cps.size() >= 2) {
            res.curve = std::make_shared<Polyline2D>(std::move(cps), false);
        }
        return res;
    }

    if (etype == "ELLIPSE") {
        Point2D center{};
        Point2D major_vec{1.0, 0.0};
        double ratio = 1.0;
        for (const auto& f : fields) {
            if (f.code == 8) res.layer = f.value;
            else if (f.code == 10) center.x = to_double(f.value) * scale_to_target;
            else if (f.code == 20) center.y = to_double(f.value) * scale_to_target;
            else if (f.code == 11) major_vec.x = to_double(f.value) * scale_to_target;
            else if (f.code == 21) major_vec.y = to_double(f.value) * scale_to_target;
            else if (f.code == 40) ratio = to_double(f.value, 1.0);
        }
        const double rx = major_vec.length();
        const double ry = rx * std::abs(ratio);
        const double rot = std::atan2(major_vec.y, major_vec.x);
        res.curve = std::make_shared<Ellipse2D>(center, rx, ry, rot);
        return res;
    }

    if (etype == "INSERT") {
        res.is_insert = true;
        for (const auto& f : fields) {
            if (f.code == 8) {
                res.layer = f.value;
                res.insert_ref.layer = f.value;
            } else if (f.code == 2) {
                res.insert_ref.block_name = f.value;
            } else if (f.code == 10) {
                res.insert_ref.insertion_point.x = to_double(f.value) * scale_to_target;
            } else if (f.code == 20) {
                res.insert_ref.insertion_point.y = to_double(f.value) * scale_to_target;
            } else if (f.code == 41) {
                res.insert_ref.scale_x = to_double(f.value, 1.0);
            } else if (f.code == 42) {
                res.insert_ref.scale_y = to_double(f.value, 1.0);
            } else if (f.code == 50) {
                res.insert_ref.rotation_rad = to_double(f.value) * deg_to_rad;
            }
        }
        return res;
    }

    return res;
}

} // namespace

std::string DxfWriter::write(
    const SketchDocument2D& sketch,
    const LayerManager& layers,
    const BlockManager& blocks,
    DxfVersion version,
    DxfUnit units
) {
    std::ostringstream out;
    out << std::fixed;

    // 1. HEADER
    write_pair(out, 0, "SECTION");
    write_pair(out, 2, "HEADER");
    write_pair(out, 9, "$ACADVER");
    write_pair(out, 1, version == DxfVersion::R12 ? "AC1009" : "AC1015");
    write_pair(out, 9, "$INSUNITS");
    write_pair_int(out, 70, static_cast<int>(units));
    write_pair(out, 0, "ENDSEC");

    // 2. TABLES (LAYER)
    write_pair(out, 0, "SECTION");
    write_pair(out, 2, "TABLES");
    write_pair(out, 0, "TABLE");
    write_pair(out, 2, "LAYER");
    const auto ordered = layers.ordered_layers();
    write_pair_int(out, 70, static_cast<int>(ordered.size()));
    for (const auto* layer : ordered) {
        if (!layer) continue;
        write_pair(out, 0, "LAYER");
        write_pair(out, 2, layer->name);
        int flags = 0;
        if (!layer->visible) flags |= 1;
        if (layer->locked) flags |= 4;
        write_pair_int(out, 70, flags);
        int aci = hex_to_aci(layer->style.color_hex);
        if (!layer->visible) aci = -aci;
        write_pair_int(out, 62, aci);
        write_pair(out, 6, dxf_linetype_name(layer->style.pattern));
        write_pair_int(out, 370, static_cast<int>(std::llround(layer->style.line_weight_mm * 100.0)));
    }
    write_pair(out, 0, "ENDTAB");
    write_pair(out, 0, "ENDSEC");

    // 3. BLOCKS
    write_pair(out, 0, "SECTION");
    write_pair(out, 2, "BLOCKS");
    for (const auto* def : blocks.blocks()) {
        if (!def) continue;
        write_pair(out, 0, "BLOCK");
        write_pair(out, 8, "0");
        write_pair(out, 2, def->name);
        write_pair_int(out, 70, 0);
        write_pair_double(out, 10, def->base_point.x);
        write_pair_double(out, 20, def->base_point.y);
        write_pair_double(out, 30, 0.0);
        for (const auto& item : def->items) {
            if (item.curve) {
                write_curve_entity(out, *item.curve, item.layer, version);
            }
        }
        for (const auto& nested : def->nested_refs) {
            write_insert(out, nested);
        }
        write_pair(out, 0, "ENDBLK");
        write_pair(out, 8, "0");
    }
    write_pair(out, 0, "ENDSEC");

    // 4. ENTITIES
    write_pair(out, 0, "SECTION");
    write_pair(out, 2, "ENTITIES");
    for (const auto& e : sketch.entities()) {
        if (e.curve) {
            write_curve_entity(out, *e.curve, e.layer, version);
        }
    }
    for (const auto& inst : blocks.instances()) {
        write_insert(out, inst);
    }
    write_pair(out, 0, "ENDSEC");

    write_pair(out, 0, "EOF");
    return out.str();
}

bool DxfWriter::write_file(
    std::string_view file_path,
    const SketchDocument2D& sketch,
    const LayerManager& layers,
    const BlockManager& blocks,
    DxfVersion version,
    DxfUnit units
) {
    std::ofstream ofs{std::string(file_path), std::ios::binary};
    if (!ofs.is_open()) return false;
    const std::string content = write(sketch, layers, blocks, version, units);
    ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
    return ofs.good();
}

bool DxfParser::parse(
    std::string_view dxf_content,
    DxfDocument& out_doc,
    DxfUnit target_unit
) {
    out_doc = DxfDocument{};
    out_doc.units = target_unit;

    const auto pairs = tokenize_dxf(dxf_content);
    if (pairs.empty()) return false;

    // Pre-scan HEADER for version and source units so coordinates can scale accurately
    DxfUnit source_unit = target_unit;
    for (size_t i = 0; i < pairs.size(); ++i) {
        if (pairs[i].code == 9 && pairs[i].value == "$ACADVER" && i + 1 < pairs.size()) {
            out_doc.version = (pairs[i + 1].value == "AC1009") ? DxfVersion::R12 : DxfVersion::AC1015_2000;
        } else if (pairs[i].code == 9 && pairs[i].value == "$INSUNITS" && i + 1 < pairs.size()) {
            const int u = to_int(pairs[i + 1].value, 4);
            if (u == 1 || u == 4 || u == 5 || u == 6 || u == 0) {
                source_unit = static_cast<DxfUnit>(u);
            }
        }
    }

    const double scale = dxf_unit_to_mm(source_unit) / dxf_unit_to_mm(target_unit);

    std::string current_section;
    size_t i = 0;
    while (i < pairs.size()) {
        const auto& p = pairs[i];
        if (p.code == 0 && upper_copy(p.value) == "EOF") {
            break;
        }
        if (p.code == 0 && upper_copy(p.value) == "SECTION") {
            if (i + 1 < pairs.size() && pairs[i + 1].code == 2) {
                current_section = upper_copy(pairs[i + 1].value);
                i += 2;
                continue;
            }
        }
        if (p.code == 0 && upper_copy(p.value) == "ENDSEC") {
            current_section.clear();
            ++i;
            continue;
        }

        if (current_section == "TABLES") {
            if (p.code == 0 && upper_copy(p.value) == "LAYER") {
                ++i;
                std::string lname;
                int flags = 0;
                int aci = 7;
                LinePattern pat = LinePattern::Solid;
                double weight = 0.25;
                while (i < pairs.size() && pairs[i].code != 0) {
                    if (pairs[i].code == 2) lname = pairs[i].value;
                    else if (pairs[i].code == 70) flags = to_int(pairs[i].value);
                    else if (pairs[i].code == 62) aci = to_int(pairs[i].value, 7);
                    else if (pairs[i].code == 6) pat = parse_linetype_name(pairs[i].value);
                    else if (pairs[i].code == 370) weight = to_double(pairs[i].value, 25.0) / 100.0;
                    ++i;
                }
                if (!lname.empty()) {
                    const LayerStyle style{aci_to_hex(aci), weight, pat};
                    const bool vis = (flags & 1) == 0 && aci >= 0;
                    const bool lck = (flags & 4) != 0;
                    if (out_doc.layers.find_layer(lname) == nullptr) {
                        out_doc.layers.add_layer(lname, "", style);
                    } else {
                        out_doc.layers.set_style(lname, style);
                    }
                    out_doc.layers.set_visible(lname, vis);
                    out_doc.layers.set_locked(lname, lck);
                }
                continue;
            }
        } else if (current_section == "BLOCKS") {
            if (p.code == 0 && upper_copy(p.value) == "BLOCK") {
                ++i;
                BlockDefinition def;
                while (i < pairs.size() && pairs[i].code != 0) {
                    if (pairs[i].code == 2) def.name = pairs[i].value;
                    else if (pairs[i].code == 10) def.base_point.x = to_double(pairs[i].value) * scale;
                    else if (pairs[i].code == 20) def.base_point.y = to_double(pairs[i].value) * scale;
                    ++i;
                }
                while (i < pairs.size()) {
                    if (pairs[i].code == 0) {
                        const std::string etype = upper_copy(pairs[i].value);
                        ++i;
                        if (etype == "ENDBLK" || etype == "ENDSEC") {
                            while (i < pairs.size() && pairs[i].code != 0) ++i;
                            break;
                        }
                        std::vector<GroupPair> fields;
                        while (i < pairs.size() && pairs[i].code != 0) {
                            fields.push_back(pairs[i]);
                            ++i;
                        }
                        auto parsed = parse_single_entity(etype, fields, scale, i, pairs);
                        if (parsed.is_insert && !parsed.insert_ref.block_name.empty()) {
                            def.nested_refs.push_back(parsed.insert_ref);
                        } else if (parsed.curve) {
                            def.items.push_back({parsed.curve, parsed.layer, false});
                        }
                        continue;
                    }
                    ++i;
                }
                if (!def.name.empty()) {
                    out_doc.blocks.create_block(std::move(def));
                }
                continue;
            }
        } else if (current_section == "ENTITIES") {
            if (p.code == 0) {
                const std::string etype = upper_copy(p.value);
                ++i;
                std::vector<GroupPair> fields;
                while (i < pairs.size() && pairs[i].code != 0) {
                    fields.push_back(pairs[i]);
                    ++i;
                }
                auto parsed = parse_single_entity(etype, fields, scale, i, pairs);
                if (parsed.is_insert && !parsed.insert_ref.block_name.empty()) {
                    out_doc.blocks.place_instance(
                        parsed.insert_ref.block_name,
                        parsed.insert_ref.insertion_point,
                        parsed.insert_ref.rotation_rad,
                        parsed.insert_ref.scale_x,
                        parsed.insert_ref.scale_y,
                        parsed.layer
                    );
                } else if (parsed.curve) {
                    out_doc.sketch.add_entity(parsed.curve, false, parsed.layer);
                }
                continue;
            }
        }

        ++i;
    }

    return true;
}

bool DxfParser::parse_file(
    std::string_view file_path,
    DxfDocument& out_doc,
    DxfUnit target_unit
) {
    std::ifstream ifs{std::string(file_path), std::ios::binary};
    if (!ifs.is_open()) return false;
    std::ostringstream ss;
    ss << ifs.rdbuf();
    return parse(ss.str(), out_doc, target_unit);
}

} // namespace shunya::cad2d
