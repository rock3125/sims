#include "sim/AutonomySystem.hpp"

#include "ecs/components.hpp"
#include "sim/motives/Motives.hpp"

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>

namespace sims {

AutonomySystem::AutonomySystem(const content::InteractionLibrary& lib,
                               const pathfinding::Pathfinder& pathfinder,
                               world::Coord coord,
                               ActionSystem& actions)
    : AutonomySystem(lib, pathfinder, coord, actions, Config{}) {}

AutonomySystem::AutonomySystem(const content::InteractionLibrary& lib,
                               const pathfinding::Pathfinder& pathfinder,
                               world::Coord coord,
                               ActionSystem& actions,
                               Config cfg)
    : lib_(lib), pathfinder_(pathfinder), coord_(coord),
      actions_(actions), cfg_(cfg) {}

namespace {

inline float urgency(float value) {
    float v = value < 0.0f ? 0.0f : (value > 100.0f ? 100.0f : value);
    return (100.0f - v) / 100.0f;
}

} // namespace

int AutonomySystem::manhattan_tiles(const glm::vec3& sim_pos,
                                    int otx, int otz) const {
    int stx = 0, stz = 0;
    if (!coord_.world_to_tile(sim_pos, stx, stz)) return 0;
    int dx = std::abs(stx - otx);
    int dz = std::abs(stz - otz);
    // The Sim approaches an adjacent tile, so subtract one if non-zero.
    int dist = dx + dz;
    return dist > 0 ? dist - 1 : 0;
}

float AutonomySystem::score(const Motives& m,
                            const content::InteractionDef& def,
                            int travel_tiles) const {
    float utility = 0.0f;
    for (std::size_t i = 0; i < Motives::kCount; ++i) {
        float delta = def.motive_deltas[i];
        if (delta == 0.0f) continue;
        float u = urgency(m.value[i]);
        utility += delta * u;
    }
    utility -= cfg_.cost_weight * static_cast<float>(def.duration_min);
    utility -= cfg_.travel_weight * static_cast<float>(travel_tiles);
    return utility;
}

bool AutonomySystem::decide(const Registry& reg, Entity sim_e,
                            std::string& out_interaction_id,
                            Entity& out_target_object) const {
    const auto* mo = reg.try_get<Motives>(sim_e);
    const auto* tr = reg.try_get<Transform>(sim_e);
    if (!mo || !tr) return false;

    float best_score = cfg_.min_score;
    bool found = false;
    std::string best_id;
    Entity best_target = entt::null;

    auto consider = [&](const content::InteractionDef& def,
                        Entity target_obj, int travel_tiles) {
        const float s = score(*mo, def, travel_tiles);
        if (s > best_score) {
            best_score = s;
            found = true;
            best_id = def.id;
            best_target = target_obj;
        }
    };

    // Self-target interactions: always available, no travel.
    for (const auto& [id, def] : lib_.interactions()) {
        if (def.target == content::InteractionTarget::Self) {
            consider(def, entt::null, 0);
        }
    }

    // Object-target interactions: scan world objects, match their offered
    // interaction ids to defs. Travel = Manhattan to the object's tile.
    auto view = reg.view<WorldObject>();
    for (auto e : view) {
        const auto& wo = view.get<WorldObject>(e);
        const auto* odef = lib_.object(wo.def_id);
        if (!odef) continue;
        int travel = manhattan_tiles(tr->position, wo.tile_x, wo.tile_z);
        for (const auto& iid : odef->interaction_ids) {
            const auto* idef = lib_.interaction(iid);
            if (!idef) continue;
            if (idef->target != content::InteractionTarget::Object) continue;
            consider(*idef, e, travel);
        }
    }

    if (!found) return false;
    out_interaction_id = best_id;
    out_target_object = best_target;
    return true;
}

void AutonomySystem::update(Registry& reg, double sim_minutes) {
    if (!enabled) return;

    auto view = reg.view<Sim, ActionQueue>();
    for (auto e : view) {
        auto& q = view.get<ActionQueue>(e);
        auto& cd = cooldown_[e];
        if (cd > 0.0) cd -= sim_minutes;

        if (q.active()) continue;        // already busy
        if (cd > 0.0) continue;          // still in cooldown

        std::string iid;
        Entity target = entt::null;
        if (!decide(reg, e, iid, target)) continue;

        if (actions_.enqueue(reg, e, iid, target)) {
            cd = cfg_.cooldown_min;
        }
    }
}

} // namespace sims
