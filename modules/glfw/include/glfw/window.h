#pragma once

#include <string>
#include <vector>

struct GLFWwindow;

namespace glfwmod {

// RAII wrapper around GLFW init + a single window (no client API; Vulkan renders into it).
class Window {
  public:
    Window(int width, int height, const std::string& title);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool shouldClose() const;
    void pollEvents() const;

    GLFWwindow* handle() const { return m_window; }

    // Vulkan helpers; only valid if vulkanSupported() is true.
    bool vulkanSupported() const;
    std::vector<const char*> requiredVulkanExtensions() const;
    // Returns a PFN_vkGetInstanceProcAddr (as void*) so the Vulkan module needs no linked loader.
    void* vulkanInstanceProcAddr() const;
    // Creates a VkSurfaceKHR for this window. `instance` is a VkInstance and `surfaceOut` points to
    // a VkSurfaceKHR (both type-erased so this header needs no Vulkan include). Returns true on
    // success.
    bool createVulkanSurface(void* instance, void* surfaceOut) const;

    void framebufferSize(int& width, int& height) const;

  private:
    GLFWwindow* m_window = nullptr;
};

} // namespace glfwmod
