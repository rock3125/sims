#pragma once

#include "ecs/registry.hpp"
#include "sim/time/SimClock.hpp"

namespace sims {

// Fixed-step system that decays each Sim's Motives at the per-sim-minute
// rates defined in `Motives::kDecayPerMin`. Decoupled from real time: the
// caller passes the sim-time delta produced by ticking the SimClock.
//
// Phase 5: decay only. Replenishment arrives with interactions (Phase 6).
class MotiveSystem {
public:
    // sim_minutes: how much sim-time elapsed this tick (drives decay).
    void update(Registry& reg, double sim_minutes);
};

} // namespace sims
