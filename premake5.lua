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
    filter "configurations:Debug"
        defines { "DEBUG" }
        symbols "On"
    filter "configurations:Release"
        defines { "NDEBUG" }
        optimize "On"
    filter {}

-- Third-party code (git submodules in vendor/).
include "vendor/glfw.lua"

-- Every folder in modules/ with a premake5.lua is a module (include/ = .h, src/ = .cpp).
for _, dir in ipairs(os.matchdirs("modules/*")) do
    if os.isfile(path.join(dir, "premake5.lua")) then
        include(dir)
    end
end

-- The executable.
project "app"
    kind "ConsoleApp"
    location "build/app"
    files { "app/src/**.cpp" }
    links { "glfw_module", "vulkan_backend" }
    includedirs { "modules/glfw/include", "modules/vulkan/include", "vendor/glfw/include", "vendor/Vulkan-Headers/include" }
    defines { "VK_NO_PROTOTYPES" }
    filter "configurations:Release"
        kind "WindowedApp"
    filter {}
