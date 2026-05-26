#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>

// Forward decl for VMA (full include only in the .cpp)
struct VmaAllocation_T;
typedef VmaAllocation_T* VmaAllocation;

namespace cgre2 {

class DeviceContext;

/// Parameters for an in-memory Texture creation. Pixels must be a packed
/// RGBA8 buffer of `width * height * 4` bytes. The format hint picks
/// between sRGB (for albedo/emissive) and UNORM (for normal/metallic-
/// roughness/AO maps) — Vulkan applies its sRGB→linear conversion in
/// hardware at sample time when the image format ends in _SRGB.
struct TextureInfo {
    uint32_t             width            = 0;
    uint32_t             height           = 0;
    VkFormat             format           = VK_FORMAT_R8G8B8A8_SRGB;
    const void*          pixels           = nullptr;   // packed RGBA8, w*h*4 bytes
    bool                 generateMips     = true;
    VkFilter             magFilter        = VK_FILTER_LINEAR;
    VkFilter             minFilter        = VK_FILTER_LINEAR;
    VkSamplerMipmapMode  mipmapMode       = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    VkSamplerAddressMode addressMode      = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    float                maxAnisotropy    = 0.0f;      // 0 = disable, >1 = enable up to deviceLimits
};

/// RAII wrapper for VkImage + VkImageView + VkSampler + VMA allocation.
/// Owns nothing outside Vulkan resources — the source pixel buffer can be
/// freed immediately after the ctor returns (we stage-upload internally).
class Texture {
public:
    /// Create a 2D texture and upload `info.pixels`. If `info.pixels` is
    /// null, the image is created empty (still in SHADER_READ_ONLY layout
    /// after the ctor) — useful for render targets, attachments, etc.
    Texture(DeviceContext& device, const TextureInfo& info);
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&&) = delete;
    Texture& operator=(Texture&&) = delete;

    [[nodiscard]] VkImage     getImage()     const { return m_image; }
    [[nodiscard]] VkImageView getImageView() const { return m_imageView; }
    [[nodiscard]] VkSampler   getSampler()   const { return m_sampler; }
    [[nodiscard]] VkFormat    getFormat()    const { return m_format; }
    [[nodiscard]] VkExtent2D  getExtent()    const { return m_extent; }
    [[nodiscard]] uint32_t    getMipLevels() const { return m_mipLevels; }

    /// VkDescriptorImageInfo prefilled with our handles and SHADER_READ
    /// layout — direct feed for vkUpdateDescriptorSets.
    [[nodiscard]] VkDescriptorImageInfo descriptorInfo() const;

private:
    void createImage(const TextureInfo& info);
    void uploadPixels(const void* pixels, VkDeviceSize bytes);
    void generateMipmaps();
    void transitionToShaderRead();   // when no pixels were provided
    void createView();
    void createSampler(const TextureInfo& info);

    DeviceContext&  m_device;
    VkImage        m_image      = VK_NULL_HANDLE;
    VmaAllocation  m_allocation = nullptr;
    VkImageView    m_imageView  = VK_NULL_HANDLE;
    VkSampler      m_sampler    = VK_NULL_HANDLE;
    VkFormat       m_format     = VK_FORMAT_UNDEFINED;
    VkExtent2D     m_extent     = {0, 0};
    uint32_t       m_mipLevels  = 1;
};

/// Cache of Texture instances keyed by a caller-chosen string. Two main
/// use cases:
///   - `loadFromBytes(key, rgba, w, h, format)` for textures embedded in
///     containers (GLB, FBX) whose pixel data is already decoded.
///   - `load(path, formatHint)` for on-disk PNG/JPG, internally decoded
///     via a private stb_image copy (link-isolated from cgmesh's copy).
///
/// `getWhitePixel()` / `getNormalPixel()` / `getBlackPixel()` expose 1x1
/// textures used as fallbacks when a material doesn't bind a given map.
/// The PBR shader still indexes 5 samplers; the fallbacks keep the
/// shading code branch-free.
class TextureManager {
public:
    explicit TextureManager(DeviceContext& device);
    ~TextureManager() = default;

    TextureManager(const TextureManager&) = delete;
    TextureManager& operator=(const TextureManager&) = delete;
    TextureManager(TextureManager&&) = delete;
    TextureManager& operator=(TextureManager&&) = delete;

    Texture& loadFromBytes(const std::string& cacheKey,
                           const uint8_t*     rgba,
                           uint32_t           width,
                           uint32_t           height,
                           VkFormat           format = VK_FORMAT_R8G8B8A8_SRGB);

    Texture& load(const std::filesystem::path& path,
                  VkFormat                     formatHint = VK_FORMAT_R8G8B8A8_SRGB);

    Texture& getWhitePixel();
    Texture& getNormalPixel();
    Texture& getBlackPixel();

    /// Drop every cached texture. Safe only when no live pipeline /
    /// descriptor set still references any of their handles.
    void clear();

private:
    Texture& makeSinglePixel(const std::string& key, uint8_t r, uint8_t g, uint8_t b, VkFormat fmt);

    DeviceContext& m_device;
    std::unordered_map<std::string, std::unique_ptr<Texture>> m_cache;
};

} // namespace cgre2
