#include "render/SimAvatar.hpp"

#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

#include <cmath>

namespace sims {

void SimAvatar::init() {
    if (inited_) return;
    cube_ = Mesh::make_cube(1.0f);
    inited_ = true;
}

void SimAvatar::draw_part(Shader& lit, const glm::mat4& model, const glm::vec3& color) {
    lit.set_mat4("u_model", &model[0][0]);
    lit.set_vec3("u_base_color", color.r, color.g, color.b);
    lit.set_int("u_has_texture", 0);
    cube_.draw();
}

void SimAvatar::draw(Shader& lit, const glm::vec3& world_pos, float facing_deg,
                     float walk_phase, bool moving) {
    init();

    float yaw = glm::radians(facing_deg);
    float bob = moving ? std::sin(walk_phase * 2.0f) * 0.04f : 0.0f;
    float leg_swing = moving ? std::sin(walk_phase) * 0.5f : 0.0f;
    float arm_swing = moving ? std::sin(walk_phase) * 0.4f : 0.0f;

    glm::vec3 base = world_pos + glm::vec3(0.0f, bob, 0.0f);

    // Root transform: translate to position, rotate to facing.
    auto root = glm::rotate(glm::translate(glm::mat4(1.0f), base), yaw, glm::vec3(0, 1, 0));

    // Torso: 0.45w x 0.7h x 0.25d, center at y=1.05
    glm::mat4 torso = glm::scale(
        glm::translate(root, {0.0f, 1.05f, 0.0f}),
        {0.45f, 0.70f, 0.25f});
    draw_part(lit, torso, {0.2f, 0.5f, 0.8f});

    // Head: 0.22 cube at y=1.55
    glm::mat4 head = glm::scale(
        glm::translate(root, {0.0f, 1.55f, 0.0f}),
        {0.22f, 0.22f, 0.22f});
    draw_part(lit, head, {0.95f, 0.78f, 0.65f});

    // Legs: 0.15w x 0.8h x 0.15d, hip at y=0.7. Swing around hip (x-axis).
    auto leg = [&](float side, float swing) {
        glm::mat4 hip = glm::translate(root, {side * 0.11f, 0.70f, 0.0f});
        hip = glm::rotate(hip, swing, glm::vec3(1, 0, 0));
        glm::mat4 leg_m = glm::scale(
            glm::translate(hip, {0.0f, -0.40f, 0.0f}),
            {0.15f, 0.80f, 0.15f});
        draw_part(lit, leg_m, {0.15f, 0.2f, 0.5f});
    };
    leg(-1.0f, leg_swing);
    leg( 1.0f, -leg_swing);

    // Arms: 0.12w x 0.55h x 0.12d, shoulder at y=1.35. Swing opposite to legs.
    auto arm = [&](float side, float swing) {
        glm::mat4 shoulder = glm::translate(root, {side * 0.28f, 1.35f, 0.0f});
        shoulder = glm::rotate(shoulder, swing, glm::vec3(1, 0, 0));
        glm::mat4 arm_m = glm::scale(
            glm::translate(shoulder, {0.0f, -0.27f, 0.0f}),
            {0.12f, 0.55f, 0.12f});
        draw_part(lit, arm_m, {0.2f, 0.5f, 0.8f});
    };
    arm(-1.0f, -arm_swing);
    arm( 1.0f, arm_swing);
}

} // namespace sims
