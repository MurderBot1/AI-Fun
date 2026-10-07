#include "glfw/window.h"
#include "vulkan_backend/backend.h"

#include <cstdio>
#include <exception>
#include <memory>

int main() {
    try {
        glfwmod::Window window(1280, 720, "AI-Fun");

        // Bring the Vulkan backend up when a loader is present; the window works either way.
        std::unique_ptr<vkbackend::Backend> backend;
        if (window.vulkanSupported()) {
            try {
                backend = std::make_unique<vkbackend::Backend>(window.vulkanInstanceProcAddr(),
                                                               window.requiredVulkanExtensions());
            } catch (const std::exception& e) {
                std::fprintf(stderr, "Vulkan backend disabled: %s\n", e.what());
            }
        }

        while (!window.shouldClose())
            window.pollEvents();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Fatal: %s\n", e.what());
        return 1;
    }
    return 0;
}
