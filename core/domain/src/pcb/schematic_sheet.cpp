#include "shunya/pcb/schematic_sheet.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <functional>
#include <map>
#include <numeric>

namespace shunya::pcb {

namespace {

core::base::Error make_sch_error(std::string msg) {
    return core::base::Error{
        .code = 5001,
        .domain = core::base::ErrorDomain::Validation,
        .severity = core::base::Severity::Error,
        .message = std::move(msg),
        .details = ""
    };
}

bool point_eq(const core::base::math::Vector2<double>& a,
              const core::base::math::Vector2<double>& b,
              double tol = 1e-3) noexcept {
    return std::abs(a.x - b.x) <= tol && std::abs(a.y - b.y) <= tol;
}

bool point_on_segment(const core::base::math::Vector2<double>& p,
                      const core::base::math::Vector2<double>& a,
                      const core::base::math::Vector2<double>& b,
                      double tol = 1e-3) noexcept {
    const double min_x = std::min(a.x, b.x) - tol;
    const double max_x = std::max(a.x, b.x) + tol;
    const double min_y = std::min(a.y, b.y) - tol;
    const double max_y = std::max(a.y, b.y) + tol;
    if (p.x < min_x || p.x > max_x || p.y < min_y || p.y > max_y) return false;

    const auto ab = b - a;
    const double len = ab.length();
    if (len <= tol) return point_eq(p, a, tol);
    const double area2 = std::abs(ab.x * (p.y - a.y) - ab.y * (p.x - a.x));
    return (area2 / len) <= tol;
}

struct UnionFind {
    std::vector<size_t> parent;
    explicit UnionFind(size_t n) : parent(n) { std::iota(parent.begin(), parent.end(), 0); }
    size_t find(size_t x) {
        while (parent[x] != x) {
            parent[x] = parent[parent[x]];
            x = parent[x];
        }
        return x;
    }
    void unite(size_t a, size_t b) {
        a = find(a);
        b = find(b);
        if (a != b) parent[b] = a;
    }
};

struct NodeItem {
    enum class Kind { WireA, WireB, Pin, Label, Port, SubPin } kind;
    core::base::math::Vector2<double> pos;
    size_t owner_idx{0};
    size_t sub_idx{0};
};

} // namespace

std::vector<std::string> expand_bus_signal(std::string_view expr) {
    const auto lbr = expr.find('[');
    const auto dots = expr.find("..");
    const auto rbr = expr.find(']');
    if (lbr == std::string_view::npos || dots == std::string_view::npos ||
        rbr == std::string_view::npos || !(lbr < dots && dots < rbr)) {
        return {std::string(expr)};
    }
    const std::string base(expr.substr(0, lbr));
    auto parse_int = [](std::string_view s, int& out) -> bool {
        if (s.empty()) return false;
        int val = 0;
        for (char c : s) {
            if (!std::isdigit(static_cast<unsigned char>(c))) return false;
            val = val * 10 + (c - '0');
        }
        out = val;
        return true;
    };
    int a = 0, b = 0;
    if (!parse_int(expr.substr(lbr + 1, dots - (lbr + 1)), a) ||
        !parse_int(expr.substr(dots + 2, rbr - (dots + 2)), b)) {
        return {std::string(expr)};
    }
    std::vector<std::string> res;
    const int step = (a <= b) ? 1 : -1;
    for (int i = a; ; i += step) {
        res.push_back(base + "[" + std::to_string(i) + "]");
        if (i == b) break;
    }
    return res;
}

const CompiledNet* CompiledNetlist::find_net(std::string_view name) const noexcept {
    for (const auto& n : nets) {
        if (n.name == name) return &n;
    }
    return nullptr;
}

core::base::Result<void> SchematicDocument::add_sheet(SchematicSheet sheet) {
    if (sheet.id.empty()) {
        return make_sch_error("Schematic sheet ID cannot be empty");
    }
    sheets_[sheet.id] = std::move(sheet);
    return {};
}

SchematicSheet* SchematicDocument::get_sheet(std::string_view id) noexcept {
    auto it = sheets_.find(std::string(id));
    return (it != sheets_.end()) ? &it->second : nullptr;
}

const SchematicSheet* SchematicDocument::get_sheet(std::string_view id) const noexcept {
    auto it = sheets_.find(std::string(id));
    return (it != sheets_.end()) ? &it->second : nullptr;
}

CompiledNetlist SchematicDocument::compile_netlist() const {
    CompiledNetlist compiled;
    std::map<std::string, std::vector<CompiledNetPin>> net_pins;
    size_t anon_counter = 1;

    std::function<void(const std::string&, const std::string&,
                       const std::unordered_map<std::string, std::string>&, int)>
        flatten = [&](const std::string& sheet_id,
                      const std::string& path_prefix,
                      const std::unordered_map<std::string, std::string>& port_to_parent_net,
                      int depth) {
            if (depth > 16) return; // Guard recursive sheet cycles
            const auto* sheet = get_sheet(sheet_id);
            if (!sheet) return;

            std::vector<NodeItem> items;
            for (size_t wi = 0; wi < sheet->wires.size(); ++wi) {
                items.push_back({NodeItem::Kind::WireA, sheet->wires[wi].start_mm, wi, 0});
                items.push_back({NodeItem::Kind::WireB, sheet->wires[wi].end_mm, wi, 0});
            }
            for (size_t ci = 0; ci < sheet->components.size(); ++ci) {
                for (size_t pi = 0; pi < sheet->components[ci].pins.size(); ++pi) {
                    items.push_back({NodeItem::Kind::Pin, sheet->components[ci].pins[pi].pos_mm, ci, pi});
                }
            }
            for (size_t li = 0; li < sheet->labels.size(); ++li) {
                items.push_back({NodeItem::Kind::Label, sheet->labels[li].pos_mm, li, 0});
            }
            for (size_t pi = 0; pi < sheet->ports.size(); ++pi) {
                items.push_back({NodeItem::Kind::Port, sheet->ports[pi].pos_mm, pi, 0});
            }
            for (size_t si = 0; si < sheet->subsheets.size(); ++si) {
                for (size_t spi = 0; spi < sheet->subsheets[si].sheet_pins.size(); ++spi) {
                    items.push_back({NodeItem::Kind::SubPin,
                                     sheet->subsheets[si].sheet_pins[spi].pos_mm, si, spi});
                }
            }

            const size_t n = items.size();
            UnionFind uf(n);

            // Connect wire segment endpoints (2*wi <-> 2*wi+1, which sit at items[2*wi], items[2*wi+1])
            for (size_t wi = 0; wi < sheet->wires.size(); ++wi) {
                uf.unite(2 * wi, 2 * wi + 1);
            }
            // Connect coincident points and points lying on wire segments
            for (size_t i = 0; i < n; ++i) {
                for (size_t j = i + 1; j < n; ++j) {
                    if (point_eq(items[i].pos, items[j].pos)) {
                        uf.unite(i, j);
                    }
                }
                for (size_t wi = 0; wi < sheet->wires.size(); ++wi) {
                    if (point_on_segment(items[i].pos,
                                         sheet->wires[wi].start_mm,
                                         sheet->wires[wi].end_mm)) {
                        uf.unite(i, 2 * wi);
                    }
                }
            }

            // Group nodes by connected component root
            std::map<size_t, std::vector<size_t>> groups;
            for (size_t i = 0; i < n; ++i) {
                groups[uf.find(i)].push_back(i);
            }

            // Resolve canonical net name for each connected component
            std::map<size_t, std::string> root_to_net_name;
            for (const auto& [root, members] : groups) {
                std::string resolved_name;
                bool has_global = false;
                bool has_explicit = false;
                std::string port_bound_name;

                for (size_t idx : members) {
                    const auto& it = items[idx];
                    if (it.kind == NodeItem::Kind::Port) {
                        const auto& pname = sheet->ports[it.owner_idx].name;
                        auto bind_it = port_to_parent_net.find(pname);
                        if (bind_it != port_to_parent_net.end()) {
                            port_bound_name = bind_it->second;
                            has_explicit = true;
                        } else if (resolved_name.empty()) {
                            resolved_name = path_prefix + pname;
                            has_explicit = true;
                        }
                    } else if (it.kind == NodeItem::Kind::Label) {
                        const auto& lbl = sheet->labels[it.owner_idx];
                        if (lbl.is_global) {
                            resolved_name = lbl.net_name;
                            has_global = true;
                        } else if (!has_global && resolved_name.empty()) {
                            resolved_name = path_prefix + lbl.net_name;
                            has_explicit = true;
                        }
                    } else if (it.kind == NodeItem::Kind::WireA) {
                        const auto& hint = sheet->wires[it.owner_idx].net_hint;
                        if (!hint.empty() && !has_global && resolved_name.empty()) {
                            resolved_name = path_prefix + hint;
                            has_explicit = true;
                        }
                    }
                }

                if (!port_bound_name.empty() && !has_global) {
                    resolved_name = port_bound_name;
                }
                if (resolved_name.empty()) {
                    // Single isolated pin check
                    bool only_one_pin = (members.size() == 1 && items[members[0]].kind == NodeItem::Kind::Pin);
                    if (only_one_pin) {
                        const auto& p = sheet->components[items[members[0]].owner_idx].pins[items[members[0]].sub_idx];
                        if (!p.no_connect) {
                            compiled.unconnected_pins.push_back(CompiledNetPin{
                                path_prefix + sheet_id, p.component_ref, p.pin_number, p.pin_name, p.electrical_type
                            });
                        }
                        continue;
                    }
                    resolved_name = path_prefix + "N$" + std::to_string(anon_counter++);
                }
                (void)has_explicit;
                root_to_net_name[root] = resolved_name;

                for (size_t idx : members) {
                    const auto& it = items[idx];
                    if (it.kind == NodeItem::Kind::Pin) {
                        const auto& comp = sheet->components[it.owner_idx];
                        const auto& pin = comp.pins[it.sub_idx];
                        for (const auto& expanded_net : expand_bus_signal(resolved_name)) {
                            net_pins[expanded_net].push_back(CompiledNetPin{
                                path_prefix + sheet_id, comp.ref_des, pin.pin_number, pin.pin_name,
                                pin.electrical_type
                            });
                        }
                    }
                }
            }

            // Recurse into sub-sheet blocks
            for (size_t si = 0; si < sheet->subsheets.size(); ++si) {
                const auto& sub = sheet->subsheets[si];
                std::unordered_map<std::string, std::string> child_bindings;
                for (size_t spi = 0; spi < sub.sheet_pins.size(); ++spi) {
                    // Locate NodeItem matching SubPin(si, spi)
                    for (size_t idx = 0; idx < n; ++idx) {
                        if (items[idx].kind == NodeItem::Kind::SubPin &&
                            items[idx].owner_idx == si && items[idx].sub_idx == spi) {
                            const size_t r = uf.find(idx);
                            auto rn_it = root_to_net_name.find(r);
                            if (rn_it != root_to_net_name.end()) {
                                child_bindings[sub.sheet_pins[spi].name] = rn_it->second;
                            }
                            break;
                        }
                    }
                }
                flatten(sub.target_sheet_id, path_prefix + sub.instance_name + "/",
                        child_bindings, depth + 1);
            }
        };

    // Flatten starting from root, then also visit any disconnected top-level sibling sheets for global nets
    flatten(root_sheet_id_, "", {}, 0);

    // Visit any peer sheets not directly instantiated from root (supporting flat multi-sheet designs)
    std::map<std::string, bool> visited_flat;
    visited_flat[root_sheet_id_] = true;
    const auto* root_s = get_sheet(root_sheet_id_);
    if (root_s) {
        std::function<void(const SchematicSheet*)> mark = [&](const SchematicSheet* sh) {
            for (const auto& sub : sh->subsheets) {
                visited_flat[sub.target_sheet_id] = true;
                if (const auto* child = get_sheet(sub.target_sheet_id)) mark(child);
            }
        };
        mark(root_s);
    }
    for (const auto& [sid, sh] : sheets_) {
        (void)sh;
        if (!visited_flat[sid]) {
            flatten(sid, sid + "/", {}, 0);
        }
    }

    for (auto& [name, pins] : net_pins) {
        compiled.nets.push_back(CompiledNet{name, std::move(pins)});
    }
    return compiled;
}

} // namespace shunya::pcb
