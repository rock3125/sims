#pragma once

#include "ecs/components.hpp"
#include "ecs/registry.hpp"
#include "sim/pathfinding/Pathfinder.hpp"

namespace sims {

// Fixed-step movement system. Moves each Sim along its Movement waypoint queue
// (produced by the A* pathfinder) at Movement.speed, rotating facing toward
// the direction of travel and advancing Sim.walk_phase while moving.
class MovementSystem {
public:
    // dt in seconds.
    void update(Registry& reg, float dt);

    // Replace a Sim's movement plan with a precomputed path. The path's tile
    // waypoints are converted to world-space tile centers; the first waypoint
    // becomes the immediate `target`. If the path is empty/invalid, the Sim
    // stops in place.
    static void set_path(Registry& reg, Entity e, const pathfinding::Path& path,
                         const world::Coord& coord);

    // Convenience: stop the Sim immediately and clear all waypoints.
    static void stop(Registry& reg, Entity e);

private:
    static constexpr float kArriveEpsilon = 0.05f;     // meters
    static constexpr float kTurnRate = 12.0f;           // rad/s, snap-ish
};

} // namespace sims
