-- Top-level Premake build script.
--   premake5 gmake2 | xcode4 | vs2022

workspace "AIFun"
    architecture "x86_64"
    configurations { "Debug", "Release" }
    startproject "app"
    location "build"
    language "C++"
    cppdialect "C++17"
    staticruntime "off"
    targetdir "bin/%{cfg.buildcfg}-%{cfg.system}"
    objdir "bin-int/%{cfg.buildcfg}-%{cfg.system}/%{prj.name}"
    warnings "Extra"

    filter "system:macosx"
        architecture "universal"
    -- Vulkan structs are routinely partially initialised; MSVC takes numeric warning ids instead.
    filter "action:gmake*"
        disablewarnings { "missing-field-initializers" }
    -- windows.h (pulled in by miniaudio) defines min/max macros that break std::min/std::max.
    filter "system:windows"
        defines { "NOMINMAX", "_CRT_SECURE_NO_WARNINGS" }
    filter "configurations:Debug"
        defines { "DEBUG" }
        symbols "On"
    filter "configurations:Release"
        defines { "NDEBUG" }
        optimize "On"
    filter {}

-- Third-party code (git submodules in vendor/).
include "vendor/glfw.lua"
include "vendor/volk.lua"
include "vendor/imgui.lua"

-- Every folder in modules/ with a premake5.lua is a module (include/ = .h, src/ = .cpp).
for _, dir in ipairs(os.matchdirs("modules/*")) do
    if os.isfile(path.join(dir, "premake5.lua")) then
        include(dir)
    end
end

-- Headless tools and tests (no GPU / window needed).
project "rtcli"
    kind "ConsoleApp"
    location "build/tools"
    files { "tools/rtcli/**.cpp" }
    links { "raytracer" }
    includedirs { "modules/raytracer/include" }
    filter "system:linux"
        links { "pthread" }
    filter {}

project "tests"
    kind "ConsoleApp"
    location "build/tests"
    files { "tests/**.cpp" }
    links { "vulkan_backend", "volk", "audio", "raytracer" }
    includedirs {
        "modules/raytracer/include", "modules/audio/include", "modules/vulkan/include",
        "app/src", "vendor/doctest", "vendor/Vulkan-Headers/include",
    }
    defines { "VK_NO_PROTOTYPES" }
    filter "system:linux"
        links { "pthread", "dl", "m" }
    filter "system:macosx"
        linkoptions { "-framework CoreFoundation", "-framework CoreAudio", "-framework AudioToolbox" }
    filter {}

-- The executable.
project "app"
    kind "ConsoleApp"
    location "build/app"
    files { "app/src/**.cpp" }
    -- Order matters for static libraries: dependents before their dependencies.
    links { "ui", "glfw_module", "vulkan_backend", "audio", "raytracer", "ImGui", "volk", "GLFW" }
    includedirs {
        "modules/glfw/include", "modules/vulkan/include", "modules/ui/include",
        "modules/audio/include", "modules/raytracer/include",
        "vendor/glfw/include", "vendor/Vulkan-Headers/include", "vendor/imgui",
    }
    defines { "GLFW_INCLUDE_NONE", "VK_NO_PROTOTYPES" }

    -- System libraries needed by GLFW/miniaudio (static libs don't carry them across).
    filter "system:linux"
        links { "X11", "pthread", "dl", "m" }
    filter "system:windows"
        links { "gdi32", "user32", "shell32" }
    filter "system:macosx"
        linkoptions {
            "-framework Cocoa", "-framework IOKit", "-framework CoreFoundation",
            "-framework CoreAudio", "-framework AudioToolbox",
        }
    filter {}
