#include "shunya/cad3d/bsp_tree.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::cad3d {

namespace {
constexpr double BSP_EPS = 1e-6;
constexpr int MAX_BSP_DEPTH = 48;

enum Classification : int {
    COPLANAR = 0,
    FRONT = 1,
    BACK = 2,
    SPANNING = 3
};

void split_polygon(
    const Plane3D& plane,
    const BspPolygon& poly,
    std::vector<BspPolygon>& coplanar_front,
    std::vector<BspPolygon>& coplanar_back,
    std::vector<BspPolygon>& front,
    std::vector<BspPolygon>& back
) {
    int polygon_type = COPLANAR;
    std::vector<int> types(poly.vertices.size(), COPLANAR);

    for (size_t i = 0; i < poly.vertices.size(); ++i) {
        const double t = plane.normal.dot(poly.vertices[i]) - plane.w;
        int type = COPLANAR;
        if (t < -BSP_EPS) type = BACK;
        else if (t > BSP_EPS) type = FRONT;
        polygon_type |= type;
        types[i] = type;
    }

    switch (polygon_type) {
        case COPLANAR:
            if (plane.normal.dot(poly.plane.normal) > 0.0) {
                coplanar_front.push_back(poly);
            } else {
                coplanar_back.push_back(poly);
            }
            break;
        case FRONT:
            front.push_back(poly);
            break;
        case BACK:
            back.push_back(poly);
            break;
        case SPANNING: {
            std::vector<Point3D> f_verts;
            std::vector<Point3D> b_verts;
            const size_t n = poly.vertices.size();
            for (size_t i = 0; i < n; ++i) {
                const size_t j = (i + 1) % n;
                const int ti = types[i];
                const int tj = types[j];
                const Point3D& vi = poly.vertices[i];
                const Point3D& vj = poly.vertices[j];

                if (ti != BACK) f_verts.push_back(vi);
                if (ti != FRONT) b_verts.push_back(vi);

                if ((ti | tj) == SPANNING) {
                    const double denom = plane.normal.dot(vj - vi);
                    const double t = (std::abs(denom) > 1e-18)
                        ? std::clamp((plane.w - plane.normal.dot(vi)) / denom, 0.0, 1.0)
                        : 0.5;
                    const Point3D mid = vi + (vj - vi) * t;
                    f_verts.push_back(mid);
                    b_verts.push_back(mid);
                }
            }
            auto push_clean = [&](std::vector<Point3D>& verts, std::vector<BspPolygon>& dest) {
                std::vector<Point3D> clean;
                clean.reserve(verts.size());
                for (const auto& v : verts) {
                    if (clean.empty() || (v - clean.back()).length_squared() > 1e-16) {
                        clean.push_back(v);
                    }
                }
                if (clean.size() >= 2 && (clean.front() - clean.back()).length_squared() <= 1e-16) {
                    clean.pop_back();
                }
                if (clean.size() >= 3) {
                    BspPolygon sub;
                    sub.vertices = std::move(clean);
                    sub.plane = poly.plane;
                    dest.push_back(std::move(sub));
                }
            };
            push_clean(f_verts, front);
            push_clean(b_verts, back);
            break;
        }
    }
}

} // namespace

Plane3D Plane3D::from_points(const Point3D& a, const Point3D& b, const Point3D& c) noexcept {
    const Vec3d n = (b - a).cross(c - a).normalized();
    return {n, n.dot(a)};
}

void Plane3D::flip() noexcept {
    normal = -normal;
    w = -w;
}

BspPolygon::BspPolygon(std::vector<Point3D> verts)
    : vertices(std::move(verts)) {
    if (vertices.size() >= 3) {
        plane = Plane3D::from_points(vertices[0], vertices[1], vertices[2]);
    }
}

void BspPolygon::flip() noexcept {
    std::reverse(vertices.begin(), vertices.end());
    plane.flip();
}

BspNode::BspNode(const std::vector<BspPolygon>& polygons) {
    build(polygons, 0);
}

void BspNode::invert() {
    for (auto& poly : coplanar_) {
        poly.flip();
    }
    if (has_plane_) {
        plane_.flip();
    }
    if (front_) front_->invert();
    if (back_) back_->invert();
    std::swap(front_, back_);
}

std::vector<BspPolygon> BspNode::clip_polygons(const std::vector<BspPolygon>& polygons) const {
    if (!has_plane_) return polygons;
    std::vector<BspPolygon> front_list;
    std::vector<BspPolygon> back_list;
    for (const auto& poly : polygons) {
        split_polygon(plane_, poly, front_list, back_list, front_list, back_list);
    }
    if (front_) front_list = front_->clip_polygons(front_list);
    if (back_) back_list = back_->clip_polygons(back_list);
    else back_list.clear();

    front_list.insert(
        front_list.end(),
        std::make_move_iterator(back_list.begin()),
        std::make_move_iterator(back_list.end())
    );
    return front_list;
}

void BspNode::clip_to(const BspNode& bsp) {
    coplanar_ = bsp.clip_polygons(coplanar_);
    if (front_) front_->clip_to(bsp);
    if (back_) back_->clip_to(bsp);
}

std::vector<BspPolygon> BspNode::all_polygons() const {
    std::vector<BspPolygon> out = coplanar_;
    if (front_) {
        auto f = front_->all_polygons();
        out.insert(out.end(), std::make_move_iterator(f.begin()), std::make_move_iterator(f.end()));
    }
    if (back_) {
        auto b = back_->all_polygons();
        out.insert(out.end(), std::make_move_iterator(b.begin()), std::make_move_iterator(b.end()));
    }
    return out;
}

void BspNode::build(const std::vector<BspPolygon>& polygons, int depth) {
    if (polygons.empty()) return;
    if (depth >= MAX_BSP_DEPTH) {
        coplanar_.insert(coplanar_.end(), polygons.begin(), polygons.end());
        return;
    }

    if (!has_plane_) {
        // Choose valid plane (prefer median/evenly-splitting plane among first few candidates)
        plane_ = polygons[0].plane;
        for (const auto& p : polygons) {
            if (p.plane.normal.length_squared() > 0.5) {
                plane_ = p.plane;
                break;
            }
        }
        has_plane_ = true;
    }

    std::vector<BspPolygon> front_list;
    std::vector<BspPolygon> back_list;
    for (const auto& poly : polygons) {
        split_polygon(plane_, poly, coplanar_, coplanar_, front_list, back_list);
    }

    if (!front_list.empty()) {
        if (!front_) front_ = std::make_unique<BspNode>();
        front_->build(front_list, depth + 1);
    }
    if (!back_list.empty()) {
        if (!back_) back_ = std::make_unique<BspNode>();
        back_->build(back_list, depth + 1);
    }
}

std::vector<BspPolygon> BspNode::mesh_to_polygons(const SolidMesh3D& mesh) {
    std::vector<BspPolygon> out;
    out.reserve(mesh.triangles.size());
    for (const auto& t : mesh.triangles) {
        const Point3D& a = mesh.vertices[t[0]];
        const Point3D& b = mesh.vertices[t[1]];
        const Point3D& c = mesh.vertices[t[2]];
        if ((b - a).cross(c - a).length_squared() > 1e-20) {
            out.emplace_back(std::vector<Point3D>{a, b, c});
        }
    }
    return out;
}

SolidMesh3D BspNode::polygons_to_mesh(const std::vector<BspPolygon>& polygons) {
    SolidMesh3D mesh;
    for (const auto& poly : polygons) {
        if (poly.vertices.size() < 3) continue;
        std::vector<uint32_t> idxs;
        idxs.reserve(poly.vertices.size());
        for (const auto& v : poly.vertices) {
            idxs.push_back(mesh.add_vertex(v));
        }
        for (size_t i = 2; i < idxs.size(); ++i) {
            mesh.add_triangle(idxs[0], idxs[i - 1], idxs[i]);
        }
    }
    mesh.weld_vertices(1e-7);
    return mesh;
}

} // namespace shunya::cad3d
