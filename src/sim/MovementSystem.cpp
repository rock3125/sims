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

void MovementSystem::set_target(Registry& reg, Entity e, const glm::vec3& world_target) {
    auto* mv = reg.try_get<Movement>(e);
    auto* tr = reg.try_get<Transform>(e);
    if (!mv || !tr) return;
    mv->target = glm::vec3(world_target.x, 0.0f, world_target.z);
    mv->has_target = true;
    (void)tr;
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
            mv.has_target = false;
            sim.moving = false;
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
