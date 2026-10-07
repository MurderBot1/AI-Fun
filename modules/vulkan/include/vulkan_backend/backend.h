#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <string>
#include <vector>

namespace vkbackend {

// Owns the Vulkan instance, device and swapchain, and drives a single-frame-in-flight render loop.
//
//   Backend backend(procAddr, extensions);
//   backend.attachSurface(surface, w, h);
//   each frame:  if (backend.beginFrame(cmd)) { ...upload...; backend.beginRenderPass(cmd);
//                                               ...draw...;  backend.endFrame(); }
class Backend {
  public:
    // getInstanceProcAddr is a PFN_vkGetInstanceProcAddr (from GLFW). Throws std::runtime_error.
    Backend(void* getInstanceProcAddr, const std::vector<const char*>& instanceExtensions);
    ~Backend();

    Backend(const Backend&) = delete;
    Backend& operator=(const Backend&) = delete;

    // Takes ownership of the surface. Picks a GPU, creates the device and the swapchain.
    void attachSurface(VkSurfaceKHR surface, uint32_t width, uint32_t height);

    // Call when the framebuffer size changes (0 x 0 means minimised: frames are skipped).
    void resize(uint32_t width, uint32_t height);

    // Waits for the previous frame, acquires an image and begins recording. Returns false when the
    // frame must be skipped (minimised or swapchain being recreated); try again next iteration.
    bool beginFrame(VkCommandBuffer& cmd);
    void beginRenderPass(VkCommandBuffer cmd, float r = 0.02f, float g = 0.02f, float b = 0.03f);
    // Ends the render pass, submits and presents.
    void endFrame();

    void waitIdle();

    VkInstance instance() const { return m_instance; }
    VkPhysicalDevice physicalDevice() const { return m_physicalDevice; }
    VkDevice device() const { return m_device; }
    VkQueue queue() const { return m_queue; }
    uint32_t queueFamily() const { return m_queueFamily; }
    VkRenderPass renderPass() const { return m_renderPass; }
    uint32_t imageCount() const { return static_cast<uint32_t>(m_images.size()); }
    uint32_t minImageCount() const { return m_minImageCount; }
    VkExtent2D extent() const { return m_extent; }
    uint32_t apiVersion() const { return kApiVersion; }
    const std::string& deviceName() const { return m_deviceName; }

    uint32_t findMemoryType(uint32_t typeBits, VkMemoryPropertyFlags props) const;

  private:
    static constexpr uint32_t kApiVersion = VK_API_VERSION_1_1;

    void createDevice();
    void createRenderPass();
    void createSwapchain();
    void destroySwapchainViews();
    void createSyncObjects();

    VkInstance m_instance = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_queue = VK_NULL_HANDLE;
    uint32_t m_queueFamily = 0;
    bool m_portabilityEnumeration = false;
    std::string m_deviceName;

    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    VkSurfaceFormatKHR m_format{};
    VkExtent2D m_extent{};
    uint32_t m_minImageCount = 2;
    std::vector<VkImage> m_images;
    std::vector<VkImageView> m_views;
    std::vector<VkFramebuffer> m_framebuffers;
    VkRenderPass m_renderPass = VK_NULL_HANDLE;

    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    VkCommandBuffer m_commandBuffer = VK_NULL_HANDLE;
    VkSemaphore m_imageAvailable = VK_NULL_HANDLE;
    std::vector<VkSemaphore> m_renderFinished; // one per swapchain image
    VkFence m_inFlight = VK_NULL_HANDLE;

    uint32_t m_width = 0, m_height = 0;
    uint32_t m_imageIndex = 0;
    bool m_needsRecreate = false;
    bool m_inRenderPass = false;
};

} // namespace vkbackend
