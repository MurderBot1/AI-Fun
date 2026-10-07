-- GLFW static library, built from the vendor/glfw submodule.
project "GLFW"
    kind "StaticLib"
    language "C"
    location "../build/vendor"
    staticruntime "off"
    warnings "Off"

    local src = "glfw/src/"
    files {
        "glfw/include/GLFW/glfw3.h", "glfw/include/GLFW/glfw3native.h",
        src .. "context.c", src .. "init.c", src .. "input.c", src .. "monitor.c",
        src .. "platform.c", src .. "vulkan.c", src .. "window.c",
        src .. "egl_context.c", src .. "osmesa_context.c",
        src .. "null_init.c", src .. "null_monitor.c", src .. "null_window.c", src .. "null_joystick.c",
    }
    includedirs { "glfw/include" }

    filter "system:linux"
        pic "On"
        defines { "_GLFW_X11" }
        files {
            src .. "x11_init.c", src .. "x11_monitor.c", src .. "x11_window.c",
            src .. "xkb_unicode.c", src .. "glx_context.c", src .. "posix_module.c",
            src .. "posix_thread.c", src .. "posix_time.c", src .. "posix_poll.c",
            src .. "linux_joystick.c",
        }

    filter "system:windows"
        defines { "_GLFW_WIN32", "_CRT_SECURE_NO_WARNINGS" }
        files {
            src .. "win32_init.c", src .. "win32_joystick.c", src .. "win32_module.c",
            src .. "win32_monitor.c", src .. "win32_time.c", src .. "win32_thread.c",
            src .. "win32_window.c", src .. "wgl_context.c",
        }

    filter "system:macosx"
        defines { "_GLFW_COCOA" }
        files {
            src .. "cocoa_init.m", src .. "cocoa_joystick.m", src .. "cocoa_monitor.m",
            src .. "cocoa_window.m", src .. "nsgl_context.m", src .. "cocoa_time.c",
            src .. "posix_module.c", src .. "posix_thread.c",
        }
    filter {}
