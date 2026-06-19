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

// Vertex format augmented with linear-blend-skinning data: up to 4 bones per
// vertex. `bone_ids` index into a SkinnedModel's bone array; `bone_weights`
// are pre-normalized so they sum to 1 (or 0 for non-skinned vertices).
struct SkinnedVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texcoord;
    glm::ivec4 bone_ids{0, 0, 0, 0};
    glm::vec4 bone_weights{0.0f};
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
    void upload_skinned(const std::vector<SkinnedVertex>& verts,
                        const std::vector<unsigned int>& indices);
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
