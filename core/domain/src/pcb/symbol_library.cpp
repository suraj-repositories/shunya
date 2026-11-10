#include "shunya/pcb/symbol_library.hpp"

#include <cmath>
#include <sstream>

namespace shunya::pcb {

namespace {

core::base::math::Vector2<double> xform_pt(const core::base::math::Vector2<double>& p,
                                           int rotation_deg,
                                           bool mirror_x,
                                           bool mirror_y) noexcept {
    double x = mirror_x ? -p.x : p.x;
    double y = mirror_y ? -p.y : p.y;
    const int rot = ((rotation_deg % 360) + 360) % 360;
    switch (rot) {
    case 90:  return {-y,  x};
    case 180: return {-x, -y};
    case 270: return { y, -x};
    default:  return { x,  y};
    }
}

} // namespace

SymbolDefinition SymbolDefinition::transformed(int rotation_deg,
                                               bool mirror_x,
                                               bool mirror_y) const {
    SymbolDefinition out = *this;
    for (auto& g : out.graphics) {
        if (g.kind == SymbolPrimitive::Kind::Line) {
            g.p0 = xform_pt(g.p0, rotation_deg, mirror_x, mirror_y);
            g.p1 = xform_pt(g.p1, rotation_deg, mirror_x, mirror_y);
        } else if (g.kind == SymbolPrimitive::Kind::Rect) {
            g.p0 = xform_pt(g.p0, rotation_deg, mirror_x, mirror_y);
            const int rot = ((rotation_deg % 360) + 360) % 360;
            if (rot == 90 || rot == 270) {
                g.p1 = {g.p1.y, g.p1.x};
            }
        } else if (g.kind == SymbolPrimitive::Kind::Circle) {
            g.p0 = xform_pt(g.p0, rotation_deg, mirror_x, mirror_y);
        }
    }
    for (auto& p : out.pins) {
        p.pos_mm = xform_pt(p.pos_mm, rotation_deg, mirror_x, mirror_y);
    }
    return out;
}

SymbolDefinition SymbolLibraryManager::get_symbol(std::string_view id) const {
    using K = SymbolPrimitive::Kind;
    using PT = PinElectricalType;

    if (id == "Resistor") {
        SymbolDefinition d{"Resistor", "Resistor", "R", {}, {
            {"1", "1", PT::Passive, {-5.08, 0.0}},
            {"2", "2", PT::Passive, { 5.08, 0.0}}
        }};
        if (standard_ == SymbolStandard::IEEE_US) {
            // Classic ANSI/IEEE zigzag
            d.graphics = {
                {K::Line, {-5.08, 0.0}, {-3.0, 0.0}},
                {K::Line, {-3.0, 0.0},  {-2.5, 1.0}},
                {K::Line, {-2.5, 1.0},  {-1.5,-1.0}},
                {K::Line, {-1.5,-1.0},  {-0.5, 1.0}},
                {K::Line, {-0.5, 1.0},  { 0.5,-1.0}},
                {K::Line, { 0.5,-1.0},  { 1.5, 1.0}},
                {K::Line, { 1.5, 1.0},  { 2.5,-1.0}},
                {K::Line, { 2.5,-1.0},  { 3.0, 0.0}},
                {K::Line, { 3.0, 0.0},  { 5.08, 0.0}}
            };
        } else {
            // IEC 60617 European rectangle box
            d.graphics = {
                {K::Line, {-5.08, 0.0}, {-3.0, 0.0}},
                {K::Rect, { 0.0,  0.0}, { 3.0, 1.2}},
                {K::Line, { 3.0,  0.0}, { 5.08, 0.0}}
            };
        }
        return d;
    }

    if (id == "Capacitor") {
        return SymbolDefinition{"Capacitor", "Capacitor", "C", {
            {K::Line, {-5.08, 0.0}, {-0.6, 0.0}},
            {K::Line, {-0.6, -2.5}, {-0.6, 2.5}},
            {K::Line, { 0.6, -2.5}, { 0.6, 2.5}},
            {K::Line, { 0.6,  0.0}, { 5.08, 0.0}}
        }, {{"1", "1", PT::Passive, {-5.08, 0.0}}, {"2", "2", PT::Passive, {5.08, 0.0}}}};
    }

    if (id == "Inductor") {
        SymbolDefinition d{"Inductor", "Inductor", "L", {}, {
            {"1", "1", PT::Passive, {-5.08, 0.0}},
            {"2", "2", PT::Passive, { 5.08, 0.0}}
        }};
        if (standard_ == SymbolStandard::IEEE_US) {
            d.graphics = {
                {K::Circle, {-2.0, 0.0}, {1.0, 0.0}},
                {K::Circle, { 0.0, 0.0}, {1.0, 0.0}},
                {K::Circle, { 2.0, 0.0}, {1.0, 0.0}}
            };
        } else {
            d.graphics = {
                {K::Rect, {0.0, 0.0}, {3.0, 1.2}}
            };
        }
        return d;
    }

    if (id == "Diode") {
        return SymbolDefinition{"Diode", "Diode", "D", {
            {K::Line, {-1.5, -1.5}, {-1.5, 1.5}},
            {K::Line, {-1.5,  1.5}, { 1.5, 0.0}},
            {K::Line, {-1.5, -1.5}, { 1.5, 0.0}},
            {K::Line, { 1.5, -1.5}, { 1.5, 1.5}}
        }, {{"1", "A", PT::Passive, {-5.08, 0.0}}, {"2", "K", PT::Passive, {5.08, 0.0}}}};
    }

    if (id == "OpAmp") {
        return SymbolDefinition{"OpAmp", "Operational Amplifier", "U", {
            {K::Line, {-5.08, -5.08}, { 5.08, 0.0}},
            {K::Line, { 5.08,  0.0},  {-5.08, 5.08}},
            {K::Line, {-5.08,  5.08}, {-5.08,-5.08}}
        }, {
            {"1", "IN+", PT::Input,  {-7.62,  2.54}},
            {"2", "IN-", PT::Input,  {-7.62, -2.54}},
            {"3", "OUT", PT::Output, { 7.62,  0.0}}
        }};
    }

    if (id == "MOSFET_NMOS") {
        return SymbolDefinition{"MOSFET_NMOS", "N-Channel MOSFET", "Q", {
            {K::Circle, {0.0, 0.0}, {3.5, 0.0}}
        }, {
            {"1", "G", PT::Input,   {-5.08,  0.0}},
            {"2", "D", PT::Passive, { 2.54,  5.08}},
            {"3", "S", PT::Passive, { 2.54, -5.08}}
        }};
    }

    // Logic NAND default
    return SymbolDefinition{"LogicGate_NAND", "2-Input NAND", "U", {
        {K::Rect, {0.0, 0.0}, {3.81, 3.81}},
        {K::Circle, {4.45, 0.0}, {0.64, 0.0}}
    }, {
        {"1", "A", PT::Input,  {-6.35,  2.54}},
        {"2", "B", PT::Input,  {-6.35, -2.54}},
        {"3", "Y", PT::Output, { 6.35,  0.0}}
    }};
}

std::vector<SymbolDefinition> SymbolLibraryManager::all_symbols() const {
    return {
        get_symbol("Resistor"),
        get_symbol("Capacitor"),
        get_symbol("Inductor"),
        get_symbol("Diode"),
        get_symbol("OpAmp"),
        get_symbol("MOSFET_NMOS"),
        get_symbol("LogicGate_NAND")
    };
}

std::string SymbolLibraryManager::to_json() const {
    std::ostringstream out;
    out << "{\n  \"standard\":\"" << (standard_ == SymbolStandard::IEEE_US ? "IEEE_US" : "IEC_EU")
        << "\",\n  \"symbols\":[\n";
    const auto syms = all_symbols();
    for (size_t i = 0; i < syms.size(); ++i) {
        const auto& s = syms[i];
        out << "    {\"id\":\"" << s.id << "\",\"pins\":" << s.pins.size()
            << ",\"primitives\":" << s.graphics.size() << "}";
        if (i + 1 < syms.size()) out << ",";
        out << "\n";
    }
    out << "  ]\n}\n";
    return out.str();
}

} // namespace shunya::pcb
