#pragma once

#include "content/InteractionLibrary.hpp"
#include "ecs/registry.hpp"
#include "sim/ActionSystem.hpp"
#include "sim/motives/Motives.hpp"
#include "sim/pathfinding/Pathfinder.hpp"
#include "sim/world/Coord.hpp"

#include <string>
#include <unordered_map>

namespace sims {

// Phase 7: Utility AI autonomy. When a Sim's ActionQueue is empty, the
// autonomy system scores every available interaction against the Sim's
// current motives and (if a candidate clears `min_score`) enqueues the best
// one via the ActionSystem.
//
// Scoring (per candidate interaction):
//   urgency(k)       = (100 - motive[k]) / 100      // 0 full, 1 empty
//   motive_utility   = sum_k motive_delta[k] * urgency(k)
//   travel_tiles     = Manhattan(sim_tile, obj_tile)  // 0 for Self-target
//   score            = motive_utility
//                      - cost_weight   * duration_min
//                      - travel_weight * travel_tiles
//
// Only candidates with score >= min_score are enqueued, and a per-Sim
// cooldown prevents re-deciding immediately after a finished action.
class AutonomySystem {
public:
    struct Config {
        float cost_weight   = 0.05f; // utility penalty per sim-minute of duration
        float travel_weight = 0.10f; // utility penalty per tile of travel
        float min_score     = 5.0f;  // minimum score to autonomously act
        double cooldown_min = 1.0;   // re-evaluation guard after a decision
    };

    AutonomySystem(const content::InteractionLibrary& lib,
                   const pathfinding::Pathfinder& pathfinder,
                   world::Coord coord,
                   ActionSystem& actions);
    AutonomySystem(const content::InteractionLibrary& lib,
                   const pathfinding::Pathfinder& pathfinder,
                   world::Coord coord,
                   ActionSystem& actions,
                   Config cfg);

    // sim_minutes: elapsed sim-time this tick (drives cooldowns).
    void update(Registry& reg, double sim_minutes);

    // Score a candidate interaction against a Sim's motives. `travel_tiles`
    // is the Manhattan tile distance to the target object (0 for Self).
    float score(const Motives& m, const content::InteractionDef& def,
                int travel_tiles) const;

    // Pick the best candidate interaction for a Sim. Returns false if no
    // candidate clears `min_score`. On success, fills out_interaction_id and
    // out_target_object (entt::null for Self-target interactions).
    bool decide(const Registry& reg, Entity sim_e,
                std::string& out_interaction_id,
                Entity& out_target_object) const;

    bool enabled = true;
    const Config& config() const { return cfg_; }
    void set_config(Config c) { cfg_ = c; }

private:
    const content::InteractionLibrary& lib_;
    const pathfinding::Pathfinder& pathfinder_;
    world::Coord coord_;
    ActionSystem& actions_;
    Config cfg_;

    std::unordered_map<Entity, double> cooldown_;

    int manhattan_tiles(const glm::vec3& sim_pos, int otx, int otz) const;
};

} // namespace sims
