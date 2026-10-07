#include "vulkan_backend/texture.h"

#include <volk.h>

#include <cstring>
#include <stdexcept>

namespace vkbackend {

namespace {
void check(VkResult r, const char* what) {
    if (r != VK_SUCCESS)
        throw std::runtime_error(std::string(what) + " failed");
}
} // namespace

Texture::Texture(Backend& backend, uint32_t width, uint32_t height)
    : m_backend(backend), m_width(width), m_height(height) {
    if (width == 0 || height == 0)
        throw std::runtime_error("Texture size must be non-zero");
    VkDevice dev = backend.device();
    const VkDeviceSize bytes = static_cast<VkDeviceSize>(width) * height * 4;

    // Staging buffer (host visible, persistently mapped).
    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bi.size = bytes;
    bi.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    check(vkCreateBuffer(dev, &bi, nullptr, &m_staging), "vkCreateBuffer");
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(dev, m_staging, &req);
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = req.size;
    ai.memoryTypeIndex =
        backend.findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    check(vkAllocateMemory(dev, &ai, nullptr, &m_stagingMemory), "vkAllocateMemory");
    vkBindBufferMemory(dev, m_staging, m_stagingMemory, 0);
    check(vkMapMemory(dev, m_stagingMemory, 0, bytes, 0, &m_mapped), "vkMapMemory");
    std::memset(m_mapped, 0, static_cast<size_t>(bytes));

    // Device-local image.
    VkImageCreateInfo ii{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.format = VK_FORMAT_R8G8B8A8_UNORM;
    ii.extent = {width, height, 1};
    ii.mipLevels = 1;
    ii.arrayLayers = 1;
    ii.samples = VK_SAMPLE_COUNT_1_BIT;
    ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    ii.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    check(vkCreateImage(dev, &ii, nullptr, &m_image), "vkCreateImage");
    vkGetImageMemoryRequirements(dev, m_image, &req);
    ai.allocationSize = req.size;
    ai.memoryTypeIndex =
        backend.findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    check(vkAllocateMemory(dev, &ai, nullptr, &m_imageMemory), "vkAllocateMemory");
    vkBindImageMemory(dev, m_image, m_imageMemory, 0);

    VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vi.image = m_image;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = ii.format;
    vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    check(vkCreateImageView(dev, &vi, nullptr, &m_view), "vkCreateImageView");

    VkSamplerCreateInfo si{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    si.magFilter = VK_FILTER_LINEAR;
    si.minFilter = VK_FILTER_LINEAR;
    si.addressModeU = si.addressModeV = si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    check(vkCreateSampler(dev, &si, nullptr, &m_sampler), "vkCreateSampler");

    m_dirty = true; // upload the (black) initial contents so the layout is valid for sampling
}

Texture::~Texture() {
    VkDevice dev = m_backend.device();
    m_backend.waitIdle();
    vkDestroySampler(dev, m_sampler, nullptr);
    vkDestroyImageView(dev, m_view, nullptr);
    vkDestroyImage(dev, m_image, nullptr);
    vkFreeMemory(dev, m_imageMemory, nullptr);
    vkUnmapMemory(dev, m_stagingMemory);
    vkDestroyBuffer(dev, m_staging, nullptr);
    vkFreeMemory(dev, m_stagingMemory, nullptr);
}

void Texture::setPixels(const uint8_t* rgba) {
    std::memcpy(m_mapped, rgba, static_cast<size_t>(m_width) * m_height * 4);
    m_dirty = true;
}

void Texture::recordUpload(VkCommandBuffer cmd) {
    if (!m_dirty)
        return;
    m_dirty = false;

    // The whole image is overwritten, so the previous contents can be discarded (UNDEFINED).
    VkImageMemoryBarrier toDst{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    toDst.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    toDst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toDst.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    toDst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toDst.srcQueueFamilyIndex = toDst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toDst.image = m_image;
    toDst.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toDst);

    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageExtent = {m_width, m_height, 1};
    vkCmdCopyBufferToImage(cmd, m_staging, m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                           &region);

    VkImageMemoryBarrier toRead = toDst;
    toRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    toRead.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toRead.newLayout = kShaderLayout;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &toRead);
}

} // namespace vkbackend
