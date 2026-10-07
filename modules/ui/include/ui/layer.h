#pragma once

#include "glfw/window.h"
#include "vulkan_backend/backend.h"
#include "vulkan_backend/texture.h"

#include <imgui.h>

namespace ui {

// Dear ImGui wired to the GLFW window and the Vulkan backend's render pass.
class Layer {
  public:
    Layer(glfwmod::Window& window, vkbackend::Backend& backend);
    ~Layer();

    Layer(const Layer&) = delete;
    Layer& operator=(const Layer&) = delete;

    // Call once per frame before issuing any ImGui:: widgets.
    void beginFrame();
    // Call inside the backend's render pass, after the widgets for this frame.
    void render(VkCommandBuffer cmd);

    // Alternative to render(): closes the ImGui frame without drawing (e.g. when a frame is
    // skipped).
    void discardFrame();

    // Makes a texture usable with ImGui::Image(). The texture must outlive the Layer.
    ImTextureID addTexture(const vkbackend::Texture& texture);

  private:
    vkbackend::Backend& m_backend;
    VkDescriptorPool m_pool = VK_NULL_HANDLE;
};

} // namespace ui
