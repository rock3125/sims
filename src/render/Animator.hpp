#pragma once

#include "render/Skeleton.hpp"

#include <glm/glm.hpp>

#include <vector>

namespace sims {

// Samples AnimationClips on a Skeleton and produces the per-bone final
// matrices (`globalNodeTransform * offset`) used by the skinning shader.
// Supports one active clip at a time with looping and a playback speed.
class Animator {
public:
    Animator() = default;
    explicit Animator(const Skeleton* skel) : skeleton_(skel) {}

    void set_skeleton(const Skeleton* skel) { skeleton_ = skel; }
    const Skeleton* skeleton() const { return skeleton_; }

    // Play a clip by name. If `name` is not found or empty, playback stops
    // (rest pose is used). Resets local time to 0 unless `resume` is true.
    void play(const std::string& name, bool loop = true, float speed = 1.0f,
              bool resume = false);

    // Advance the active clip by `dt` (real seconds).
    void update(float dt);

    // Fill `out` with one mat4 per bone (indexed by bone index). Sized to
    // skeleton->bones().size(). When no clip is playing, the rest pose is
    // used (bone matrices = nodeGlobal * offset).
    void compute_bone_matrices(std::vector<glm::mat4>& out) const;

    const std::string& current_clip() const { return current_clip_; }
    bool playing() const { return current_ != nullptr; }
    float time_ticks() const { return time_ticks_; }

private:
    const Skeleton* skeleton_ = nullptr;
    const AnimationClip* current_ = nullptr;
    std::string current_clip_;
    float time_ticks_ = 0.0f;
    float speed_ = 1.0f;
    bool loop_ = true;

    void traverse(int node_idx, const glm::mat4& parent_global,
                  std::vector<glm::mat4>& node_globals,
                  bool ancestor_animated) const;
};

} // namespace sims
