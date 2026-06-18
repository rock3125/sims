#pragma once

#include <glm/glm.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace sims {
namespace content {

// Who or what an interaction targets.
enum class InteractionTarget : std::uint8_t {
    Self,     // performed on the Sim itself (no object needed)
    Object,   // performed at a world object's tile
};

// A JSON-driven interaction definition. `motive_deltas` indices align with
// `Motives::Kind` (Hunger/Energy/Bladder/Fun/Social); unknown keys are
// ignored at load time so the schema can grow.
struct InteractionDef {
    std::string id;
    std::string label;
    std::string category;
    double duration_min = 0.0;
    std::array<float, 5> motive_deltas{0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    InteractionTarget target = InteractionTarget::Object;
};

// A placeable object definition: a colored footprint plus the interaction
// ids it offers. Instances are spawned as entities carrying a `WorldObject`
// component (see ecs/components.hpp).
struct ObjectDef {
    std::string id;
    std::string label;
    glm::vec3 color{1.0f};
    int footprint_w = 1;
    int footprint_d = 1;
    std::vector<std::string> interaction_ids;
};

// Loads interactions.json + objects.json into in-memory lookup tables.
// Missing files are a hard error (returns false). Unknown interaction ids
// referenced by an object are skipped with a stderr warning.
class InteractionLibrary {
public:
    bool load(const std::string& interactions_path,
              const std::string& objects_path);

    bool empty() const { return interactions_.empty() && objects_.empty(); }

    const InteractionDef* interaction(const std::string& id) const;
    const ObjectDef* object(const std::string& id) const;

    const std::unordered_map<std::string, InteractionDef>& interactions() const {
        return interactions_;
    }
    const std::unordered_map<std::string, ObjectDef>& objects() const {
        return objects_;
    }

private:
    std::unordered_map<std::string, InteractionDef> interactions_;
    std::unordered_map<std::string, ObjectDef> objects_;
};

} // namespace content
} // namespace sims
