project "ui"
    kind "StaticLib"
    location "../../build/modules/ui"
    files { "include/**.h", "src/**.cpp" }
    includedirs {
        "include", "../glfw/include", "../vulkan/include",
        "../../vendor/imgui", "../../vendor/imgui/backends", "../../vendor/glfw/include",
        "../../vendor/volk", "../../vendor/Vulkan-Headers/include",
    }
    defines { "GLFW_INCLUDE_NONE", "VK_NO_PROTOTYPES", "IMGUI_IMPL_VULKAN_USE_VOLK" }
