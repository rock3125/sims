#pragma once

#include <glad/gl.h>
#include <glm/glm.hpp>

#include <string>
#include <vector>

namespace sims {

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texcoord;
};

// A single drawable mesh: VAO + VBO + IBO, owning its GL buffers.
// Non-copyable, movable. Renders with glDrawElements.
class Mesh {
public:
    Mesh() = default;
    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&& o) noexcept;
    Mesh& operator=(Mesh&& o) noexcept;

    void upload(const std::vector<Vertex>& verts, const std::vector<unsigned int>& indices);
    void draw() const;

    GLsizei index_count() const { return index_count_; }

    // Build simple primitive meshes (used as placeholders until real assets land).
    static Mesh make_cube(float size = 1.0f);
    static Mesh make_plane(float w, float d, int x_segments = 1, int z_segments = 1);

private:
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint ibo_ = 0;
    GLsizei index_count_ = 0;
};

} // namespace sims
