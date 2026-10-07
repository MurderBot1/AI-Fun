#include "ui/layer.h"

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <volk.h>

#include <stdexcept>

namespace ui {

Layer::Layer(glfwmod::Window& window, vkbackend::Backend& backend) : m_backend(backend) {
    // Descriptor sets for ImGui::Image() textures (plus headroom for the font atlas).
    VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 32};
    VkDescriptorPoolCreateInfo pool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pool.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool.maxSets = 32;
    pool.poolSizeCount = 1;
    pool.pPoolSizes = &size;
    if (vkCreateDescriptorPool(backend.device(), &pool, nullptr, &m_pool) != VK_SUCCESS)
        throw std::runtime_error("Failed to create ImGui descriptor pool");

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForVulkan(window.handle(), true);

    ImGui_ImplVulkan_InitInfo info{};
    info.ApiVersion = backend.apiVersion();
    info.Instance = backend.instance();
    info.PhysicalDevice = backend.physicalDevice();
    info.Device = backend.device();
    info.QueueFamily = backend.queueFamily();
    info.Queue = backend.queue();
    info.DescriptorPool = m_pool;
    info.RenderPass = backend.renderPass();
    info.MinImageCount = backend.minImageCount() < 2 ? 2 : backend.minImageCount();
    info.ImageCount =
        backend.imageCount() < info.MinImageCount ? info.MinImageCount : backend.imageCount();
    info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    if (!ImGui_ImplVulkan_Init(&info))
        throw std::runtime_error("Failed to initialise the ImGui Vulkan backend");
}

Layer::~Layer() {
    m_backend.waitIdle();
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    vkDestroyDescriptorPool(m_backend.device(), m_pool, nullptr);
}

void Layer::beginFrame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void Layer::render(VkCommandBuffer cmd) {
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}

void Layer::discardFrame() {
    ImGui::EndFrame();
}

ImTextureID Layer::addTexture(const vkbackend::Texture& texture) {
    VkDescriptorSet set = ImGui_ImplVulkan_AddTexture(texture.sampler(), texture.view(),
                                                      vkbackend::Texture::kShaderLayout);
    return static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(set));
}

} // namespace ui
