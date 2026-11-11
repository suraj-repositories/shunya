#pragma once

#include "shunya/core/base/result.hpp"

#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace shunya::bench {

struct VcdSignalDef {
    std::string symbol;
    std::string name;
    int width{1};
};

struct VcdTransition {
    uint64_t timestamp{0};
    std::string signal_name;
    char logic_char{'0'};
};

struct VcdTraceDocument {
    std::string timescale{"1ps"};
    std::vector<VcdSignalDef> signals;
    std::vector<VcdTransition> transitions;
};

/**
 * @brief IEEE 1364 / 1164 Value Change Dump (.vcd) serializer & parser for digital timing benchmarks.
 */
class VcdParser {
public:
    [[nodiscard]] static std::string serialize(const VcdTraceDocument& doc) {
        std::ostringstream os;
        os << "$timescale " << doc.timescale << " $end\n"
           << "$scope module bench $end\n";
        for (const auto& s : doc.signals) {
            os << "$var wire " << s.width << " " << s.symbol << " " << s.name << " $end\n";
        }
        os << "$upscope $end\n$enddefinitions $end\n";

        std::unordered_map<std::string, std::string> name_to_sym;
        for (const auto& s : doc.signals) name_to_sym[s.name] = s.symbol;

        uint64_t last_t = UINT64_MAX;
        for (const auto& tr : doc.transitions) {
            if (tr.timestamp != last_t) {
                os << "#" << tr.timestamp << "\n";
                last_t = tr.timestamp;
            }
            const auto it = name_to_sym.find(tr.signal_name);
            if (it != name_to_sym.end()) {
                os << tr.logic_char << it->second << "\n";
            }
        }
        return os.str();
    }

    [[nodiscard]] static core::base::Result<VcdTraceDocument> parse(std::string_view vcd_text) {
        VcdTraceDocument doc;
        std::unordered_map<std::string, std::string> sym_to_name;
        uint64_t current_t = 0;

        std::istringstream is{std::string(vcd_text)};
        std::string line;
        while (std::getline(is, line)) {
            if (line.empty()) continue;
            if (line.rfind("$timescale", 0) == 0) {
                std::istringstream ls(line);
                std::string kw, ts;
                ls >> kw >> ts;
                if (!ts.empty() && ts != "$end") doc.timescale = ts;
            } else if (line.rfind("$var", 0) == 0) {
                std::istringstream ls(line);
                std::string kw, type, sym, name;
                int width = 1;
                ls >> kw >> type >> width >> sym >> name;
                if (!sym.empty() && !name.empty()) {
                    doc.signals.push_back(VcdSignalDef{sym, name, width});
                    sym_to_name[sym] = name;
                }
            } else if (line[0] == '#') {
                current_t = std::stoull(line.substr(1));
            } else if (line[0] != '$' && line.size() >= 2) {
                const char val = line[0];
                const std::string sym = line.substr(1);
                const auto it = sym_to_name.find(sym);
                if (it != sym_to_name.end()) {
                    doc.transitions.push_back(VcdTransition{current_t, it->second, val});
                }
            }
        }

        if (doc.signals.empty()) {
            return core::base::Error{
                601, core::base::ErrorDomain::IO, core::base::Severity::Error,
                "Invalid VCD stream: no $var signals parsed", ""
            };
        }
        return doc;
    }
};

} // namespace shunya::bench
