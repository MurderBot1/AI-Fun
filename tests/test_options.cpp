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
    CHECK(o.demoSamples == 256);
    CHECK(o.demoFrames == 4);
    CHECK(o.demoHoldSeconds == doctest::Approx(3.0));
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
    Options o = parse({"--demo", "--demo-samples", "512", "--demo-frames", "6", "--demo-hold",
                       "1.5", "--size", "640x360"});
    CHECK(o.valid);
    CHECK(o.demoSamples == 512);
    CHECK(o.demoFrames == 6);
    CHECK(o.demoHoldSeconds == doctest::Approx(1.5));
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
    CHECK_FALSE(parse({"--demo-frames", "0"}).valid);
    CHECK_FALSE(parse({"--demo-frames"}).valid);
    CHECK_FALSE(parse({"--demo-hold", "-1"}).valid);
    CHECK_FALSE(parse({"--size", "640"}).valid);
    CHECK_FALSE(parse({"--size", "640x"}).valid);
    CHECK_FALSE(parse({"--size", "x360"}).valid);
    CHECK_FALSE(parse({"--size", "4x4"}).valid);
    CHECK_FALSE(parse({"--size", "99999x99999"}).valid);
    CHECK_FALSE(parse({"--size", "axb"}).valid);
    CHECK_FALSE(parse({"--bogus"}).error.empty());
}
