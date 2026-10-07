#include "audio/engine.h"
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

namespace {

constexpr int kRenderWidth = 960;
constexpr int kRenderHeight = 540;

} // namespace

int main(int argc, char** argv) {
    const Options options = parseOptions(argc, argv);
    if (!options.valid) {
        std::fprintf(stderr, "%s\nusage: app [--demo] [--seconds N]\n", options.error.c_str());
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

        ui::Layer ui(window, backend);
        vkbackend::Texture texture(backend, kRenderWidth, kRenderHeight);
        const ImTextureID imageId = ui.addTexture(texture);

        audio::Engine audio;

        rt::Renderer renderer(kRenderWidth, kRenderHeight);
        renderer.setScene(rt::makeDemoScene());
        rt::OrbitCamera orbit;
        renderer.setCamera(orbit.toCamera());
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
        double demoCameraTimer = 0.0;

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

            // Mouse orbit (right button) and zoom (wheel) unless ImGui wants the mouse.
            bool cameraChanged = false;
            if (!io.WantCaptureMouse) {
                if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                    orbit.rotate(-io.MouseDelta.x * 0.005f, io.MouseDelta.y * 0.005f);
                    cameraChanged |= io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f;
                }
                if (io.MouseWheel != 0.0f) {
                    orbit.zoom(1.0f - io.MouseWheel * 0.1f);
                    cameraChanged = true;
                }
            }

            // Demo mode: slowly orbit. The renderer restarts accumulation on every camera push, so
            // push at a low rate to let a few samples build up between moves.
            if (options.demo) {
                orbit.rotate(dt * 0.3f, 0.0f);
                demoCameraTimer += dt;
                if (demoCameraTimer >= 0.5) {
                    demoCameraTimer = 0.0;
                    cameraChanged = true;
                }
            }

            ImGui::SetNextWindowPos(ImVec2(12, 12), ImGuiCond_FirstUseEver);
            ImGui::Begin("Ray Tracer", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Text("Samples: %u", renderer.sampleCount());
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

            // Upload the newest ray-traced frame when there is one.
            const uint32_t samples = renderer.sampleCount();
            if (samples != uploadedSamples) {
                uploadedSamples = renderer.copyFrame(pixels);
                texture.setPixels(pixels.data());
            }
            texture.recordUpload(cmd);

            // Draw the render letterboxed behind the UI.
            const ImVec2 area = io.DisplaySize;
            const float scale = std::min(area.x / kRenderWidth, area.y / kRenderHeight);
            const ImVec2 size(kRenderWidth * scale, kRenderHeight * scale);
            const ImVec2 min((area.x - size.x) * 0.5f, (area.y - size.y) * 0.5f);
            ImGui::GetBackgroundDrawList()->AddImage(imageId, min,
                                                     ImVec2(min.x + size.x, min.y + size.y));

            backend.beginRenderPass(cmd);
            ui.render(cmd);
            backend.endFrame();
        }

        renderer.stop();
        backend.waitIdle();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Fatal: %s\n", e.what());
        return 1;
    }
    return 0;
}
