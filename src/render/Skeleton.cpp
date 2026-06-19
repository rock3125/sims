#include "render/Skeleton.hpp"

namespace sims {

int Skeleton::node_index(const std::string& name) const {
    auto it = node_index_.find(name);
    return it == node_index_.end() ? -1 : it->second;
}

int Skeleton::bone_index(const std::string& name) const {
    auto it = bone_index_.find(name);
    return it == bone_index_.end() ? -1 : it->second;
}

const AnimationClip* Skeleton::clip(const std::string& name) const {
    for (const auto& c : clips_) if (c.name == name) return &c;
    return nullptr;
}

} // namespace sims
