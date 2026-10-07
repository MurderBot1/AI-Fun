#include "raytracer/orbit_camera.h"

#include <doctest/doctest.h>

using namespace rt;

TEST_CASE("Orbit camera keeps its distance from the target") {
    OrbitCamera o;
    o.distance = 5.0f;
    for (float yaw = 0.0f; yaw < 6.5f; yaw += 0.7f) {
        o.yaw = yaw;
        Camera c = o.toCamera();
        CHECK(length(c.position - o.target) == doctest::Approx(5.0f).epsilon(1e-4));
        CHECK(c.target.y == doctest::Approx(o.target.y));
    }
}

TEST_CASE("Orbit camera at yaw 0 / pitch 0 sits on +Z looking at the target") {
    OrbitCamera o;
    o.pitch = 0.0f;
    o.yaw = 0.0f;
    o.distance = 4.0f;
    Camera c = o.toCamera();
    CHECK(c.position.x == doctest::Approx(0.0f).epsilon(1e-4));
    CHECK(c.position.y == doctest::Approx(o.target.y));
    CHECK(c.position.z == doctest::Approx(4.0f));
}

TEST_CASE("rotate clamps pitch, zoom clamps distance") {
    OrbitCamera o;
    o.rotate(0.0f, 100.0f);
    CHECK(o.pitch == doctest::Approx(OrbitCamera::kMaxPitch));
    o.rotate(0.0f, -1000.0f);
    CHECK(o.pitch == doctest::Approx(-OrbitCamera::kMaxPitch));
    o.zoom(1000.0f);
    CHECK(o.distance == doctest::Approx(OrbitCamera::kMaxDistance));
    o.zoom(0.0001f);
    CHECK(o.distance == doctest::Approx(OrbitCamera::kMinDistance));
}

TEST_CASE("rotate accumulates yaw and conversion carries lens settings") {
    OrbitCamera o;
    o.fov = 55.0f;
    o.aperture = 0.3f;
    o.focus = 9.0f;
    o.rotate(0.25f, 0.0f);
    o.rotate(0.25f, 0.0f);
    CHECK(o.yaw == doctest::Approx(0.5f));
    Camera c = o.toCamera();
    CHECK(c.verticalFovDegrees == doctest::Approx(55.0f));
    CHECK(c.aperture == doctest::Approx(0.3f));
    CHECK(c.focusDistance == doctest::Approx(9.0f));
}
