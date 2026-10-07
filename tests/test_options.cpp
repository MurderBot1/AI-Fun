#include "options.h"

#include <doctest/doctest.h>

#include <vector>

namespace {
Options parse(std::initializer_list<const char*> args) {
    std::vector<const char*> argv{"app"};
    argv.insert(argv.end(), args.begin(), args.end());
    return parseOptions(static_cast<int>(argv.size()), argv.data());
}
} // namespace

TEST_CASE("No arguments gives defaults") {
    Options o = parse({});
    CHECK(o.valid);
    CHECK_FALSE(o.demo);
    CHECK(o.seconds == doctest::Approx(0.0));
    CHECK(o.renderWidth == 960);
    CHECK(o.renderHeight == 540);
    CHECK(o.demoSamples == 192);
    CHECK(o.demoFps == 24);
    CHECK(o.demoDurationSeconds == doctest::Approx(5.0));
    CHECK(o.demoSweepDegrees == doctest::Approx(50.0));
    CHECK_FALSE(o.demoPingPong);
}

TEST_CASE("--demo and --seconds parse together in any order") {
    Options a = parse({"--demo", "--seconds", "12.5"});
    CHECK(a.valid);
    CHECK(a.demo);
    CHECK(a.seconds == doctest::Approx(12.5));
    Options b = parse({"--seconds", "3", "--demo"});
    CHECK(b.valid);
    CHECK(b.demo);
    CHECK(b.seconds == doctest::Approx(3.0));
}

TEST_CASE("Demo tuning options") {
    Options o = parse({"--demo", "--demo-samples", "512", "--demo-fps", "30", "--demo-duration",
                       "4.5", "--demo-sweep", "90", "--demo-pingpong", "--size", "640x360"});
    CHECK(o.valid);
    CHECK(o.demoSamples == 512);
    CHECK(o.demoFps == 30);
    CHECK(o.demoDurationSeconds == doctest::Approx(4.5));
    CHECK(o.demoSweepDegrees == doctest::Approx(90.0));
    CHECK(o.demoPingPong);
    CHECK(o.renderWidth == 640);
    CHECK(o.renderHeight == 360);
}

TEST_CASE("Bad arguments are rejected with a message") {
    CHECK_FALSE(parse({"--bogus"}).valid);
    CHECK_FALSE(parse({"--seconds"}).valid);
    CHECK_FALSE(parse({"--seconds", "abc"}).valid);
    CHECK_FALSE(parse({"--seconds", "-4"}).valid);
    CHECK_FALSE(parse({"--seconds", "1e99"}).valid);
    CHECK_FALSE(parse({"--seconds", "5x"}).valid);
    CHECK_FALSE(parse({"--seconds", "nan"}).valid);
    CHECK_FALSE(parse({"--demo-samples", "0"}).valid);
    CHECK_FALSE(parse({"--demo-samples", "-5"}).valid);
    CHECK_FALSE(parse({"--demo-fps", "0"}).valid);
    CHECK_FALSE(parse({"--demo-fps"}).valid);
    CHECK_FALSE(parse({"--demo-fps", "1000"}).valid);
    CHECK_FALSE(parse({"--demo-duration", "0"}).valid);
    CHECK_FALSE(parse({"--demo-duration", "-1"}).valid);
    CHECK_FALSE(parse({"--demo-sweep", "0"}).valid);
    CHECK_FALSE(parse({"--demo-sweep", "720"}).valid);
    CHECK_FALSE(parse({"--size", "640"}).valid);
    CHECK_FALSE(parse({"--size", "640x"}).valid);
    CHECK_FALSE(parse({"--size", "x360"}).valid);
    CHECK_FALSE(parse({"--size", "4x4"}).valid);
    CHECK_FALSE(parse({"--size", "99999x99999"}).valid);
    CHECK_FALSE(parse({"--size", "axb"}).valid);
    CHECK_FALSE(parse({"--bogus"}).error.empty());
}
