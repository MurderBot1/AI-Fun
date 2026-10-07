#pragma once

#include <vulkan/vulkan.h>

#include <vector>

namespace vkbackend {

// Minimal Vulkan backend: owns a VkInstance. Entry points are loaded from the
// vkGetInstanceProcAddr pointer supplied by the caller (e.g. from GLFW).
class Backend {
  public:
    Backend(void* getInstanceProcAddr, const std::vector<const char*>& instanceExtensions);
    ~Backend();

    Backend(const Backend&) = delete;
    Backend& operator=(const Backend&) = delete;

    VkInstance instance() const { return m_instance; }

  private:
    PFN_vkGetInstanceProcAddr m_getInstanceProcAddr = nullptr;
    PFN_vkDestroyInstance m_destroyInstance = nullptr;
    VkInstance m_instance = VK_NULL_HANDLE;
};

} // namespace vkbackend
