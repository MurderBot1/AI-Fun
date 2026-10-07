project "raytracer"
    kind "StaticLib"
    location "../../build/modules/raytracer"
    files { "include/**.h", "src/**.cpp" }
    includedirs { "include" }
    filter "system:linux"
        links { "pthread" }
    filter {}
