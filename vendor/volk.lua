-- volk: meta-loader for Vulkan entry points (submodule vendor/volk).
project "volk"
    kind "StaticLib"
    language "C"
    location "../build/vendor"
    staticruntime "off"
    warnings "Off"
    pic "On"
    files { "volk/volk.c", "volk/volk.h" }
    includedirs { "volk", "Vulkan-Headers/include" }
    defines { "VK_NO_PROTOTYPES" }
    filter "system:linux"
        links { "dl" }
    filter {}
