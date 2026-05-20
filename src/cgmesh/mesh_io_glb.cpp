// GLB (binary glTF 2.0) importer for Mesh::load. tinygltf itself is
// vendored at <repo>/extern/tinygltf and its TINYGLTF_IMPLEMENTATION
// translation unit lives in vmeshes.cpp (kept there so the existing
// VMeshes path doesn't change). We only consume the public API here.
//
// Scope: we extract the first primitive of the first mesh in TRIANGLES
// mode, lifting positions/normals/(colors)/indices into the cgmesh
// Mesh layout. Materials, textures, animations, skins and multiple
// meshes are intentionally out of scope for this first cut — they map
// to features the rest of cgmesh / vecna don't model yet. Vecna's
// own PBR loader (vecna/src/Loader/GltfPbrLoader.cpp) handles those
// for the renderer side.

#include "mesh.h"

// Must match the defines vmeshes.cpp uses for its own tinygltf include —
// otherwise this TU expects the stb-backed LoadImageData/WriteImageData
// symbols that vmeshes.cpp specifically suppresses, and the link fails.
// We don't care about textures here, so we install a no-op image
// loader on the TinyGLTF instance.
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_INCLUDE_JSON
#include <nlohmann/json.hpp>
#include <tinygltf/tiny_gltf.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

// No-op image loader. tinygltf still parses image entries from the
// GLB even when we don't intend to decode them; without a callback the
// load fails outright. We don't model textures here, so accept whatever
// bytes are in the file and report success with empty pixel data.
bool noopLoadImage(tinygltf::Image* image, const int /*image_idx*/,
                   std::string* /*err*/, std::string* /*warn*/,
                   int /*req_width*/, int /*req_height*/,
                   const unsigned char* /*bytes*/, int /*size*/,
                   void* /*user_data*/)
{
    if (image) {
        image->width = 0;
        image->height = 0;
        image->component = 0;
        image->bits = 8;
        image->pixel_type = TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
        image->image.clear();
    }
    return true;
}

// Pull a typed pointer + element count out of an accessor. Returns
// nullptr if the accessor doesn't reference a buffer view (sparse-only
// accessors aren't supported here).
const void* accessorData(const tinygltf::Model& model,
                         const tinygltf::Accessor& accessor,
                         size_t* outStride)
{
    if (accessor.bufferView < 0) return nullptr;
    const auto& view = model.bufferViews[accessor.bufferView];
    const auto& buffer = model.buffers[view.buffer];
    const uint8_t* base = buffer.data.data() + view.byteOffset + accessor.byteOffset;

    const size_t componentSize = tinygltf::GetComponentSizeInBytes(accessor.componentType);
    const size_t componentCount = tinygltf::GetNumComponentsInType(accessor.type);
    const size_t implicitStride = componentSize * componentCount;
    if (outStride) {
        *outStride = view.byteStride ? view.byteStride : implicitStride;
    }
    return base;
}

bool copyVec3Float(const tinygltf::Model& model,
                   const tinygltf::Accessor& accessor,
                   float* dst, size_t count)
{
    if (accessor.type != TINYGLTF_TYPE_VEC3) return false;
    if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) return false;
    if (accessor.count != count) return false;

    size_t stride = 0;
    const auto* base = static_cast<const uint8_t*>(accessorData(model, accessor, &stride));
    if (!base) return false;

    for (size_t i = 0; i < count; ++i) {
        const float* src = reinterpret_cast<const float*>(base + i * stride);
        dst[3 * i + 0] = src[0];
        dst[3 * i + 1] = src[1];
        dst[3 * i + 2] = src[2];
    }
    return true;
}

bool copyVertexColors(const tinygltf::Model& model,
                      const tinygltf::Accessor& accessor,
                      float* dst, size_t count)
{
    if (accessor.count != count) return false;
    const bool isVec4 = (accessor.type == TINYGLTF_TYPE_VEC4);
    const bool isVec3 = (accessor.type == TINYGLTF_TYPE_VEC3);
    if (!isVec3 && !isVec4) return false;

    size_t stride = 0;
    const auto* base = static_cast<const uint8_t*>(accessorData(model, accessor, &stride));
    if (!base) return false;

    for (size_t i = 0; i < count; ++i) {
        const uint8_t* src = base + i * stride;
        float r = 0.5f, g = 0.5f, b = 0.5f;
        switch (accessor.componentType) {
            case TINYGLTF_COMPONENT_TYPE_FLOAT: {
                const float* f = reinterpret_cast<const float*>(src);
                r = f[0]; g = f[1]; b = f[2];
                break;
            }
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
                r = src[0] / 255.0f;
                g = src[1] / 255.0f;
                b = src[2] / 255.0f;
                break;
            }
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
                const uint16_t* u = reinterpret_cast<const uint16_t*>(src);
                r = u[0] / 65535.0f;
                g = u[1] / 65535.0f;
                b = u[2] / 65535.0f;
                break;
            }
            default:
                return false;
        }
        dst[3 * i + 0] = r;
        dst[3 * i + 1] = g;
        dst[3 * i + 2] = b;
    }
    return true;
}

bool readIndices(const tinygltf::Model& model,
                 const tinygltf::Accessor& accessor,
                 std::vector<uint32_t>& out)
{
    if (accessor.type != TINYGLTF_TYPE_SCALAR) return false;

    size_t stride = 0;
    const auto* base = static_cast<const uint8_t*>(accessorData(model, accessor, &stride));
    if (!base) return false;

    out.resize(accessor.count);
    for (size_t i = 0; i < accessor.count; ++i) {
        const uint8_t* src = base + i * stride;
        switch (accessor.componentType) {
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

} // anonymous namespace

int Mesh::import_glb(const char* filename)
{
    if (!filename) return -1;

    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    loader.SetImageLoader(noopLoadImage, nullptr);
    std::string err, warn;

    const bool ok = loader.LoadBinaryFromFile(&model, &err, &warn, filename);
    if (!warn.empty()) {
        std::fprintf(stderr, "import_glb warning: %s\n", warn.c_str());
    }
    if (!ok) {
        std::fprintf(stderr, "import_glb: failed to parse %s: %s\n",
                     filename, err.c_str());
        return -1;
    }

    if (model.meshes.empty() || model.meshes[0].primitives.empty()) {
        std::fprintf(stderr, "import_glb: %s has no mesh data\n", filename);
        return -1;
    }

    const tinygltf::Mesh& gMesh = model.meshes[0];
    const tinygltf::Primitive& prim = gMesh.primitives[0];

    if (prim.mode != TINYGLTF_MODE_TRIANGLES) {
        std::fprintf(stderr, "import_glb: %s uses non-TRIANGLES primitive (mode=%d)\n",
                     filename, prim.mode);
        return -1;
    }

    const auto posIt = prim.attributes.find("POSITION");
    if (posIt == prim.attributes.end()) {
        std::fprintf(stderr, "import_glb: %s primitive missing POSITION\n", filename);
        return -1;
    }
    const auto& posAcc = model.accessors[posIt->second];
    const size_t nVerts = posAcc.count;

    std::vector<uint32_t> indices;
    if (prim.indices >= 0) {
        const auto& idxAcc = model.accessors[prim.indices];
        if (!readIndices(model, idxAcc, indices)) {
            std::fprintf(stderr, "import_glb: %s has unsupported index format\n", filename);
            return -1;
        }
    } else {
        indices.resize(nVerts);
        for (size_t i = 0; i < nVerts; ++i) indices[i] = static_cast<uint32_t>(i);
    }
    if (indices.size() % 3 != 0) {
        std::fprintf(stderr, "import_glb: %s index count %zu not a multiple of 3\n",
                     filename, indices.size());
        return -1;
    }
    const size_t nFaces = indices.size() / 3;

    Init(static_cast<unsigned int>(nVerts), static_cast<unsigned int>(nFaces));

    if (!copyVec3Float(model, posAcc, m_pVertices.data(), nVerts)) {
        std::fprintf(stderr, "import_glb: %s POSITION accessor format unsupported\n", filename);
        return -1;
    }

    const auto nrmIt = prim.attributes.find("NORMAL");
    if (nrmIt != prim.attributes.end()) {
        const auto& nrmAcc = model.accessors[nrmIt->second];
        if (!copyVec3Float(model, nrmAcc, m_pVertexNormals.data(), nVerts)) {
            std::fprintf(stderr, "import_glb warning: %s NORMAL format unsupported, will recompute\n", filename);
        }
    }

    const auto colIt = prim.attributes.find("COLOR_0");
    if (colIt != prim.attributes.end()) {
        const auto& colAcc = model.accessors[colIt->second];
        if (!copyVertexColors(model, colAcc, m_pVertexColors.data(), nVerts)) {
            std::fprintf(stderr, "import_glb warning: %s COLOR_0 format unsupported, defaulting to gray\n", filename);
        }
    }

    for (size_t f = 0; f < nFaces; ++f) {
        Face* face = m_pFaces[f];
        face->SetVertex(0, indices[3 * f + 0]);
        face->SetVertex(1, indices[3 * f + 1]);
        face->SetVertex(2, indices[3 * f + 2]);
    }

    return 0;
}
