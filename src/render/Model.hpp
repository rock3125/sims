#pragma once

#include "render/Mesh.hpp"
#include "render/Texture.hpp"

#include <glm/glm.hpp>

#include <memory>
#include <string>
#include <vector>

namespace sims {

// A textured mesh submesh within a Model. Owns its Material's texture
// (loaded once by the Model and shared via pointer).
struct ModelMesh {
    Mesh mesh;
    Texture* texture = nullptr;   // non-owning; may be null → untextured
    glm::vec4 base_color{1.0f};   // fallback tint when texture is null
    glm::mat4 transform{1.0f};    // local transform relative to model root
};

// Loads a 3D model (glTF/FBX/OBJ) via Assimp and stores all submeshes
// with their textures. Texture paths are resolved relative to the model file.
// All meshes are baked into the same vertex format (Vertex) at load time.
class Model {
public:
    Model() = default;

    // Returns false on load failure; model is left empty.
    bool load_from_file(const std::string& path);

    // Draw every submesh with the currently bound shader. Does NOT set
    // model uniform — caller is responsible for `set_mat4("u_model", ...)`.
    void draw() const;

    bool valid() const { return !meshes_.empty(); }
    const std::vector<ModelMesh>& meshes() const { return meshes_; }

private:
    std::vector<ModelMesh> meshes_;
    std::vector<std::unique_ptr<Texture>> textures_; // owning storage
    std::string directory_;

    Texture* load_texture(const std::string& path);
};

} // namespace sims
