#include "shunya/rig3d/modifier_stack.hpp"

#include <algorithm>
#include <map>
#include <set>

namespace shunya::rig3d {

static void recompute_solid_normals(SolidMesh3D& mesh) {
    mesh.normals.assign(mesh.vertices.size(), Vec3d(0.0, 0.0, 0.0));
    for (const auto& tri : mesh.triangles) {
        if (tri[0] >= mesh.vertices.size() || tri[1] >= mesh.vertices.size() || tri[2] >= mesh.vertices.size()) {
            continue;
        }
        const auto& p0 = mesh.vertices[tri[0]];
        const auto& p1 = mesh.vertices[tri[1]];
        const auto& p2 = mesh.vertices[tri[2]];
        const Vec3d fn = (p1 - p0).cross(p2 - p0);
        mesh.normals[tri[0]] += fn;
        mesh.normals[tri[1]] += fn;
        mesh.normals[tri[2]] += fn;
    }
    for (auto& n : mesh.normals) {
        if (n.length_squared() > 1e-12) {
            n = n.normalized();
        } else {
            n = Vec3d(0.0, 0.0, 1.0);
        }
    }
}

HalfEdgeMesh ModifierStage::evaluate(const HalfEdgeMesh& input) const {
    SolidMesh3D solid_in = input.to_solid_mesh();
    SolidMesh3D solid_out = evaluate(solid_in);
    HalfEdgeMesh he_out;
    he_out.build_from_solid(solid_out);
    return he_out;
}

// =============================================================================
// Mirror Modifier Implementation (PH4-11.01.002)
// =============================================================================
SolidMesh3D MirrorModifier::evaluate(const SolidMesh3D& input) const {
    if (!enabled_) return input;

    SolidMesh3D output = input;
    const size_t orig_vcount = input.vertices.size();
    if (orig_vcount == 0) return output;

    auto mirror_pt = [this](Point3D p) -> Point3D {
        if (config_.mirror_x) p.x = 2.0 * config_.mirror_plane_offset - p.x;
        if (config_.mirror_y) p.y = 2.0 * config_.mirror_plane_offset - p.y;
        if (config_.mirror_z) p.z = 2.0 * config_.mirror_plane_offset - p.z;
        return p;
    };

    std::vector<uint32_t> mir_indices(orig_vcount);

    for (size_t i = 0; i < orig_vcount; ++i) {
        Point3D orig_pos = input.vertices[i];

        if (config_.clip) {
            if (config_.mirror_x && std::abs(orig_pos.x - config_.mirror_plane_offset) < config_.merge_threshold) {
                orig_pos.x = config_.mirror_plane_offset;
                output.vertices[i].x = config_.mirror_plane_offset;
            }
            if (config_.mirror_y && std::abs(orig_pos.y - config_.mirror_plane_offset) < config_.merge_threshold) {
                orig_pos.y = config_.mirror_plane_offset;
                output.vertices[i].y = config_.mirror_plane_offset;
            }
            if (config_.mirror_z && std::abs(orig_pos.z - config_.mirror_plane_offset) < config_.merge_threshold) {
                orig_pos.z = config_.mirror_plane_offset;
                output.vertices[i].z = config_.mirror_plane_offset;
            }
        }

        Point3D mir_pos = mirror_pt(orig_pos);
        const double dist = (mir_pos - orig_pos).length();

        if (config_.merge && dist <= config_.merge_threshold) {
            mir_indices[i] = static_cast<uint32_t>(i);
        } else {
            const uint32_t new_idx = static_cast<uint32_t>(output.vertices.size());
            output.vertices.push_back(mir_pos);
            mir_indices[i] = new_idx;
        }
    }

    // Mirror triangles with reversed winding order
    const size_t orig_tcount = input.triangles.size();
    for (size_t t = 0; t < orig_tcount; ++t) {
        const uint32_t i0 = input.triangles[t][0];
        const uint32_t i1 = input.triangles[t][1];
        const uint32_t i2 = input.triangles[t][2];

        const uint32_t m0 = mir_indices[i0];
        const uint32_t m1 = mir_indices[i1];
        const uint32_t m2 = mir_indices[i2];

        if (m0 == m1 || m1 == m2 || m2 == m0) {
            continue; // Degenerate triangle
        }

        // Reversed winding order: (m0, m2, m1)
        output.triangles.push_back({m0, m2, m1});
    }

    recompute_solid_normals(output);
    return output;
}

// =============================================================================
// Array Modifier Implementation (PH4-11.01.003)
// =============================================================================
SolidMesh3D ArrayModifier::evaluate(const SolidMesh3D& input) const {
    if (!enabled_ || config_.count <= 1 || input.vertices.empty()) return input;

    SolidMesh3D output = input;

    Point3D bmin = input.vertices[0];
    Point3D bmax = input.vertices[0];
    for (const auto& v : input.vertices) {
        bmin.x = std::min(bmin.x, v.x);
        bmin.y = std::min(bmin.y, v.y);
        bmin.z = std::min(bmin.z, v.z);
        bmax.x = std::max(bmax.x, v.x);
        bmax.y = std::max(bmax.y, v.y);
        bmax.z = std::max(bmax.z, v.z);
    }
    const Vec3d bbox_size(bmax.x - bmin.x, bmax.y - bmin.y, bmax.z - bmin.z);

    Vec3d step_offset(0.0, 0.0, 0.0);
    if (config_.use_relative_offset) {
        step_offset.x += config_.relative_offset.x * bbox_size.x;
        step_offset.y += config_.relative_offset.y * bbox_size.y;
        step_offset.z += config_.relative_offset.z * bbox_size.z;
    }
    if (config_.use_constant_offset) {
        step_offset += config_.constant_offset;
    }

    const size_t vcount = input.vertices.size();
    const size_t tcount = input.triangles.size();

    for (uint32_t c = 1; c < config_.count; ++c) {
        const uint32_t v_offset = static_cast<uint32_t>(output.vertices.size());

        for (size_t i = 0; i < vcount; ++i) {
            Point3D nv = input.vertices[i];

            if (config_.use_circular) {
                const double angle = config_.circular_angle_rad * static_cast<double>(c);
                const double cos_a = std::cos(angle);
                const double sin_a = std::sin(angle);

                Point3D rel = nv - config_.circular_center;
                Point3D rot(
                    rel.x * cos_a - rel.y * sin_a,
                    rel.x * sin_a + rel.y * cos_a,
                    rel.z
                );
                nv = config_.circular_center + rot;
            } else {
                const Vec3d shift = step_offset * static_cast<double>(c);
                nv = nv + shift;
            }

            output.vertices.push_back(nv);
        }

        for (size_t j = 0; j < tcount; ++j) {
            output.triangles.push_back({
                input.triangles[j][0] + v_offset,
                input.triangles[j][1] + v_offset,
                input.triangles[j][2] + v_offset
            });
        }
    }

    recompute_solid_normals(output);
    return output;
}

// =============================================================================
// Solidify Modifier Implementation (PH4-11.01.004)
// =============================================================================
SolidMesh3D SolidifyModifier::evaluate(const SolidMesh3D& input) const {
    if (!enabled_ || std::abs(config_.thickness) < 1e-9 || input.vertices.empty()) return input;

    SolidMesh3D output;
    const size_t vcount = input.vertices.size();
    const size_t tcount = input.triangles.size();

    // Ensure input has valid vertex normals
    SolidMesh3D input_with_norms = input;
    if (input_with_norms.normals.size() != vcount) {
        recompute_solid_normals(input_with_norms);
    }

    const double t = config_.thickness;
    const double off = std::clamp(config_.offset, -1.0, 1.0);
    const double outer_disp = t * (off + 1.0) * 0.5;
    const double inner_disp = -t * (1.0 - off) * 0.5;

    output.vertices.resize(vcount * 2);

    for (size_t i = 0; i < vcount; ++i) {
        const auto& p = input_with_norms.vertices[i];
        Vec3d norm = (i < input_with_norms.normals.size()) ? input_with_norms.normals[i] : Vec3d(0, 0, 1);
        if (norm.length_squared() < 1e-12) norm = Vec3d(0, 0, 1);
        else norm = norm.normalized();

        // Outer vertex (0 .. vcount - 1)
        output.vertices[i] = p + norm * outer_disp;

        // Inner vertex (vcount .. 2*vcount - 1)
        output.vertices[i + vcount] = p + norm * inner_disp;
    }

    // Outer triangles
    for (size_t j = 0; j < tcount; ++j) {
        output.triangles.push_back(input.triangles[j]);
    }

    // Inner triangles (reverse winding order)
    for (size_t k = 0; k < tcount; ++k) {
        const uint32_t i0 = input.triangles[k][0] + static_cast<uint32_t>(vcount);
        const uint32_t i1 = input.triangles[k][1] + static_cast<uint32_t>(vcount);
        const uint32_t i2 = input.triangles[k][2] + static_cast<uint32_t>(vcount);

        output.triangles.push_back({i0, i2, i1});
    }

    // If fill_rim, detect open boundary edges and stitch quads
    if (config_.fill_rim) {
        std::map<std::pair<uint32_t, uint32_t>, uint32_t> edge_count;
        for (size_t k = 0; k < tcount; ++k) {
            uint32_t a = input.triangles[k][0];
            uint32_t b = input.triangles[k][1];
            uint32_t c = input.triangles[k][2];

            auto add_e = [&](uint32_t u, uint32_t w) {
                if (u > w) std::swap(u, w);
                edge_count[{u, w}]++;
            };
            add_e(a, b);
            add_e(b, c);
            add_e(c, a);
        }

        for (const auto& [edge, cnt] : edge_count) {
            if (cnt == 1) {
                const uint32_t va = edge.first;
                const uint32_t vb = edge.second;
                const uint32_t va_in = va + static_cast<uint32_t>(vcount);
                const uint32_t vb_in = vb + static_cast<uint32_t>(vcount);

                output.triangles.push_back({va, vb, vb_in});
                output.triangles.push_back({va, vb_in, va_in});
            }
        }
    }

    recompute_solid_normals(output);
    return output;
}

// =============================================================================
// Subdivision Surface (Subsurf) Implementation (PH4-11.01.005)
// =============================================================================
SolidMesh3D SubsurfModifier::evaluate(const SolidMesh3D& input) const {
    if (!enabled_ || config_.levels == 0 || input.vertices.empty()) return input;

    SolidMesh3D current = input;

    for (uint32_t lvl = 0; lvl < std::min(config_.levels, 4u); ++lvl) {
        SolidMesh3D next;
        const size_t tcount = current.triangles.size();

        // Face centers
        std::vector<Point3D> face_points(tcount);
        for (size_t f = 0; f < tcount; ++f) {
            const Point3D& p0 = current.vertices[current.triangles[f][0]];
            const Point3D& p1 = current.vertices[current.triangles[f][1]];
            const Point3D& p2 = current.vertices[current.triangles[f][2]];
            face_points[f] = Point3D(
                (p0.x + p1.x + p2.x) / 3.0,
                (p0.y + p1.y + p2.y) / 3.0,
                (p0.z + p1.z + p2.z) / 3.0
            );
        }

        struct EdgeInfo {
            uint32_t v0{0};
            uint32_t v1{0};
            std::vector<uint32_t> adj_faces;
            uint32_t new_vertex_idx{0};
        };

        std::map<std::pair<uint32_t, uint32_t>, EdgeInfo> edges;
        for (size_t f = 0; f < tcount; ++f) {
            for (size_t e = 0; e < 3; ++e) {
                uint32_t a = current.triangles[f][e];
                uint32_t b = current.triangles[f][(e + 1) % 3];
                uint32_t u = std::min(a, b);
                uint32_t w = std::max(a, b);
                auto& einfo = edges[{u, w}];
                einfo.v0 = u;
                einfo.v1 = w;
                einfo.adj_faces.push_back(static_cast<uint32_t>(f));
            }
        }

        // Copy vertices
        next.vertices = current.vertices;

        // Add edge vertices
        for (auto& [key, einfo] : edges) {
            const Point3D& pa = current.vertices[einfo.v0];
            const Point3D& pb = current.vertices[einfo.v1];
            const Point3D linear_mid((pa.x + pb.x) * 0.5, (pa.y + pb.y) * 0.5, (pa.z + pb.z) * 0.5);

            Point3D smooth_pt = linear_mid;
            if (einfo.adj_faces.size() == 2) {
                const Point3D& f0 = face_points[einfo.adj_faces[0]];
                const Point3D& f1 = face_points[einfo.adj_faces[1]];
                smooth_pt = Point3D(
                    (pa.x + pb.x + f0.x + f1.x) * 0.25,
                    (pa.y + pb.y + f0.y + f1.y) * 0.25,
                    (pa.z + pb.z + f0.z + f1.z) * 0.25
                );
            }

            const double c = std::clamp(config_.edge_crease, 0.0, 1.0);
            Point3D final_pt(
                (1.0 - c) * smooth_pt.x + c * linear_mid.x,
                (1.0 - c) * smooth_pt.y + c * linear_mid.y,
                (1.0 - c) * smooth_pt.z + c * linear_mid.z
            );

            einfo.new_vertex_idx = static_cast<uint32_t>(next.vertices.size());
            next.vertices.push_back(final_pt);
        }

        auto get_edge_v = [&](uint32_t a, uint32_t b) -> uint32_t {
            uint32_t u = std::min(a, b);
            uint32_t w = std::max(a, b);
            return edges[{u, w}].new_vertex_idx;
        };

        for (size_t f = 0; f < tcount; ++f) {
            uint32_t v0 = current.triangles[f][0];
            uint32_t v1 = current.triangles[f][1];
            uint32_t v2 = current.triangles[f][2];

            uint32_t m01 = get_edge_v(v0, v1);
            uint32_t m12 = get_edge_v(v1, v2);
            uint32_t m20 = get_edge_v(v2, v0);

            next.triangles.push_back({v0, m01, m20});
            next.triangles.push_back({v1, m12, m01});
            next.triangles.push_back({v2, m20, m12});
            next.triangles.push_back({m01, m12, m20});
        }

        recompute_solid_normals(next);
        current = std::move(next);
    }

    return current;
}

// =============================================================================
// Decimate Modifier Implementation (PH4-11.01.006)
// =============================================================================
SolidMesh3D DecimateModifier::evaluate(const SolidMesh3D& input) const {
    if (!enabled_ || input.triangles.empty()) return input;
    return MeshDecimator::simplify(input, params_);
}

// =============================================================================
// Modifier Stack Implementation (PH4-11.01.001 & PH4-11.01.007)
// =============================================================================
void ModifierStack::add_modifier(std::shared_ptr<ModifierStage> stage) {
    if (stage) {
        stages_.push_back(std::move(stage));
    }
}

bool ModifierStack::remove_modifier(size_t index) {
    if (index >= stages_.size()) return false;
    stages_.erase(stages_.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

bool ModifierStack::move_up(size_t index) {
    if (index == 0 || index >= stages_.size()) return false;
    std::swap(stages_[index], stages_[index - 1]);
    return true;
}

bool ModifierStack::move_down(size_t index) {
    if (index + 1 >= stages_.size()) return false;
    std::swap(stages_[index], stages_[index + 1]);
    return true;
}

void ModifierStack::clear() noexcept {
    stages_.clear();
}

std::shared_ptr<ModifierStage> ModifierStack::get(size_t index) const {
    if (index < stages_.size()) {
        return stages_[index];
    }
    return nullptr;
}

SolidMesh3D ModifierStack::evaluate(const SolidMesh3D& base_mesh) const {
    SolidMesh3D current = base_mesh;
    for (const auto& stage : stages_) {
        if (stage && stage->is_enabled()) {
            current = stage->evaluate(current);
        }
    }
    return current;
}

HalfEdgeMesh ModifierStack::evaluate(const HalfEdgeMesh& base_mesh) const {
    SolidMesh3D solid_in = base_mesh.to_solid_mesh();
    SolidMesh3D solid_out = evaluate(solid_in);
    HalfEdgeMesh he_out;
    he_out.build_from_solid(solid_out);
    return he_out;
}

bool ModifierStack::apply_modifier(size_t index, SolidMesh3D& inout_mesh) {
    if (index >= stages_.size()) return false;
    auto stage = stages_[index];
    if (stage && stage->is_enabled()) {
        inout_mesh = stage->evaluate(inout_mesh);
    }
    stages_.erase(stages_.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

bool ModifierStack::apply_modifier(size_t index, HalfEdgeMesh& inout_mesh) {
    SolidMesh3D solid = inout_mesh.to_solid_mesh();
    if (!apply_modifier(index, solid)) return false;
    inout_mesh.build_from_solid(solid);
    return true;
}

void ModifierStack::apply_all(SolidMesh3D& inout_mesh) {
    inout_mesh = evaluate(inout_mesh);
    clear();
}

void ModifierStack::apply_all(HalfEdgeMesh& inout_mesh) {
    inout_mesh = evaluate(inout_mesh);
    clear();
}

} // namespace shunya::rig3d
