#include "render/Animator.hpp"

#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

namespace sims {

namespace {

template <typename Key, typename Val>
Val sample_keys(const std::vector<std::pair<float, Key>>& keys, float t) {
    if (keys.empty()) return Val{};
    if (t <= keys.front().first) return keys.front().second;
    if (t >= keys.back().first) return keys.back().second;
    // Binary search for the upper bound.
    auto it = std::upper_bound(keys.begin(), keys.end(), t,
        [](float v, const std::pair<float, Key>& k) { return v < k.first; });
    auto hi = it;
    auto lo = std::prev(it);
    float span = hi->first - lo->first;
    float a = span > 1e-6f ? (t - lo->first) / span : 0.0f;
    if constexpr (std::is_same_v<Key, glm::quat>) {
        return glm::slerp(lo->second, hi->second, a);
    } else if constexpr (std::is_same_v<Key, glm::vec3>) {
        return glm::mix(lo->second, hi->second, a);
    }
}

glm::mat4 trs(const glm::vec3& t, const glm::quat& r, const glm::vec3& s) {
    glm::mat4 m = glm::translate(glm::mat4(1.0f), t) * glm::mat4_cast(r);
    m = glm::scale(m, s);
    return m;
}

} // namespace

void Animator::play(const std::string& name, bool loop, float speed, bool resume) {
    if (!skeleton_) { current_ = nullptr; current_clip_.clear(); return; }
    if (name.empty()) { current_ = nullptr; current_clip_.clear(); return; }
    const AnimationClip* clip = skeleton_->clip(name);
    current_ = clip;
    current_clip_ = name;
    loop_ = loop;
    speed_ = speed;
    if (!resume) time_ticks_ = 0.0f;
}

void Animator::update(float dt) {
    if (!current_ || !skeleton_) return;
    double tps = current_->ticks_per_second > 0.0 ? current_->ticks_per_second : 25.0;
    time_ticks_ += static_cast<float>(dt * speed_ * tps);
    double dur = current_->duration_ticks;
    if (dur <= 0.0) { time_ticks_ = 0.0f; return; }
    if (loop_) {
        time_ticks_ = std::fmod(time_ticks_, static_cast<float>(dur));
    } else if (time_ticks_ > static_cast<float>(dur)) {
        time_ticks_ = static_cast<float>(dur);
    }
}

void Animator::traverse(int node_idx, const glm::mat4& parent_global,
                        std::vector<glm::mat4>& node_globals,
                        bool ancestor_animated) const {
    const SkeletonNode& n = skeleton_->nodes()[node_idx];
    glm::mat4 local = n.local_transform;

    // Sample the animation channel targeting this node, if present.
    bool node_animated = false;
    if (current_) {
        for (const auto& ch : current_->channels) {
            if (ch.node_name == n.name) {
                glm::vec3 p = sample_keys<glm::vec3, glm::vec3>(ch.position_keys, time_ticks_);
                glm::quat r = sample_keys<glm::quat, glm::quat>(ch.rotation_keys, time_ticks_);
                glm::vec3 s = sample_keys<glm::vec3, glm::vec3>(ch.scale_keys, time_ticks_);
                // Root-motion suppression: the topmost animated node in the
                // hierarchy carries the clip's forward/strafe progression. The
                // MovementSystem already drives the world-space position along
                // the A* path, so we anchor this node's x/z to the clip's first
                // frame to keep the mesh in place. Y is preserved so the
                // vertical bob still plays. This also removes the loop-wrap
                // snap-back that caused the avatar to teleport each loop.
                if (!ancestor_animated && !ch.position_keys.empty()) {
                    const glm::vec3& p0 = ch.position_keys.front().second;
                    p.x = p0.x;
                    p.z = p0.z;
                }
                local = trs(p, r, s);
                node_animated = true;
                break;
            }
        }
    }
    glm::mat4 global = parent_global * local;
    node_globals[node_idx] = global;
    for (int c : n.children)
        traverse(c, global, node_globals, node_animated || ancestor_animated);
}

void Animator::compute_bone_matrices(std::vector<glm::mat4>& out) const {
    if (!skeleton_) { out.clear(); return; }
    std::vector<glm::mat4> node_globals(skeleton_->nodes().size(), glm::mat4(1.0f));
    if (!skeleton_->nodes().empty()) traverse(0, glm::mat4(1.0f), node_globals, false);

    out.assign(skeleton_->bones().size(), glm::mat4(1.0f));
    for (std::size_t b = 0; b < skeleton_->bones().size(); ++b) {
        const Bone& bone = skeleton_->bones()[b];
        if (bone.node_index >= 0 && bone.node_index < static_cast<int>(node_globals.size())) {
            out[b] = node_globals[bone.node_index] * bone.offset_matrix;
        } else {
            out[b] = bone.offset_matrix;
        }
    }
}

} // namespace sims
