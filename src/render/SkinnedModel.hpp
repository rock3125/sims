#pragma once

#include "render/Mesh.hpp"
#include "render/Skeleton.hpp"
#include "render/Texture.hpp"

#include <glm/glm.hpp>

#include <memory>
#include <string>
#include <vector>

struct aiScene; // forward decl — keeps Assimp out of the header

namespace sims {

// A skinned mesh submesh: GL buffers with bone attributes + its material.
struct SkinnedModelMesh {
    Mesh mesh;
    Texture* texture = nullptr;   // non-owning; may be null → untextured
    glm::vec4 base_color{1.0f};
    bool has_bones = false;       // false → mesh is static (no skinning)
};

// Loads a skinned character model (glTF/FBX) via Assimp: vertex bone weights,
// the node armature, and all animation clips. Stores a Skeleton + an Animator
// ready to drive linear-blend skinning in the vertex shader.
class SkinnedModel {
public:
    SkinnedModel() = default;

    // Returns false on load failure; model is left empty.
    bool load_from_file(const std::string& path);

    // Merge animation clips from a separate file (e.g. a Mixamo Walk.fbx)
    // onto the already-loaded skeleton. Channels target nodes by name, so the
    // animation file must use the same rig as the avatar. `clip_name_override`
    // (if non-empty) replaces each imported clip's name — useful because
    // Mixamo FBX clips are all named "mixamo.com". Returns false if no clips
    // were added. No-op if the main model hasn't been loaded yet.
    bool load_animations_from_file(const std::string& path,
                                   const std::string& clip_name_override = "");

    bool valid() const { return !meshes_.empty(); }
    const Skeleton& skeleton() const { return skeleton_; }
    const std::vector<std::string>& clip_names() const { return clip_names_; }
    const std::vector<SkinnedModelMesh>& meshes() const { return meshes_; }

    // Axis-aligned bounding box of bind-pose vertices (model space). Used by
    // the avatar to normalize scale (Mixamo FBX exports in centimeters).
    glm::vec3 bbox_min() const { return bbox_min_; }
    glm::vec3 bbox_max() const { return bbox_max_; }
    float height() const { return bbox_max_.y - bbox_min_.y; }

    // Draw every submesh. Caller must have a skinning shader bound with
    // u_view_proj/u_model/u_bone_matrices[0..N-1] set, plus lighting uniforms.
    // `bone_matrices` must be sized to skeleton().bones().size(); pass nullptr
    // for static (non-skinned) fallback rendering.
    void draw(const std::vector<glm::mat4>& bone_matrices) const;

private:
    std::vector<SkinnedModelMesh> meshes_;
    std::vector<std::unique_ptr<Texture>> textures_;
    std::string directory_;
    Skeleton skeleton_;
    std::vector<std::string> clip_names_;
    glm::vec3 bbox_min_{0.0f};
    glm::vec3 bbox_max_{0.0f};

    void import_clips(const aiScene* scene, const std::string& name_override);
};

} // namespace sims
