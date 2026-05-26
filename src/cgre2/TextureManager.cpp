#include "cgre2/Texture.hpp"
#include "cgre2/DeviceContext.hpp"
#include "cgre2/Logger.hpp"

// cgmesh's vmeshes.cpp also pulls stb_image in, with TINYGLTF_NO_STB_IMAGE
// + its own STB_IMAGE_IMPLEMENTATION. To avoid duplicate-symbol link
// errors when vecna links cgre2.lib + cgmesh.lib, we compile our own
// copy here with internal linkage (STB_IMAGE_STATIC promotes every
// stbi_ function to `static`). The two copies coexist peacefully.
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#include <stb/stb_image.h>

#include <array>
#include <memory>
#include <stdexcept>
#include <utility>

namespace cgre2 {

TextureManager::TextureManager(DeviceContext& device) : m_device(device) {}

Texture& TextureManager::loadFromBytes(const std::string& cacheKey,
                                       const uint8_t*     rgba,
                                       uint32_t           width,
                                       uint32_t           height,
                                       VkFormat           format)
{
    auto it = m_cache.find(cacheKey);
    if (it != m_cache.end()) {
        return *it->second;
    }

    TextureInfo info{};
    info.width        = width;
    info.height       = height;
    info.format       = format;
    info.pixels       = rgba;
    info.generateMips = true;
    info.maxAnisotropy = 8.0f;  // graceful no-op if device doesn't support

    auto tex = std::make_unique<Texture>(m_device, info);
    auto& ref = *tex;
    m_cache.emplace(cacheKey, std::move(tex));
    return ref;
}

Texture& TextureManager::load(const std::filesystem::path& path, VkFormat formatHint)
{
    const std::string key = path.generic_string();
    auto it = m_cache.find(key);
    if (it != m_cache.end()) {
        return *it->second;
    }

    int w = 0, h = 0, channels = 0;
    // Force 4 channels — Vulkan's R8G8B8A8 formats need 4 components, and
    // R8G8B8 sampled images aren't universally supported by GPUs.
    stbi_uc* rawPixels = stbi_load(path.string().c_str(), &w, &h, &channels, STBI_rgb_alpha);
    if (!rawPixels) {
        const std::string err = "TextureManager::load: stbi_load failed for " +
                                path.string() + " (" + stbi_failure_reason() + ")";
        cgre2::Logger::error("Renderer", err);
        throw std::runtime_error(err);
    }
    // RAII guard: stbi_image_free runs even if Texture's ctor throws.
    std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> pixels(rawPixels, &stbi_image_free);

    TextureInfo info{};
    info.width         = static_cast<uint32_t>(w);
    info.height        = static_cast<uint32_t>(h);
    info.format        = formatHint;
    info.pixels        = pixels.get();
    info.generateMips  = true;
    info.maxAnisotropy = 8.0f;

    auto tex = std::make_unique<Texture>(m_device, info);

    auto& ref = *tex;
    m_cache.emplace(key, std::move(tex));
    return ref;
}

Texture& TextureManager::makeSinglePixel(const std::string& key,
                                         uint8_t r, uint8_t g, uint8_t b, VkFormat fmt)
{
    auto it = m_cache.find(key);
    if (it != m_cache.end()) {
        return *it->second;
    }
    const std::array<uint8_t, 4> rgba = { r, g, b, 255u };

    TextureInfo info{};
    info.width        = 1;
    info.height       = 1;
    info.format       = fmt;
    info.pixels       = rgba.data();
    info.generateMips = false;
    info.magFilter    = VK_FILTER_NEAREST;
    info.minFilter    = VK_FILTER_NEAREST;
    info.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    info.addressMode  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    auto tex = std::make_unique<Texture>(m_device, info);
    auto& ref = *tex;
    m_cache.emplace(key, std::move(tex));
    return ref;
}

Texture& TextureManager::getWhitePixel()
{
    // sRGB: a (255,255,255) source maps to a (1.0, 1.0, 1.0) linear sample
    // — neutral multiplier when used as a baseColor fallback.
    return makeSinglePixel("__cgre2_white_srgb", 255, 255, 255,
                           VK_FORMAT_R8G8B8A8_SRGB);
}

Texture& TextureManager::getNormalPixel()
{
    // (128, 128, 255) decoded as a normal map gives (0, 0, 1) in tangent
    // space — a "no perturbation" sample. UNORM so we don't apply gamma.
    return makeSinglePixel("__cgre2_normal_unorm", 128, 128, 255,
                           VK_FORMAT_R8G8B8A8_UNORM);
}

Texture& TextureManager::getBlackPixel()
{
    // Used as emissive fallback. UNORM is fine — emissive is already
    // a linear quantity.
    return makeSinglePixel("__cgre2_black_unorm", 0, 0, 0,
                           VK_FORMAT_R8G8B8A8_UNORM);
}

void TextureManager::clear()
{
    m_cache.clear();
}

} // namespace cgre2
