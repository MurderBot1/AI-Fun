// End-to-end check of the demo's bake -> playback pipeline using the real renderer (no GPU).
#include "demo_director.h"
#include "raytracer/orbit_camera.h"
#include "raytracer/renderer.h"

#include <doctest/doctest.h>

#include <cmath>
#include <cstdlib>
#include <vector>

namespace {

double meanAbsDiff(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    REQUIRE(a.size() == b.size());
    double sum = 0.0;
    for (size_t i = 0; i < a.size(); ++i)
        sum += std::abs(static_cast<int>(a[i]) - static_cast<int>(b[i]));
    return sum / static_cast<double>(a.size());
}

// Bakes frames exactly like the app does, but drives the renderer synchronously.
std::vector<std::vector<uint8_t>> bake(DemoDirector& director, rt::Renderer& renderer,
                                       rt::OrbitCamera& orbit, uint32_t targetSamples) {
    std::vector<std::vector<uint8_t>> frames;
    const float startYaw = orbit.yaw;
    renderer.setSampleLimit(targetSamples);
    renderer.setCamera(orbit.toCamera());
    int guard = 0;
    while (director.phase() == DemoDirector::Phase::Baking && guard++ < 100000) {
        renderer.step();
        const uint32_t generation = renderer.publishedGeneration();
        const uint32_t samples = renderer.sampleCount();
        if (director.bakeUpdate(samples, generation)) {
            frames.emplace_back();
            renderer.copyFrame(frames.back());
            if (director.phase() == DemoDirector::Phase::Baking) {
                orbit.yaw = startYaw + director.yawFor(director.captured());
                renderer.setCamera(orbit.toCamera());
            }
        }
    }
    return frames;
}

} // namespace

TEST_CASE("Baked sweep has one converged frame per step and rotates smoothly") {
    DemoDirector::Config cfg;
    cfg.targetSamples = 6;
    cfg.frames = 16;
    cfg.fps = 24.0;
    cfg.totalYaw = 0.9f;
    DemoDirector director(cfg);

    rt::Renderer renderer(64, 36, 2);
    renderer.setScene(rt::makeDemoScene());
    rt::OrbitCamera orbit;

    const auto frames = bake(director, renderer, orbit, cfg.targetSamples);

    REQUIRE(frames.size() == 16);
    CHECK(director.phase() == DemoDirector::Phase::Playing);

    // Every frame is a real image, and consecutive frames differ (the camera really moves)...
    std::vector<double> steps;
    for (size_t i = 1; i < frames.size(); ++i) {
        const double d = meanAbsDiff(frames[i - 1], frames[i]);
        CHECK(d > 0.0);
        steps.push_back(d);
    }
    // ...but no step is a jump: each is a small fraction of the whole sweep's change.
    const double total = meanAbsDiff(frames.front(), frames.back());
    CHECK(total > 0.0);
    for (double d : steps)
        CHECK(d < total * 0.5);
    // Eased path: the first and last steps are the gentlest ones.
    double biggest = 0.0;
    for (double d : steps)
        biggest = std::max(biggest, d);
    CHECK(steps.front() < biggest);
    CHECK(steps.back() < biggest);
}

TEST_CASE("Playback after baking shows every frame in order at the fixed rate") {
    DemoDirector::Config cfg;
    cfg.targetSamples = 2;
    cfg.frames = 8;
    cfg.fps = 24.0;
    DemoDirector director(cfg);
    rt::Renderer renderer(16, 9, 1);
    renderer.setScene(rt::makeDemoScene());
    rt::OrbitCamera orbit;
    const auto frames = bake(director, renderer, orbit, cfg.targetSamples);
    REQUIRE(frames.size() == 8);

    // Simulate a UI loop running at an uneven ~60 fps; the shown frame must follow wall-clock
    // time (24 fps), never skipping backwards and covering every frame.
    std::vector<int> shown;
    double wall = 0.0;
    int last = -1;
    while (director.phase() == DemoDirector::Phase::Playing) {
        const double dt = 1.0 / 60.0 * (1.0 + 0.3 * std::sin(wall * 40.0)); // jittery dt
        wall += dt;
        const int frame = static_cast<int>(director.playbackFrame(dt));
        CHECK(frame >= last);
        if (frame != last)
            shown.push_back(frame);
        last = frame;
        REQUIRE(wall < 10.0);
    }
    const std::vector<int> expected{0, 1, 2, 3, 4, 5, 6, 7};
    CHECK(shown == expected);
    CHECK(director.phase() == DemoDirector::Phase::Done);
    // 8 frames @ 24 fps + 1 s hold, within one UI frame of jitter.
    CHECK(wall == doctest::Approx(8.0 / 24.0 + 1.0).epsilon(0.05));
}
