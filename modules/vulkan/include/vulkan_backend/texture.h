#pragma once

#include "vulkan_backend/backend.h"

namespace vkbackend {

// A sampled RGBA8 image that the CPU can overwrite every frame (e.g. the ray-traced output).
// setPixels() copies into a host-visible staging buffer; recordUpload() must then be called on the
// frame's command buffer *outside* a render pass. The backend runs one frame in flight, so the
// staging buffer is never rewritten while the GPU is still reading it.
class Texture {
  public:
    Texture(Backend& backend, uint32_t width, uint32_t height);
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    uint32_t width() const { return m_width; }
    uint32_t height() const { return m_height; }

    // Expects width*height*4 bytes. Safe to call every frame.
    void setPixels(const uint8_t* rgba);
    // Records the copy + layout transitions if new pixels were set since the last upload.
    void recordUpload(VkCommandBuffer cmd);

    VkImageView view() const { return m_view; }
    VkSampler sampler() const { return m_sampler; }
    static constexpr VkImageLayout kShaderLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  private:
    Backend& m_backend;
    uint32_t m_width, m_height;
    VkImage m_image = VK_NULL_HANDLE;
    VkDeviceMemory m_imageMemory = VK_NULL_HANDLE;
    VkImageView m_view = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;
    VkBuffer m_staging = VK_NULL_HANDLE;
    VkDeviceMemory m_stagingMemory = VK_NULL_HANDLE;
    void* m_mapped = nullptr;
    bool m_dirty = false;
};

} // namespace vkbackend
