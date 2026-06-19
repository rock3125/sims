#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <string>
#include <unordered_map>
#include <vector>

namespace sims {

// A node in the armature/scene tree. Matches Assimp's aiNode structure:
// a name, a local transform (relative to parent), and child indices.
struct SkeletonNode {
    std::string name;
    int parent = -1;
    std::vector<int> children;
    glm::mat4 local_transform{1.0f}; // bind/rest local transform
};

// A skinned bone: links a bone name to its owning node and stores the
// inverse-bind (offset) matrix that brings mesh vertices into bone space.
struct Bone {
    std::string name;
    int node_index = -1;      // index into Skeleton::nodes
    glm::mat4 offset_matrix{1.0f};
};

// A single animation clip: duration in ticks + per-node TRS keyframes.
// Each channel targets one node by name. Keys are sorted by time (Assimp
// guarantees this) and we sample with linear interpolation + repeat looping.
struct AnimationChannel {
    std::string node_name;
    std::vector<std::pair<float, glm::vec3>> position_keys; // (time_ticks, vec)
    std::vector<std::pair<float, glm::quat>> rotation_keys;
    std::vector<std::pair<float, glm::vec3>> scale_keys;
};

struct AnimationClip {
    std::string name;
    double duration_ticks = 0.0;
    double ticks_per_second = 25.0;
    std::vector<AnimationChannel> channels;
};

// Owns the node tree, bone table, and animation clips. Built once from an
// Assimp scene by SkinnedModel. Immutable after construction.
class Skeleton {
public:
    Skeleton() = default;

    const std::vector<SkeletonNode>& nodes() const { return nodes_; }
    const std::vector<Bone>& bones() const { return bones_; }
    const std::vector<AnimationClip>& clips() const { return clips_; }

    int node_index(const std::string& name) const;
    int bone_index(const std::string& name) const;
    const AnimationClip* clip(const std::string& name) const;

    // Build hooks used by SkinnedModel during Assimp traversal.
    int add_node(const std::string& name, int parent, const glm::mat4& local) {
        int idx = static_cast<int>(nodes_.size());
        nodes_.push_back({name, parent, {}, local});
        node_index_[name] = idx;
        if (parent >= 0) nodes_[parent].children.push_back(idx);
        return idx;
    }
    int add_bone(const std::string& name, int node_index, const glm::mat4& offset) {
        int idx = static_cast<int>(bones_.size());
        bones_.push_back({name, node_index, offset});
        bone_index_[name] = idx;
        return idx;
    }
    void add_clip(AnimationClip clip) { clips_.push_back(std::move(clip)); }

private:
    std::vector<SkeletonNode> nodes_;
    std::vector<Bone> bones_;
    std::vector<AnimationClip> clips_;
    std::unordered_map<std::string, int> node_index_;
    std::unordered_map<std::string, int> bone_index_;
};

} // namespace sims
