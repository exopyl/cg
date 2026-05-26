#include "CgreQuickItem.h"

#include "MeshModel.h"
#include "QtDeviceAdapter.h"

#include "cgre2/Buffer.hpp"
#include "cgre2/DescriptorLayout.hpp"
#include "cgre2/DeviceContext.hpp"
#include "cgre2/Pipeline.hpp"
#include "cgre2/Shader.hpp"
#include "cgre2/Vertex.hpp"

#include "mesh.h"

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

constexpr const char *kBasicVertSpv = QMLVIEWER_SHADER_DIR "/basic.vert.spv";
constexpr const char *kBasicFragSpv = QMLVIEWER_SHADER_DIR "/basic.frag.spv";

// basic.vert pushes 2 mat4 = 128 bytes via push constants.
struct PushConstants {
    float mvp[16];
    float model[16];
};

void writeMatrix(const QMatrix4x4 &m, float out[16])
{
    // QMatrix4x4::constData() returns 16 floats in COLUMN-major order, which
    // is exactly what GLSL uses for `mat4`. memcpy is safe.
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

    // Try to upload immediately if we already have a device.
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

    // Mesh data may already be loaded — upload now.
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

    try {
        m_shaderManager = std::make_unique<cgre2::ShaderManager>(dctx);

        cgre2::ShaderModule &vert = m_shaderManager->load(kBasicVertSpv, VK_SHADER_STAGE_VERTEX_BIT);
        cgre2::ShaderModule &frag = m_shaderManager->load(kBasicFragSpv, VK_SHADER_STAGE_FRAGMENT_BIT);

        m_descriptorLayouts = std::make_unique<cgre2::DescriptorLayouts>(
            dctx, std::vector<const cgre2::ShaderModule *>{ &vert, &frag });

        m_vertexLayout = cgre2::Vertex::getLayout();

        cgre2::PipelineCreateInfo info{};
        info.renderPass           = renderPass;
        info.vertexShader         = &vert;
        info.fragmentShader       = &frag;
        info.vertexLayout         = &m_vertexLayout;
        info.descriptorSetLayouts = m_descriptorLayouts->getSetLayouts();
        info.pushConstantRanges   = m_descriptorLayouts->getPushConstantRanges();
        // Quick3D / Qt render pass uses CCW front faces with Y flipped in
        // the projection matrix; we'll bake that into the proj. Cull back
        // faces; depth test ON, depth write ON.
        info.cullMode      = VK_CULL_MODE_BACK_BIT;
        info.frontFace     = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        info.depthTest     = true;
        info.depthWrite    = true;
        info.depthCompareOp = VK_COMPARE_OP_LESS;

        m_pipeline = std::make_unique<cgre2::Pipeline>(dctx, info);
        qInfo("CgreQuickItem: cgre2 pipeline built against Qt render pass");
    } catch (const std::exception &e) {
        qWarning("CgreQuickItem: pipeline build failed: %s", e.what());
        m_pipeline.reset();
        m_descriptorLayouts.reset();
        m_shaderManager.reset();
    }
}

void CgreQuickItem::rebuildMeshBuffers()
{
    if (!m_adapter || !m_meshModel)
        return;

    Mesh *mesh = m_meshModel->internalMesh();
    if (!mesh || mesh->GetNVertices() == 0) {
        m_vertexBuffer.reset();
        m_indexBuffer.reset();
        m_indexCount = 0;
        m_uploadedMesh = nullptr;
        return;
    }

    // Idempotency: if we already uploaded this exact Mesh instance, skip.
    // MeshModel::load() always builds a fresh Mesh on success, so a pointer
    // compare is sufficient to detect "same mesh as last time".
    if (mesh == m_uploadedMesh && m_vertexBuffer && m_indexBuffer)
        return;

    // Build the polygon-preserving render data via cgmesh (same path the
    // Quick3D-side MeshGeometry uses).
    mesh->ComputeNormals();
    const auto rd = mesh->BuildPolygonRenderData();
    const std::size_t vertexCount = rd.positions.size() / 3;
    if (vertexCount == 0 || rd.indices.empty()) {
        m_vertexBuffer.reset();
        m_indexBuffer.reset();
        m_indexCount = 0;
        return;
    }

    const bool haveNormals = rd.normals.size() == rd.positions.size();
    const bool haveColors  = rd.colors.size()  == rd.positions.size();

    std::vector<cgre2::Vertex> verts(vertexCount);
    for (std::size_t i = 0; i < vertexCount; ++i) {
        verts[i].position[0] = rd.positions[3 * i + 0];
        verts[i].position[1] = rd.positions[3 * i + 1];
        verts[i].position[2] = rd.positions[3 * i + 2];
        verts[i].normal[0] = haveNormals ? rd.normals[3 * i + 0] : 0.0f;
        verts[i].normal[1] = haveNormals ? rd.normals[3 * i + 1] : 1.0f;
        verts[i].normal[2] = haveNormals ? rd.normals[3 * i + 2] : 0.0f;
        verts[i].color[0]  = haveColors  ? rd.colors[3 * i + 0]  : 0.6f;
        verts[i].color[1]  = haveColors  ? rd.colors[3 * i + 1]  : 0.7f;
        verts[i].color[2]  = haveColors  ? rd.colors[3 * i + 2]  : 1.0f;
    }

    try {
        cgre2::DeviceContext &dctx = m_adapter->context();
        m_vertexBuffer = std::make_unique<cgre2::VertexBuffer>(
            dctx, verts.data(),
            static_cast<VkDeviceSize>(verts.size() * sizeof(cgre2::Vertex)),
            static_cast<uint32_t>(verts.size()));
        m_indexBuffer  = std::make_unique<cgre2::IndexBuffer>(dctx, rd.indices);
        m_indexCount   = static_cast<uint32_t>(rd.indices.size());
        m_uploadedMesh = mesh;
        m_zoomFactor   = 1.0f;  // reset framing for the new mesh
        qInfo("CgreQuickItem: uploaded mesh — %zu verts, %u indices",
              vertexCount, m_indexCount);
    } catch (const std::exception &e) {
        qWarning("CgreQuickItem: mesh upload failed: %s", e.what());
        m_vertexBuffer.reset();
        m_indexBuffer.reset();
        m_indexCount = 0;
    }
}

void CgreQuickItem::onMeshChanged()
{
    rebuildMeshBuffers();
    // QQuickItem::update() requires ItemHasContents and we draw via raw
    // Vulkan in beforeRenderPassRecording — schedule a window repaint
    // instead so the next frame picks up our new VBO/IBO.
    if (QQuickWindow *w = window())
        w->update();
}

// -- per-frame draw -------------------------------------------------------

void CgreQuickItem::onBeforeRenderPassRecording()
{
    if (!m_adapter)
        return;

    ensurePipeline();
    if (!m_pipeline || !m_vertexBuffer || !m_indexBuffer || m_indexCount == 0)
        return;

    QQuickWindow *w = window();
    if (!w)
        return;

    QSGRendererInterface *rif = w->rendererInterface();
    void *cbPtr = rif->getResource(w, QSGRendererInterface::CommandListResource);
    if (!cbPtr)
        return;
    const VkCommandBuffer cmd = *static_cast<VkCommandBuffer *>(cbPtr);

    // Viewport / scissor cover the QQuickItem's logical rect (mapped to
    // window pixel coords). Origin at top-left, OpenGL-style.
    const QRectF itemRect(0, 0, width(), height());
    const QSize  fb = w->size() * w->effectiveDevicePixelRatio();
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

    // Camera matrices — fixed framing around the loaded mesh's bbox.
    QVector3D bboxMin(0, 0, 0), bboxMax(0, 0, 0);
    if (Mesh *mesh = m_meshModel ? m_meshModel->internalMesh() : nullptr) {
        const auto &bb = mesh->bbox();
        bboxMin = QVector3D(bb.GetMinX(), bb.GetMinY(), bb.GetMinZ());
        bboxMax = QVector3D(bb.GetMaxX(), bb.GetMaxY(), bb.GetMaxZ());
    }
    const QVector3D center = 0.5f * (bboxMin + bboxMax);
    const float     diag   = std::max(2.0f, (bboxMax - bboxMin).length());

    QMatrix4x4 view;
    const QVector3D camOffset = QVector3D(1.0f, 0.6f, 1.4f) * diag * m_zoomFactor;
    view.lookAt(center + camOffset, center, QVector3D(0, 1, 0));

    const float aspect = vp.height > 0.f ? vp.width / vp.height : 1.f;
    QMatrix4x4 proj;
    proj.perspective(45.0f, aspect, 0.01f, diag * 100.0f);
    // Qt's Vulkan render pass uses a top-left origin and Y flipped relative
    // to standard Vulkan. Flip Y in projection so our mesh is right-side-up.
    proj.scale(1.0f, -1.0f, 1.0f);

    // Apply the trackball rotation as a model-space rotation around the
    // mesh's bbox center: translate to center, rotate, translate back.
    // Trackball stores 4x4 column-major; QMatrix4x4(const float[16]) reads
    // row-major, hence the .transposed() conversion.
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

    VkBuffer     vbo    = m_vertexBuffer->getBuffer();
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vbo, &offset);
    vkCmdBindIndexBuffer  (cmd, m_indexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);

    vkCmdDrawIndexed(cmd, m_indexCount, 1, 0, 0, 0);
    (void)fb;
}

void CgreQuickItem::onSceneGraphInvalidated()
{
    releaseGpuResources();
}

void CgreQuickItem::releaseGpuResources()
{
    m_indexBuffer.reset();
    m_vertexBuffer.reset();
    m_pipeline.reset();
    m_descriptorLayouts.reset();
    m_shaderManager.reset();
    m_adapter.reset();
    m_indexCount   = 0;
    m_uploadedMesh = nullptr;
}

} // namespace qmlviewer
