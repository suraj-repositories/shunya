#include "shunya/pcb/excellon_exporter.hpp"

#include <iomanip>
#include <map>
#include <sstream>

namespace shunya::pcb {

std::string ExcellonExporter::export_drill_file(
    const std::vector<PcbVia>& vias,
    const std::vector<MountingHole>& mounting_holes) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(3);
    out << "M48\n";
    out << "; Shunya EDA Excellon Drill File\n";
    out << "METRIC,TZ\n";
    out << "FMAT,2\n";

    std::map<double, int> tool_ids;
    int next_t = 1;
    for (const auto& v : vias) {
        if (!tool_ids.count(v.drill_mm)) tool_ids[v.drill_mm] = next_t++;
    }
    for (const auto& mh : mounting_holes) {
        if (!tool_ids.count(mh.drill_diameter_mm)) tool_ids[mh.drill_diameter_mm] = next_t++;
    }

    for (const auto& [dia, tid] : tool_ids) {
        out << "T" << tid << "C" << dia << "\n";
    }
    out << "%\nG90\nG05\n";

    for (const auto& [dia, tid] : tool_ids) {
        out << "T" << tid << "\n";
        for (const auto& v : vias) {
            if (v.drill_mm == dia) {
                out << "X" << v.center_mm.x << "Y" << v.center_mm.y << "\n";
            }
        }
        for (const auto& mh : mounting_holes) {
            if (mh.drill_diameter_mm == dia) {
                out << "X" << mh.center_mm.x << "Y" << mh.center_mm.y << "\n";
            }
        }
    }
    out << "M30\n";
    return out.str();
}

} // namespace shunya::pcb
