#include "sim/MovementSystem.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>

namespace sims {

namespace {

// Shortest signed angular distance from `from` to `to`, both in radians,
// result in [-pi, pi].
float angle_diff(float from, float to) {
    float d = std::fmod(to - from, glm::two_pi<float>());
    if (d > glm::pi<float>())  d -= glm::two_pi<float>();
    if (d < -glm::pi<float>()) d += glm::two_pi<float>();
    return d;
}

} // namespace

void MovementSystem::stop(Registry& reg, Entity e) {
    auto* mv = reg.try_get<Movement>(e);
    if (!mv) return;
    mv->has_target = false;
    mv->waypoints.clear();
}

void MovementSystem::set_path(Registry& reg, Entity e, const pathfinding::Path& path,
                              const world::Coord& coord) {
    auto* mv = reg.try_get<Movement>(e);
    auto* tr = reg.try_get<Transform>(e);
    if (!mv || !tr) return;

    mv->waypoints.clear();
    mv->has_target = false;

    if (!path.valid || path.waypoints.empty()) return;

    // First waypoint should be the Sim's current tile; skip it if so to avoid
    // a zero-length first step. Otherwise start from the first entry.
    int cur_tx = 0, cur_tz = 0;
    bool on_tile = coord.world_to_tile(tr->position, cur_tx, cur_tz);

    std::size_t start_idx = 0;
    if (on_tile && path.waypoints.size() > 1 &&
        path.waypoints.front().x == cur_tx && path.waypoints.front().y == cur_tz) {
        start_idx = 1;
    }

    for (std::size_t i = start_idx; i < path.waypoints.size(); ++i) {
        const auto& wp = path.waypoints[i];
        glm::vec3 w = coord.tile_to_world(wp.x, wp.y);
        mv->waypoints.push_back(w);
    }

    if (mv->waypoints.empty()) return;

    mv->target = mv->waypoints.front();
    mv->final_target = mv->waypoints.back();
    mv->waypoints.pop_front();
    mv->has_target = true;
}

void MovementSystem::update(Registry& reg, float dt) {
    auto view = reg.view<Transform, Sim, Movement>();
    for (auto e : view) {
        auto& tr = view.get<Transform>(e);
        auto& sim = view.get<Sim>(e);
        auto& mv = view.get<Movement>(e);

        if (!mv.has_target) {
            sim.moving = false;
            continue;
        }

        glm::vec3 to_target = mv.target - tr.position;
        to_target.y = 0.0f;
        float dist = glm::length(to_target);

        if (dist <= kArriveEpsilon) {
            tr.position = mv.target;
            if (!mv.waypoints.empty()) {
                mv.target = mv.waypoints.front();
                mv.waypoints.pop_front();
            } else {
                mv.has_target = false;
                sim.moving = false;
            }
            continue;
        }

        glm::vec3 dir = to_target / dist;

        // Face the direction of travel (smoothed turn).
        float target_yaw = std::atan2(dir.x, dir.z);
        float diff = angle_diff(glm::radians(sim.facing_deg), target_yaw);
        float max_step = kTurnRate * dt;
        float step = std::clamp(diff, -max_step, max_step);
        sim.facing_deg = glm::degrees(glm::radians(sim.facing_deg) + step);

        // Translate (clamp step to remaining distance).
        float step_len = std::min(mv.speed * dt, dist);
        tr.position += dir * step_len;
        sim.moving = true;

        // Advance walk phase proportional to distance covered.
        sim.walk_phase += step_len * 4.0f;
    }
}

} // namespace sims
