project "vulkan_backend"
    kind "StaticLib"
    location "../../build/modules/vulkan"
    files { "include/**.h", "src/**.cpp" }
    -- Only the Vulkan headers are needed: the loader is resolved at runtime
    -- through the function pointer handed over by the window module.
    includedirs { "include", "../../vendor/Vulkan-Headers/include" }
    defines { "VK_NO_PROTOTYPES" }
