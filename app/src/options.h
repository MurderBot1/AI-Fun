#pragma once

#include <cstdlib>
#include <string>

// Command line options for the app.
//   --demo         auto-orbit the camera (used for the release video); no input needed
//   --seconds N    exit after N seconds (0 / absent = run until the window is closed)
struct Options {
    bool demo = false;
    double seconds = 0.0;
    bool valid = true;
    std::string error;
};

inline Options parseOptions(int argc, const char* const* argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--demo") {
            o.demo = true;
        } else if (arg == "--seconds") {
            if (i + 1 >= argc) {
                o.valid = false;
                o.error = "--seconds needs a value";
                return o;
            }
            char* end = nullptr;
            const double v = std::strtod(argv[++i], &end);
            if (end == argv[i] || *end != '\0' || !(v >= 0.0) || v > 86400.0) {
                o.valid = false;
                o.error = std::string("invalid value for --seconds: ") + argv[i];
                return o;
            }
            o.seconds = v;
        } else {
            o.valid = false;
            o.error = "unknown argument: " + arg;
            return o;
        }
    }
    return o;
}
