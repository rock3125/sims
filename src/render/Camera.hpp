#pragma once

#include <glm/glm.hpp>

#include <optional>

namespace sims {

// Orbit camera: yaw/pitch around a target, distance dolly, pan target.
// View + perspective projection with glm. Intended for build/inspection views
// during early phases; will be supplemented by a follow-cam in later phases.
class Camera {
public:
    Camera() = default;

    void set_perspective(float fov_deg, float aspect, float near_p, float far_p);
    void set_orbit(float yaw_deg, float pitch_deg, float distance);
    void set_target(glm::vec3 t) { target_ = t; }

    // Input mutations (called from Application event handler).
    void rotate(float yaw_delta_deg, float pitch_delta_deg);
    void dolly(float distance_delta);
    void pan(float dx_screen, float dy_screen);

    glm::vec3 position() const;
    const glm::vec3& target() const { return target_; }
    float yaw() const { return yaw_deg_; }
    float pitch() const { return pitch_deg_; }
    float distance() const { return distance_; }

    glm::mat4 view() const;
    glm::mat4 projection() const { return proj_; }
    glm::mat4 view_projection() const { return proj_ * view(); }

    // Unproject a screen-space pixel (top-left origin) to a world point on the
    // y=0 plane. Returns nullopt if the ray is parallel to the plane or the
    // intersection is behind the camera.
    std::optional<glm::vec3> screen_to_ground(int pixel_x, int pixel_y, int vp_w, int vp_h) const;

private:
    float fov_deg_ = 50.0f;
    float aspect_ = 16.0f / 9.0f;
    float near_ = 0.1f;
    float far_ = 200.0f;

    float yaw_deg_ = 45.0f;
    float pitch_deg_ = 35.0f;
    float distance_ = 12.0f;
    glm::vec3 target_{0.0f, 1.0f, 0.0f};

    glm::mat4 proj_{1.0f};
};

} // namespace sims
