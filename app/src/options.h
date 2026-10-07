#pragma once

#include <cstdlib>
#include <string>

// Command line options for the app.
//   --demo               scripted run: each camera view is rendered until it has converged and is
//                        only shown once finished (no progressive refinement on screen)
//   --demo-samples N     samples per pixel a demo view needs before it is shown (default 256)
//   --demo-frames N      number of finished views to show, then exit (default 4)
//   --demo-hold S        minimum seconds each finished view stays on screen (default 3)
//   --size WxH           internal render resolution (default 960x540)
//   --seconds N          exit after N seconds (0 / absent = run until the window is closed)
struct Options {
    bool demo = false;
    unsigned demoSamples = 256;
    unsigned demoFrames = 4;
    double demoHoldSeconds = 3.0;
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
        const bool takesValue = arg == "--seconds" || arg == "--demo-samples" ||
                                arg == "--demo-frames" || arg == "--demo-hold" || arg == "--size";
        if (!takesValue)
            return fail("unknown argument: " + arg);
        if (i + 1 >= argc)
            return fail(arg + " needs a value");
        const char* value = argv[++i];

        if (arg == "--seconds") {
            if (!parseDouble(value, 0.0, 86400.0, o.seconds))
                return fail(std::string("invalid value for --seconds: ") + value);
        } else if (arg == "--demo-hold") {
            if (!parseDouble(value, 0.0, 600.0, o.demoHoldSeconds))
                return fail(std::string("invalid value for --demo-hold: ") + value);
        } else if (arg == "--demo-samples") {
            if (!parseUnsigned(value, 1, 100000, o.demoSamples))
                return fail(std::string("invalid value for --demo-samples: ") + value);
        } else if (arg == "--demo-frames") {
            if (!parseUnsigned(value, 1, 1000, o.demoFrames))
                return fail(std::string("invalid value for --demo-frames: ") + value);
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
