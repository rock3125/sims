#pragma once

#include "render/Mesh.hpp"
#include "render/Shader.hpp"

#include <glm/glm.hpp>

namespace sims {

// Procedural humanoid Sim avatar drawn from cube primitives. Phase 3
// placeholder until real skeletal meshes + animation land. Walk cycle
// is driven by walk_phase (radians); facing is yaw around +Y.
class SimAvatar {
public:
    SimAvatar() = default;

    // Build the unit cube primitive (lazy on first draw).
    void init();

    // Draw the avatar at world_pos with given facing (degrees) and walk_phase.
    // If moving, legs/arms swing and the torso bobs. Parts are tinted via the
    // bound lit shader's u_base_color uniform per part.
    // Caller must have `lit` shader bound + u_view_proj/u_light_* set.
    void draw(Shader& lit, const glm::vec3& world_pos, float facing_deg,
              float walk_phase, bool moving);

private:
    Mesh cube_;
    bool inited_ = false;

    void draw_part(Shader& lit, const glm::mat4& model, const glm::vec3& color);
};

} // namespace sims
