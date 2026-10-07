#pragma once

#include "raytracer/camera.h"

#include <algorithm>
#include <cmath>

namespace rt {

// Camera that orbits a target point; yaw/pitch in radians. Converts to the renderer's Camera.
struct OrbitCamera {
    static constexpr float kMaxPitch = 1.4f;
    static constexpr float kMinDistance = 1.5f;
    static constexpr float kMaxDistance = 40.0f;

    float yaw = 0.0f;
    float pitch = 0.18f;
    float distance = 7.0f;
    Vec3 target{0, 1, 0};
    float fov = 40.0f;
    float aperture = 0.0f;
    float focus = 7.0f;

    void rotate(float dYaw, float dPitch) {
        yaw += dYaw;
        pitch = std::clamp(pitch + dPitch, -kMaxPitch, kMaxPitch);
    }

    // factor < 1 moves closer, > 1 moves away.
    void zoom(float factor) {
        distance = std::clamp(distance * factor, kMinDistance, kMaxDistance);
    }

    Camera toCamera() const {
        Camera c;
        c.position =
            target + Vec3{std::sin(yaw) * std::cos(pitch) * distance, std::sin(pitch) * distance,
                          std::cos(yaw) * std::cos(pitch) * distance};
        c.target = target;
        c.verticalFovDegrees = fov;
        c.aperture = aperture;
        c.focusDistance = focus;
        return c;
    }
};

} // namespace rt
