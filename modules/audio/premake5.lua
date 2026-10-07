project "audio"
    kind "StaticLib"
    location "../../build/modules/audio"
    files { "include/**.h", "src/**.cpp" }
    includedirs { "include", "../../vendor/miniaudio" }
    filter "system:linux"
        links { "pthread", "dl", "m" }
    filter "system:macosx"
        linkoptions { "-framework CoreFoundation", "-framework CoreAudio", "-framework AudioToolbox" }
    filter {}
