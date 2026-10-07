#include "raytracer/renderer.h"

#include <doctest/doctest.h>

#include <chrono>
#include <thread>

using namespace rt;

namespace {
std::shared_ptr<Scene> tinyScene() {
    auto s = std::make_shared<Scene>();
    Material light;
    light.type = MaterialType::Emissive;
    light.emission = {2, 2, 2};
    int l = s->addMaterial(light);
    int d = s->addMaterial({});
    s->addSphere({{0, 0, 0}, 1.0f, l});
    s->addSphere({{0, -1001, 0}, 1000.0f, d});
    s->build();
    return s;
}

Camera tinyCamera() {
    Camera c;
    c.position = {0, 0, 5};
    c.target = {0, 0, 0};
    return c;
}
} // namespace

TEST_CASE("Renderer without a scene is idle") {
    Renderer r(8, 8, 1);
    CHECK_FALSE(r.step());
    CHECK(r.sampleCount() == 0);
}

TEST_CASE("Renderer accumulates samples and produces an opaque non-black image") {
    Renderer r(32, 32, 2);
    r.setScene(tinyScene());
    r.setCamera(tinyCamera());
    for (int i = 0; i < 4; ++i)
        CHECK(r.step());
    std::vector<uint8_t> img;
    CHECK(r.copyFrame(img) == 4);
    REQUIRE(img.size() == 32u * 32u * 4u);
    int lit = 0;
    for (size_t i = 0; i < img.size(); i += 4) {
        CHECK(img[i + 3] == 255);
        lit += img[i] + img[i + 1] + img[i + 2] > 0;
    }
    CHECK(lit > 32 * 32 / 2);
    // The emissive sphere at the centre is brighter than the sky corner... and both are lit.
    auto px = [&](int x, int y) { return img[(y * 32 + x) * 4]; };
    CHECK(px(16, 16) > 200);
}

TEST_CASE("Rendering is deterministic regardless of thread count") {
    auto scene = tinyScene();
    std::vector<uint8_t> a, b;
    {
        Renderer r(24, 24, 1);
        r.setScene(scene);
        r.setCamera(tinyCamera());
        r.step();
        r.step();
        r.copyFrame(a);
    }
    {
        Renderer r(24, 24, 4);
        r.setScene(scene);
        r.setCamera(tinyCamera());
        r.step();
        r.step();
        r.copyFrame(b);
    }
    CHECK(a == b);
}

TEST_CASE("Changing the camera restarts accumulation") {
    Renderer r(16, 16, 1);
    r.setScene(tinyScene());
    r.setCamera(tinyCamera());
    r.step();
    r.step();
    CHECK(r.sampleCount() == 2);
    Camera moved = tinyCamera();
    moved.position = {0, 0, 9};
    r.setCamera(moved);
    r.step();
    CHECK(r.sampleCount() == 1);
}

TEST_CASE("Sample limit stops accumulation") {
    Renderer r(8, 8, 1);
    r.setScene(tinyScene());
    r.setCamera(tinyCamera());
    r.setSampleLimit(3);
    int rendered = 0;
    while (r.step())
        ++rendered;
    CHECK(rendered == 3);
    CHECK(r.sampleCount() == 3);
    r.setSampleLimit(5); // raising the limit resumes without a reset
    while (r.step())
        ++rendered;
    CHECK(rendered == 5);
}

TEST_CASE("Output never contains NaN-driven garbage for the demo scene") {
    Renderer r(40, 24, 2);
    r.setScene(makeDemoScene());
    for (int i = 0; i < 3; ++i)
        r.step();
    std::vector<uint8_t> img;
    r.copyFrame(img);
    long sum = 0;
    for (size_t i = 0; i < img.size(); i += 4)
        sum += img[i] + img[i + 1] + img[i + 2];
    CHECK(sum > 0);
    CHECK(sum < 255L * 3 * 40 * 24); // not saturated white either
}

TEST_CASE("Background thread renders and stops cleanly") {
    Renderer r(16, 16, 2);
    r.setScene(tinyScene());
    r.setCamera(tinyCamera());
    r.start();
    r.start(); // idempotent
    for (int i = 0; i < 200 && r.sampleCount() < 3; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    r.stop();
    CHECK(r.sampleCount() >= 3);
    r.stop(); // idempotent
}
