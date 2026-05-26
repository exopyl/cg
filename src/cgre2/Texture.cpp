#include "cgre2/Texture.hpp"
#include "cgre2/DeviceContext.hpp"
#include "cgre2/Logger.hpp"

#include <vk_mem_alloc.h>

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace cgre2 {

namespace {

uint32_t computeMipLevels(uint32_t w, uint32_t h)
{
    return 1u + static_cast<uint32_t>(std::floor(std::log2(std::max(w, h))));
}

// Single-shot command buffer recorded onto the graphics queue. The
// transferCommandPool exposed by DeviceContext is allocated on the
// graphics queue family, so blit/copy/barriers run there.
VkCommandBuffer beginOneShot(DeviceContext& device)
{
    VkCommandBufferAllocateInfo info{};
    info.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    info.commandPool        = device.getTransferCommandPool();
    info.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(device.getDevice(), &info, &cmd) != VK_SUCCESS) {
        throw std::runtime_error("Texture: failed to allocate one-shot command buffer");
    }

    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin);
    return cmd;
}

void endOneShot(DeviceContext& device, VkCommandBuffer cmd)
{
    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit{};
    submit.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers    = &cmd;

    // Wait via a transient fence so we can free the cmd buffer right
    // after the queue has consumed it. Simpler than queueWaitIdle when
    // multiple textures are uploaded close in time.
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence = VK_NULL_HANDLE;
    vkCreateFence(device.getDevice(), &fenceInfo, nullptr, &fence);

    vkQueueSubmit(device.getGraphicsQueue(), 1, &submit, fence);
    vkWaitForFences(device.getDevice(), 1, &fence, VK_TRUE, UINT64_MAX);

    vkDestroyFence(device.getDevice(), fence, nullptr);
    vkFreeCommandBuffers(device.getDevice(), device.getTransferCommandPool(), 1, &cmd);
}

void recordTransitionBarrier(VkCommandBuffer cmd, VkImage image,
                             VkImageLayout oldLayout, VkImageLayout newLayout,
                             VkAccessFlags srcAccess, VkAccessFlags dstAccess,
                             VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage,
                             uint32_t baseMip, uint32_t levelCount)
{
    VkImageMemoryBarrier b{};
    b.sType                       = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    b.oldLayout                   = oldLayout;
    b.newLayout                   = newLayout;
    b.srcQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
    b.image                       = image;
    b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    b.subresourceRange.baseMipLevel   = baseMip;
    b.subresourceRange.levelCount     = levelCount;
    b.subresourceRange.baseArrayLayer = 0;
    b.subresourceRange.layerCount     = 1;
    b.srcAccessMask = srcAccess;
    b.dstAccessMask = dstAccess;
    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &b);
}

} // namespace

VkDescriptorImageInfo Texture::descriptorInfo() const
{
    VkDescriptorImageInfo info{};
    info.sampler     = m_sampler;
    info.imageView   = m_imageView;
    info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    return info;
}

Texture::Texture(DeviceContext& device, const TextureInfo& info)
    : m_device(device), m_format(info.format), m_extent{info.width, info.height}
{
    if (info.width == 0 || info.height == 0) {
        throw std::runtime_error("Texture: invalid extent (width or height is zero)");
    }

    m_mipLevels = info.generateMips ? computeMipLevels(info.width, info.height) : 1u;

    createImage(info);
    if (info.pixels) {
        const VkDeviceSize bytes =
            static_cast<VkDeviceSize>(info.width) * info.height * 4u;
        uploadPixels(info.pixels, bytes);
        if (m_mipLevels > 1) {
            generateMipmaps();
        } else {
            // Single-mip path: still needs the final SHADER_READ transition.
            VkCommandBuffer cmd = beginOneShot(m_device);
            recordTransitionBarrier(cmd, m_image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0, m_mipLevels);
            endOneShot(m_device, cmd);
        }
    } else {
        transitionToShaderRead();
    }

    createView();
    createSampler(info);
}

Texture::~Texture()
{
    if (m_sampler   != VK_NULL_HANDLE) vkDestroySampler(m_device.getDevice(),   m_sampler,   nullptr);
    if (m_imageView != VK_NULL_HANDLE) vkDestroyImageView(m_device.getDevice(), m_imageView, nullptr);
    if (m_image     != VK_NULL_HANDLE) vmaDestroyImage(m_device.getAllocator(), m_image, m_allocation);
}

void Texture::createImage(const TextureInfo& info)
{
    VkImageCreateInfo ic{};
    ic.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ic.imageType     = VK_IMAGE_TYPE_2D;
    ic.format        = info.format;
    ic.extent        = { info.width, info.height, 1 };
    ic.mipLevels     = m_mipLevels;
    ic.arrayLayers   = 1;
    ic.samples       = VK_SAMPLE_COUNT_1_BIT;
    ic.tiling        = VK_IMAGE_TILING_OPTIMAL;
    // SAMPLED + TRANSFER_DST is the minimum. TRANSFER_SRC is required
    // additionally if we run vkCmdBlitImage in generateMipmaps().
    ic.usage         = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (m_mipLevels > 1) ic.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    ic.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    ic.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo ac{};
    ac.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    if (vmaCreateImage(m_device.getAllocator(), &ic, &ac, &m_image, &m_allocation, nullptr) != VK_SUCCESS) {
        throw std::runtime_error("Texture: vmaCreateImage failed");
    }
}

void Texture::uploadPixels(const void* pixels, VkDeviceSize bytes)
{
    // Staging buffer (host-visible, coherent) for the CPU→GPU copy.
    VkBufferCreateInfo bc{};
    bc.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bc.size        = bytes;
    bc.usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo ac{};
    ac.usage = VMA_MEMORY_USAGE_CPU_ONLY;
    ac.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VkBuffer        staging      = VK_NULL_HANDLE;
    VmaAllocation   stagingAlloc = nullptr;
    VmaAllocationInfo stagingInfo{};
    if (vmaCreateBuffer(m_device.getAllocator(), &bc, &ac, &staging, &stagingAlloc, &stagingInfo) != VK_SUCCESS) {
        throw std::runtime_error("Texture: staging buffer allocation failed");
    }
    std::memcpy(stagingInfo.pMappedData, pixels, static_cast<size_t>(bytes));

    VkCommandBuffer cmd = beginOneShot(m_device);

    // All mip levels start in UNDEFINED. Move them to TRANSFER_DST.
    recordTransitionBarrier(cmd, m_image,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        0, VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, m_mipLevels);

    VkBufferImageCopy region{};
    region.bufferOffset                    = 0;
    region.bufferRowLength                 = 0;  // tightly packed
    region.bufferImageHeight               = 0;
    region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel       = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount     = 1;
    region.imageOffset                     = {0, 0, 0};
    region.imageExtent                     = { m_extent.width, m_extent.height, 1 };

    vkCmdCopyBufferToImage(cmd, staging, m_image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    endOneShot(m_device, cmd);
    vmaDestroyBuffer(m_device.getAllocator(), staging, stagingAlloc);
}

void Texture::generateMipmaps()
{
    VkCommandBuffer cmd = beginOneShot(m_device);

    int32_t mipW = static_cast<int32_t>(m_extent.width);
    int32_t mipH = static_cast<int32_t>(m_extent.height);

    for (uint32_t i = 1; i < m_mipLevels; ++i) {
        // Mip (i-1) currently in TRANSFER_DST (from upload or previous
        // iteration). Move it to TRANSFER_SRC so we can read from it.
        recordTransitionBarrier(cmd, m_image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            i - 1, 1);

        VkImageBlit blit{};
        blit.srcOffsets[0] = {0, 0, 0};
        blit.srcOffsets[1] = {mipW, mipH, 1};
        blit.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel       = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount     = 1;
        blit.dstOffsets[0] = {0, 0, 0};
        blit.dstOffsets[1] = { std::max(mipW / 2, 1), std::max(mipH / 2, 1), 1 };
        blit.dstSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel       = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount     = 1;

        vkCmdBlitImage(cmd,
            m_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &blit, VK_FILTER_LINEAR);

        // Mip (i-1) is fully consumed: move it to SHADER_READ.
        recordTransitionBarrier(cmd, m_image,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            i - 1, 1);

        mipW = std::max(mipW / 2, 1);
        mipH = std::max(mipH / 2, 1);
    }

    // The last mip is still in TRANSFER_DST — flip it to SHADER_READ.
    recordTransitionBarrier(cmd, m_image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        m_mipLevels - 1, 1);

    endOneShot(m_device, cmd);
}

void Texture::transitionToShaderRead()
{
    VkCommandBuffer cmd = beginOneShot(m_device);
    recordTransitionBarrier(cmd, m_image,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        0, VK_ACCESS_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, m_mipLevels);
    endOneShot(m_device, cmd);
}

void Texture::createView()
{
    VkImageViewCreateInfo vi{};
    vi.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vi.image    = m_image;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format   = m_format;
    vi.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    vi.subresourceRange.baseMipLevel   = 0;
    vi.subresourceRange.levelCount     = m_mipLevels;
    vi.subresourceRange.baseArrayLayer = 0;
    vi.subresourceRange.layerCount     = 1;

    if (vkCreateImageView(m_device.getDevice(), &vi, nullptr, &m_imageView) != VK_SUCCESS) {
        throw std::runtime_error("Texture: vkCreateImageView failed");
    }
}

void Texture::createSampler(const TextureInfo& info)
{
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(m_device.getPhysicalDevice(), &props);
    VkPhysicalDeviceFeatures feat{};
    vkGetPhysicalDeviceFeatures(m_device.getPhysicalDevice(), &feat);

    VkSamplerCreateInfo sc{};
    sc.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sc.magFilter    = info.magFilter;
    sc.minFilter    = info.minFilter;
    sc.mipmapMode   = info.mipmapMode;
    sc.addressModeU = info.addressMode;
    sc.addressModeV = info.addressMode;
    sc.addressModeW = info.addressMode;
    sc.mipLodBias   = 0.0f;
    sc.compareEnable = VK_FALSE;
    sc.compareOp    = VK_COMPARE_OP_ALWAYS;
    sc.minLod       = 0.0f;
    // LOD range is [0, mipLevels-1] — the smallest mip is index mipLevels-1.
    sc.maxLod       = static_cast<float>(m_mipLevels > 0 ? m_mipLevels - 1u : 0u);
    sc.borderColor  = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sc.unnormalizedCoordinates = VK_FALSE;

    if (info.maxAnisotropy > 1.0f && feat.samplerAnisotropy == VK_TRUE) {
        sc.anisotropyEnable = VK_TRUE;
        sc.maxAnisotropy    = std::min(info.maxAnisotropy, props.limits.maxSamplerAnisotropy);
    } else {
        sc.anisotropyEnable = VK_FALSE;
        sc.maxAnisotropy    = 1.0f;
    }

    if (vkCreateSampler(m_device.getDevice(), &sc, nullptr, &m_sampler) != VK_SUCCESS) {
        throw std::runtime_error("Texture: vkCreateSampler failed");
    }
}

} // namespace cgre2
