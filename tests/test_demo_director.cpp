#include "demo_director.h"

#include <doctest/doctest.h>

#include <cmath>
#include <vector>

namespace {
DemoDirector::Config cfg() {
    DemoDirector::Config c;
    c.targetSamples = 100;
    c.frames = 5;
    c.fps = 10.0;
    c.totalYaw = 1.0f;
    c.pingPong = false;
    c.endHoldSeconds = 1.0;
    return c;
}

// Bakes every frame of a director the way the app does.
void bakeAll(DemoDirector& d) {
    uint32_t gen = 0;
    while (d.phase() == DemoDirector::Phase::Baking) {
        ++gen; // renderer publishes a frame from the new camera
        REQUIRE(d.bakeUpdate(100, gen));
    }
}
} // namespace

TEST_CASE("Camera path starts at 0, ends at totalYaw and only moves forward") {
    DemoDirector d(cfg());
    CHECK(d.yawFor(0) == doctest::Approx(0.0f));
    CHECK(d.yawFor(4) == doctest::Approx(1.0f));
    float prev = -1.0f;
    for (uint32_t i = 0; i < 5; ++i) {
        CHECK(d.yawFor(i) > prev);
        prev = d.yawFor(i);
    }
    CHECK(d.yawFor(100) == doctest::Approx(1.0f)); // clamped
}

TEST_CASE("Camera path is smooth: eased start/stop, no sudden speed changes") {
    DemoDirector::Config c;
    c.frames = 120;
    c.totalYaw = 0.9f;
    DemoDirector d(c);
    const float mid = d.yawFor(60) - d.yawFor(59);
    const float first = d.yawFor(1) - d.yawFor(0);
    const float last = d.yawFor(119) - d.yawFor(118);
    CHECK(first < mid * 0.03f); // starts almost at rest
    CHECK(last < mid * 0.03f);  // and comes to rest
    // Per-frame speed changes by only a tiny amount from one frame to the next.
    for (uint32_t i = 1; i + 1 < 120; ++i) {
        const float a = d.yawFor(i) - d.yawFor(i - 1);
        const float b = d.yawFor(i + 1) - d.yawFor(i);
        CHECK(std::fabs(b - a) < mid * 0.1f);
    }
}

TEST_CASE("A frame is only captured once converged and fresh") {
    DemoDirector d(cfg());
    CHECK_FALSE(d.bakeUpdate(0, 0));
    CHECK_FALSE(d.bakeUpdate(99, 1)); // one sample short
    CHECK(d.bakeUpdate(100, 1));
    CHECK(d.captured() == 1);
    // Stale state right after the camera moved: same generation, old (converged) count.
    for (int i = 0; i < 50; ++i)
        CHECK_FALSE(d.bakeUpdate(100, 1));
    CHECK_FALSE(d.bakeUpdate(5, 2)); // new camera, not converged
    CHECK(d.bakeUpdate(100, 2));
    CHECK(d.captured() == 2);
}

TEST_CASE("Baking ends after the last frame and playback begins") {
    DemoDirector d(cfg());
    bakeAll(d);
    CHECK(d.captured() == 5);
    CHECK(d.phase() == DemoDirector::Phase::Playing);
    CHECK_FALSE(d.bakeUpdate(100, 99)); // nothing more to bake
}

TEST_CASE("Playback runs at a fixed frame rate regardless of loop timing") {
    DemoDirector d(cfg()); // 10 fps
    bakeAll(d);
    CHECK(d.playbackFrame(0.0) == 0);
    CHECK(d.playbackFrame(0.05) == 0); // t=0.05
    CHECK(d.playbackFrame(0.06) == 1); // t=0.11
    CHECK(d.playbackFrame(0.2) == 3);  // t=0.31
    CHECK(d.playbackFrame(0.0) == 3);  // paused clock -> same frame
    CHECK(d.playbackFrame(1.0) == 4);  // t=1.31: clamped on the last frame
}

TEST_CASE("Same total time gives the same frame whether stepped finely or coarsely") {
    DemoDirector fine(cfg()), coarse(cfg());
    bakeAll(fine);
    bakeAll(coarse);
    uint32_t f = 0;
    for (int i = 0; i < 25; ++i)
        f = fine.playbackFrame(0.01);
    CHECK(f == coarse.playbackFrame(0.25));
}

TEST_CASE("Playback holds the last frame, then finishes") {
    DemoDirector d(cfg()); // 5 frames @ 10fps = 0.5 s, + 1 s hold
    bakeAll(d);
    d.playbackFrame(0.5);
    CHECK(d.phase() == DemoDirector::Phase::Playing);
    CHECK(d.playbackFrame(0.9) == 4);
    CHECK(d.phase() == DemoDirector::Phase::Playing);
    CHECK(d.playbackFrame(0.2) == 4);
    CHECK(d.phase() == DemoDirector::Phase::Done);
}

TEST_CASE("Ping-pong plays forward then backward, visiting every frame") {
    DemoDirector::Config c = cfg();
    c.pingPong = true;
    DemoDirector d(c);
    bakeAll(d);
    CHECK(d.playbackLength() == 8); // 0 1 2 3 4 3 2 1
    std::vector<uint32_t> seen;
    for (int i = 0; i < 8; ++i)
        seen.push_back(d.playbackFrame(i == 0 ? 0.0 : 0.1));
    const std::vector<uint32_t> expected{0, 1, 2, 3, 4, 3, 2, 1};
    CHECK(seen == expected);
}

TEST_CASE("Degenerate configs are sanitised instead of crashing") {
    DemoDirector::Config c;
    c.frames = 0;
    c.fps = 0.0;
    c.targetSamples = 0;
    DemoDirector d(c);
    CHECK(d.frames() == 2);
    bakeAll(d);
    CHECK(d.playbackFrame(1000.0) < 2);
    CHECK(d.phase() == DemoDirector::Phase::Done);
}
