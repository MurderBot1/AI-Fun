-- Dear ImGui with the GLFW + Vulkan backends (submodule vendor/imgui). Vulkan symbols come from volk.
project "ImGui"
    kind "StaticLib"
    language "C++"
    location "../build/vendor"
    staticruntime "off"
    warnings "Off"
    pic "On"
    files {
        "imgui/imgui.cpp", "imgui/imgui_draw.cpp", "imgui/imgui_tables.cpp",
        "imgui/imgui_widgets.cpp", "imgui/imgui_demo.cpp",
        "imgui/backends/imgui_impl_glfw.cpp", "imgui/backends/imgui_impl_vulkan.cpp",
    }
    includedirs { "imgui", "imgui/backends", "glfw/include", "volk", "Vulkan-Headers/include" }
    defines { "GLFW_INCLUDE_NONE", "VK_NO_PROTOTYPES", "IMGUI_IMPL_VULKAN_USE_VOLK" }
