#pragma once

// The unique_ptr<cgre2::X> members in PBRMaterial / PBRMesh below require
// complete types at the point of (implicit) destructor instantiation in
// any TU that includes this header — forward decls would force callers to
// also include these themselves. Easier to pull them in here.
#include "cgre2/Buffer.hpp"
#include "cgre2/UniformBuffer.hpp"

#include <vulkan/vulkan.h>

#include <cfloat>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <vector>

namespace cgre2 {
class DeviceContext;
class TextureManager;
class DescriptorPool;
class Texture;
} // namespace cgre2

namespace Vecna::Loader {

/// PBR material aggregating the descriptor set + uniform buffer + the
/// (non-owning) texture pointers populated from a glTF material block.
/// Texture pointers are owned by the caller's `cgre2::TextureManager`;
/// a null pointer means "use the fallback (white/normal/black) pixel
/// from TextureManager".
struct PBRMaterial {
    std::unique_ptr<cgre2::UniformBuffer> ubo;
    VkDescriptorSet                       descriptorSet     = VK_NULL_HANDLE;
    cgre2::Texture*                       albedo            = nullptr;
    cgre2::Texture*                       metallicRoughness = nullptr;
    cgre2::Texture*                       normal            = nullptr;
    cgre2::Texture*                       occlusion         = nullptr;
    cgre2::Texture*                       emissive          = nullptr;
};

/// One drawable primitive: per-primitive vertex/index buffers + an index
/// into the parent PBRScene's `materials` vector.
struct PBRMesh {
    std::unique_ptr<cgre2::VertexBuffer> vertexBuffer;
    std::unique_ptr<cgre2::IndexBuffer>  indexBuffer;
    uint32_t                             materialIndex = 0;
};

/// Output of `loadGltfAsPBR`. Default-constructible and assignable so
/// callers can `scene = {}` to release everything before reloading.
struct PBRScene {
    std::vector<PBRMaterial> materials;
    std::vector<PBRMesh>     meshes;
    float bboxMin[3] = { +FLT_MAX, +FLT_MAX, +FLT_MAX };
    float bboxMax[3] = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
};

/// Parse a .glb / .gltf file at `path` and populate `outScene`. Materials
/// are built first (so primitives can reference them by index), then
/// every TRIANGLES primitive is uploaded as a `PBRMesh`. A final default
/// material is appended at index `materials.size() - 1` for primitives
/// whose `material` field is -1.
///
/// Returns false on parse error or if no triangle primitive was found.
/// On false, `outScene` may be partially populated — the caller should
/// `outScene = {}` before retrying with another path.
bool loadGltfAsPBR(const std::filesystem::path& path,
                   cgre2::DeviceContext&        device,
                   cgre2::TextureManager&       textureManager,
                   cgre2::DescriptorPool&       descriptorPool,
                   VkDescriptorSetLayout        materialSetLayout,
                   PBRScene&                    outScene);

} // namespace Vecna::Loader
