#include "raytracer/camera.h"
#include "raytracer/math.h"

#include <doctest/doctest.h>

using namespace rt;

TEST_CASE("Vec3 arithmetic") {
    Vec3 a{1, 2, 3}, b{4, 5, 6};
    CHECK(dot(a, b) == doctest::Approx(32.0f));
    Vec3 c = cross(Vec3{1, 0, 0}, Vec3{0, 1, 0});
    CHECK(c.z == doctest::Approx(1.0f));
    CHECK(length(Vec3{3, 4, 0}) == doctest::Approx(5.0f));
    CHECK(length(normalize(Vec3{10, -3, 7})) == doctest::Approx(1.0f));
    CHECK(length(normalize(Vec3{0, 0, 0})) == doctest::Approx(0.0f)); // no NaN
}

TEST_CASE("reflect mirrors about the normal") {
    Vec3 r = reflect(Vec3{1, -1, 0}, Vec3{0, 1, 0});
    CHECK(r.x == doctest::Approx(1.0f));
    CHECK(r.y == doctest::Approx(1.0f));
}

TEST_CASE("Rng is deterministic and in range") {
    Rng a(123, 7), b(123, 7), c(124, 7);
    bool differs = false;
    for (int i = 0; i < 1000; ++i) {
        float fa = a.nextFloat();
        CHECK(fa == b.nextFloat());
        CHECK(fa >= 0.0f);
        CHECK(fa < 1.0f);
        differs |= fa != c.nextFloat();
    }
    CHECK(differs);
}

TEST_CASE("Rng sampling helpers stay inside their shapes") {
    Rng rng(1);
    for (int i = 0; i < 500; ++i) {
        CHECK(dot(rng.inUnitSphere(), rng.inUnitSphere()) < 3.0f);
        CHECK(length(rng.unitVector()) == doctest::Approx(1.0f).epsilon(1e-3));
        Vec3 d = rng.inUnitDisk();
        CHECK(d.z == 0.0f);
        CHECK(dot(d, d) < 1.0f);
    }
}

TEST_CASE("Camera rays point at the target through the image centre") {
    Camera cam;
    cam.position = {0, 0, 5};
    cam.target = {0, 0, 0};
    Rng rng(1);
    Ray centre = cam.getRay(0.5f, 0.5f, 16.0f / 9.0f, rng);
    CHECK(centre.direction.z == doctest::Approx(-1.0f));
    CHECK(length(centre.direction) == doctest::Approx(1.0f));
    Ray left = cam.getRay(0.0f, 0.5f, 1.0f, rng);
    Ray right = cam.getRay(1.0f, 0.5f, 1.0f, rng);
    CHECK(left.direction.x < 0.0f);
    CHECK(right.direction.x > 0.0f);
    Ray top = cam.getRay(0.5f, 1.0f, 1.0f, rng);
    CHECK(top.direction.y > 0.0f);
}

TEST_CASE("Camera aperture jitters ray origins but keeps them on the lens") {
    Camera cam;
    cam.aperture = 0.5f;
    Rng rng(9);
    bool moved = false;
    for (int i = 0; i < 100; ++i) {
        Ray r = cam.getRay(0.5f, 0.5f, 1.0f, rng);
        CHECK(length(r.origin - cam.position) <= 0.25f + 1e-4f);
        moved |= length(r.origin - cam.position) > 1e-4f;
    }
    CHECK(moved);
}
