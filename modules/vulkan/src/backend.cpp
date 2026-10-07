#include "vulkan_backend/backend.h"

#include <stdexcept>

namespace vkbackend {

Backend::Backend(void* getInstanceProcAddr, const std::vector<const char*>& instanceExtensions) {
    m_getInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(getInstanceProcAddr);
    if (!m_getInstanceProcAddr)
        throw std::runtime_error("vkGetInstanceProcAddr unavailable");

    auto createInstance = reinterpret_cast<PFN_vkCreateInstance>(
        m_getInstanceProcAddr(VK_NULL_HANDLE, "vkCreateInstance"));
    if (!createInstance)
        throw std::runtime_error("vkCreateInstance unavailable");

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "AI-Fun";
    appInfo.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    info.pApplicationInfo = &appInfo;
    info.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
    info.ppEnabledExtensionNames = instanceExtensions.data();

    if (createInstance(&info, nullptr, &m_instance) != VK_SUCCESS)
        throw std::runtime_error("Failed to create VkInstance");

    m_destroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(
        m_getInstanceProcAddr(m_instance, "vkDestroyInstance"));
}

Backend::~Backend() {
    if (m_instance && m_destroyInstance)
        m_destroyInstance(m_instance, nullptr);
}

} // namespace vkbackend
