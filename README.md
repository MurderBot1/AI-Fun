# AI-Fun

C++17 project built with [Premake](https://premake.github.io/). The executable opens a blank GLFW window and
initialises a Vulkan instance when a Vulkan loader is available.

## Layout
- `app/` – the executable (`app/src/main.cpp`)
- `modules/<name>/include` (`.h`) and `modules/<name>/src` (`.cpp`), each with a `premake5.lua`
  - `glfw` – window wrapper around GLFW
  - `vulkan` – Vulkan backend (loads entry points at runtime via GLFW; headers only)
- `vendor/` – third-party submodules (GLFW, Vulkan-Headers)

Adding a module: create `modules/<name>/{include,src,premake5.lua}`; it is picked up automatically.

## Build
```
git clone --recursive <repo>
premake5 gmake2   # or vs2022 / xcode4
make -C build config=release
```
Linux needs `libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev`.

## CI
- `format.yml` – clang-format check
- `build.yml` – Linux / macOS / Windows builds
- `release.yml` – push to `dev` or `dev/**` publishes a GitHub release with binaries
