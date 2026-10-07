#include "demo_director.h"

#include <doctest/doctest.h>

namespace {
DemoDirector::Config cfg() {
    DemoDirector::Config c;
    c.settleSamples = 8;
    c.settleMaxSeconds = 6;
    c.stepSamples = 4;
    c.stepMaxSeconds = 3;
    return c;
}
} // namespace

TEST_CASE("Camera does not move before the first frame has been rendered") {
    DemoDirector d(cfg());
    CHECK_FALSE(d.update(0.1, 0, 0)); // nothing published yet
    CHECK_FALSE(d.update(0.1, 1, 1)); // one sample is not a rendered frame
    CHECK_FALSE(d.update(0.1, 7, 1)); // still short of the settle target
    CHECK(d.update(0.1, 8, 1));       // settled: now it may move
}

TEST_CASE("Stale sample counts right after a move do not trigger another move") {
    DemoDirector d(cfg());
    REQUIRE(d.update(0.1, 8, 1)); // first move
    // The renderer has not republished yet: sample count is still the old 8, generation still 1.
    CHECK_FALSE(d.update(0.1, 8, 1));
    CHECK_FALSE(d.update(0.1, 8, 1));
    // New camera's first frame is published (generation 2, 1 sample): not enough yet.
    CHECK_FALSE(d.update(0.1, 1, 2));
    CHECK_FALSE(d.update(0.1, 3, 2));
    CHECK(d.update(0.1, 4, 2)); // enough samples at the new position
}

TEST_CASE("Time cap keeps the demo moving on a slow machine") {
    DemoDirector d(cfg());
    bool moved = false;
    double t = 0;
    while (t < 6.5 && !moved) {
        moved = d.update(0.5, 0, 0); // renderer never publishes anything
        t += 0.5;
    }
    CHECK(moved);
    CHECK(t >= 6.0); // waited for the whole settle cap first

    // Later stops use the shorter cap.
    int frames = 0;
    bool movedAgain = false;
    while (frames < 100 && !movedAgain) {
        movedAgain = d.update(0.5, 1, 1); // stale: never fresh
        ++frames;
    }
    CHECK(movedAgain);
    CHECK(frames == 6); // 6 * 0.5s = 3s step cap
}

TEST_CASE("Repeated moves each wait for their own fresh frame") {
    DemoDirector d(cfg());
    uint32_t gen = 1;
    REQUIRE(d.update(0.1, 8, gen));
    for (int move = 0; move < 5; ++move) {
        ++gen;
        CHECK_FALSE(d.update(0.1, 2, gen));
        CHECK(d.update(0.1, 4, gen));
    }
}

TEST_CASE("yawStep reflects the config") {
    DemoDirector::Config c = cfg();
    c.yawStep = 0.4f;
    CHECK(DemoDirector(c).yawStep() == doctest::Approx(0.4f));
}
