#include "glfw/window.h"

#include <GLFW/glfw3.h>

#include <stdexcept>

namespace glfwmod {

Window::Window(int width, int height, const std::string& title) {
    if (!glfwInit())
        throw std::runtime_error("Failed to initialise GLFW");

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    m_window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!m_window) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }
}

Window::~Window() {
    glfwDestroyWindow(m_window);
    glfwTerminate();
}

bool Window::shouldClose() const {
    return glfwWindowShouldClose(m_window) != 0;
}

void Window::pollEvents() const {
    glfwPollEvents();
}

bool Window::vulkanSupported() const {
    return glfwVulkanSupported() == GLFW_TRUE;
}

std::vector<const char*> Window::requiredVulkanExtensions() const {
    uint32_t count = 0;
    const char** exts = glfwGetRequiredInstanceExtensions(&count);
    return exts ? std::vector<const char*>(exts, exts + count) : std::vector<const char*>{};
}

void* Window::vulkanInstanceProcAddr() const {
    return reinterpret_cast<void*>(glfwGetInstanceProcAddress(nullptr, "vkGetInstanceProcAddr"));
}

bool Window::createVulkanSurface(void* instance, void* surfaceOut) const {
    return glfwCreateWindowSurface(static_cast<VkInstance>(instance), m_window, nullptr,
                                   static_cast<VkSurfaceKHR*>(surfaceOut)) == VK_SUCCESS;
}

void Window::framebufferSize(int& width, int& height) const {
    glfwGetFramebufferSize(m_window, &width, &height);
}

} // namespace glfwmod
