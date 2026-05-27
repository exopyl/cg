#include "CgreQuickItem.h"

#include "MeshModel.h"
#include "QtDeviceAdapter.h"

#include "cgre2/Buffer.hpp"
#include "cgre2/DescriptorLayout.hpp"
#include "cgre2/DescriptorPool.hpp"
#include "cgre2/DeviceContext.hpp"
#include "cgre2/Pipeline.hpp"
#include "cgre2/Shader.hpp"
#include "cgre2/Texture.hpp"
#include "cgre2/Vertex.hpp"

#include "mesh.h"
#include "vmeshes.h"

#include <QDebug>
#include <QMatrix4x4>
#include <QMouseEvent>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QVector3D>
#include <QWheelEvent>

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>

namespace {

constexpr const char *kTexturedVertSpv = QMLVIEWER_SHADER_DIR "/textured.vert.spv";
constexpr const char *kTexturedFragSpv = QMLVIEWER_SHADER_DIR "/textured.frag.spv";

// textured.vert pushes 2 mat4 = 128 bytes via push constants.
struct PushConstants {
    float mvp[16];
    float model[16];
};

void writeMatrix(const QMatrix4x4 &m, float out[16])
{
    // QMatrix4x4::constData() returns 16 floats in COLUMN-major order,
    // which is exactly what GLSL uses for `mat4`. memcpy is safe.
    std::memcpy(out, m.constData(), 16 * sizeof(float));
}

} // namespace

namespace qmlviewer {

CgreQuickItem::CgreQuickItem(QQuickItem *parent)
    : QQuickItem(parent)
{
    setAcceptedMouseButtons(Qt::LeftButton);

    connect(this, &QQuickItem::windowChanged, this, &CgreQuickItem::onWindowChanged);
    if (window())
        onWindowChanged(window());
}

CgreQuickItem::~CgreQuickItem() = default;

// -- mouse / geometry -----------------------------------------------------

void CgreQuickItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    m_trackball.setDimensions(static_cast<int>(newGeometry.width()),
                              static_cast<int>(newGeometry.height()));
}

void CgreQuickItem::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }
    m_trackball.onMousePress(0, true,
                             static_cast<int>(event->position().x()),
                             static_cast<int>(event->position().y()));
    event->accept();
    if (auto *w = window()) w->update();
}

void CgreQuickItem::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }
    m_trackball.onMousePress(0, false,
                             static_cast<int>(event->position().x()),
                             static_cast<int>(event->position().y()));
    event->accept();
}

void CgreQuickItem::mouseMoveEvent(QMouseEvent *event)
{
    m_trackball.onMouseMove(static_cast<int>(event->position().x()),
                            static_cast<int>(event->position().y()));
    event->accept();
    if (auto *w = window()) w->update();
}

void CgreQuickItem::wheelEvent(QWheelEvent *event)
{
    // angleDelta().y() is in 8ths of a degree, conventionally ±120 per
    // notch. Positive = scroll up = zoom in (closer).
    const int delta = event->angleDelta().y();
    if (delta == 0) {
        event->ignore();
        return;
    }
    constexpr float kStep = 1.1f;
    m_zoomFactor = (delta > 0) ? (m_zoomFactor / kStep) : (m_zoomFactor * kStep);
    m_zoomFactor = std::clamp(m_zoomFactor, 0.05f, 100.0f);

    event->accept();
    if (auto *w = window()) w->update();
}

// -- meshModel binding ----------------------------------------------------

void CgreQuickItem::setMeshModel(MeshModel *model)
{
    if (m_meshModel == model)
        return;

    if (m_meshModel)
        disconnect(m_meshModel, nullptr, this, nullptr);

    m_meshModel = model;

    if (m_meshModel) {
        connect(m_meshModel, SIGNAL(meshChanged()),
                this, SLOT(onMeshChanged()));
    }

    emit meshModelChanged();

    if (m_adapter)
        rebuildMeshBuffers();
}

// -- window / scene-graph hooks -------------------------------------------

void CgreQuickItem::onWindowChanged(QQuickWindow *window)
{
    if (!window)
        return;

    connect(window, &QQuickWindow::sceneGraphInitialized,
            this, &CgreQuickItem::onSceneGraphInitialized,
            Qt::DirectConnection);
    connect(window, &QQuickWindow::beforeRenderPassRecording,
            this, &CgreQuickItem::onBeforeRenderPassRecording,
            Qt::DirectConnection);
    connect(window, &QQuickWindow::sceneGraphInvalidated,
            this, &CgreQuickItem::onSceneGraphInvalidated,
            Qt::DirectConnection);
}

void CgreQuickItem::onSceneGraphInitialized()
{
    QQuickWindow *w = window();
    if (!w)
        return;

    try {
        m_adapter = std::make_unique<QtDeviceAdapter>(w);
        qInfo("CgreQuickItem: extracted Vulkan handles from Qt (cgre2::DeviceContext ready)");
    } catch (const std::exception &e) {
        qWarning("CgreQuickItem: failed to build QtDeviceAdapter: %s", e.what());
        return;
    }

    rebuildMeshBuffers();
}

void CgreQuickItem::ensurePipeline()
{
    if (m_pipeline || !m_adapter)
        return;

    QQuickWindow *w = window();
    if (!w)
        return;

    QSGRendererInterface *rif = w->rendererInterface();
    if (!rif)
        return;

    void *rpPtr = rif->getResource(w, QSGRendererInterface::RenderPassResource);
    if (!rpPtr) {
        qWarning("CgreQuickItem: no Vulkan render pass from Qt yet");
        return;
    }
    const VkRenderPass renderPass = *static_cast<VkRenderPass *>(rpPtr);

    cgre2::DeviceContext &dctx = m_adapter->context();

    ensureDeviceResources();

    try {
        m_shaderManager = std::make_unique<cgre2::ShaderManager>(dctx);

        cgre2::ShaderModule &vert = m_shaderManager->load(kTexturedVertSpv, VK_SHADER_STAGE_VERTEX_BIT);
        cgre2::ShaderModule &frag = m_shaderManager->load(kTexturedFragSpv, VK_SHADER_STAGE_FRAGMENT_BIT);

        m_descriptorLayouts = std::make_unique<cgre2::DescriptorLayouts>(
            dctx, std::vector<const cgre2::ShaderModule *>{ &vert, &frag });

        m_vertexLayout = cgre2::VertexPBR::getLayout();

        cgre2::PipelineCreateInfo info{};
        info.renderPass           = renderPass;
        info.vertexShader         = &vert;
        info.fragmentShader       = &frag;
        info.vertexLayout         = &m_vertexLayout;
        info.descriptorSetLayouts = m_descriptorLayouts->getSetLayouts();
        info.pushConstantRanges   = m_descriptorLayouts->getPushConstantRanges();
        info.cullMode             = VK_CULL_MODE_BACK_BIT;
        info.frontFace            = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        info.depthTest            = true;
        info.depthWrite           = true;
        info.depthCompareOp       = VK_COMPARE_OP_LESS;

        m_pipeline = std::make_unique<cgre2::Pipeline>(dctx, info);
        qInfo("CgreQuickItem: cgre2 pipeline built against Qt render pass");
    } catch (const std::exception &e) {
        qWarning("CgreQuickItem: pipeline build failed: %s", e.what());
        m_pipeline.reset();
        m_descriptorLayouts.reset();
        m_shaderManager.reset();
    }
}

void CgreQuickItem::ensureDeviceResources()
{
    if (!m_adapter)
        return;
    // Texture cache + descriptor pool live as long as the device does; the
    // pool grows on demand, so reuse across mesh reloads is fine. Created
    // here (not only in ensurePipeline) so rebuildMeshBuffers can upload
    // material textures before the first frame builds the pipeline.
    cgre2::DeviceContext &dctx = m_adapter->context();
    if (!m_textures)
        m_textures = std::make_unique<cgre2::TextureManager>(dctx);
    if (!m_descriptorPool)
        m_descriptorPool = std::make_unique<cgre2::DescriptorPool>(dctx);
}

void CgreQuickItem::ensureMaterialSets()
{
    if (!m_descriptorPool || !m_textures || !m_descriptorLayouts)
        return;

    const auto &setLayouts = m_descriptorLayouts->getSetLayouts();
    if (setLayouts.empty())
        return;  // shader declares no descriptor set (shouldn't happen now)

    if (m_materialSets.size() == m_vertexBuffers.size())
        return;  // already in sync with the uploaded sub-meshes

    // One set per sub-mesh, binding its albedo texture — or the 1x1 white
    // fallback when the sub-mesh has no texture material (then
    // texture(albedo,uv) == 1 and the color reduces to the vertex color).
    m_materialSets.clear();

    const VkDevice device = m_adapter->context().getDevice();
    cgre2::Texture &white  = m_textures->getWhitePixel();

    m_materialSets.reserve(m_vertexBuffers.size());
    for (std::size_t i = 0; i < m_vertexBuffers.size(); ++i) {
        cgre2::Texture *tex = (i < m_submeshTextures.size() && m_submeshTextures[i])
            ? m_submeshTextures[i] : &white;
        VkDescriptorImageInfo imageInfo = tex->descriptorInfo();

        VkDescriptorSet set = m_descriptorPool->allocate(setLayouts[0]);

        VkWriteDescriptorSet write{};
        write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet          = set;
        write.dstBinding      = 0;
        write.dstArrayElement = 0;
        write.descriptorCount = 1;
        write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.pImageInfo      = &imageInfo;

        vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
        m_materialSets.push_back(set);
    }
}

cgre2::Texture *CgreQuickItem::loadAlbedo(MaterialTexture *mt)
{
    if (!mt || !m_textures)
        return nullptr;

    const std::string fname = mt->GetFilename();

    try {
        // Prefer the already-decoded Img the loader attached.
        if (Img *img = mt->GetImage(); img && img->width() > 0 && img->height() > 0) {
            const unsigned int w = img->width();
            const unsigned int h = img->height();
            std::vector<unsigned char> rgba(static_cast<std::size_t>(w) * h * 4);
            for (unsigned int y = 0; y < h; ++y) {
                for (unsigned int x = 0; x < w; ++x) {
                    unsigned char r, g, b, a;
                    img->get_pixel(x, y, &r, &g, &b, &a);
                    const std::size_t o = (static_cast<std::size_t>(y) * w + x) * 4;
                    rgba[o + 0] = r; rgba[o + 1] = g; rgba[o + 2] = b; rgba[o + 3] = a;
                }
            }
            const std::string key = !fname.empty() ? fname : ("embedded:" + mt->GetName());
            return &m_textures->loadFromBytes(key, rgba.data(), w, h,
                                              VK_FORMAT_R8G8B8A8_SRGB);
        }

        // Fallback: decode from disk, resolving a relative path against the
        // model file's directory.
        if (!fname.empty()) {
            std::filesystem::path p(fname);
            if (p.is_relative() && m_meshModel) {
                const std::filesystem::path base(m_meshModel->source().toStdString());
                p = base.parent_path() / p;
            }
            return &m_textures->load(p, VK_FORMAT_R8G8B8A8_SRGB);
        }
    } catch (const std::exception &e) {
        qWarning("CgreQuickItem: albedo load failed (%s): %s", fname.c_str(), e.what());
    }
    return nullptr;
}

void CgreQuickItem::rebuildMeshBuffers()
{
    if (!m_adapter || !m_meshModel)
        return;

    ensureDeviceResources();

    VMeshes *meshes = m_meshModel->internalMeshes();
    if (!meshes || meshes->GetNVertices() == 0) {
        m_vertexBuffers.clear();
        m_indexBuffers.clear();
        m_indexCounts.clear();
        m_materialSets.clear();
        m_submeshTextures.clear();
        m_uploadedMeshes = nullptr;
        return;
    }

    // Idempotency: same VMeshes pointer + already populated → no-op.
    if (meshes == m_uploadedMeshes && !m_vertexBuffers.empty())
        return;

    m_vertexBuffers.clear();
    m_indexBuffers.clear();
    m_indexCounts.clear();
    m_materialSets.clear();
    m_submeshTextures.clear();

    cgre2::DeviceContext &dctx = m_adapter->context();
    std::size_t totalVerts = 0, totalIndices = 0;

    // glTF/GLB store UVs with a top-left origin (same as Vulkan); OBJ/MTL
    // use a bottom-left origin and need a vertical flip. Decide per source
    // file extension (UV convention is per-format, not per-mesh).
    const bool flipV =
        m_meshModel->source().toLower().endsWith(QStringLiteral(".obj"));

    try {
        for (Mesh *mesh : meshes->GetMeshes()) {
            if (!mesh || mesh->GetNVertices() == 0)
                continue;

            // Build the polygon-preserving render data (handles n-gons,
            // shared normals where possible). Shares the same path the
            // legacy Quick3D MeshGeometry used.
            mesh->ComputeNormals();
            const auto rd = mesh->BuildPolygonRenderData();
            const std::size_t vertexCount = rd.positions.size() / 3;
            if (vertexCount == 0 || rd.indices.empty())
                continue;

            const bool haveNormals   = rd.normals.size()   == rd.positions.size();
            const bool haveColors    = rd.colors.size()    == rd.positions.size();
            const bool haveTexCoords = rd.texCoords.size() == vertexCount * 2;

            // Per-sub-mesh material (minimal scope: one material per
            // sub-mesh). Faces may carry MATERIAL_NONE (e.g. unassigned
            // faces in 3DS), so scan for the first face with a real material
            // instead of assuming face 0. Color materials drive the vertex
            // color (used only when the mesh has no per-vertex colors); a
            // texture material loads an albedo map and whites out the color.
            float matColor[3] = { 0.6f, 0.7f, 1.0f };
            cgre2::Texture *submeshTex = nullptr;
            Material *mat = nullptr;
            const unsigned int nF = mesh->GetNFaces();
            for (unsigned int fi = 0; fi < nF && !mat; ++fi) {
                const int mid = mesh->GetFaceMaterialId(fi);
                if (mid >= 0)
                    mat = mesh->GetMaterial(static_cast<unsigned int>(mid));
            }
            if (mat) {
                const MaterialType mtype = mat->GetType();
                if (mtype == MATERIAL_COLOR) {
                    auto *mc = static_cast<MaterialColor *>(mat);
                    matColor[0] = mc->GetFloatRed();
                    matColor[1] = mc->GetFloatGreen();
                    matColor[2] = mc->GetFloatBlue();
                } else if (mtype == MATERIAL_COLOR_ADV) {
                    float d[4];
                    static_cast<MaterialColorExt *>(mat)->GetDiffuse(d);
                    matColor[0] = d[0]; matColor[1] = d[1]; matColor[2] = d[2];
                } else if (mtype == MATERIAL_TEXTURE && m_textures) {
                    // Texture is the color source — keep the vertex color
                    // neutral white (texture, or the white fallback if the
                    // image is missing, then carries the look).
                    matColor[0] = matColor[1] = matColor[2] = 1.0f;
                    submeshTex = loadAlbedo(static_cast<MaterialTexture *>(mat));
                }
            }

            // A material is authoritative for the surface color. cgmesh's
            // Mesh::Init() fills vertex colors with a flat 0.5 grey
            // placeholder, so haveColors is essentially always true — using
            // it would mask every material's diffuse (all sub-meshes look
            // identically grey). Fall back to vertex colors only when the
            // sub-mesh has no material at all.
            const bool useVertexColor = (mat == nullptr) && haveColors;

            // VertexPBR (68 B): value-initialised, so tangent/uv1 default to
            // zero — unused in the minimal scope (no normal mapping). UVs are
            // copied straight; any V-flip is deferred to when the albedo
            // texture is actually sampled.
            std::vector<cgre2::VertexPBR> verts(vertexCount);
            for (std::size_t i = 0; i < vertexCount; ++i) {
                verts[i].position[0] = rd.positions[3 * i + 0];
                verts[i].position[1] = rd.positions[3 * i + 1];
                verts[i].position[2] = rd.positions[3 * i + 2];
                verts[i].normal[0] = haveNormals ? rd.normals[3 * i + 0] : 0.0f;
                verts[i].normal[1] = haveNormals ? rd.normals[3 * i + 1] : 1.0f;
                verts[i].normal[2] = haveNormals ? rd.normals[3 * i + 2] : 0.0f;
                verts[i].color[0]  = useVertexColor ? rd.colors[3 * i + 0] : matColor[0];
                verts[i].color[1]  = useVertexColor ? rd.colors[3 * i + 1] : matColor[1];
                verts[i].color[2]  = useVertexColor ? rd.colors[3 * i + 2] : matColor[2];
                verts[i].uv[0] = haveTexCoords ? rd.texCoords[2 * i + 0] : 0.0f;
                const float v = haveTexCoords ? rd.texCoords[2 * i + 1] : 0.0f;
                verts[i].uv[1] = flipV ? (1.0f - v) : v;
            }

            m_vertexBuffers.push_back(std::make_unique<cgre2::VertexBuffer>(
                dctx, verts.data(),
                static_cast<VkDeviceSize>(verts.size() * sizeof(cgre2::VertexPBR)),
                static_cast<uint32_t>(verts.size())));
            m_indexBuffers.push_back(std::make_unique<cgre2::IndexBuffer>(dctx, rd.indices));
            m_indexCounts.push_back(static_cast<uint32_t>(rd.indices.size()));
            m_submeshTextures.push_back(submeshTex);  // aligned with m_vertexBuffers

            totalVerts   += vertexCount;
            totalIndices += rd.indices.size();
        }

        m_uploadedMeshes = meshes;
        m_zoomFactor     = 1.0f;
        qInfo("CgreQuickItem: uploaded %zu sub-mesh(es) — %zu verts, %zu indices",
              m_vertexBuffers.size(), totalVerts, totalIndices);
    } catch (const std::exception &e) {
        qWarning("CgreQuickItem: mesh upload failed: %s", e.what());
        m_vertexBuffers.clear();
        m_indexBuffers.clear();
        m_indexCounts.clear();
        m_materialSets.clear();
        m_submeshTextures.clear();
        m_uploadedMeshes = nullptr;
    }
}

void CgreQuickItem::onMeshChanged()
{
    rebuildMeshBuffers();
    if (QQuickWindow *w = window())
        w->update();
}

// -- per-frame draw -------------------------------------------------------

void CgreQuickItem::onBeforeRenderPassRecording()
{
    if (!m_adapter)
        return;

    ensurePipeline();
    if (!m_pipeline || m_vertexBuffers.empty())
        return;

    // Allocate one material descriptor set per sub-mesh on first use (and
    // whenever the sub-mesh set changed). Done here, not in
    // rebuildMeshBuffers, because it needs m_descriptorLayouts which only
    // exists once ensurePipeline has run.
    ensureMaterialSets();

    QQuickWindow *w = window();
    if (!w)
        return;

    QSGRendererInterface *rif = w->rendererInterface();
    void *cbPtr = rif->getResource(w, QSGRendererInterface::CommandListResource);
    if (!cbPtr)
        return;
    const VkCommandBuffer cmd = *static_cast<VkCommandBuffer *>(cbPtr);

    // Viewport / scissor cover the QQuickItem's logical rect (mapped to
    // window pixel coords).
    const QRectF itemRect(0, 0, width(), height());
    const QPointF topLeft = mapToScene(itemRect.topLeft())
                            * w->effectiveDevicePixelRatio();
    const QPointF botRight = mapToScene(itemRect.bottomRight())
                            * w->effectiveDevicePixelRatio();

    VkViewport vp{};
    vp.x        = static_cast<float>(topLeft.x());
    vp.y        = static_cast<float>(topLeft.y());
    vp.width    = static_cast<float>(botRight.x() - topLeft.x());
    vp.height   = static_cast<float>(botRight.y() - topLeft.y());
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = { int32_t(vp.x), int32_t(vp.y) };
    scissor.extent = { uint32_t(vp.width), uint32_t(vp.height) };

    // Camera matrices — fixed framing around the loaded mesh's aggregate
    // bbox (computed by MeshModel at load time).
    const QVector3D bboxMin = m_meshModel ? m_meshModel->bboxMin() : QVector3D();
    const QVector3D bboxMax = m_meshModel ? m_meshModel->bboxMax() : QVector3D();
    const QVector3D center  = 0.5f * (bboxMin + bboxMax);
    const float     diag    = std::max(2.0f, (bboxMax - bboxMin).length());

    QMatrix4x4 view;
    const QVector3D camOffset = QVector3D(1.0f, 0.6f, 1.4f) * diag * m_zoomFactor;
    view.lookAt(center + camOffset, center, QVector3D(0, 1, 0));

    const float aspect = vp.height > 0.f ? vp.width / vp.height : 1.f;
    QMatrix4x4 proj;
    proj.perspective(45.0f, aspect, 0.01f, diag * 100.0f);
    // Qt's Vulkan render pass uses a top-left origin and Y flipped relative
    // to standard Vulkan. Flip Y in projection so our mesh is right-side-up.
    proj.scale(1.0f, -1.0f, 1.0f);

    // Trackball rotation as model-space rotation around the mesh's bbox
    // center. Trackball stores 4x4 column-major; QMatrix4x4(const float[16])
    // reads row-major, hence the .transposed() conversion.
    QMatrix4x4 trackballMat(m_trackball.getTransform());
    trackballMat = trackballMat.transposed();

    QMatrix4x4 model;
    model.translate(center);
    model = model * trackballMat;
    model.translate(-center);

    PushConstants pc{};
    writeMatrix(proj * view * model, pc.mvp);
    writeMatrix(model, pc.model);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline->getPipeline());
    vkCmdSetViewport(cmd, 0, 1, &vp);
    vkCmdSetScissor (cmd, 0, 1, &scissor);

    vkCmdPushConstants(cmd, m_pipeline->getPipelineLayout(),
                       VK_SHADER_STAGE_VERTEX_BIT,
                       0, sizeof(pc), &pc);

    // Draw every sub-mesh. Pipeline + push constants are state-shared, so
    // we only re-bind the per-mesh VBO/IBO each iteration.
    const std::size_t n = m_vertexBuffers.size();
    for (std::size_t i = 0; i < n; ++i) {
        VkBuffer     vbo    = m_vertexBuffers[i]->getBuffer();
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &vbo, &offset);
        vkCmdBindIndexBuffer  (cmd, m_indexBuffers[i]->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
        if (i < m_materialSets.size()) {
            VkDescriptorSet set = m_materialSets[i];
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    m_pipeline->getPipelineLayout(),
                                    0, 1, &set, 0, nullptr);
        }
        vkCmdDrawIndexed(cmd, m_indexCounts[i], 1, 0, 0, 0);
    }
}

void CgreQuickItem::onSceneGraphInvalidated()
{
    releaseGpuResources();
}

void CgreQuickItem::releaseGpuResources()
{
    m_vertexBuffers.clear();
    m_indexBuffers.clear();
    m_indexCounts.clear();
    m_materialSets.clear();
    m_submeshTextures.clear();
    m_pipeline.reset();
    m_descriptorLayouts.reset();
    m_shaderManager.reset();
    // Pool + textures hold a DeviceContext& — drop them before the adapter
    // that owns the context.
    m_descriptorPool.reset();
    m_textures.reset();
    m_adapter.reset();
    m_uploadedMeshes = nullptr;
}

} // namespace qmlviewer
