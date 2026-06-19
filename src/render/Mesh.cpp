#include "render/Mesh.hpp"

#include <cstdio>

namespace sims {

Mesh::~Mesh() {
    if (vao_) {
        glDeleteVertexArrays(1, &vao_);
        glDeleteBuffers(1, &vbo_);
        glDeleteBuffers(1, &ibo_);
    }
}

Mesh::Mesh(Mesh&& o) noexcept
    : vao_(o.vao_), vbo_(o.vbo_), ibo_(o.ibo_), index_count_(o.index_count_) {
    o.vao_ = 0; o.vbo_ = 0; o.ibo_ = 0; o.index_count_ = 0;
}

Mesh& Mesh::operator=(Mesh&& o) noexcept {
    if (this != &o) {
        if (vao_) {
            glDeleteVertexArrays(1, &vao_);
            glDeleteBuffers(1, &vbo_);
            glDeleteBuffers(1, &ibo_);
        }
        vao_ = o.vao_; vbo_ = o.vbo_; ibo_ = o.ibo_; index_count_ = o.index_count_;
        o.vao_ = 0; o.vbo_ = 0; o.ibo_ = 0; o.index_count_ = 0;
    }
    return *this;
}

void Mesh::upload(const std::vector<Vertex>& verts, const std::vector<unsigned int>& indices) {
    if (!vao_) {
        glGenVertexArrays(1, &vao_);
        glGenBuffers(1, &vbo_);
        glGenBuffers(1, &ibo_);
    }
    glBindVertexArray(vao_);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(Vertex), verts.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texcoord));

    glBindVertexArray(0);
    index_count_ = static_cast<GLsizei>(indices.size());
}

void Mesh::upload_skinned(const std::vector<SkinnedVertex>& verts,
                          const std::vector<unsigned int>& indices) {
    if (!vao_) {
        glGenVertexArrays(1, &vao_);
        glGenBuffers(1, &vbo_);
        glGenBuffers(1, &ibo_);
    }
    glBindVertexArray(vao_);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(SkinnedVertex),
                 verts.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
                 indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex),
                          (void*)offsetof(SkinnedVertex, position));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex),
                          (void*)offsetof(SkinnedVertex, normal));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex),
                          (void*)offsetof(SkinnedVertex, texcoord));
    // Bone IDs (location 3) and weights (location 4) — used by lit_skin.vert.
    glEnableVertexAttribArray(3);
    glVertexAttribIPointer(3, 4, GL_INT, sizeof(SkinnedVertex),
                           (void*)offsetof(SkinnedVertex, bone_ids));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex),
                          (void*)offsetof(SkinnedVertex, bone_weights));

    glBindVertexArray(0);
    index_count_ = static_cast<GLsizei>(indices.size());
}

void Mesh::draw() const {
    if (!vao_) return;
    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, index_count_, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

Mesh Mesh::make_cube(float size) {
    // 24 vertices (4 per face) for per-face normals / UVs.
    const float h = size * 0.5f;
    std::vector<Vertex> v;
    v.reserve(24);
    std::vector<unsigned int> idx;
    idx.reserve(36);

    auto push_face = [&](glm::vec3 n, glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3) {
        unsigned int base = static_cast<unsigned int>(v.size());
        v.push_back({p0, n, {0, 0}});
        v.push_back({p1, n, {1, 0}});
        v.push_back({p2, n, {1, 1}});
        v.push_back({p3, n, {0, 1}});
        idx.insert(idx.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    };

    push_face({ 0,  1,  0}, {-h, h, -h}, { h, h, -h}, { h, h,  h}, {-h, h,  h}); // top
    push_face({ 0, -1,  0}, {-h,-h,  h}, { h,-h,  h}, { h,-h, -h}, {-h,-h, -h}); // bottom
    push_face({ 0,  0,  1}, {-h,-h,  h}, {-h, h,  h}, { h, h,  h}, { h,-h,  h}); // front
    push_face({ 0,  0, -1}, { h,-h, -h}, { h, h, -h}, {-h, h, -h}, {-h,-h, -h}); // back
    push_face({-1,  0,  0}, {-h,-h, -h}, {-h, h, -h}, {-h, h,  h}, {-h,-h,  h}); // left
    push_face({ 1,  0,  0}, { h,-h,  h}, { h, h,  h}, { h, h, -h}, { h,-h, -h}); // right

    Mesh m;
    m.upload(v, idx);
    return m;
}

Mesh Mesh::make_plane(float w, float d, int x_segments, int z_segments) {
    std::vector<Vertex> v;
    std::vector<unsigned int> idx;
    const int rows = z_segments + 1;
    const int cols = x_segments + 1;
    v.reserve(static_cast<size_t>(rows) * cols);
    idx.reserve(static_cast<size_t>(z_segments) * x_segments * 6);

    for (int z = 0; z < rows; ++z) {
        for (int x = 0; x < cols; ++x) {
            float u = static_cast<float>(x) / x_segments;
            float t = static_cast<float>(z) / z_segments;
            Vertex vert;
            vert.position = { -w * 0.5f + u * w, 0.0f, -d * 0.5f + t * d };
            vert.normal   = { 0.0f, 1.0f, 0.0f };
            vert.texcoord = { u * 4.0f, t * 4.0f }; // tile 4x for visible texture on floor
            v.push_back(vert);
        }
    }
    for (int z = 0; z < z_segments; ++z) {
        for (int x = 0; x < x_segments; ++x) {
            unsigned int a = static_cast<unsigned int>(z * cols + x);
            unsigned int b = a + 1;
            unsigned int c = a + cols;
            unsigned int e = c + 1;
            idx.insert(idx.end(), {a, c, b, b, c, e});
        }
    }
    Mesh m;
    m.upload(v, idx);
    return m;
}

} // namespace sims
