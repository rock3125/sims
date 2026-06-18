#pragma once

#include "ecs/components.hpp"
#include "ecs/registry.hpp"

namespace sims {

// Fixed-step movement system. Moves each Sim toward its Movement target along
// a straight line at Movement.speed, rotating facing toward the direction of
// travel and advancing Sim.walk_phase while moving. Phase 3: no pathfinding;
// Phase 4 will replace straight-line motion with A* waypoint following.
class MovementSystem {
public:
    // dt in seconds.
    void update(Registry& reg, float dt);

    // Convenience: set a world-space target for a Sim's movement component.
    static void set_target(Registry& reg, Entity e, const glm::vec3& world_target);

private:
    static constexpr float kArriveEpsilon = 0.05f;     // meters
    static constexpr float kTurnRate = 12.0f;           // rad/s, snap-ish
};

} // namespace sims
