#pragma once

#include "raytracer/math.h"

namespace rt {

// Thin-lens perspective camera.
struct Camera {
    Vec3 position{0, 2, 7};
    Vec3 target{0, 1, 0};
    Vec3 up{0, 1, 0};
    float verticalFovDegrees = 40.0f;
    float aperture = 0.0f; // lens diameter; 0 = pinhole
    float focusDistance = 7.0f;

    // u, v in [0,1] across the image (v = 0 is the bottom row).
    Ray getRay(float u, float v, float aspect, Rng& rng) const;
};

} // namespace rt
