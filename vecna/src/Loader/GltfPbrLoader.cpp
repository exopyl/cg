#include "Vecna/Loader/GltfPbrLoader.hpp"
#include "cgre2/DescriptorPool.hpp"
#include "cgre2/Texture.hpp"
#include "cgre2/UniformBuffer.hpp"
#include "cgre2/UniformLayouts.hpp"
#include "cgre2/Vertex.hpp"
#include "cgre2/DeviceContext.hpp"
#include "cgre2/Logger.hpp"

// Each of cgmesh's vmeshes.cpp, cgre2's TextureManager.cpp, and this file
// compiles a private copy of stb_image (STB_IMAGE_STATIC = symbols are
// internal). No link-level conflict; the extra binary cost is small.
//
// vecna is built with /WX (warnings as errors); stb_image emits unused-
// static-function warnings (C4505) for the half-dozen stbi_* helpers we
// don't reference. Silence them around the include — they're benign and
// fixing stb_image upstream is out of scope.
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4505)
#endif
#include <stb/stb_image.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

// Same TINYGLTF_NO_* macros vmeshes.cpp uses, so the inlined ctors of
// TinyGLTF don't reference the symbols we suppressed there. Bringing
// nlohmann/json explicitly because TINYGLTF_NO_INCLUDE_JSON tells
// tiny_gltf not to.
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_INCLUDE_JSON
#include <nlohmann/json.hpp>
#include <tinygltf/tiny_gltf.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace Vecna::Loader {

namespace {

// ------------------ Accessor helpers --------------------------------

const uint8_t* accessorBytes(const tinygltf::Model& model,
                             const tinygltf::Accessor& accessor,
                             size_t* outStride)
{
    if (accessor.bufferView < 0) return nullptr;
    const auto& view   = model.bufferViews[accessor.bufferView];
    const auto& buffer = model.buffers[view.buffer];
    const size_t componentSize  = tinygltf::GetComponentSizeInBytes(accessor.componentType);
    const size_t componentCount = tinygltf::GetNumComponentsInType(accessor.type);
    const size_t implicitStride = componentSize * componentCount;
    if (outStride) {
        *outStride = view.byteStride ? view.byteStride : implicitStride;
    }
    return buffer.data.data() + view.byteOffset + accessor.byteOffset;
}

bool readVec3Floats(const tinygltf::Model& model, int accIdx,
                    std::vector<float>& out)
{
    if (accIdx < 0) return false;
    const auto& acc = model.accessors[accIdx];
    if (acc.type != TINYGLTF_TYPE_VEC3 || acc.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) return false;

    size_t stride = 0;
    const uint8_t* base = accessorBytes(model, acc, &stride);
    if (!base) return false;

    out.resize(acc.count * 3u);
    for (size_t i = 0; i < acc.count; ++i) {
        const float* src = reinterpret_cast<const float*>(base + i * stride);
        out[3 * i + 0] = src[0];
        out[3 * i + 1] = src[1];
        out[3 * i + 2] = src[2];
    }
    return true;
}

bool readVec2Floats(const tinygltf::Model& model, int accIdx,
                    std::vector<float>& out)
{
    if (accIdx < 0) return false;
    const auto& acc = model.accessors[accIdx];
    if (acc.type != TINYGLTF_TYPE_VEC2 || acc.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) return false;

    size_t stride = 0;
    const uint8_t* base = accessorBytes(model, acc, &stride);
    if (!base) return false;

    out.resize(acc.count * 2u);
    for (size_t i = 0; i < acc.count; ++i) {
        const float* src = reinterpret_cast<const float*>(base + i * stride);
        out[2 * i + 0] = src[0];
        out[2 * i + 1] = src[1];
    }
    return true;
}

bool readVec4Floats(const tinygltf::Model& model, int accIdx,
                    std::vector<float>& out)
{
    if (accIdx < 0) return false;
    const auto& acc = model.accessors[accIdx];
    if (acc.type != TINYGLTF_TYPE_VEC4 || acc.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) return false;

    size_t stride = 0;
    const uint8_t* base = accessorBytes(model, acc, &stride);
    if (!base) return false;

    out.resize(acc.count * 4u);
    for (size_t i = 0; i < acc.count; ++i) {
        const float* src = reinterpret_cast<const float*>(base + i * stride);
        out[4 * i + 0] = src[0];
        out[4 * i + 1] = src[1];
        out[4 * i + 2] = src[2];
        out[4 * i + 3] = src[3];
    }
    return true;
}

bool readIndices(const tinygltf::Model& model, int accIdx,
                 std::vector<uint32_t>& out)
{
    if (accIdx < 0) return false;
    const auto& acc = model.accessors[accIdx];
    if (acc.type != TINYGLTF_TYPE_SCALAR) return false;

    size_t stride = 0;
    const uint8_t* base = accessorBytes(model, acc, &stride);
    if (!base) return false;

    out.resize(acc.count);
    for (size_t i = 0; i < acc.count; ++i) {
        const uint8_t* src = base + i * stride;
        switch (acc.componentType) {
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                out[i] = *src; break;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                out[i] = *reinterpret_cast<const uint16_t*>(src); break;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                out[i] = *reinterpret_cast<const uint32_t*>(src); break;
            default:
                return false;
        }
    }
    return true;
}

// ------------------ tinygltf image loader callback ------------------

// Replaces the noopLoadImage we use elsewhere — here we DO want the
// images decoded so we can upload them to the GPU. tinygltf calls this
// for each image entry while parsing; image->image is filled with raw
// RGBA8 bytes.
bool decodeGltfImage(tinygltf::Image* image, const int /*idx*/,
                     std::string* err, std::string* /*warn*/,
                     int /*req_w*/, int /*req_h*/,
                     const unsigned char* bytes, int size,
                     void* /*user_data*/)
{
    int w = 0, h = 0, channels = 0;
    stbi_uc* pixels = stbi_load_from_memory(bytes, size, &w, &h, &channels, STBI_rgb_alpha);
    if (!pixels) {
        if (err) *err += std::string("stbi_load_from_memory: ") + stbi_failure_reason() + "\n";
        return false;
    }
    image->width     = w;
    image->height    = h;
    image->component = 4;
    image->bits      = 8;
    image->pixel_type = TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
    image->image.assign(pixels, pixels + static_cast<size_t>(w) * h * 4u);
    stbi_image_free(pixels);
    return true;
}

// ------------------ Texture upload ----------------------------------

// Picks the right Vulkan format depending on whether the texture is used
// as albedo/emissive (sRGB, gamma applied at sample) or as data
// (normal/MR/AO, linear).
cgre2::Texture* uploadTexture(cgre2::TextureManager& tm,
                              const tinygltf::Model& model,
                              int textureIndex,
                              VkFormat format,
                              const std::string& sourceLabel)
{
    if (textureIndex < 0) return nullptr;
    const auto& tex = model.textures[textureIndex];
    if (tex.source < 0) return nullptr;
    const auto& img = model.images[tex.source];
    if (img.image.empty() || img.width <= 0 || img.height <= 0) {
        cgre2::Logger::warn("Loader",
            "GLB image #" + std::to_string(tex.source) + " (" + sourceLabel + ") is empty");
        return nullptr;
    }
    const std::string key = sourceLabel + "#" + std::to_string(tex.source);
    return &tm.loadFromBytes(key, img.image.data(),
                             static_cast<uint32_t>(img.width),
                             static_cast<uint32_t>(img.height),
                             format);
}

// ------------------ Material build ----------------------------------

void buildMaterial(const tinygltf::Model&   model,
                   const tinygltf::Material& srcMat,
                   cgre2::DeviceContext&     device,
                   cgre2::TextureManager&   textureManager,
                   cgre2::DescriptorPool&   descriptorPool,
                   VkDescriptorSetLayout    layout,
                   PBRMaterial&             out)
{
    cgre2::MaterialUBO ubo = cgre2::makeDefaultMaterial();

    // pbrMetallicRoughness factors
    const auto& pbr = srcMat.pbrMetallicRoughness;
    if (pbr.baseColorFactor.size() >= 4) {
        ubo.baseColorFactor[0] = static_cast<float>(pbr.baseColorFactor[0]);
        ubo.baseColorFactor[1] = static_cast<float>(pbr.baseColorFactor[1]);
        ubo.baseColorFactor[2] = static_cast<float>(pbr.baseColorFactor[2]);
        ubo.baseColorFactor[3] = static_cast<float>(pbr.baseColorFactor[3]);
    }
    ubo.metallicFactor  = static_cast<float>(pbr.metallicFactor);
    ubo.roughnessFactor = static_cast<float>(pbr.roughnessFactor);

    if (srcMat.emissiveFactor.size() >= 3) {
        ubo.emissiveFactor[0] = static_cast<float>(srcMat.emissiveFactor[0]);
        ubo.emissiveFactor[1] = static_cast<float>(srcMat.emissiveFactor[1]);
        ubo.emissiveFactor[2] = static_cast<float>(srcMat.emissiveFactor[2]);
    }
    ubo.normalScale       = static_cast<float>(srcMat.normalTexture.scale);
    ubo.occlusionStrength = static_cast<float>(srcMat.occlusionTexture.strength);
    ubo.alphaCutoff       = static_cast<float>(srcMat.alphaCutoff);

    // glTF alphaMode: OPAQUE (default, nothing to do) / MASK (discard at
    // alphaCutoff — flag → shader) / BLEND (needs alpha blending in the
    // pipeline state, which we don't enable yet → warn and render as
    // opaque so the asset still appears).
    if (srcMat.alphaMode == "MASK") {
        ubo.flags |= cgre2::MAT_ALPHA_MASK;
    } else if (srcMat.alphaMode == "BLEND") {
        cgre2::Logger::warn("Loader",
            "Material '" + srcMat.name + "' uses alphaMode=BLEND; the PBR "
            "pipeline doesn't enable alpha blending yet — rendered opaque.");
    }

    // Textures (sRGB for color-bearing maps, UNORM for data maps).
    out.albedo            = uploadTexture(textureManager, model, pbr.baseColorTexture.index,         VK_FORMAT_R8G8B8A8_SRGB,  "albedo");
    out.metallicRoughness = uploadTexture(textureManager, model, pbr.metallicRoughnessTexture.index, VK_FORMAT_R8G8B8A8_UNORM, "mr");
    out.normal            = uploadTexture(textureManager, model, srcMat.normalTexture.index,         VK_FORMAT_R8G8B8A8_UNORM, "normal");
    out.occlusion         = uploadTexture(textureManager, model, srcMat.occlusionTexture.index,      VK_FORMAT_R8G8B8A8_UNORM, "ao");
    out.emissive          = uploadTexture(textureManager, model, srcMat.emissiveTexture.index,       VK_FORMAT_R8G8B8A8_SRGB,  "emissive");

    if (out.albedo)            ubo.flags |= cgre2::MAT_HAS_ALBEDO_MAP;
    if (out.normal)            ubo.flags |= cgre2::MAT_HAS_NORMAL_MAP;
    if (out.metallicRoughness) ubo.flags |= cgre2::MAT_HAS_MR_MAP;
    if (out.occlusion)         ubo.flags |= cgre2::MAT_HAS_AO_MAP;
    if (out.emissive)          ubo.flags |= cgre2::MAT_HAS_EMISSIVE_MAP;

    // Per-texture UV set selector. glTF stores the chosen TEXCOORD on the
    // *TextureInfo node (texCoord = 0 or 1). We only support sets 0 and 1
    // (which is also what the glTF 2.0 core spec defines); anything else
    // would need a vertex-layout extension.
    auto setUv1 = [&](int tc, uint32_t bit, const char* mapName) {
        if (tc == 1) {
            ubo.flags |= bit;
        } else if (tc > 1) {
            cgre2::Logger::warn("Loader",
                std::string("Material '") + srcMat.name + "' " + mapName +
                " uses TEXCOORD_" + std::to_string(tc) +
                " — only sets 0 and 1 are supported; falling back to 0.");
        }
    };
    if (out.albedo)            setUv1(pbr.baseColorTexture.texCoord,         cgre2::MAT_UV1_ALBEDO,   "baseColor");
    if (out.metallicRoughness) setUv1(pbr.metallicRoughnessTexture.texCoord, cgre2::MAT_UV1_MR,       "metallicRoughness");
    if (out.normal)            setUv1(srcMat.normalTexture.texCoord,         cgre2::MAT_UV1_NORMAL,   "normal");
    if (out.occlusion)         setUv1(srcMat.occlusionTexture.texCoord,      cgre2::MAT_UV1_AO,       "occlusion");
    if (out.emissive)          setUv1(srcMat.emissiveTexture.texCoord,       cgre2::MAT_UV1_EMISSIVE, "emissive");

    // UBO upload
    out.ubo = std::make_unique<cgre2::UniformBuffer>(device, sizeof(cgre2::MaterialUBO));
    out.ubo->update(&ubo, sizeof(ubo));

    // Descriptor set (binding 0 = UBO; bindings 1..5 = samplers).
    out.descriptorSet = descriptorPool.allocate(layout);

    VkDescriptorBufferInfo bufInfo = out.ubo->descriptorInfo();
    const std::array<VkDescriptorImageInfo, 5> samplers = {
        (out.albedo            ? out.albedo            : &textureManager.getWhitePixel() )->descriptorInfo(),
        (out.normal            ? out.normal            : &textureManager.getNormalPixel())->descriptorInfo(),
        (out.metallicRoughness ? out.metallicRoughness : &textureManager.getWhitePixel() )->descriptorInfo(),
        (out.occlusion         ? out.occlusion         : &textureManager.getWhitePixel() )->descriptorInfo(),
        (out.emissive          ? out.emissive          : &textureManager.getBlackPixel() )->descriptorInfo(),
    };

    std::array<VkWriteDescriptorSet, 6> writes{};
    writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet          = out.descriptorSet;
    writes[0].dstBinding      = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].pBufferInfo     = &bufInfo;
    for (uint32_t i = 0; i < 5; ++i) {
        writes[1 + i].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1 + i].dstSet          = out.descriptorSet;
        writes[1 + i].dstBinding      = 1 + i;
        writes[1 + i].descriptorCount = 1;
        writes[1 + i].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[1 + i].pImageInfo      = &samplers[i];
    }
    vkUpdateDescriptorSets(device.getDevice(),
                           static_cast<uint32_t>(writes.size()), writes.data(),
                           0, nullptr);
}

// ------------------ Primitive assembly ------------------------------

bool assemblePrimitive(const tinygltf::Model&     model,
                       const tinygltf::Primitive& prim,
                       cgre2::DeviceContext&       device,
                       uint32_t                   defaultMaterialIndex,
                       PBRScene&                  scene,
                       float                      bboxMinOut[3],
                       float                      bboxMaxOut[3])
{
    if (prim.mode != TINYGLTF_MODE_TRIANGLES) {
        cgre2::Logger::warn("Loader", "GLB: skipping non-TRIANGLES primitive");
        return false;
    }

    const auto posIt = prim.attributes.find("POSITION");
    if (posIt == prim.attributes.end()) return false;

    std::vector<float>    positions, normals, uvs, uvs1, tangents, colors;
    std::vector<uint32_t> indices;

    if (!readVec3Floats(model, posIt->second, positions)) return false;
    const size_t nVerts = positions.size() / 3;

    auto findIt = prim.attributes.find("NORMAL");
    if (findIt != prim.attributes.end()) readVec3Floats(model, findIt->second, normals);

    findIt = prim.attributes.find("TEXCOORD_0");
    if (findIt != prim.attributes.end()) readVec2Floats(model, findIt->second, uvs);

    findIt = prim.attributes.find("TEXCOORD_1");
    if (findIt != prim.attributes.end()) readVec2Floats(model, findIt->second, uvs1);

    findIt = prim.attributes.find("TANGENT");
    if (findIt != prim.attributes.end()) readVec4Floats(model, findIt->second, tangents);

    findIt = prim.attributes.find("COLOR_0");
    if (findIt != prim.attributes.end()) readVec3Floats(model, findIt->second, colors);

    // Indices (or sequential).
    if (prim.indices >= 0) {
        if (!readIndices(model, prim.indices, indices)) return false;
    } else {
        indices.resize(nVerts);
        for (size_t i = 0; i < nVerts; ++i) indices[i] = static_cast<uint32_t>(i);
    }
    if (indices.empty() || indices.size() % 3 != 0) return false;

    // VertexPBR assembly + bbox accumulation.
    std::vector<cgre2::VertexPBR> verts(nVerts);
    for (size_t i = 0; i < nVerts; ++i) {
        auto& v = verts[i];

        v.position[0] = positions[3 * i + 0];
        v.position[1] = positions[3 * i + 1];
        v.position[2] = positions[3 * i + 2];

        for (int k = 0; k < 3; ++k) {
            if (v.position[k] < bboxMinOut[k]) bboxMinOut[k] = v.position[k];
            if (v.position[k] > bboxMaxOut[k]) bboxMaxOut[k] = v.position[k];
        }

        if (!normals.empty()) {
            v.normal[0] = normals[3 * i + 0];
            v.normal[1] = normals[3 * i + 1];
            v.normal[2] = normals[3 * i + 2];
        } else {
            v.normal[0] = 0.0f; v.normal[1] = 0.0f; v.normal[2] = 1.0f;
        }

        if (!colors.empty()) {
            v.color[0] = colors[3 * i + 0];
            v.color[1] = colors[3 * i + 1];
            v.color[2] = colors[3 * i + 2];
        } else {
            v.color[0] = 1.0f; v.color[1] = 1.0f; v.color[2] = 1.0f;
        }

        if (!uvs.empty()) {
            v.uv[0] = uvs[2 * i + 0];
            v.uv[1] = uvs[2 * i + 1];
        } else {
            v.uv[0] = 0.0f; v.uv[1] = 0.0f;
        }

        if (!uvs1.empty()) {
            v.uv1[0] = uvs1[2 * i + 0];
            v.uv1[1] = uvs1[2 * i + 1];
        } else {
            v.uv1[0] = 0.0f; v.uv1[1] = 0.0f;
        }

        if (!tangents.empty()) {
            v.tangent[0] = tangents[4 * i + 0];
            v.tangent[1] = tangents[4 * i + 1];
            v.tangent[2] = tangents[4 * i + 2];
            v.tangent[3] = tangents[4 * i + 3];
        } else {
            // Will be replaced by MikkTSpace in M5 when no tangents but
            // a normal map is present. For now: identity-along-X with
            // positive handedness — fine when no normal map binds.
            v.tangent[0] = 1.0f; v.tangent[1] = 0.0f; v.tangent[2] = 0.0f; v.tangent[3] = 1.0f;
        }
    }

    PBRMesh m;
    m.vertexBuffer = std::make_unique<cgre2::VertexBuffer>(
        device,
        verts.data(),
        static_cast<VkDeviceSize>(verts.size() * sizeof(cgre2::VertexPBR)),
        static_cast<uint32_t>(verts.size()));
    m.indexBuffer  = std::make_unique<cgre2::IndexBuffer>(device, indices);
    m.materialIndex = (prim.material >= 0)
        ? static_cast<uint32_t>(prim.material)
        : defaultMaterialIndex;
    scene.meshes.push_back(std::move(m));
    return true;
}

} // anonymous namespace

bool loadGltfAsPBR(const std::filesystem::path& path,
                   cgre2::DeviceContext&         device,
                   cgre2::TextureManager&       textureManager,
                   cgre2::DescriptorPool&       descriptorPool,
                   VkDescriptorSetLayout        materialSetLayout,
                   PBRScene&                    outScene)
{
    tinygltf::Model    model;
    tinygltf::TinyGLTF loader;
    loader.SetImageLoader(decodeGltfImage, nullptr);

    std::string err, warn;
    const std::string filenameStr = path.string();
    const bool isBinary = path.extension() == ".glb";
    const bool ok = isBinary
        ? loader.LoadBinaryFromFile(&model, &err, &warn, filenameStr)
        : loader.LoadASCIIFromFile (&model, &err, &warn, filenameStr);
    if (!warn.empty()) cgre2::Logger::warn("Loader", "glTF: " + warn);
    if (!ok) {
        cgre2::Logger::error("Loader", "glTF parse failed: " + err);
        return false;
    }

    // Build materials first so primitives can reference them by index.
    // We always append a default-material at the end so primitives with
    // material == -1 still get one.
    outScene.materials.reserve(model.materials.size() + 1);
    for (const auto& srcMat : model.materials) {
        PBRMaterial mat;
        buildMaterial(model, srcMat, device, textureManager, descriptorPool, materialSetLayout, mat);
        outScene.materials.push_back(std::move(mat));
    }
    // Default material for primitives without one. Pure data, no textures.
    {
        PBRMaterial def;
        cgre2::MaterialUBO ubo = cgre2::makeDefaultMaterial();
        def.ubo = std::make_unique<cgre2::UniformBuffer>(device, sizeof(cgre2::MaterialUBO));
        def.ubo->update(&ubo, sizeof(ubo));
        def.descriptorSet = descriptorPool.allocate(materialSetLayout);

        VkDescriptorBufferInfo bufInfo = def.ubo->descriptorInfo();
        const std::array<VkDescriptorImageInfo, 5> samplers = {
            textureManager.getWhitePixel().descriptorInfo(),
            textureManager.getNormalPixel().descriptorInfo(),
            textureManager.getWhitePixel().descriptorInfo(),
            textureManager.getWhitePixel().descriptorInfo(),
            textureManager.getBlackPixel().descriptorInfo(),
        };
        std::array<VkWriteDescriptorSet, 6> writes{};
        writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet          = def.descriptorSet;
        writes[0].dstBinding      = 0;
        writes[0].descriptorCount = 1;
        writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[0].pBufferInfo     = &bufInfo;
        for (uint32_t i = 0; i < 5; ++i) {
            writes[1 + i].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[1 + i].dstSet          = def.descriptorSet;
            writes[1 + i].dstBinding      = 1 + i;
            writes[1 + i].descriptorCount = 1;
            writes[1 + i].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[1 + i].pImageInfo      = &samplers[i];
        }
        vkUpdateDescriptorSets(device.getDevice(),
                               static_cast<uint32_t>(writes.size()), writes.data(),
                               0, nullptr);

        outScene.materials.push_back(std::move(def));
    }
    const uint32_t defaultMaterialIndex =
        static_cast<uint32_t>(outScene.materials.size() - 1);

    // Build meshes.
    for (const auto& gltfMesh : model.meshes) {
        for (const auto& prim : gltfMesh.primitives) {
            assemblePrimitive(model, prim, device, defaultMaterialIndex,
                              outScene, outScene.bboxMin, outScene.bboxMax);
        }
    }

    if (outScene.meshes.empty()) {
        cgre2::Logger::warn("Loader", "GLB: no triangle primitives extracted from " + filenameStr);
        return false;
    }

    cgre2::Logger::info("Loader",
        "GLB PBR: " + std::to_string(outScene.meshes.size()) + " primitives, " +
        std::to_string(outScene.materials.size() - 1) + " materials (+1 default)");

    return true;
}

} // namespace Vecna::Loader
