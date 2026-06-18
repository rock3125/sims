#include "render/Camera.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>

namespace sims {

void Camera::set_perspective(float fov_deg, float aspect, float near_p, float far_p) {
    fov_deg_ = fov_deg;
    aspect_ = aspect;
    near_ = near_p;
    far_ = far_p;
    proj_ = glm::perspective(glm::radians(fov_deg_), aspect_, near_, far_);
}

void Camera::set_orbit(float yaw_deg, float pitch_deg, float distance) {
    yaw_deg_ = yaw_deg;
    pitch_deg_ = pitch_deg;
    distance_ = distance;
}

void Camera::rotate(float yaw_delta_deg, float pitch_delta_deg) {
    yaw_deg_ += yaw_delta_deg;
    pitch_deg_ += pitch_delta_deg;
    pitch_deg_ = std::clamp(pitch_deg_, -89.0f, 89.0f);
}

void Camera::dolly(float distance_delta) {
    distance_ = std::clamp(distance_ + distance_delta, 1.5f, 80.0f);
}

void Camera::pan(float dx_screen, float dy_screen) {
    // Pan in the camera's local right/up plane.
    glm::vec3 fwd = glm::normalize(target_ - position());
    glm::vec3 world_up{0.0f, 1.0f, 0.0f};
    glm::vec3 right = glm::normalize(glm::cross(fwd, world_up));
    glm::vec3 up = glm::cross(right, fwd);
    float scale = distance_ * 0.0015f;
    target_ -= right * dx_screen * scale + up * dy_screen * scale;
}

glm::vec3 Camera::position() const {
    float yaw = glm::radians(yaw_deg_);
    float pitch = glm::radians(pitch_deg_);
    float cp = std::cos(pitch);
    glm::vec3 offset{
        distance_ * cp * std::sin(yaw),
        -distance_ * std::sin(pitch),
        distance_ * cp * std::cos(yaw),
    };
    return target_ - offset;
}

glm::mat4 Camera::view() const {
    return glm::lookAt(position(), target_, glm::vec3(0.0f, 1.0f, 0.0f));
}

std::optional<glm::vec3> Camera::screen_to_ground(int pixel_x, int pixel_y, int vp_w, int vp_h) const {
    if (vp_w <= 0 || vp_h <= 0) return std::nullopt;
    float nx = (2.0f * static_cast<float>(pixel_x)) / static_cast<float>(vp_w) - 1.0f;
    float ny = 1.0f - (2.0f * static_cast<float>(pixel_y)) / static_cast<float>(vp_h);
    glm::vec4 near_point = glm::inverse(view_projection()) * glm::vec4(nx, ny, -1.0f, 1.0f);
    glm::vec4 far_point  = glm::inverse(view_projection()) * glm::vec4(nx, ny, 1.0f, 1.0f);
    if (near_point.w == 0.0f || far_point.w == 0.0f) return std::nullopt;
    glm::vec3 p0 = glm::vec3(near_point) / near_point.w;
    glm::vec3 p1 = glm::vec3(far_point) / far_point.w;
    glm::vec3 dir = p1 - p0;
    if (std::abs(dir.y) < 1e-6f) return std::nullopt;
    float t = -p0.y / dir.y;
    if (t < 0.0f) return std::nullopt;
    return p0 + dir * t;
}

} // namespace sims
