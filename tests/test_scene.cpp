#include "raytracer/scene.h"

#include <doctest/doctest.h>

using namespace rt;

TEST_CASE("Ray hits a sphere from outside and inside") {
    Scene s;
    int m = s.addMaterial({});
    s.addSphere({{0, 0, 0}, 1.0f, m});
    s.build();

    Hit h;
    REQUIRE(s.intersect({{0, 0, 5}, {0, 0, -1}}, 1e-3f, 100.0f, h));
    CHECK(h.t == doctest::Approx(4.0f));
    CHECK(h.frontFace);
    CHECK(h.normal.z == doctest::Approx(1.0f));

    REQUIRE(s.intersect({{0, 0, 0}, {0, 0, -1}}, 1e-3f, 100.0f, h));
    CHECK(h.t == doctest::Approx(1.0f));
    CHECK_FALSE(h.frontFace);
    CHECK(h.normal.z == doctest::Approx(1.0f)); // flipped to face the ray
}

TEST_CASE("Misses and range limits") {
    Scene s;
    int m = s.addMaterial({});
    s.addSphere({{0, 0, 0}, 1.0f, m});
    s.build();
    Hit h;
    CHECK_FALSE(s.intersect({{0, 3, 5}, {0, 0, -1}}, 1e-3f, 100.0f, h));
    CHECK_FALSE(s.intersect({{0, 0, 5}, {0, 0, -1}}, 1e-3f, 3.0f, h));  // tMax before the hit
    CHECK_FALSE(s.intersect({{0, 0, 5}, {0, 0, 1}}, 1e-3f, 100.0f, h)); // pointing away
}

TEST_CASE("Triangle intersection") {
    Scene s;
    int m = s.addMaterial({});
    s.addTriangle({{-1, -1, 0}, {1, -1, 0}, {0, 1, 0}, m});
    s.build();
    Hit h;
    REQUIRE(s.intersect({{0, 0, 3}, {0, 0, -1}}, 1e-3f, 100.0f, h));
    CHECK(h.t == doctest::Approx(3.0f));
    CHECK(h.point.z == doctest::Approx(0.0f));
    CHECK_FALSE(s.intersect({{2, 0, 3}, {0, 0, -1}}, 1e-3f, 100.0f, h));
    // Parallel to the plane.
    CHECK_FALSE(s.intersect({{0, 0, 3}, {1, 0, 0}}, 1e-3f, 100.0f, h));
}

TEST_CASE("Nearest hit wins") {
    Scene s;
    int m = s.addMaterial({});
    s.addSphere({{0, 0, -10}, 1.0f, m});
    s.addSphere({{0, 0, -3}, 1.0f, m});
    s.build();
    Hit h;
    REQUIRE(s.intersect({{0, 0, 0}, {0, 0, -1}}, 1e-3f, 100.0f, h));
    CHECK(h.t == doctest::Approx(2.0f));
}

TEST_CASE("Empty scene never hits and sky is a gradient") {
    Scene s;
    s.build();
    Hit h;
    CHECK_FALSE(s.intersect({{0, 0, 0}, {0, 0, -1}}, 1e-3f, 100.0f, h));
    Vec3 down = s.sky({0, -1, 0});
    Vec3 up = s.sky({0, 1, 0});
    CHECK(down.x == doctest::Approx(s.skyBottom.x));
    CHECK(up.x == doctest::Approx(s.skyTop.x));
}

TEST_CASE("BVH matches brute force on a random scene") {
    Scene s;
    int m = s.addMaterial({});
    Rng rng(2024);
    for (int i = 0; i < 300; ++i) {
        Vec3 c{rng.nextFloat() * 20 - 10, rng.nextFloat() * 20 - 10, rng.nextFloat() * 20 - 10};
        if (i % 3 == 0) {
            s.addTriangle(
                {c, c + Vec3{rng.nextFloat(), 0, 0.5f}, c + Vec3{0, rng.nextFloat(), 0.2f}, m});
        } else {
            s.addSphere({c, 0.2f + rng.nextFloat() * 0.6f, m});
        }
    }
    s.build();

    int hits = 0;
    for (int i = 0; i < 3000; ++i) {
        Ray r{{rng.nextFloat() * 4 - 2, rng.nextFloat() * 4 - 2, 15},
              normalize(Vec3{rng.nextFloat() - 0.5f, rng.nextFloat() - 0.5f, -1.0f})};
        Hit a, b;
        bool ha = s.intersect(r, 1e-3f, 1e30f, a);
        bool hb = s.intersectBruteForce(r, 1e-3f, 1e30f, b);
        REQUIRE(ha == hb);
        if (ha) {
            ++hits;
            CHECK(a.t == doctest::Approx(b.t));
        }
    }
    CHECK(hits > 100); // the test must actually exercise hits
}

TEST_CASE("Quad adds two triangles") {
    Scene s;
    int m = s.addMaterial({});
    s.addQuad({0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}, m);
    CHECK(s.primitiveCount() == 2);
    s.build();
    Hit h;
    CHECK(s.intersect({{0.9f, 0.1f, 1}, {0, 0, -1}}, 1e-3f, 10.0f, h));
    CHECK(s.intersect({{0.1f, 0.9f, 1}, {0, 0, -1}}, 1e-3f, 10.0f, h));
}
