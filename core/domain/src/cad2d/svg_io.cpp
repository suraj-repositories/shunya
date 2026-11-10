#include "shunya/cad2d/svg_io.hpp"

#include "shunya/core/base/math.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <unordered_map>
#include <utility>
#include <vector>

namespace shunya::cad2d {

namespace {

constexpr double SVG_PX_TO_MM = 25.4 / 96.0;

std::string unit_suffix(SvgLengthUnit unit) {
    switch (unit) {
        case SvgLengthUnit::Inches:      return "in";
        case SvgLengthUnit::Centimeters: return "cm";
        case SvgLengthUnit::Pixels:      return "px";
        case SvgLengthUnit::Millimeters:
        default:                         return "mm";
    }
}

double mm_per_svg_unit(SvgLengthUnit unit) {
    switch (unit) {
        case SvgLengthUnit::Inches:      return 25.4;
        case SvgLengthUnit::Centimeters: return 10.0;
        case SvgLengthUnit::Pixels:      return SVG_PX_TO_MM;
        case SvgLengthUnit::Millimeters:
        default:                         return 1.0;
    }
}

std::string stroke_dasharray(LinePattern p) {
    switch (p) {
        case LinePattern::Dashed:  return "6,3";
        case LinePattern::Dotted:  return "1.5,2.5";
        case LinePattern::DashDot: return "8,2.5,1.5,2.5";
        case LinePattern::Center:  return "12,3,2,3";
        case LinePattern::Solid:
        default:                   return "";
    }
}

LinePattern parse_dasharray(std::string_view dash) {
    if (dash.empty() || dash == "none") return LinePattern::Solid;
    if (dash.find("12,") != std::string_view::npos) return LinePattern::Center;
    if (dash.find("8,") != std::string_view::npos) return LinePattern::DashDot;
    if (dash.find("1.5,") != std::string_view::npos) return LinePattern::Dotted;
    return LinePattern::Dashed;
}

std::string xml_escape(std::string_view sv) {
    std::string out;
    out.reserve(sv.size());
    for (char c : sv) {
        switch (c) {
            case '&':  out += "&amp;"; break;
            case '<':  out += "&lt;"; break;
            case '>':  out += "&gt;"; break;
            case '"':  out += "&quot;"; break;
            case '\'': out += "&apos;"; break;
            default:   out += c; break;
        }
    }
    return out;
}

void append_curve_svg_element(
    std::ostringstream& out,
    const Curve2D& curve,
    double scale_from_mm
) {
    const auto sx = [scale_from_mm](double v) { return v * scale_from_mm; };

    switch (curve.type()) {
        case CurveType2D::LineSegment: {
            const auto& seg = static_cast<const LineSegment2D&>(curve);
            out << "      <path d=\"M " << sx(seg.p0.x) << " " << sx(seg.p0.y)
                << " L " << sx(seg.p1.x) << " " << sx(seg.p1.y) << "\"/>\n";
            break;
        }
        case CurveType2D::Polyline: {
            const auto& poly = static_cast<const Polyline2D&>(curve);
            if (poly.points.size() < 2) break;
            out << "      <path d=\"M " << sx(poly.points[0].x) << " " << sx(poly.points[0].y);
            for (size_t i = 1; i < poly.points.size(); ++i) {
                out << " L " << sx(poly.points[i].x) << " " << sx(poly.points[i].y);
            }
            if (poly.closed) out << " Z";
            out << "\"/>\n";
            break;
        }
        case CurveType2D::Circle: {
            const auto& c = static_cast<const Circle2D&>(curve);
            out << "      <circle cx=\"" << sx(c.center.x)
                << "\" cy=\"" << sx(c.center.y)
                << "\" r=\"" << sx(c.radius) << "\"/>\n";
            break;
        }
        case CurveType2D::Arc: {
            const auto& arc = static_cast<const Arc2D&>(curve);
            const Point2D p0 = arc.evaluate(0.0);
            const Point2D p1 = arc.evaluate(1.0);
            const double r_u = sx(arc.radius);
            const int large_arc = std::abs(arc.sweep_angle) > core::base::math::PI ? 1 : 0;
            const int sweep_flag = arc.sweep_angle >= 0.0 ? 1 : 0;
            out << "      <path d=\"M " << sx(p0.x) << " " << sx(p0.y)
                << " A " << r_u << " " << r_u << " 0 " << large_arc << " " << sweep_flag
                << " " << sx(p1.x) << " " << sx(p1.y) << "\"/>\n";
            break;
        }
        case CurveType2D::BezierCubic: {
            const auto& bez = static_cast<const BezierCubic2D&>(curve);
            out << "      <path d=\"M " << sx(bez.p0.x) << " " << sx(bez.p0.y)
                << " C " << sx(bez.p1.x) << " " << sx(bez.p1.y)
                << ", " << sx(bez.p2.x) << " " << sx(bez.p2.y)
                << ", " << sx(bez.p3.x) << " " << sx(bez.p3.y);
            if (bez.is_closed()) out << " Z";
            out << "\"/>\n";
            break;
        }
        case CurveType2D::Ellipse: {
            const auto& ell = static_cast<const Ellipse2D&>(curve);
            out << "      <ellipse cx=\"" << sx(ell.center.x)
                << "\" cy=\"" << sx(ell.center.y)
                << "\" rx=\"" << sx(ell.rx)
                << "\" ry=\"" << sx(ell.ry) << "\"";
            if (std::abs(ell.rotation) > 1e-9) {
                const double deg = ell.rotation * 180.0 / core::base::math::PI;
                out << " transform=\"rotate(" << deg << " " << sx(ell.center.x) << " " << sx(ell.center.y) << ")\"";
            }
            out << "/>\n";
            break;
        }
    }
}

struct XmlTag {
    std::string name;
    bool is_closing{false};
    bool is_self_closing{false};
    std::unordered_map<std::string, std::string> attrs;
};

std::string parse_attr_value(const std::string& s, size_t& pos) {
    while (pos < s.size() && std::isspace(static_cast<unsigned char>(s[pos]))) ++pos;
    if (pos >= s.size()) return {};
    char quote = s[pos];
    if (quote == '"' || quote == '\'') {
        ++pos;
        const size_t start = pos;
        while (pos < s.size() && s[pos] != quote) ++pos;
        std::string val = s.substr(start, pos - start);
        if (pos < s.size()) ++pos;
        return val;
    }
    const size_t start = pos;
    while (pos < s.size() && !std::isspace(static_cast<unsigned char>(s[pos])) && s[pos] != '>' && s[pos] != '/') ++pos;
    return s.substr(start, pos - start);
}

std::vector<XmlTag> tokenize_xml_tags(std::string_view xml) {
    std::vector<XmlTag> tags;
    size_t pos = 0;
    while (pos < xml.size()) {
        const size_t lt = xml.find('<', pos);
        if (lt == std::string_view::npos) break;
        if (lt + 1 < xml.size() && (xml[lt + 1] == '?' || xml[lt + 1] == '!')) {
            const size_t gt = xml.find('>', lt + 2);
            if (gt == std::string_view::npos) break;
            pos = gt + 1;
            continue;
        }
        const size_t gt = xml.find('>', lt + 1);
        if (gt == std::string_view::npos) break;
        std::string inner(xml.substr(lt + 1, gt - lt - 1));
        pos = gt + 1;

        XmlTag tag;
        size_t i = 0;
        while (i < inner.size() && std::isspace(static_cast<unsigned char>(inner[i]))) ++i;
        if (i < inner.size() && inner[i] == '/') {
            tag.is_closing = true;
            ++i;
        }
        if (!inner.empty() && inner.back() == '/') {
            tag.is_self_closing = true;
            inner.pop_back();
        }
        const size_t name_start = i;
        while (i < inner.size() && !std::isspace(static_cast<unsigned char>(inner[i]))) ++i;
        tag.name = inner.substr(name_start, i - name_start);

        while (!tag.is_closing && i < inner.size()) {
            while (i < inner.size() && std::isspace(static_cast<unsigned char>(inner[i]))) ++i;
            if (i >= inner.size()) break;
            const size_t k_start = i;
            while (i < inner.size() && inner[i] != '=' && !std::isspace(static_cast<unsigned char>(inner[i]))) ++i;
            std::string key = inner.substr(k_start, i - k_start);
            while (i < inner.size() && std::isspace(static_cast<unsigned char>(inner[i]))) ++i;
            if (i < inner.size() && inner[i] == '=') {
                ++i;
                std::string val = parse_attr_value(inner, i);
                if (!key.empty()) tag.attrs[std::move(key)] = std::move(val);
            }
        }
        if (!tag.name.empty()) {
            tags.push_back(std::move(tag));
        }
    }
    return tags;
}

double parse_length_with_unit(std::string_view sv, SvgLengthUnit& detected_unit) {
    char* end_ptr = nullptr;
    std::string s(sv);
    const double val = std::strtod(s.c_str(), &end_ptr);
    std::string unit_str;
    while (end_ptr && *end_ptr != '\0') {
        if (!std::isspace(static_cast<unsigned char>(*end_ptr))) {
            unit_str.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(*end_ptr))));
        }
        ++end_ptr;
    }
    if (unit_str == "in") detected_unit = SvgLengthUnit::Inches;
    else if (unit_str == "cm") detected_unit = SvgLengthUnit::Centimeters;
    else if (unit_str == "mm") detected_unit = SvgLengthUnit::Millimeters;
    else detected_unit = SvgLengthUnit::Pixels;
    return val * mm_per_svg_unit(detected_unit);
}

std::vector<double> parse_number_list(std::string_view sv) {
    std::vector<double> nums;
    const char* p = sv.data();
    const char* end = p + sv.size();
    while (p < end) {
        while (p < end && (*p == ' ' || *p == ',' || *p == '\t' || *p == '\r' || *p == '\n')) ++p;
        if (p >= end) break;
        char* next = nullptr;
        const double v = std::strtod(p, &next);
        if (next == p) {
            ++p;
            continue;
        }
        nums.push_back(v);
        p = next;
    }
    return nums;
}

std::shared_ptr<Arc2D> svg_endpoint_arc_to_center_arc(
    Point2D p0,
    double rx,
    double ry,
    bool large_arc,
    bool sweep,
    Point2D p1,
    double scale
) {
    if (std::abs(rx - ry) > 1e-6 * std::max(1.0, rx) || rx <= 1e-12) {
        return nullptr;
    }
    const double r = rx;
    const Point2D mid{(p0.x + p1.x) * 0.5, (p0.y + p1.y) * 0.5};
    const Vec2d half_chord{(p1.x - p0.x) * 0.5, (p1.y - p0.y) * 0.5};
    const double d_sq = half_chord.length_squared();
    const double d = std::sqrt(d_sq);
    if (d <= 1e-12) return nullptr;

    const double r_eff = std::max(r, d);
    const double h = std::sqrt(std::max(0.0, r_eff * r_eff - d_sq));
    Vec2d perp = perp_ccw(half_chord).normalized();
    const bool sign_pos = (large_arc != sweep);
    const Point2D center = sign_pos ? (mid + perp * h) : (mid - perp * h);

    const double a0 = std::atan2(p0.y - center.y, p0.x - center.x);
    const double a1 = std::atan2(p1.y - center.y, p1.x - center.x);
    double da = a1 - a0;
    if (sweep && da < 0.0) da += 2.0 * core::base::math::PI;
    if (!sweep && da > 0.0) da -= 2.0 * core::base::math::PI;

    return std::make_shared<Arc2D>(
        Point2D{center.x * scale, center.y * scale},
        r_eff * scale,
        a0,
        da
    );
}

void parse_svg_path_d(
    std::string_view d,
    double scale,
    std::vector<std::shared_ptr<Curve2D>>& out_curves
) {
    size_t i = 0;
    char cmd = 0;
    Point2D curr{0.0, 0.0};
    Point2D start{0.0, 0.0};
    Point2D prev_ctrl{0.0, 0.0};
    char prev_cmd = 0;

    std::vector<Point2D> poly_accum;
    auto flush_poly = [&](bool closed) {
        if (poly_accum.size() == 2 && !closed) {
            out_curves.push_back(std::make_shared<LineSegment2D>(
                poly_accum[0] * scale, poly_accum[1] * scale
            ));
        } else if (poly_accum.size() >= 2) {
            std::vector<Point2D> scaled;
            scaled.reserve(poly_accum.size());
            for (const auto& pt : poly_accum) scaled.push_back(pt * scale);
            out_curves.push_back(std::make_shared<Polyline2D>(std::move(scaled), closed));
        }
        poly_accum.clear();
    };

    auto next_number = [&](double& out_val) -> bool {
        while (i < d.size() && (d[i] == ' ' || d[i] == ',' || d[i] == '\t' || d[i] == '\r' || d[i] == '\n')) ++i;
        if (i >= d.size() || std::isalpha(static_cast<unsigned char>(d[i]))) return false;
        const char* p = d.data() + i;
        char* next = nullptr;
        out_val = std::strtod(p, &next);
        if (next == p) return false;
        i = static_cast<size_t>(next - d.data());
        return true;
    };

    while (i < d.size()) {
        while (i < d.size() && (d[i] == ' ' || d[i] == ',' || d[i] == '\t' || d[i] == '\r' || d[i] == '\n')) ++i;
        if (i >= d.size()) break;
        if (std::isalpha(static_cast<unsigned char>(d[i]))) {
            cmd = d[i++];
        }

        const bool rel = std::islower(static_cast<unsigned char>(cmd)) != 0;
        const char ucmd = static_cast<char>(std::toupper(static_cast<unsigned char>(cmd)));

        if (ucmd == 'Z') {
            if (!poly_accum.empty()) {
                flush_poly(true);
            } else if (!curr.approx_equals(start, 1e-9)) {
                out_curves.push_back(std::make_shared<LineSegment2D>(curr * scale, start * scale));
            }
            curr = start;
            prev_cmd = 'Z';
            continue;
        }

        if (ucmd == 'M') {
            double x = 0.0, y = 0.0;
            if (!next_number(x) || !next_number(y)) break;
            flush_poly(false);
            curr = rel ? Point2D{curr.x + x, curr.y + y} : Point2D{x, y};
            start = curr;
            poly_accum.push_back(curr);
            cmd = rel ? 'l' : 'L';
            prev_cmd = 'M';
            continue;
        }

        if (ucmd == 'L') {
            double x = 0.0, y = 0.0;
            if (!next_number(x) || !next_number(y)) break;
            if (poly_accum.empty()) poly_accum.push_back(curr);
            curr = rel ? Point2D{curr.x + x, curr.y + y} : Point2D{x, y};
            poly_accum.push_back(curr);
            prev_cmd = 'L';
            continue;
        }

        if (ucmd == 'H') {
            double x = 0.0;
            if (!next_number(x)) break;
            if (poly_accum.empty()) poly_accum.push_back(curr);
            curr.x = rel ? (curr.x + x) : x;
            poly_accum.push_back(curr);
            prev_cmd = 'H';
            continue;
        }

        if (ucmd == 'V') {
            double y = 0.0;
            if (!next_number(y)) break;
            if (poly_accum.empty()) poly_accum.push_back(curr);
            curr.y = rel ? (curr.y + y) : y;
            poly_accum.push_back(curr);
            prev_cmd = 'V';
            continue;
        }

        if (ucmd == 'C') {
            double x1 = 0, y1 = 0, x2 = 0, y2 = 0, x = 0, y = 0;
            if (!next_number(x1) || !next_number(y1) ||
                !next_number(x2) || !next_number(y2) ||
                !next_number(x)  || !next_number(y)) break;
            flush_poly(false);
            const Point2D p0 = curr;
            const Point2D p1 = rel ? Point2D{curr.x + x1, curr.y + y1} : Point2D{x1, y1};
            const Point2D p2 = rel ? Point2D{curr.x + x2, curr.y + y2} : Point2D{x2, y2};
            const Point2D p3 = rel ? Point2D{curr.x + x, curr.y + y} : Point2D{x, y};
            out_curves.push_back(std::make_shared<BezierCubic2D>(p0 * scale, p1 * scale, p2 * scale, p3 * scale));
            prev_ctrl = p2;
            curr = p3;
            prev_cmd = 'C';
            continue;
        }

        if (ucmd == 'S') {
            double x2 = 0, y2 = 0, x = 0, y = 0;
            if (!next_number(x2) || !next_number(y2) || !next_number(x) || !next_number(y)) break;
            flush_poly(false);
            const Point2D p0 = curr;
            const Point2D p1 = (prev_cmd == 'C' || prev_cmd == 'S') ? (curr * 2.0 - prev_ctrl) : curr;
            const Point2D p2 = rel ? Point2D{curr.x + x2, curr.y + y2} : Point2D{x2, y2};
            const Point2D p3 = rel ? Point2D{curr.x + x, curr.y + y} : Point2D{x, y};
            out_curves.push_back(std::make_shared<BezierCubic2D>(p0 * scale, p1 * scale, p2 * scale, p3 * scale));
            prev_ctrl = p2;
            curr = p3;
            prev_cmd = 'S';
            continue;
        }

        if (ucmd == 'A') {
            double rx = 0, ry = 0, xrot = 0, large = 0, swp = 0, x = 0, y = 0;
            if (!next_number(rx) || !next_number(ry) || !next_number(xrot) ||
                !next_number(large) || !next_number(swp) ||
                !next_number(x) || !next_number(y)) break;
            flush_poly(false);
            const Point2D p1 = rel ? Point2D{curr.x + x, curr.y + y} : Point2D{x, y};
            auto arc = svg_endpoint_arc_to_center_arc(curr, std::abs(rx), std::abs(ry), large > 0.5, swp > 0.5, p1, scale);
            if (arc) {
                out_curves.push_back(arc);
            } else {
                out_curves.push_back(std::make_shared<LineSegment2D>(curr * scale, p1 * scale));
            }
            curr = p1;
            prev_cmd = 'A';
            continue;
        }

        double skip = 0.0;
        if (!next_number(skip)) ++i;
    }

    flush_poly(false);
}

} // namespace

std::string SvgExchange::export_svg(
    const SketchDocument2D& sketch,
    const LayerManager& layers,
    const BlockManager* blocks,
    const SvgExportOptions& options
) {
    struct DrawItem {
        std::shared_ptr<Curve2D> curve;
        std::string layer;
    };
    std::vector<DrawItem> all_items;
    all_items.reserve(sketch.entities().size());
    for (const auto& e : sketch.entities()) {
        if (e.curve) all_items.push_back({e.curve, e.layer});
    }
    if (blocks && options.include_blocks) {
        for (const auto& resolved : blocks->resolve_all_instances()) {
            if (resolved.curve) all_items.push_back({resolved.curve, resolved.layer});
        }
    }

    BBox2D bbox;
    for (const auto& item : all_items) {
        bbox.expand(item.curve->bounding_box());
    }
    if (!bbox.is_valid()) {
        bbox = BBox2D{{0.0, 0.0}, {100.0, 100.0}};
    }

    const double pad = std::max(0.0, options.padding_mm);
    const double min_x_mm = bbox.min.x - pad;
    const double min_y_mm = bbox.min.y - pad;
    const double max_y_mm = bbox.max.y + pad;
    const double width_mm = bbox.width() + 2.0 * pad;
    const double height_mm = bbox.height() + 2.0 * pad;

    const double scale_from_mm = 1.0 / mm_per_svg_unit(options.unit);
    const std::string suffix = unit_suffix(options.unit);

    const double vb_min_x = min_x_mm * scale_from_mm;
    const double vb_min_y = options.flip_y ? (-max_y_mm * scale_from_mm) : (min_y_mm * scale_from_mm);
    const double vb_w = width_mm * scale_from_mm;
    const double vb_h = height_mm * scale_from_mm;

    // Preserve declaration order from LayerManager while catching extra entity layers
    std::map<std::string, std::vector<std::shared_ptr<Curve2D>>> by_layer;
    for (const auto& item : all_items) {
        by_layer[item.layer.empty() ? std::string("0") : item.layer].push_back(item.curve);
    }

    std::ostringstream out;
    out << std::fixed << std::setprecision(6);
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\" "
        << "width=\"" << vb_w << suffix << "\" "
        << "height=\"" << vb_h << suffix << "\" "
        << "viewBox=\"" << vb_min_x << " " << vb_min_y << " " << vb_w << " " << vb_h << "\">\n";

    if (options.flip_y) {
        out << "  <g id=\"shunya-cad-root\" transform=\"scale(1,-1)\">\n";
    } else {
        out << "  <g id=\"shunya-cad-root\">\n";
    }

    for (const auto& [layer_name, curves] : by_layer) {
        const Layer2D* ldef = layers.find_layer(layer_name);
        const std::string color = ldef ? ldef->style.color_hex : "#61afef";
        const double weight_u = (ldef ? ldef->style.line_weight_mm : 0.25) * scale_from_mm;
        const std::string dash = ldef ? stroke_dasharray(ldef->style.pattern) : "";
        const bool vis = ldef ? layers.is_effectively_visible(layer_name) : true;

        out << "    <g id=\"layer_" << xml_escape(layer_name)
            << "\" data-layer=\"" << xml_escape(layer_name)
            << "\" fill=\"none\" stroke=\"" << xml_escape(color)
            << "\" stroke-width=\"" << weight_u << "\"";
        if (!dash.empty()) {
            out << " stroke-dasharray=\"" << dash << "\"";
        }
        if (!vis) {
            out << " display=\"none\"";
        }
        out << ">\n";

        for (const auto& curve : curves) {
            append_curve_svg_element(out, *curve, scale_from_mm);
        }
        out << "    </g>\n";
    }

    out << "  </g>\n";
    out << "</svg>\n";
    return out.str();
}

bool SvgExchange::export_svg_file(
    std::string_view file_path,
    const SketchDocument2D& sketch,
    const LayerManager& layers,
    const BlockManager* blocks,
    const SvgExportOptions& options
) {
    std::ofstream ofs{std::string(file_path), std::ios::binary};
    if (!ofs.is_open()) return false;
    const std::string xml = export_svg(sketch, layers, blocks, options);
    ofs.write(xml.data(), static_cast<std::streamsize>(xml.size()));
    return ofs.good();
}

bool SvgExchange::import_svg(std::string_view svg_xml, SvgDocument& out_doc) {
    out_doc = SvgDocument{};
    const auto tags = tokenize_xml_tags(svg_xml);
    if (tags.empty()) return false;

    double unit_scale_to_mm = 1.0;
    bool found_svg = false;

    struct GroupState {
        std::string layer{"0"};
        bool is_root{false};
    };
    std::vector<GroupState> group_stack;

    for (const auto& tag : tags) {
        if (tag.is_closing) {
            if (tag.name == "g" && !group_stack.empty()) {
                group_stack.pop_back();
            }
            continue;
        }

        if (tag.name == "svg") {
            found_svg = true;
            SvgLengthUnit u_w = SvgLengthUnit::Millimeters;
            SvgLengthUnit u_h = SvgLengthUnit::Millimeters;
            if (auto it = tag.attrs.find("width"); it != tag.attrs.end()) {
                out_doc.width_mm = parse_length_with_unit(it->second, u_w);
                out_doc.unit = u_w;
            }
            if (auto it = tag.attrs.find("height"); it != tag.attrs.end()) {
                out_doc.height_mm = parse_length_with_unit(it->second, u_h);
            }
            if (auto it = tag.attrs.find("viewBox"); it != tag.attrs.end()) {
                const auto vb = parse_number_list(it->second);
                if (vb.size() == 4 && vb[2] > 1e-12 && tag.attrs.contains("width")) {
                    unit_scale_to_mm = out_doc.width_mm / vb[2];
                } else {
                    unit_scale_to_mm = mm_per_svg_unit(out_doc.unit);
                }
            } else {
                unit_scale_to_mm = mm_per_svg_unit(out_doc.unit);
            }
            continue;
        }

        const std::string active_layer = group_stack.empty() ? std::string("0") : group_stack.back().layer;

        if (tag.name == "g") {
            std::string layer_name = active_layer;
            bool is_root = false;
            if (auto it = tag.attrs.find("id"); it != tag.attrs.end() && it->second == "shunya-cad-root") {
                is_root = true;
            } else {
                if (auto dit = tag.attrs.find("data-layer"); dit != tag.attrs.end() && !dit->second.empty()) {
                    layer_name = dit->second;
                } else if (auto iit = tag.attrs.find("id"); iit != tag.attrs.end() && !iit->second.empty()) {
                    layer_name = iit->second;
                }
                if (layer_name != "0") {
                    LayerStyle style;
                    if (auto sit = tag.attrs.find("stroke"); sit != tag.attrs.end() && !sit->second.empty()) {
                        style.color_hex = sit->second;
                    }
                    if (auto wit = tag.attrs.find("stroke-width"); wit != tag.attrs.end()) {
                        style.line_weight_mm = std::strtod(wit->second.c_str(), nullptr) * unit_scale_to_mm;
                    }
                    if (auto dit = tag.attrs.find("stroke-dasharray"); dit != tag.attrs.end()) {
                        style.pattern = parse_dasharray(dit->second);
                    }
                    if (out_doc.layers.find_layer(layer_name) == nullptr) {
                        out_doc.layers.add_layer(layer_name, "", style);
                    } else {
                        out_doc.layers.set_style(layer_name, style);
                    }
                    if (auto dis = tag.attrs.find("display"); dis != tag.attrs.end() && dis->second == "none") {
                        out_doc.layers.set_visible(layer_name, false);
                    }
                }
            }
            if (!tag.is_self_closing) {
                group_stack.push_back({layer_name, is_root});
            }
            continue;
        }

        const auto attr_d = [&](std::string_view key, double def = 0.0) {
            auto it = tag.attrs.find(std::string(key));
            return (it != tag.attrs.end()) ? std::strtod(it->second.c_str(), nullptr) : def;
        };

        if (tag.name == "line") {
            const Point2D p0{attr_d("x1") * unit_scale_to_mm, attr_d("y1") * unit_scale_to_mm};
            const Point2D p1{attr_d("x2") * unit_scale_to_mm, attr_d("y2") * unit_scale_to_mm};
            out_doc.sketch.add_entity(std::make_shared<LineSegment2D>(p0, p1), false, active_layer);
        } else if (tag.name == "circle") {
            const Point2D c{attr_d("cx") * unit_scale_to_mm, attr_d("cy") * unit_scale_to_mm};
            const double r = attr_d("r") * unit_scale_to_mm;
            out_doc.sketch.add_entity(std::make_shared<Circle2D>(c, r), false, active_layer);
        } else if (tag.name == "ellipse") {
            const Point2D c{attr_d("cx") * unit_scale_to_mm, attr_d("cy") * unit_scale_to_mm};
            const double rx = attr_d("rx") * unit_scale_to_mm;
            const double ry = attr_d("ry") * unit_scale_to_mm;
            out_doc.sketch.add_entity(std::make_shared<Ellipse2D>(c, rx, ry, 0.0), false, active_layer);
        } else if (tag.name == "rect") {
            const double x = attr_d("x") * unit_scale_to_mm;
            const double y = attr_d("y") * unit_scale_to_mm;
            const double w = attr_d("width") * unit_scale_to_mm;
            const double h = attr_d("height") * unit_scale_to_mm;
            out_doc.sketch.add_entity(
                std::make_shared<Polyline2D>(
                    std::vector<Point2D>{{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}}, true),
                false,
                active_layer
            );
        } else if (tag.name == "polyline" || tag.name == "polygon") {
            if (auto it = tag.attrs.find("points"); it != tag.attrs.end()) {
                const auto nums = parse_number_list(it->second);
                std::vector<Point2D> pts;
                for (size_t k = 0; k + 1 < nums.size(); k += 2) {
                    pts.push_back({nums[k] * unit_scale_to_mm, nums[k + 1] * unit_scale_to_mm});
                }
                if (pts.size() >= 2) {
                    out_doc.sketch.add_entity(
                        std::make_shared<Polyline2D>(std::move(pts), tag.name == "polygon"),
                        false,
                        active_layer
                    );
                }
            }
        } else if (tag.name == "path") {
            if (auto it = tag.attrs.find("d"); it != tag.attrs.end()) {
                std::vector<std::shared_ptr<Curve2D>> curves;
                parse_svg_path_d(it->second, unit_scale_to_mm, curves);
                for (const auto& c : curves) {
                    out_doc.sketch.add_entity(c, false, active_layer);
                }
            }
        }
    }

    return found_svg;
}

bool SvgExchange::import_svg_file(std::string_view file_path, SvgDocument& out_doc) {
    std::ifstream ifs{std::string(file_path), std::ios::binary};
    if (!ifs.is_open()) return false;
    std::ostringstream ss;
    ss << ifs.rdbuf();
    return import_svg(ss.str(), out_doc);
}

} // namespace shunya::cad2d
