#include "demo_director.h"

#include <doctest/doctest.h>

namespace {
DemoDirector::Config cfg() {
    DemoDirector::Config c;
    c.targetSamples = 100;
    c.minHoldSeconds = 3.0;
    c.totalFrames = 3;
    return c;
}
} // namespace

TEST_CASE("Nothing is shown until the view has converged") {
    DemoDirector d(cfg());
    CHECK_FALSE(d.update(0.1, 0, 0));
    CHECK_FALSE(d.update(0.1, 1, 1));
    CHECK_FALSE(d.update(0.1, 99, 1)); // one sample short
    CHECK(d.update(0.1, 100, 1));      // converged: show it
    CHECK(d.framesShown() == 1);
}

TEST_CASE("A converged frame is shown exactly once") {
    DemoDirector d(cfg());
    REQUIRE(d.update(0.1, 100, 1));
    // Same generation keeps reporting the same finished count while the caller moves the camera.
    for (int i = 0; i < 100; ++i)
        CHECK_FALSE(d.update(0.1, 100, 1));
    CHECK(d.framesShown() == 1);
}

TEST_CASE("Stale sample counts after a camera move are ignored") {
    DemoDirector d(cfg());
    REQUIRE(d.update(0.1, 100, 1));
    // The new view has not published a frame yet: old count (100) and old generation (1).
    CHECK_FALSE(d.update(5.0, 100, 1));
    // New view's first frame arrives, far from converged.
    CHECK_FALSE(d.update(0.1, 1, 2));
    CHECK_FALSE(d.update(0.1, 60, 2));
    CHECK(d.update(0.1, 100, 2));
}

TEST_CASE("A finished view is held for the minimum time even if the next is ready sooner") {
    DemoDirector d(cfg());
    REQUIRE(d.update(0.1, 100, 1));
    CHECK_FALSE(d.update(1.0, 100, 2)); // converged but only 1s since the last one was shown
    CHECK_FALSE(d.update(1.0, 100, 2));
    CHECK(d.update(1.0, 100, 2)); // 3s have passed
}

TEST_CASE("A slow render makes the previous view stay up as long as needed") {
    DemoDirector d(cfg());
    REQUIRE(d.update(0.1, 100, 1));
    for (int i = 0; i < 500; ++i)
        CHECK_FALSE(d.update(1.0, 50, 2)); // never converges within 500 s: never shown early
    CHECK(d.update(1.0, 100, 2));
}

TEST_CASE("Demo finishes only after the last view has been held") {
    DemoDirector d(cfg());
    uint32_t gen = 0;
    for (int frame = 0; frame < 3; ++frame) {
        ++gen;
        CHECK_FALSE(d.finished());
        REQUIRE(d.update(3.0, 100, gen));
    }
    CHECK(d.framesShown() == 3);
    CHECK_FALSE(d.finished()); // just shown
    d.update(1.0, 100, gen + 1);
    CHECK_FALSE(d.finished());
    d.update(2.0, 100, gen + 1);
    CHECK(d.finished());
    // No further frames are ever requested.
    CHECK_FALSE(d.update(10.0, 100, gen + 5));
    CHECK(d.framesShown() == 3);
}

TEST_CASE("yawStep reflects the config") {
    DemoDirector::Config c = cfg();
    c.yawStep = 0.4f;
    CHECK(DemoDirector(c).yawStep() == doctest::Approx(0.4f));
}
