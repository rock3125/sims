#pragma once

#include "render/Animator.hpp"
#include "render/Mesh.hpp"
#include "render/Shader.hpp"
#include "render/SkinnedModel.hpp"

#include <glm/glm.hpp>

#include <string>

namespace sims {

// Drives the Sim's on-screen avatar. Two modes:
//
//   • Skinned — when a real glTF/FBX character asset (skeleton + animation
//     clips) is found under assets/models/, it is loaded via SkinnedModel and
//     animated with linear-blend skinning. Walk / idle clips are selected from
//     the Sim's movement state; run clip is used when available.
//
//   • Procedural — the original Phase-3 cube humanoid with a sine-wave walk
//     cycle. Used as a fallback so the build still runs without an asset.
//
// The caller (Renderer) must bind the appropriate shader and set the shared
// u_view_proj / u_light_dir / u_light_color / u_ambient uniforms before
// calling draw(). u_model and (for skinned) u_bone_matrices are set here.
class SimAvatar {
public:
    SimAvatar() = default;

    // Lazily builds the procedural cube and (if an asset exists) loads the
    // skinned model + skin shader. `assets_dir` is the project assets root.
    // Non-fatal: a missing/broken asset silently degrades to procedural.
    void init(const std::string& assets_dir);

    // Advance animation state by real-time dt (seconds).
    void update(float dt, bool moving);

    // Draw the avatar at world_pos with given facing (degrees) and walk_phase.
    // `lit` is used for the procedural path; `lit_skin` for the skinned path.
    void draw(Shader& lit, Shader& lit_skin, const glm::vec3& world_pos,
              float facing_deg, float walk_phase, bool moving);

    bool skinned() const { return mode_ == Mode::Skinned; }

private:
    enum class Mode { Uninit, Procedural, Skinned };
    Mode mode_ = Mode::Uninit;

    // Procedural fallback.
    Mesh cube_;

    // Skinned path.
    SkinnedModel model_;
    Animator animator_;
    Shader skin_shader_;
    std::vector<glm::mat4> bone_matrices_;
    std::string idle_clip_;
    std::string walk_clip_;
    std::string run_clip_;

    void load_skinned_asset(const std::string& assets_dir);
    void pick_clips();
    void draw_procedural(Shader& lit, const glm::vec3& world_pos,
                         float facing_deg, float walk_phase, bool moving);
    void draw_skinned(Shader& lit_skin, const glm::vec3& world_pos, float facing_deg);
    void draw_part(Shader& lit, const glm::mat4& model, const glm::vec3& color);
};

} // namespace sims
