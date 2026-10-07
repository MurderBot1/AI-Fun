project "vulkan_backend"
    kind "StaticLib"
    location "../../build/modules/vulkan"
    files { "include/**.h", "src/**.cpp" }
    -- Entry points are loaded at runtime with volk through the vkGetInstanceProcAddr pointer
    -- handed over by the window module, so no Vulkan loader library is linked.
    includedirs { "include", "../../vendor/Vulkan-Headers/include", "../../vendor/volk" }
    defines { "VK_NO_PROTOTYPES" }
