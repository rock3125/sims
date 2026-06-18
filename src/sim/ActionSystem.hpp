#pragma once

#include "content/InteractionLibrary.hpp"
#include "ecs/components.hpp"
#include "ecs/registry.hpp"
#include "sim/pathfinding/Pathfinder.hpp"
#include "sim/time/SimClock.hpp"
#include "sim/world/Coord.hpp"

namespace sims {

// Drives each Sim's ActionQueue. For the current action:
//   - Self-target  : jump straight to Performing.
//   - Object-target: compute a path to an adjacent tile of the object's
//                    footprint (PendingMove), wait for movement to complete
//                    (Moving), then perform the interaction over its
//                    duration_min (Performing), applying motive deltas on
//                    completion.
//
// Aborts an action if no path exists or the Sim is interrupted mid-move
// (movement target changed by an external click). On completion or abort,
// the front action is popped and the next (if any) begins on a subsequent
// tick.
class ActionSystem {
public:
    ActionSystem(const content::InteractionLibrary& lib,
                 const pathfinding::Pathfinder& pathfinder,
                 world::Coord coord)
        : lib_(lib), pathfinder_(pathfinder), coord_(coord) {}

    // sim_minutes: elapsed sim-time this tick (drives Performing progress).
    void update(Registry& reg, double sim_minutes);

    // Enqueue an interaction onto a Sim's action queue. Resolves the
    // interaction def + (for Object targets) the object entity's footprint.
    // Returns false if the interaction id is unknown or the target entity is
    // not a WorldObject.
    bool enqueue(Registry& reg, Entity sim_e, const std::string& interaction_id,
                 Entity target_object = entt::null);

    // Clear a Sim's action queue and abort the current action.
    static void cancel(Registry& reg, Entity sim_e);

    // Apply a single interaction's motive deltas to a Sim. Public so tests
    // and (later) the autonomy system can use it.
    static void apply_motive_deltas(Registry& reg, Entity sim_e,
                                    const content::InteractionDef& def);

private:
    const content::InteractionLibrary& lib_;
    const pathfinding::Pathfinder& pathfinder_;
    world::Coord coord_;

    void start_action(Registry& reg, Entity sim_e, ActionQueue::Action& a) const;
    void finish_action(Registry& reg, Entity sim_e, ActionQueue::Action& a) const;
    void abort_action(Registry& reg, Entity sim_e, ActionQueue::Action& a) const;
};

} // namespace sims
