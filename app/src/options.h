#pragma once

#include <cstdlib>
#include <string>

// Command line options for the app.
//   --demo               scripted run: pre-render a smooth camera sweep (every frame fully
//                        converged, nothing shown while baking), then play it back in real time
//   --demo-samples N     samples per pixel per frame (default 192)
//   --demo-fps N         playback frame rate (default 24)
//   --demo-duration S    seconds of camera sweep; frames = fps * S (default 5)
//   --demo-sweep DEG     degrees swept around the centre (default 50)
//   --demo-pingpong      after the sweep, play it backwards too
//   --size WxH           internal render resolution (default 960x540)
//   --seconds N          exit after N seconds (0 / absent = run until the window is closed)
struct Options {
    bool demo = false;
    bool demoPingPong = false;
    unsigned demoSamples = 192;
    unsigned demoFps = 24;
    double demoDurationSeconds = 5.0;
    double demoSweepDegrees = 50.0;
    int renderWidth = 960;
    int renderHeight = 540;
    double seconds = 0.0;
    bool valid = true;
    std::string error;
};

namespace options_detail {

inline bool parseDouble(const char* text, double minValue, double maxValue, double& out) {
    char* end = nullptr;
    const double v = std::strtod(text, &end);
    if (end == text || *end != '\0' || !(v >= minValue) || !(v <= maxValue))
        return false;
    out = v;
    return true;
}

inline bool parseUnsigned(const char* text, unsigned minValue, unsigned maxValue, unsigned& out) {
    char* end = nullptr;
    const unsigned long v = std::strtoul(text, &end, 10);
    if (end == text || *end != '\0' || text[0] == '-' || v < minValue || v > maxValue)
        return false;
    out = static_cast<unsigned>(v);
    return true;
}

} // namespace options_detail

inline Options parseOptions(int argc, const char* const* argv) {
    using namespace options_detail;
    Options o;
    auto fail = [&](const std::string& message) {
        o.valid = false;
        o.error = message;
        return o;
    };

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--demo") {
            o.demo = true;
            continue;
        }
        if (arg == "--demo-pingpong") {
            o.demoPingPong = true;
            continue;
        }
        const bool takesValue = arg == "--seconds" || arg == "--demo-samples" ||
                                arg == "--demo-fps" || arg == "--demo-duration" ||
                                arg == "--demo-sweep" || arg == "--size";
        if (!takesValue)
            return fail("unknown argument: " + arg);
        if (i + 1 >= argc)
            return fail(arg + " needs a value");
        const char* value = argv[++i];

        if (arg == "--seconds") {
            if (!parseDouble(value, 0.0, 86400.0, o.seconds))
                return fail(std::string("invalid value for --seconds: ") + value);
        } else if (arg == "--demo-duration") {
            if (!parseDouble(value, 0.1, 600.0, o.demoDurationSeconds))
                return fail(std::string("invalid value for --demo-duration: ") + value);
        } else if (arg == "--demo-sweep") {
            if (!parseDouble(value, 1.0, 340.0, o.demoSweepDegrees))
                return fail(std::string("invalid value for --demo-sweep: ") + value);
        } else if (arg == "--demo-samples") {
            if (!parseUnsigned(value, 1, 100000, o.demoSamples))
                return fail(std::string("invalid value for --demo-samples: ") + value);
        } else if (arg == "--demo-fps") {
            if (!parseUnsigned(value, 1, 240, o.demoFps))
                return fail(std::string("invalid value for --demo-fps: ") + value);
        } else { // --size WxH
            const std::string text = value;
            const size_t x = text.find('x');
            unsigned w = 0, h = 0;
            if (x == std::string::npos || !parseUnsigned(text.substr(0, x).c_str(), 16, 4096, w) ||
                !parseUnsigned(text.substr(x + 1).c_str(), 16, 4096, h))
                return fail(std::string("invalid value for --size (expected WxH): ") + value);
            o.renderWidth = static_cast<int>(w);
            o.renderHeight = static_cast<int>(h);
        }
    }
    return o;
}
