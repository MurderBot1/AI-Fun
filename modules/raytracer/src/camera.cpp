#include "raytracer/camera.h"

namespace rt {

Ray Camera::getRay(float u, float v, float aspect, Rng& rng) const {
    const float theta = verticalFovDegrees * 3.14159265358979f / 180.0f;
    const float halfH = std::tan(theta * 0.5f);
    const float halfW = aspect * halfH;

    const Vec3 w = normalize(position - target);
    const Vec3 right = normalize(cross(up, w));
    const Vec3 u3 = cross(w, right);

    const Vec3 horizontal = right * (2.0f * halfW * focusDistance);
    const Vec3 vertical = u3 * (2.0f * halfH * focusDistance);
    const Vec3 lowerLeft = position - horizontal * 0.5f - vertical * 0.5f - w * focusDistance;

    Vec3 offset{0, 0, 0};
    if (aperture > 0.0f) {
        Vec3 rd = rng.inUnitDisk() * (aperture * 0.5f);
        offset = right * rd.x + u3 * rd.y;
    }
    const Vec3 origin = position + offset;
    return {origin, normalize(lowerLeft + horizontal * u + vertical * v - origin)};
}

} // namespace rt
