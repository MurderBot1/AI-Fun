#include "vulkan_backend/backend.h"

#include <volk.h>

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string>

namespace vkbackend {

namespace {

void check(VkResult result, const char* what) {
    if (result != VK_SUCCESS)
        throw std::runtime_error(std::string(what) + " failed (VkResult " +
                                 std::to_string(static_cast<int>(result)) + ")");
}

bool hasExtension(const std::vector<VkExtensionProperties>& list, const char* name) {
    for (const auto& e : list)
        if (std::strcmp(e.extensionName, name) == 0)
            return true;
    return false;
}

} // namespace

Backend::Backend(void* getInstanceProcAddr, const std::vector<const char*>& instanceExtensions) {
    if (!getInstanceProcAddr)
        throw std::runtime_error("vkGetInstanceProcAddr unavailable");
    volkInitializeCustom(reinterpret_cast<PFN_vkGetInstanceProcAddr>(getInstanceProcAddr));
    if (!vkCreateInstance) // the supplied loader function could not resolve the global entry points
        throw std::runtime_error("Vulkan loader does not provide vkCreateInstance");

    std::vector<const char*> extensions = instanceExtensions;
    for (const char* e : instanceExtensions)
        if (std::strcmp(e, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) == 0)
            m_portabilityEnumeration = true;

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "AI-Fun";
    appInfo.apiVersion = kApiVersion;

    VkInstanceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    info.pApplicationInfo = &appInfo;
    info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    info.ppEnabledExtensionNames = extensions.data();
    if (m_portabilityEnumeration)
        info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

    check(vkCreateInstance(&info, nullptr, &m_instance), "vkCreateInstance");
    volkLoadInstance(m_instance);
}

Backend::~Backend() {
    if (m_device) {
        vkDeviceWaitIdle(m_device);
        if (m_inFlight)
            vkDestroyFence(m_device, m_inFlight, nullptr);
        if (m_imageAvailable)
            vkDestroySemaphore(m_device, m_imageAvailable, nullptr);
        for (VkSemaphore s : m_renderFinished)
            vkDestroySemaphore(m_device, s, nullptr);
        if (m_commandPool)
            vkDestroyCommandPool(m_device, m_commandPool, nullptr);
        destroySwapchainViews();
        if (m_swapchain)
            vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
        if (m_renderPass)
            vkDestroyRenderPass(m_device, m_renderPass, nullptr);
        vkDestroyDevice(m_device, nullptr);
    }
    if (m_surface)
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    if (m_instance)
        vkDestroyInstance(m_instance, nullptr);
}

void Backend::attachSurface(VkSurfaceKHR surface, uint32_t width, uint32_t height) {
    m_surface = surface;
    m_width = width;
    m_height = height;
    createDevice();
    createSwapchain(); // also creates the render pass once the surface format is known
    createSyncObjects();
}

void Backend::createDevice() {
    uint32_t count = 0;
    check(vkEnumeratePhysicalDevices(m_instance, &count, nullptr), "vkEnumeratePhysicalDevices");
    if (count == 0)
        throw std::runtime_error("No Vulkan-capable GPU found");
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(m_instance, &count, devices.data());

    int bestScore = -1;
    for (VkPhysicalDevice dev : devices) {
        uint32_t extCount = 0;
        vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, nullptr);
        std::vector<VkExtensionProperties> exts(extCount);
        vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, exts.data());
        if (!hasExtension(exts, VK_KHR_SWAPCHAIN_EXTENSION_NAME))
            continue;

        uint32_t qCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &qCount, nullptr);
        std::vector<VkQueueFamilyProperties> families(qCount);
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &qCount, families.data());
        for (uint32_t q = 0; q < qCount; ++q) {
            VkBool32 present = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(dev, q, m_surface, &present);
            if (!(families[q].queueFlags & VK_QUEUE_GRAPHICS_BIT) || !present)
                continue;
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(dev, &props);
            int score = props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? 2 : 1;
            if (score > bestScore) {
                bestScore = score;
                m_physicalDevice = dev;
                m_queueFamily = q;
            }
            break;
        }
    }
    if (!m_physicalDevice)
        throw std::runtime_error("No GPU with graphics + present support for this window");

    VkPhysicalDeviceProperties chosenProps;
    vkGetPhysicalDeviceProperties(m_physicalDevice, &chosenProps);
    m_deviceName = chosenProps.deviceName;

    uint32_t extCount = 0;
    vkEnumerateDeviceExtensionProperties(m_physicalDevice, nullptr, &extCount, nullptr);
    std::vector<VkExtensionProperties> exts(extCount);
    vkEnumerateDeviceExtensionProperties(m_physicalDevice, nullptr, &extCount, exts.data());

    std::vector<const char*> deviceExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    if (hasExtension(exts, "VK_KHR_portability_subset")) // MoltenVK requires this to be enabled
        deviceExtensions.push_back("VK_KHR_portability_subset");

    const float priority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo{};
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = m_queueFamily;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &priority;

    VkDeviceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    info.queueCreateInfoCount = 1;
    info.pQueueCreateInfos = &queueInfo;
    info.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    info.ppEnabledExtensionNames = deviceExtensions.data();
    check(vkCreateDevice(m_physicalDevice, &info, nullptr, &m_device), "vkCreateDevice");
    volkLoadDevice(m_device);
    vkGetDeviceQueue(m_device, m_queueFamily, 0, &m_queue);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = m_queueFamily;
    check(vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool), "vkCreateCommandPool");

    VkCommandBufferAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc.commandPool = m_commandPool;
    alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc.commandBufferCount = 1;
    check(vkAllocateCommandBuffers(m_device, &alloc, &m_commandBuffer), "vkAllocateCommandBuffers");
}

void Backend::createRenderPass() {
    if (m_renderPass)
        return;
    VkAttachmentDescription color{};
    color.format = m_format.format;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference ref{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &ref;

    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.attachmentCount = 1;
    info.pAttachments = &color;
    info.subpassCount = 1;
    info.pSubpasses = &subpass;
    info.dependencyCount = 1;
    info.pDependencies = &dep;
    check(vkCreateRenderPass(m_device, &info, nullptr, &m_renderPass), "vkCreateRenderPass");
}

void Backend::destroySwapchainViews() {
    for (VkFramebuffer f : m_framebuffers)
        vkDestroyFramebuffer(m_device, f, nullptr);
    for (VkImageView v : m_views)
        vkDestroyImageView(m_device, v, nullptr);
    m_framebuffers.clear();
    m_views.clear();
}

// (Re)creates the swapchain and, once the render pass exists, its framebuffers.
void Backend::createSwapchain() {
    VkSurfaceCapabilitiesKHR caps;
    check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface, &caps),
          "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");

    VkExtent2D extent = caps.currentExtent;
    if (extent.width == UINT32_MAX) {
        extent.width = std::clamp(m_width, caps.minImageExtent.width, caps.maxImageExtent.width);
        extent.height =
            std::clamp(m_height, caps.minImageExtent.height, caps.maxImageExtent.height);
    }
    if (extent.width == 0 || extent.height == 0) {
        m_needsRecreate = true; // minimised; retry once we have a size again
        return;
    }

    uint32_t fmtCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &fmtCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(fmtCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &fmtCount, formats.data());
    if (formats.empty())
        throw std::runtime_error("Surface reports no formats");
    // UNORM: the path tracer already gamma-encodes, and ImGui expects a non-sRGB target.
    m_format = formats[0];
    for (const auto& f : formats)
        if (f.format == VK_FORMAT_B8G8R8A8_UNORM || f.format == VK_FORMAT_R8G8B8A8_UNORM) {
            m_format = f;
            break;
        }

    createRenderPass();

    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0)
        imageCount = std::min(imageCount, caps.maxImageCount);
    m_minImageCount = caps.minImageCount;

    VkCompositeAlphaFlagBitsKHR alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    if (!(caps.supportedCompositeAlpha & alpha))
        alpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;

    VkSwapchainCreateInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.surface = m_surface;
    info.minImageCount = imageCount;
    info.imageFormat = m_format.format;
    info.imageColorSpace = m_format.colorSpace;
    info.imageExtent = extent;
    info.imageArrayLayers = 1;
    info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.preTransform = caps.currentTransform;
    info.compositeAlpha = alpha;
    info.presentMode = VK_PRESENT_MODE_FIFO_KHR; // always available
    info.clipped = VK_TRUE;

    vkDeviceWaitIdle(m_device);
    destroySwapchainViews();
    VkSwapchainKHR old = m_swapchain;
    info.oldSwapchain = old;
    check(vkCreateSwapchainKHR(m_device, &info, nullptr, &m_swapchain), "vkCreateSwapchainKHR");
    if (old)
        vkDestroySwapchainKHR(m_device, old, nullptr);
    m_extent = extent;

    uint32_t n = 0;
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &n, nullptr);
    m_images.resize(n);
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &n, m_images.data());

    m_views.resize(n);
    for (uint32_t i = 0; i < n; ++i) {
        VkImageViewCreateInfo v{};
        v.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        v.image = m_images[i];
        v.viewType = VK_IMAGE_VIEW_TYPE_2D;
        v.format = m_format.format;
        v.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        check(vkCreateImageView(m_device, &v, nullptr, &m_views[i]), "vkCreateImageView");
    }

    if (m_renderPass) {
        m_framebuffers.resize(n);
        for (uint32_t i = 0; i < n; ++i) {
            VkFramebufferCreateInfo fb{};
            fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            fb.renderPass = m_renderPass;
            fb.attachmentCount = 1;
            fb.pAttachments = &m_views[i];
            fb.width = extent.width;
            fb.height = extent.height;
            fb.layers = 1;
            check(vkCreateFramebuffer(m_device, &fb, nullptr, &m_framebuffers[i]),
                  "vkCreateFramebuffer");
        }
    }

    // One render-finished semaphore per image (the count can change on recreation).
    for (VkSemaphore s : m_renderFinished)
        vkDestroySemaphore(m_device, s, nullptr);
    m_renderFinished.assign(n, VK_NULL_HANDLE);
    for (auto& s : m_renderFinished) {
        VkSemaphoreCreateInfo si{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        check(vkCreateSemaphore(m_device, &si, nullptr, &s), "vkCreateSemaphore");
    }
    m_needsRecreate = false;
}

void Backend::createSyncObjects() {
    VkSemaphoreCreateInfo si{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    check(vkCreateSemaphore(m_device, &si, nullptr, &m_imageAvailable), "vkCreateSemaphore");
    VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    check(vkCreateFence(m_device, &fi, nullptr, &m_inFlight), "vkCreateFence");
}

void Backend::resize(uint32_t width, uint32_t height) {
    if (width == m_width && height == m_height)
        return;
    m_width = width;
    m_height = height;
    m_needsRecreate = true;
}

bool Backend::beginFrame(VkCommandBuffer& cmd) {
    if (m_width == 0 || m_height == 0)
        return false;
    if (m_needsRecreate) {
        createSwapchain();
        if (m_needsRecreate) // still minimised
            return false;
    }

    vkWaitForFences(m_device, 1, &m_inFlight, VK_TRUE, UINT64_MAX);

    VkResult r = vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX, m_imageAvailable,
                                       VK_NULL_HANDLE, &m_imageIndex);
    if (r == VK_ERROR_OUT_OF_DATE_KHR) {
        m_needsRecreate = true;
        return false;
    }
    if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR)
        check(r, "vkAcquireNextImageKHR");

    // Only reset the fence once we know we will submit work that signals it again.
    vkResetFences(m_device, 1, &m_inFlight);
    vkResetCommandBuffer(m_commandBuffer, 0);
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    check(vkBeginCommandBuffer(m_commandBuffer, &begin), "vkBeginCommandBuffer");
    cmd = m_commandBuffer;
    return true;
}

void Backend::beginRenderPass(VkCommandBuffer cmd, float r, float g, float b) {
    VkClearValue clear{};
    clear.color = {{r, g, b, 1.0f}};
    VkRenderPassBeginInfo info{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    info.renderPass = m_renderPass;
    info.framebuffer = m_framebuffers[m_imageIndex];
    info.renderArea = {{0, 0}, m_extent};
    info.clearValueCount = 1;
    info.pClearValues = &clear;
    vkCmdBeginRenderPass(cmd, &info, VK_SUBPASS_CONTENTS_INLINE);
    m_inRenderPass = true;
}

void Backend::endFrame() {
    if (m_inRenderPass) {
        vkCmdEndRenderPass(m_commandBuffer);
        m_inRenderPass = false;
    }
    check(vkEndCommandBuffer(m_commandBuffer), "vkEndCommandBuffer");

    const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &m_imageAvailable;
    submit.pWaitDstStageMask = &waitStage;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &m_commandBuffer;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &m_renderFinished[m_imageIndex];
    check(vkQueueSubmit(m_queue, 1, &submit, m_inFlight), "vkQueueSubmit");

    VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &m_renderFinished[m_imageIndex];
    present.swapchainCount = 1;
    present.pSwapchains = &m_swapchain;
    present.pImageIndices = &m_imageIndex;
    VkResult r = vkQueuePresentKHR(m_queue, &present);
    if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR)
        m_needsRecreate = true;
    else
        check(r, "vkQueuePresentKHR");
}

void Backend::waitIdle() {
    if (m_device)
        vkDeviceWaitIdle(m_device);
}

uint32_t Backend::findMemoryType(uint32_t typeBits, VkMemoryPropertyFlags props) const {
    VkPhysicalDeviceMemoryProperties mem;
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &mem);
    for (uint32_t i = 0; i < mem.memoryTypeCount; ++i)
        if ((typeBits & (1u << i)) && (mem.memoryTypes[i].propertyFlags & props) == props)
            return i;
    throw std::runtime_error("No suitable Vulkan memory type");
}

} // namespace vkbackend
