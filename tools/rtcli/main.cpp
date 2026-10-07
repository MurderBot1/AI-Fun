// Headless renderer: rtcli [width height samples output.ppm]
#include "raytracer/renderer.h"

#include <cstdio>
#include <cstdlib>
#include <vector>

int main(int argc, char** argv) {
    const int width = argc > 1 ? std::atoi(argv[1]) : 480;
    const int height = argc > 2 ? std::atoi(argv[2]) : 270;
    const int samples = argc > 3 ? std::atoi(argv[3]) : 16;
    const char* path = argc > 4 ? argv[4] : "out.ppm";
    if (width <= 0 || height <= 0 || samples <= 0) {
        std::fprintf(stderr, "usage: rtcli [width height samples output.ppm]\n");
        return 1;
    }

    rt::Renderer renderer(width, height);
    renderer.setScene(rt::makeDemoScene());
    for (int i = 0; i < samples; ++i)
        renderer.step();

    std::vector<uint8_t> rgba;
    renderer.copyFrame(rgba);

    std::FILE* f = std::fopen(path, "wb");
    if (!f) {
        std::fprintf(stderr, "cannot open %s\n", path);
        return 1;
    }
    std::fprintf(f, "P6\n%d %d\n255\n", width, height);
    for (size_t i = 0; i < rgba.size(); i += 4)
        std::fwrite(&rgba[i], 1, 3, f);
    std::fclose(f);
    std::printf("wrote %s (%dx%d, %d spp)\n", path, width, height, samples);
    return 0;
}
