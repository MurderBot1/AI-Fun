# AI-Fun

C++17 project built with [Premake](https://premake.github.io/). The executable opens a blank GLFW window and
initialises a Vulkan instance when a Vulkan loader is available.

## Layout
- `app/` – the executable: a Vulkan window that shows a live path-traced image with an ImGui control panel
- `modules/<name>/include` (`.h`) and `modules/<name>/src` (`.cpp`), each with a `premake5.lua`
  - `raytracer` – CPU path tracer (BVH, spheres/triangles, diffuse/metal/glass/emissive, thin-lens camera,
    progressive multi-threaded renderer). No GPU or window needed.
  - `glfw` – window wrapper around GLFW
  - `vulkan` – Vulkan backend (instance/device/swapchain/frame loop, texture upload; loaded with volk)
  - `ui` – Dear ImGui on top of GLFW + the Vulkan backend
  - `audio` – miniaudio engine wrapper (volume, test tone, file playback); degrades to a no-op without a device
- `tools/rtcli` – headless renderer that writes a PPM: `rtcli [width height samples out.ppm]`
- `tests/` – doctest unit tests (math, BVH vs brute force, renderer determinism, audio/Vulkan failure paths)
- `vendor/` – submodules: GLFW, Vulkan-Headers, volk, Dear ImGui, miniaudio, doctest

Adding a module: create `modules/<name>/{include,src,premake5.lua}`; it is picked up automatically.
Link it from `app`/`tests` in the top-level `premake5.lua`.

## Build
```
git clone --recursive <repo>
premake5 gmake2   # or vs2022 / xcode4
make -C build config=release
```
Run the tests (no GPU/audio device required): `./bin/Release-*/tests`

Linux needs `libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev`.

## CI
- `format.yml` – clang-format check
- `build.yml` – Linux / macOS / Windows builds + unit tests + rtcli smoke render
- `release.yml` – push to `dev` or `dev/**` publishes a GitHub release with binaries, then runs the Linux build
  under Xvfb with software Vulkan (lavapipe), records it with ffmpeg and attaches `demo.mp4`/`demo.gif` to the release

The app takes `--demo` (auto-orbit the camera) and `--seconds N` (exit after N seconds).
