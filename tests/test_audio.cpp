#include "audio/engine.h"

#include <doctest/doctest.h>

// These tests must pass on machines with no sound card (CI), so they only assert behaviour that
// holds both with and without a device.

TEST_CASE("Audio engine constructs and destructs without a device") {
    audio::Engine engine;
    (void)engine.ok(); // either value is acceptable
}

TEST_CASE("Master volume is clamped") {
    audio::Engine engine;
    engine.setMasterVolume(2.5f);
    CHECK(engine.masterVolume() == doctest::Approx(1.0f));
    engine.setMasterVolume(-1.0f);
    CHECK(engine.masterVolume() == doctest::Approx(0.0f));
    engine.setMasterVolume(0.4f);
    CHECK(engine.masterVolume() == doctest::Approx(0.4f));
}

TEST_CASE("Tone toggling is safe and tracks state") {
    audio::Engine engine;
    CHECK_FALSE(engine.toneEnabled());
    engine.setTone(true, 220.0f, 0.1f);
    CHECK(engine.toneEnabled());
    engine.setTone(true, -5.0f, 9.0f); // out-of-range values are sanitised, not fatal
    engine.setTone(false);
    CHECK_FALSE(engine.toneEnabled());
}

TEST_CASE("Playing a missing file reports failure instead of crashing") {
    audio::Engine engine;
    CHECK_FALSE(engine.playFile("/definitely/not/a/real/file.wav"));
}
