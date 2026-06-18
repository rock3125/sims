#include "sim/ActionSystem.hpp"

#include "ecs/components.hpp"
#include "sim/MovementSystem.hpp"
#include "sim/motives/Motives.hpp"

#include <glm/glm.hpp>

#include <cmath>

namespace sims {

namespace {

constexpr float kArriveEpsilon = 0.6f; // meters: "close enough to object"

} // namespace

void ActionSystem::cancel(Registry& reg, Entity sim_e) {
    auto* q = reg.try_get<ActionQueue>(sim_e);
    if (!q) return;
    auto* mv = reg.try_get<Movement>(sim_e);
    if (mv) MovementSystem::stop(reg, sim_e);
    q->queue.clear();
}

void ActionSystem::apply_motive_deltas(Registry& reg, Entity sim_e,
                                       const content::InteractionDef& def) {
    auto* mo = reg.try_get<Motives>(sim_e);
    if (!mo) return;
    for (std::size_t i = 0; i < Motives::kCount; ++i) {
        const auto k = static_cast<Motives::Kind>(i);
        mo->adjust(k, def.motive_deltas[i]);
    }
}

bool ActionSystem::enqueue(Registry& reg, Entity sim_e,
                           const std::string& interaction_id,
                           Entity target_object) {
    const auto* def = lib_.interaction(interaction_id);
    if (!def) return false;

    auto& q = reg.get_or_emplace<ActionQueue>(sim_e);
    ActionQueue::Action a;
    a.interaction_id = interaction_id;
    a.target_object = target_object;
    a.duration_min = def->duration_min;
    a.phase = (def->target == content::InteractionTarget::Self)
              ? ActionQueue::Phase::Performing
              : ActionQueue::Phase::PendingMove;
    q.queue.push_back(std::move(a));
    return true;
}

void ActionSystem::start_action(Registry& reg, Entity sim_e,
                                ActionQueue::Action& a) const {
    if (a.phase == ActionQueue::Phase::Performing) {
        a.elapsed_min = 0.0;
        return;
    }

    auto* tr = reg.try_get<Transform>(sim_e);
    auto* obj = (a.target_object != entt::null)
                ? reg.try_get<WorldObject>(a.target_object) : nullptr;
    if (!tr || !obj) {
        a.aborted = true;
        a.phase = ActionQueue::Phase::Done;
        return;
    }

    int stx = 0, stz = 0;
    if (!coord_.world_to_tile(tr->position, stx, stz)) {
        a.aborted = true;
        a.phase = ActionQueue::Phase::Done;
        return;
    }

    auto path = pathfinder_.find_to_adjacent({stx, stz},
                                             obj->tile_x, obj->tile_z,
                                             obj->footprint_w, obj->footprint_d);
    if (!path.valid || path.empty()) {
        a.aborted = true;
        a.phase = ActionQueue::Phase::Done;
        return;
    }

    // Endpoint tile = last waypoint; approach target = its world center.
    const auto& end_tile = path.waypoints.back();
    a.approach_target = coord_.tile_to_world(end_tile.x, end_tile.y);
    a.phase = ActionQueue::Phase::Moving;
    MovementSystem::set_path(reg, sim_e, path, coord_);
}

void ActionSystem::finish_action(Registry& reg, Entity sim_e,
                                 ActionQueue::Action& a) const {
    const auto* def = lib_.interaction(a.interaction_id);
    if (def) apply_motive_deltas(reg, sim_e, *def);
    a.phase = ActionQueue::Phase::Done;
}

void ActionSystem::abort_action(Registry& reg, Entity sim_e,
                                ActionQueue::Action& a) const {
    a.aborted = true;
    a.phase = ActionQueue::Phase::Done;
    MovementSystem::stop(reg, sim_e);
}

void ActionSystem::update(Registry& reg, double sim_minutes) {
    auto view = reg.view<Transform, Sim, Movement, ActionQueue>();
    for (auto e : view) {
        auto& q = view.get<ActionQueue>(e);
        if (q.queue.empty()) continue;

        auto& a = q.queue.front();
        auto* tr = reg.try_get<Transform>(e);
        auto* mv = reg.try_get<Movement>(e);

        switch (a.phase) {
            case ActionQueue::Phase::PendingMove:
                start_action(reg, e, a);
                break;

            case ActionQueue::Phase::Moving: {
                if (!mv) { abort_action(reg, e, a); break; }
                if (!mv->has_target) {
                    // Movement finished — did we arrive at the approach target?
                    float d = 0.0f;
                    if (tr) {
                        glm::vec3 d3 = a.approach_target - tr->position;
                        d3.y = 0.0f;
                        d = glm::length(d3);
                    }
                    if (d <= kArriveEpsilon) {
                        a.phase = ActionQueue::Phase::Performing;
                        a.elapsed_min = 0.0;
                    } else {
                        // Interrupted (user clicked elsewhere). Abort.
                        abort_action(reg, e, a);
                    }
                }
                break;
            }

            case ActionQueue::Phase::Performing: {
                a.elapsed_min += sim_minutes;
                if (a.elapsed_min >= a.duration_min) {
                    finish_action(reg, e, a);
                }
                break;
            }

            case ActionQueue::Phase::Done:
                break;
        }

        if (a.phase == ActionQueue::Phase::Done) {
            q.queue.pop_front();
        }
    }
}

} // namespace sims
