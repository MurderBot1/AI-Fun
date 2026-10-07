#include "audio/engine.h"
#include "glfw/window.h"
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

// Orbit camera driven by the mouse; converts to the path tracer's Camera.
struct OrbitCamera {
    float yaw = 0.0f;
    float pitch = 0.18f;
    float distance = 7.0f;
    rt::Vec3 target{0, 1, 0};
    float fov = 40.0f;
    float aperture = 0.0f;
    float focus = 7.0f;

    rt::Camera toCamera() const {
        rt::Camera c;
        c.position = target + rt::Vec3{std::sin(yaw) * std::cos(pitch) * distance,
                                       std::sin(pitch) * distance,
                                       std::cos(yaw) * std::cos(pitch) * distance};
        c.target = target;
        c.verticalFovDegrees = fov;
        c.aperture = aperture;
        c.focusDistance = focus;
        return c;
    }
};

} // namespace

int main() {
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

        ui::Layer ui(window, backend);
        vkbackend::Texture texture(backend, kRenderWidth, kRenderHeight);
        const ImTextureID imageId = ui.addTexture(texture);

        audio::Engine audio;

        rt::Renderer renderer(kRenderWidth, kRenderHeight);
        renderer.setScene(rt::makeDemoScene());
        OrbitCamera orbit;
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

        while (!window.shouldClose()) {
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
                    orbit.yaw -= io.MouseDelta.x * 0.005f;
                    orbit.pitch = std::clamp(orbit.pitch + io.MouseDelta.y * 0.005f, -1.4f, 1.4f);
                    cameraChanged |= io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f;
                }
                if (io.MouseWheel != 0.0f) {
                    orbit.distance =
                        std::clamp(orbit.distance * (1.0f - io.MouseWheel * 0.1f), 1.5f, 40.0f);
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
