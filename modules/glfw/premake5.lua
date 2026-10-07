project "glfw_module"
    kind "StaticLib"
    location "../../build/modules/glfw"
    files { "include/**.h", "src/**.cpp" }
    includedirs { "include", "../../vendor/glfw/include", "../../vendor/Vulkan-Headers/include" }
    defines { "GLFW_INCLUDE_NONE", "GLFW_INCLUDE_VULKAN", "VK_NO_PROTOTYPES" }
    links { "GLFW" }

    filter "system:linux"
        links { "X11", "pthread", "dl" }
    filter "system:windows"
        links { "gdi32", "user32", "shell32" }
    filter "system:macosx"
        linkoptions { "-framework Cocoa", "-framework IOKit", "-framework CoreFoundation" }
    filter {}
