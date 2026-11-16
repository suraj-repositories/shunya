#include "shunya/cad3d/half_edge_pool.hpp"

#include <stdexcept>
#include <unordered_map>

namespace shunya::cad3d {

PooledHalfEdgeMesh::PooledHalfEdgeMesh(size_t initial_chunk_size)
    : vertex_block_pool_(initial_chunk_size),
      half_edge_block_pool_(initial_chunk_size),
      face_block_pool_(initial_chunk_size) {
    vertices_.reserve(initial_chunk_size);
    vertex_active_.reserve(initial_chunk_size);
    half_edges_.reserve(initial_chunk_size * 3);
    half_edge_active_.reserve(initial_chunk_size * 3);
    faces_.reserve(initial_chunk_size);
    face_active_.reserve(initial_chunk_size);
}

uint32_t PooledHalfEdgeMesh::allocate_vertex(Point3D pos, Vec3d norm) {
    uint32_t idx = HE_INVALID;
    if (!vertex_free_list_.empty()) {
        idx = vertex_free_list_.back();
        vertex_free_list_.pop_back();
        vertices_[idx] = HEVertex{pos, norm, HE_INVALID};
        vertex_active_[idx].active = true;
    } else {
        idx = static_cast<uint32_t>(vertices_.size());
        vertices_.push_back(HEVertex{pos, norm, HE_INVALID});
        vertex_active_.push_back(SlotFlags{true});
    }
    ++active_vertices_;
    return idx;
}

void PooledHalfEdgeMesh::free_vertex(uint32_t v_idx) {
    if (v_idx >= vertices_.size() || !vertex_active_[v_idx].active) return;
    vertex_active_[v_idx].active = false;
    vertex_free_list_.push_back(v_idx);
    if (active_vertices_ > 0) --active_vertices_;
}

uint32_t PooledHalfEdgeMesh::allocate_half_edge(uint32_t origin_v) {
    uint32_t idx = HE_INVALID;
    if (!half_edge_free_list_.empty()) {
        idx = half_edge_free_list_.back();
        half_edge_free_list_.pop_back();
        half_edges_[idx] = HEHalfEdge{origin_v, HE_INVALID, HE_INVALID, HE_INVALID, HE_INVALID};
        half_edge_active_[idx].active = true;
    } else {
        idx = static_cast<uint32_t>(half_edges_.size());
        half_edges_.push_back(HEHalfEdge{origin_v, HE_INVALID, HE_INVALID, HE_INVALID, HE_INVALID});
        half_edge_active_.push_back(SlotFlags{true});
    }
    ++active_half_edges_;
    return idx;
}

void PooledHalfEdgeMesh::free_half_edge(uint32_t he_idx) {
    if (he_idx >= half_edges_.size() || !half_edge_active_[he_idx].active) return;
    half_edge_active_[he_idx].active = false;
    half_edge_free_list_.push_back(he_idx);
    if (active_half_edges_ > 0) --active_half_edges_;
}

uint32_t PooledHalfEdgeMesh::allocate_face(uint32_t he_idx, uint32_t degree) {
    uint32_t idx = HE_INVALID;
    if (!face_free_list_.empty()) {
        idx = face_free_list_.back();
        face_free_list_.pop_back();
        faces_[idx] = HEFace{he_idx, {0.0, 0.0, 0.0}, degree};
        face_active_[idx].active = true;
    } else {
        idx = static_cast<uint32_t>(faces_.size());
        faces_.push_back(HEFace{he_idx, {0.0, 0.0, 0.0}, degree});
        face_active_.push_back(SlotFlags{true});
    }
    ++active_faces_;
    return idx;
}

void PooledHalfEdgeMesh::free_face(uint32_t f_idx) {
    if (f_idx >= faces_.size() || !face_active_[f_idx].active) return;
    face_active_[f_idx].active = false;
    face_free_list_.push_back(f_idx);
    if (active_faces_ > 0) --active_faces_;
}

HEVertex& PooledHalfEdgeMesh::vertex(uint32_t idx) {
    return vertices_.at(idx);
}

const HEVertex& PooledHalfEdgeMesh::vertex(uint32_t idx) const {
    return vertices_.at(idx);
}

HEHalfEdge& PooledHalfEdgeMesh::half_edge(uint32_t idx) {
    return half_edges_.at(idx);
}

const HEHalfEdge& PooledHalfEdgeMesh::half_edge(uint32_t idx) const {
    return half_edges_.at(idx);
}

HEFace& PooledHalfEdgeMesh::face(uint32_t idx) {
    return faces_.at(idx);
}

const HEFace& PooledHalfEdgeMesh::face(uint32_t idx) const {
    return faces_.at(idx);
}

bool PooledHalfEdgeMesh::is_vertex_active(uint32_t idx) const noexcept {
    return idx < vertex_active_.size() && vertex_active_[idx].active;
}

bool PooledHalfEdgeMesh::is_half_edge_active(uint32_t idx) const noexcept {
    return idx < half_edge_active_.size() && half_edge_active_[idx].active;
}

bool PooledHalfEdgeMesh::is_face_active(uint32_t idx) const noexcept {
    return idx < face_active_.size() && face_active_[idx].active;
}

void PooledHalfEdgeMesh::load_from_mesh(const HalfEdgeMesh& mesh) {
    clear();
    for (const auto& v : mesh.vertices) {
        uint32_t vid = allocate_vertex(v.position, v.normal);
        vertices_[vid].half_edge = v.half_edge;
    }
    for (const auto& he : mesh.half_edges) {
        uint32_t hid = allocate_half_edge(he.vertex);
        half_edges_[hid].next = he.next;
        half_edges_[hid].prev = he.prev;
        half_edges_[hid].twin = he.twin;
        half_edges_[hid].face = he.face;
    }
    for (const auto& f : mesh.faces) {
        uint32_t fid = allocate_face(f.half_edge, f.degree);
        faces_[fid].normal = f.normal;
    }
}

HalfEdgeMesh PooledHalfEdgeMesh::to_compact_mesh() const {
    HalfEdgeMesh compact;
    std::unordered_map<uint32_t, uint32_t> v_map;
    std::unordered_map<uint32_t, uint32_t> he_map;
    std::unordered_map<uint32_t, uint32_t> f_map;

    v_map.reserve(active_vertices_);
    he_map.reserve(active_half_edges_);
    f_map.reserve(active_faces_);

    for (size_t i = 0; i < vertices_.size(); ++i) {
        if (vertex_active_[i].active) {
            uint32_t new_idx = static_cast<uint32_t>(compact.vertices.size());
            v_map[static_cast<uint32_t>(i)] = new_idx;
            compact.vertices.push_back(vertices_[i]);
        }
    }

    for (size_t i = 0; i < half_edges_.size(); ++i) {
        if (half_edge_active_[i].active) {
            uint32_t new_idx = static_cast<uint32_t>(compact.half_edges.size());
            he_map[static_cast<uint32_t>(i)] = new_idx;
            compact.half_edges.push_back(half_edges_[i]);
        }
    }

    for (size_t i = 0; i < faces_.size(); ++i) {
        if (face_active_[i].active) {
            uint32_t new_idx = static_cast<uint32_t>(compact.faces.size());
            f_map[static_cast<uint32_t>(i)] = new_idx;
            compact.faces.push_back(faces_[i]);
        }
    }

    // Remap vertex indices
    for (auto& v : compact.vertices) {
        if (v.half_edge != HE_INVALID) {
            auto it = he_map.find(v.half_edge);
            v.half_edge = (it != he_map.end()) ? it->second : HE_INVALID;
        }
    }

    // Remap half-edge indices
    for (auto& he : compact.half_edges) {
        if (he.vertex != HE_INVALID) {
            auto it = v_map.find(he.vertex);
            he.vertex = (it != v_map.end()) ? it->second : HE_INVALID;
        }
        if (he.next != HE_INVALID) {
            auto it = he_map.find(he.next);
            he.next = (it != he_map.end()) ? it->second : HE_INVALID;
        }
        if (he.prev != HE_INVALID) {
            auto it = he_map.find(he.prev);
            he.prev = (it != he_map.end()) ? it->second : HE_INVALID;
        }
        if (he.twin != HE_INVALID) {
            auto it = he_map.find(he.twin);
            he.twin = (it != he_map.end()) ? it->second : HE_INVALID;
        }
        if (he.face != HE_INVALID) {
            auto it = f_map.find(he.face);
            he.face = (it != f_map.end()) ? it->second : HE_INVALID;
        }
    }

    // Remap face indices
    for (auto& f : compact.faces) {
        if (f.half_edge != HE_INVALID) {
            auto it = he_map.find(f.half_edge);
            f.half_edge = (it != he_map.end()) ? it->second : HE_INVALID;
        }
    }

    return compact;
}

size_t PooledHalfEdgeMesh::memory_bytes_allocated() const noexcept {
    return vertices_.capacity() * sizeof(HEVertex) +
           vertex_active_.capacity() * sizeof(SlotFlags) +
           vertex_free_list_.capacity() * sizeof(uint32_t) +
           half_edges_.capacity() * sizeof(HEHalfEdge) +
           half_edge_active_.capacity() * sizeof(SlotFlags) +
           half_edge_free_list_.capacity() * sizeof(uint32_t) +
           faces_.capacity() * sizeof(HEFace) +
           face_active_.capacity() * sizeof(SlotFlags) +
           face_free_list_.capacity() * sizeof(uint32_t);
}

void PooledHalfEdgeMesh::clear() noexcept {
    vertices_.clear();
    vertex_active_.clear();
    vertex_free_list_.clear();
    active_vertices_ = 0;

    half_edges_.clear();
    half_edge_active_.clear();
    half_edge_free_list_.clear();
    active_half_edges_ = 0;

    faces_.clear();
    face_active_.clear();
    face_free_list_.clear();
    active_faces_ = 0;
}

} // namespace shunya::cad3d
