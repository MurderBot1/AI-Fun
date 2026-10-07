project "glfw_module"
    kind "StaticLib"
    location "../../build/modules/glfw"
    files { "include/**.h", "src/**.cpp" }
    includedirs { "include", "../../vendor/glfw/include", "../../vendor/Vulkan-Headers/include" }
    defines { "GLFW_INCLUDE_NONE", "GLFW_INCLUDE_VULKAN", "VK_NO_PROTOTYPES" }
    links { "GLFW" }
