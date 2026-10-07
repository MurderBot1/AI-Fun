#include "audio/engine.h"
#include "demo_director.h"
#include "glfw/window.h"
#include "options.h"
#include "raytracer/orbit_camera.h"
#include "raytracer/renderer.h"
#include "ui/layer.h"
#include "vulkan_backend/backend.h"
#include "vulkan_backend/texture.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <exception>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {} // namespace

int main(int argc, char** argv) {
    const Options options = parseOptions(argc, argv);
    if (!options.valid) {
        std::fprintf(
            stderr,
            "%s\nusage: app [--demo] [--demo-samples N] [--demo-fps N] [--demo-duration S] "
            "[--demo-sweep DEG] [--demo-pingpong] [--size WxH] [--seconds N]\n",
            options.error.c_str());
        return 2;
    }

    try {
        glfwmod::Window window(1280, 720, "AI-Fun Ray Tracer");

        if (!window.vulkanSupported())
            throw std::runtime_error("Vulkan is not supported on this system (no loader/driver)");

        vkbackend::Backend backend(window.vulkanInstanceProcAddr(),
                                   window.requiredVulkanExtensions());
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        if (!window.createVulkanSurface(backend.instance(), &surface))
            throw std::runtime_error("Failed to create the Vulkan window surface");
        int fbw = 0, fbh = 0;
        window.framebufferSize(fbw, fbh);
        backend.attachSurface(surface, static_cast<uint32_t>(fbw), static_cast<uint32_t>(fbh));
        std::printf("Vulkan device: %s\n", backend.deviceName().c_str());
        std::fflush(stdout);

        ui::Layer ui(window, backend);
        const int renderWidth = options.renderWidth;
        const int renderHeight = options.renderHeight;
        vkbackend::Texture texture(backend, static_cast<uint32_t>(renderWidth),
                                   static_cast<uint32_t>(renderHeight));
        const ImTextureID imageId = ui.addTexture(texture);

        audio::Engine audio;

        rt::Renderer renderer(renderWidth, renderHeight);
        renderer.setScene(rt::makeDemoScene());
        rt::OrbitCamera orbit;
        renderer.setCamera(orbit.toCamera());
        if (options.demo) // stop accumulating once a frame has converged
            renderer.setSampleLimit(options.demoSamples);
        renderer.start();

        std::vector<uint8_t> pixels;
        uint32_t uploadedSamples = 0;
        int maxDepth = 8;
        int sampleLimit = 0;
        float volume = 0.5f;
        bool tone = false;
        float toneHz = 440.0f;
        audio.setMasterVolume(volume);

        const auto startTime = std::chrono::steady_clock::now();
        auto lastTime = startTime;
        DemoDirector::Config demoConfig;
        demoConfig.targetSamples = options.demoSamples;
        demoConfig.fps = options.demoFps;
        demoConfig.frames =
            static_cast<uint32_t>(std::lround(options.demoFps * options.demoDurationSeconds));
        demoConfig.totalYaw =
            static_cast<float>(options.demoSweepDegrees * 3.14159265358979 / 180.0);
        demoConfig.pingPong = options.demoPingPong;
        DemoDirector director(demoConfig);
        std::vector<std::vector<uint8_t>> bakedFrames; // demo: every converged frame, in order
        const float startYaw = orbit.yaw;
        int shownFrame = -1;
        bool announcePlayback = false;

        while (!window.shouldClose()) {
            const auto now = std::chrono::steady_clock::now();
            const double elapsed = std::chrono::duration<double>(now - startTime).count();
            const float dt = std::chrono::duration<float>(now - lastTime).count();
            lastTime = now;
            if (options.seconds > 0.0 && elapsed >= options.seconds)
                window.requestClose();

            window.pollEvents();
            window.framebufferSize(fbw, fbh);
            backend.resize(static_cast<uint32_t>(fbw), static_cast<uint32_t>(fbh));

            VkCommandBuffer cmd = VK_NULL_HANDLE;
            if (!backend.beginFrame(cmd)) {
                // Minimised or swapchain being rebuilt: don't spin.
                std::this_thread::sleep_for(std::chrono::milliseconds(16));
                continue;
            }

            ui.beginFrame();
            ImGuiIO& io = ImGui::GetIO();

            // Mouse orbit (right button) and zoom (wheel) unless ImGui wants the mouse. The
            // scripted demo owns the camera.
            bool cameraChanged = false;
            if (!options.demo && !io.WantCaptureMouse) {
                if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                    orbit.rotate(-io.MouseDelta.x * 0.005f, io.MouseDelta.y * 0.005f);
                    cameraChanged |= io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f;
                }
                if (io.MouseWheel != 0.0f) {
                    orbit.zoom(1.0f - io.MouseWheel * 0.1f);
                    cameraChanged = true;
                }
            }

            // Demo mode, as if rendered in real time: first bake every frame of a smooth camera
            // sweep (each fully converged, rendered out of sight), then play them back at a fixed
            // frame rate.
            if (options.demo) {
                if (director.phase() == DemoDirector::Phase::Baking) {
                    // Read the generation first: the renderer publishes samples before generation,
                    // so a generation seen here can never be paired with an older sample count.
                    const uint32_t generation = renderer.publishedGeneration();
                    const uint32_t samples = renderer.sampleCount();
                    if (director.bakeUpdate(samples, generation)) {
                        bakedFrames.emplace_back();
                        renderer.copyFrame(bakedFrames.back());
                        texture.setPixels(bakedFrames.back().data()); // latest baked frame
                        if (director.phase() == DemoDirector::Phase::Baking) {
                            orbit.yaw = startYaw + director.yawFor(director.captured());
                            cameraChanged = true;
                        }
                    }
                } else {
                    const uint32_t frame = director.playbackFrame(dt);
                    if (static_cast<int>(frame) != shownFrame) {
                        texture.setPixels(bakedFrames[frame].data());
                        if (shownFrame < 0)
                            announcePlayback = true;
                        shownFrame = static_cast<int>(frame);
                    }
                    if (director.phase() == DemoDirector::Phase::Done)
                        window.requestClose();
                }
            }

            ImGui::SetNextWindowPos(ImVec2(12, 12), ImGuiCond_FirstUseEver);
            ImGui::Begin("Ray Tracer", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
            if (options.demo) {
                if (director.phase() == DemoDirector::Phase::Baking) {
                    ImGui::Text("Demo: baking frame %u/%u", director.captured() + 1,
                                director.frames());
                    ImGui::Text("Frame progress: %u/%u spp", renderer.sampleCount(),
                                options.demoSamples);
                } else {
                    ImGui::Text("Demo: playback %.0f fps", demoConfig.fps);
                }
            } else {
                ImGui::Text("Samples: %u", renderer.sampleCount());
            }
            ImGui::Text("%.1f FPS (UI)", io.Framerate);
            ImGui::Separator();
            if (ImGui::SliderInt("Max bounces", &maxDepth, 1, 16))
                renderer.setMaxDepth(maxDepth);
            if (ImGui::SliderInt("Sample limit (0 = off)", &sampleLimit, 0, 2048))
                renderer.setSampleLimit(static_cast<uint32_t>(sampleLimit));
            cameraChanged |= ImGui::SliderFloat("FOV", &orbit.fov, 10.0f, 90.0f);
            cameraChanged |= ImGui::SliderFloat("Aperture", &orbit.aperture, 0.0f, 1.0f);
            cameraChanged |= ImGui::SliderFloat("Focus distance", &orbit.focus, 1.0f, 30.0f);
            if (ImGui::Button("Restart accumulation"))
                cameraChanged = true;
            ImGui::TextDisabled("Right-drag: orbit   Wheel: zoom");
            ImGui::End();

            ImGui::SetNextWindowPos(ImVec2(12, 300), ImGuiCond_FirstUseEver);
            ImGui::Begin("Audio", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Text("Device: %s", audio.ok() ? "ready" : "unavailable");
            if (ImGui::SliderFloat("Volume", &volume, 0.0f, 1.0f))
                audio.setMasterVolume(volume);
            bool toneChanged = ImGui::Checkbox("Test tone", &tone);
            toneChanged |= ImGui::SliderFloat("Frequency (Hz)", &toneHz, 80.0f, 2000.0f);
            if (toneChanged)
                audio.setTone(tone, toneHz);
            ImGui::End();

            if (cameraChanged)
                renderer.setCamera(orbit.toCamera());

            // Interactive mode shows the progressively refining image; demo mode only shows
            // finished frames (uploaded above).
            if (!options.demo && renderer.sampleCount() != uploadedSamples) {
                uploadedSamples = renderer.copyFrame(pixels);
                texture.setPixels(pixels.data());
            }
            texture.recordUpload(cmd);

            // Draw the render letterboxed behind the UI.
            const ImVec2 area = io.DisplaySize;
            const float scale = std::min(area.x / renderWidth, area.y / renderHeight);
            const ImVec2 size(renderWidth * scale, renderHeight * scale);
            const ImVec2 min((area.x - size.x) * 0.5f, (area.y - size.y) * 0.5f);
            ImGui::GetBackgroundDrawList()->AddImage(imageId, min,
                                                     ImVec2(min.x + size.x, min.y + size.y));

            backend.beginRenderPass(cmd);
            ui.render(cmd);
            backend.endFrame();

            if (announcePlayback) {
                // The recording script waits for this line before it starts capturing.
                std::printf("DEMO_PLAYBACK_START\n");
                std::fflush(stdout);
                announcePlayback = false;
            }
            if (options.demo) {
                // While baking, keep a software-rendered UI from starving the path tracer of CPU;
                // during playback the renderer is idle and the loop must keep the frame rate.
                const bool baking = director.phase() == DemoDirector::Phase::Baking;
                std::this_thread::sleep_for(std::chrono::milliseconds(baking ? 100 : 8));
            }
        }

        renderer.stop();
        backend.waitIdle();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Fatal: %s\n", e.what());
        return 1;
    }
    return 0;
}
