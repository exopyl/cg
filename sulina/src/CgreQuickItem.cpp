#include "CgreQuickItem.h"

#include "MeshModel.h"
#include "QtDeviceAdapter.h"

#include "cgre2/Buffer.hpp"
#include "cgre2/DescriptorLayout.hpp"
#include "cgre2/DescriptorPool.hpp"
#include "cgre2/DeviceContext.hpp"
#include "cgre2/Logger.hpp"
#include "cgre2/Pipeline.hpp"
#include "cgre2/Shader.hpp"
#include "cgre2/Texture.hpp"
#include "cgre2/Vertex.hpp"

#include "mesh.h"
#include "vmeshes.h"
#include "mesh_half_edge.h"
#include "DiffParamEvaluator.h"
#include "thickness.h"

#include <QDebug>
#include <QHoverEvent>
#include <QMatrix4x4>
#include <QMouseEvent>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QVector3D>
#include <QVector4D>
#include <QWheelEvent>

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>

namespace {

constexpr const char *kTexturedVertSpv = SULINA_SHADER_DIR "/textured.vert.spv";
constexpr const char *kTexturedFragSpv = SULINA_SHADER_DIR "/textured.frag.spv";
constexpr const char *kUnlitVertSpv    = SULINA_SHADER_DIR "/unlit.vert.spv";
constexpr const char *kUnlitFragSpv    = SULINA_SHADER_DIR "/unlit.frag.spv";

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

// Curvature drop-down label → cgmesh curvature id. Defaults to mean.
CurvatureType curvatureIdFromLabel(const QString &type)
{
    if (type == QStringLiteral("Gaussienne")) return CurvatureType::Gaussian;
    if (type == QStringLiteral("Minimale"))   return CurvatureType::Min;
    if (type == QStringLiteral("Maximale"))   return CurvatureType::Max;
    return CurvatureType::Mean;
}

} // namespace

namespace sulina {

CgreQuickItem::CgreQuickItem(QQuickItem *parent)
    : QQuickItem(parent)
{
    // Left: orbit + anchor the distance first point. Right: clear that anchor.
    setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
    setAcceptHoverEvents(true);   // for "Mesure / Point" surface picking

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
    if (m_anchorValid) emit anchorChanged();   // reproject overlay anchor
}

void CgreQuickItem::mousePressEvent(QMouseEvent *event)
{
    // Right click while measuring drops the anchored distance "first point".
    if (event->button() == Qt::RightButton) {
        if (m_pickingEnabled && m_anchorValid) {
            clearAnchor();
            event->accept();
            return;
        }
        event->ignore();
        return;
    }
    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }
    // In measure picking mode, a left click anchors the current hover hit as
    // the distance "first point". Orbit still works: the trackball press is
    // recorded too, so a press-drag rotates from here.
    if (m_pickingEnabled && m_hoverValid) {
        m_anchorPoint = m_hoverPoint;
        m_anchorValid = true;
        emit anchorChanged();
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
    emit axisTransformChanged();
    if (m_anchorValid) emit anchorChanged();   // reproject overlay anchor
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
    if (m_anchorValid) emit anchorChanged();   // reproject overlay anchor
}

// -- hover picking ("Mesure / Point") -------------------------------------

void CgreQuickItem::hoverMoveEvent(QHoverEvent *event)
{
    m_hoverScreen = event->position();
    if (m_pickingEnabled)
        updatePick(event->position());
    else
        QQuickItem::hoverMoveEvent(event);
}

void CgreQuickItem::hoverLeaveEvent(QHoverEvent *event)
{
    if (m_hoverValid) {
        m_hoverValid = false;
        emit hoverChanged();
    }
    QQuickItem::hoverLeaveEvent(event);
}

void CgreQuickItem::setPickingEnabled(bool on)
{
    if (m_pickingEnabled == on)
        return;
    m_pickingEnabled = on;

    if (on && m_meshModel) {
        // The cgmesh octree picker needs triangle faces (m_nFaces aligned with
        // GetTriangles()). Triangulate non-triangle meshes once — no-op for
        // meshes that are already triangular — and re-upload matching buffers.
        if (VMeshes *meshes = m_meshModel->internalMeshes()) {
            bool changed = false;
            for (Mesh *mesh : meshes->GetMeshes()) {
                if (mesh && mesh->GetNVertices() > 0 && !mesh->IsTriangleMesh()) {
                    mesh->Triangulate();
                    changed = true;
                }
            }
            if (changed) {
                m_uploadedMeshes = nullptr;
                rebuildMeshBuffers();
                if (auto *w = window()) w->update();
            }
        }
    }

    if (!on && m_hoverValid) {
        m_hoverValid = false;
        emit hoverChanged();
    }
    if (!on)
        clearAnchor();
    emit pickingEnabledChanged();
}

void CgreQuickItem::clearAnchor()
{
    if (!m_anchorValid)
        return;
    m_anchorValid = false;
    emit anchorChanged();
    if (auto *w = window()) w->update();
}

QPointF CgreQuickItem::anchorScreen() const
{
    if (!m_anchorValid)
        return QPointF(-1, -1);
    const float w = float(width());
    const float h = float(height());
    if (w <= 0.0f || h <= 0.0f)
        return QPointF(-1, -1);

    QMatrix4x4 proj, view, model;
    cameraMatrices(proj, view, model);
    const QVector4D clip = (proj * view * model) * QVector4D(m_anchorPoint, 1.0f);
    if (clip.w() <= 0.0f)             // behind the camera
        return QPointF(-1, -1);
    // Same NDC→pixel mapping the picker inverts: pos = (ndc + 1)/2 * dim.
    const float ndcX = clip.x() / clip.w();
    const float ndcY = clip.y() / clip.w();
    return QPointF((ndcX + 1.0f) * 0.5f * w,
                   (ndcY + 1.0f) * 0.5f * h);
}

void CgreQuickItem::cameraMatrices(QMatrix4x4 &proj, QMatrix4x4 &view, QMatrix4x4 &model) const
{
    // Mirror onBeforeRenderPassRecording so the picker unprojects with the
    // exact same camera the renderer uses.
    const QVector3D bboxMin = m_meshModel ? m_meshModel->bboxMin() : QVector3D();
    const QVector3D bboxMax = m_meshModel ? m_meshModel->bboxMax() : QVector3D();
    const QVector3D center  = 0.5f * (bboxMin + bboxMax);
    const float     diag    = std::max(2.0f, (bboxMax - bboxMin).length());

    view.setToIdentity();
    const QVector3D camOffset = QVector3D(1.0f, 0.6f, 1.4f) * diag * m_zoomFactor;
    view.lookAt(center + camOffset, center, QVector3D(0, 1, 0));

    const float aspect = height() > 0.0 ? float(width()) / float(height()) : 1.0f;
    proj.setToIdentity();
    proj.perspective(45.0f, aspect, 0.01f, diag * 100.0f);
    proj.scale(1.0f, -1.0f, 1.0f);   // Vulkan Y flip (as in the renderer)

    QMatrix4x4 tb(m_trackball.getTransform());
    tb = tb.transposed();
    model.setToIdentity();
    model.translate(center);
    model = model * tb;
    model.translate(-center);
}

void CgreQuickItem::updatePick(const QPointF &posItem)
{
    const float w = float(width());
    const float h = float(height());
    VMeshes *meshes = m_meshModel ? m_meshModel->internalMeshes() : nullptr;

    auto invalidate = [this]() {
        if (m_hoverValid) { m_hoverValid = false; }
        emit hoverChanged();   // also refreshes hoverScreen-bound readout
    };

    if (!meshes || w <= 0.0f || h <= 0.0f) { invalidate(); return; }

    QMatrix4x4 proj, view, model;
    cameraMatrices(proj, view, model);

    bool ok = false;
    const QMatrix4x4 invVP = (proj * view).inverted(&ok);
    if (!ok) { invalidate(); return; }

    // Cursor → NDC. The Vulkan viewport maps item-top (y=0) to ndc.y=-1, so
    // ndcY increases downward — consistent with the Y-flipped proj above.
    const float ndcX = 2.0f * float(posItem.x()) / w - 1.0f;
    const float ndcY = 2.0f * float(posItem.y()) / h - 1.0f;

    const QVector4D nearH = invVP * QVector4D(ndcX, ndcY, -1.0f, 1.0f);
    const QVector4D farH  = invVP * QVector4D(ndcX, ndcY,  1.0f, 1.0f);
    if (qFuzzyIsNull(nearH.w()) || qFuzzyIsNull(farH.w())) { invalidate(); return; }

    const QVector3D nearW = nearH.toVector3DAffine();
    const QVector3D farW  = farH.toVector3DAffine();

    // Into mesh space: the stored geometry is un-rotated; the displayed model
    // is `model` (trackball about the bbox centre), so undo it on the ray.
    const QMatrix4x4 invModel = model.inverted();
    const QVector3D o = invModel.map(nearW);
    const QVector3D d = (invModel.map(farW) - o).normalized();

    float oo[3] = { o.x(), o.y(), o.z() };
    float dd[3] = { d.x(), d.y(), d.z() };

    float     bestT = -1.0f;
    QVector3D bestHit;
    bool      found = false;
    for (Mesh *mesh : meshes->GetMeshes()) {
        if (!mesh || mesh->GetNVertices() == 0)
            continue;
        float t = -1.0f;
        float hit[3] = { 0, 0, 0 };
        float nrm[3] = { 0, 0, 0 };
        if (mesh->GetIntersectionWithRay(oo, dd, &t, hit, nrm) > 0 && t > 0.0f) {
            if (!found || t < bestT) {
                bestT = t;
                bestHit = QVector3D(hit[0], hit[1], hit[2]);
                found = true;
            }
        }
    }

    m_hoverValid = found;
    if (found)
        m_hoverPoint = bestHit;
    emit hoverChanged();
}

void CgreQuickItem::resetView()
{
    // Trackball has no reset(); a fresh instance is identity. Re-seed its
    // viewport dimensions so the next drag projects correctly.
    m_trackball = cgre2::Trackball();
    m_trackball.setDimensions(static_cast<int>(width()),
                              static_cast<int>(height()));
    m_zoomFactor = 1.0f;
    if (auto *w = window()) w->update();
    emit axisTransformChanged();
    if (m_anchorValid) emit anchorChanged();   // reproject overlay anchor
}

QMatrix4x4 CgreQuickItem::axisTransform() const
{
    // Camera rotation: same framing direction as the renderer's view matrix
    // (eye at camOffset direction, looking at the origin). Only the rotation
    // part matters for axis directions, so center/zoom are irrelevant here.
    QMatrix4x4 view;
    const QVector3D camDir = QVector3D(1.0f, 0.6f, 1.4f).normalized();
    view.lookAt(camDir, QVector3D(0, 0, 0), QVector3D(0, 1, 0));

    // Trackball rotation (column-major → transpose for QMatrix4x4), exactly
    // as applied to the model in onBeforeRenderPassRecording.
    QMatrix4x4 tb(m_trackball.getTransform());
    tb = tb.transposed();

    return view * tb;
}

bool CgreQuickItem::evaluateCurvature(const QString &type)
{
    if (!m_meshModel)
        return false;
    VMeshes *meshes = m_meshModel->internalMeshes();
    if (!meshes)
        return false;

    const CurvatureType cid = curvatureIdFromLabel(type);

    bool any = false;
    for (Mesh *mesh : meshes->GetMeshes()) {
        if (!mesh || mesh->GetNVertices() == 0)
            continue;
        try {
            // Desbrun operates on a triangulated, normal-equipped manifold.
            mesh->Triangulate();    // no-op when already triangular
            mesh->ComputeNormals();

            // Mesh_half_edge builds an internal triangulated copy (same
            // vertex order/count); the evaluator fills that copy's tensors.
            Mesh_half_edge mhe(mesh);
            MeshAlgoTensorEvaluator eval;
            if (!eval.Init(&mhe))
                continue;
            eval.Evaluate(TENSOR_DESBRUN);   // → mhe.m_pMesh->m_pTensors

            // Cache the per-vertex tensors on the rendered mesh (same vertex
            // order/count) so re-colouring for another curvature type is cheap
            // (recolorCurvature, no re-evaluation). Then colour from `cid`.
            mesh->m_pTensors = std::move(mhe.m_pMesh->m_pTensors);
            // The tensors were computed for this mesh's current geometry
            // (same vertex order/count); stamp them valid on the destination
            // so the staleness check and recolorCurvature() accept them.
            mesh->MarkTensorsComputed();
            mesh->InitVertexColorsFromCurvatures(cid);
            if (!mesh->m_pVertexColors.empty())
                any = true;
        } catch (const std::exception &e) {
            cgre2::Logger::warn("Viewer",
                std::string("curvature evaluation failed: ") + e.what());
        }
    }

    if (any) {
        m_curvatureMode  = true;
        m_thicknessMode  = false;     // the two analysis heatmaps are exclusive
        m_uploadedMeshes = nullptr;   // force a rebuild with the new colours
        rebuildMeshBuffers();
        if (auto *w = window()) w->update();
        cgre2::Logger::info("Viewer",
            "curvature evaluated (Desbrun, " + type.toStdString() + ")");
    }
    return any;
}

bool CgreQuickItem::recolorCurvature(const QString &type)
{
    // Only meaningful while a curvature heatmap is shown and tensors are
    // cached. Re-maps the existing tensors to the new type — no Desbrun
    // re-evaluation — and re-uploads the colours.
    if (!m_curvatureMode || !m_meshModel)
        return false;
    VMeshes *meshes = m_meshModel->internalMeshes();
    if (!meshes)
        return false;

    const CurvatureType cid = curvatureIdFromLabel(type);

    bool any = false;
    for (Mesh *mesh : meshes->GetMeshes()) {
        if (!mesh || mesh->m_pTensors.empty())
            continue;
        mesh->InitVertexColorsFromCurvatures(cid);
        if (!mesh->m_pVertexColors.empty())
            any = true;
    }

    if (any) {
        m_uploadedMeshes = nullptr;   // force a rebuild with the new colours
        rebuildMeshBuffers();
        if (auto *w = window()) w->update();
    }
    return any;
}

bool CgreQuickItem::evaluateThickness(const QString &method,
                                      bool autoScale, double scaleMin, double scaleMax)
{
    if (!m_meshModel)
        return false;
    VMeshes *meshes = m_meshModel->internalMeshes();
    if (!meshes)
        return false;

    // "Rayon normal" = single-ray wall thickness (M2); anything else = the
    // robust cone-of-rays Shape Diameter Function (M1, default).
    const bool useRay = (method == QStringLiteral("Rayon normal"));

    // Auto scale -> sentinel (-1,-1) lets cgmesh use the field's own min/max.
    const float lo = autoScale ? -1.f : (float)scaleMin;
    const float hi = autoScale ? -1.f : (float)scaleMax;

    bool any = false;
    for (Mesh *mesh : meshes->GetMeshes()) {
        if (!mesh || mesh->GetNVertices() == 0)
            continue;
        try {
            // Both methods need a triangulated, normal-equipped mesh.
            // Recompute normals after triangulation (which may change
            // topology) so the inward ray direction -n is fresh — symmetric
            // with evaluateCurvature.
            mesh->Triangulate();   // no-op when already triangular
            mesh->ComputeNormals();
            std::vector<float> thickness;
            std::vector<char>  defined;
            const bool ok = useRay
                ? MeshAlgoThickness::ColorizeWallThickness(*mesh, thickness, defined, lo, hi)
                : MeshAlgoThickness::ColorizeShapeDiameter(*mesh, thickness, defined,
                                                           16, 60.f, 1, lo, hi);
            if (ok && !mesh->m_pVertexColors.empty())
                any = true;
        } catch (const std::exception &e) {
            cgre2::Logger::warn("Viewer",
                std::string("thickness evaluation failed: ") + e.what());
        }
    }

    if (any) {
        m_thicknessMode  = true;
        m_curvatureMode  = false;     // the two analysis heatmaps are exclusive
        m_uploadedMeshes = nullptr;   // force a rebuild with the new colours
        rebuildMeshBuffers();
        if (auto *w = window()) w->update();
        cgre2::Logger::info("Viewer",
            std::string("thickness evaluated (") + (useRay ? "ray" : "SDF cone") + ")");
    }
    return any;
}

void CgreQuickItem::clearAnalysis()
{
    if (!m_curvatureMode && !m_thicknessMode)
        return;
    m_curvatureMode  = false;
    m_thicknessMode  = false;
    m_uploadedMeshes = nullptr;   // force a rebuild back to material shading
    rebuildMeshBuffers();
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
        cgre2::Logger::info("Viewer", "extracted Vulkan handles from Qt (DeviceContext ready)");
    } catch (const std::exception &e) {
        cgre2::Logger::error("Viewer", std::string("failed to build QtDeviceAdapter: ") + e.what());
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
        cgre2::Logger::warn("Viewer", "no Vulkan render pass from Qt yet");
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
        cgre2::Logger::info("Viewer", "pipeline built against Qt render pass");

        // Line + point pipelines: unlit shaders (no sampler → no descriptor
        // sets), same VertexPBR vertex layout, different primitive topology.
        // They draw Mesh::m_pLines / m_pPoints as part of the mesh.
        cgre2::ShaderModule &uvert = m_shaderManager->load(kUnlitVertSpv, VK_SHADER_STAGE_VERTEX_BIT);
        cgre2::ShaderModule &ufrag = m_shaderManager->load(kUnlitFragSpv, VK_SHADER_STAGE_FRAGMENT_BIT);
        m_unlitDescriptorLayouts = std::make_unique<cgre2::DescriptorLayouts>(
            dctx, std::vector<const cgre2::ShaderModule *>{ &uvert, &ufrag });

        cgre2::PipelineCreateInfo unlitInfo{};
        unlitInfo.renderPass           = renderPass;
        unlitInfo.vertexShader         = &uvert;
        unlitInfo.fragmentShader       = &ufrag;
        unlitInfo.vertexLayout         = &m_vertexLayout;
        unlitInfo.descriptorSetLayouts = m_unlitDescriptorLayouts->getSetLayouts();
        unlitInfo.pushConstantRanges   = m_unlitDescriptorLayouts->getPushConstantRanges();
        unlitInfo.cullMode             = VK_CULL_MODE_NONE;
        unlitInfo.depthTest            = true;
        unlitInfo.depthWrite           = true;
        unlitInfo.depthCompareOp       = VK_COMPARE_OP_LESS;

        cgre2::PipelineCreateInfo lineInfo = unlitInfo;
        lineInfo.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        m_linePipeline = std::make_unique<cgre2::Pipeline>(dctx, lineInfo);

        cgre2::PipelineCreateInfo pointInfo = unlitInfo;
        pointInfo.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        m_pointPipeline = std::make_unique<cgre2::Pipeline>(dctx, pointInfo);
        cgre2::Logger::info("Viewer", "line + point pipelines built");
    } catch (const std::exception &e) {
        cgre2::Logger::error("Viewer", std::string("pipeline build failed: ") + e.what());
        m_pipeline.reset();
        m_linePipeline.reset();
        m_pointPipeline.reset();
        m_descriptorLayouts.reset();
        m_unlitDescriptorLayouts.reset();
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
        cgre2::Logger::warn("Viewer",
            "albedo load failed (" + fname + "): " + e.what());
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
        m_lineVertexBuffers.clear();
        m_lineIndexBuffers.clear();
        m_lineIndexCounts.clear();
        m_pointVertexBuffers.clear();
        m_pointIndexBuffers.clear();
        m_pointIndexCounts.clear();
        m_uploadedMeshes = nullptr;
        return;
    }

    // Idempotency: same VMeshes pointer + already populated → no-op. A pure
    // lines/points mesh has no surface VBOs, so also check the primitive ones.
    if (meshes == m_uploadedMeshes &&
        (!m_vertexBuffers.empty() || !m_lineVertexBuffers.empty() || !m_pointVertexBuffers.empty()))
        return;

    m_vertexBuffers.clear();
    m_indexBuffers.clear();
    m_indexCounts.clear();
    m_materialSets.clear();
    m_submeshTextures.clear();
    m_lineVertexBuffers.clear();
    m_lineIndexBuffers.clear();
    m_lineIndexCounts.clear();
    m_pointVertexBuffers.clear();
    m_pointIndexBuffers.clear();
    m_pointIndexCounts.clear();

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

            // Line ('l') and point ('p') primitives belong to the mesh and are
            // built first — a lines/points-only mesh has no faces, so the
            // surface path below `continue`s and would otherwise emit nothing.
            // They index the raw mesh vertices, so they get their own VBO built
            // straight from m_pVertices (flat colour; the 0.5 grey placeholder
            // Mesh::Init writes into m_pVertexColors is not a usable colour).
            {
                const unsigned int nV = mesh->GetNVertices();
                auto buildPrim = [&](const std::vector<unsigned int> &idx,
                                     float r, float g, float b,
                                     std::vector<std::unique_ptr<cgre2::VertexBuffer>> &vbs,
                                     std::vector<std::unique_ptr<cgre2::IndexBuffer>> &ibs,
                                     std::vector<uint32_t> &counts) {
                    if (idx.empty())
                        return;
                    std::vector<uint32_t> indices;
                    indices.reserve(idx.size());
                    for (unsigned int v : idx)
                        if (v < nV) indices.push_back(v);   // defensive range check
                    if (indices.empty())
                        return;
                    std::vector<cgre2::VertexPBR> verts(nV);
                    for (unsigned int i = 0; i < nV; ++i) {
                        verts[i] = cgre2::VertexPBR{};
                        verts[i].position[0] = mesh->m_pVertices[3 * i + 0];
                        verts[i].position[1] = mesh->m_pVertices[3 * i + 1];
                        verts[i].position[2] = mesh->m_pVertices[3 * i + 2];
                        verts[i].normal[1]   = 1.0f;   // unused by the unlit shader
                        verts[i].color[0] = r; verts[i].color[1] = g; verts[i].color[2] = b;
                    }
                    vbs.push_back(std::make_unique<cgre2::VertexBuffer>(
                        dctx, verts.data(),
                        static_cast<VkDeviceSize>(verts.size() * sizeof(cgre2::VertexPBR)),
                        static_cast<uint32_t>(verts.size())));
                    ibs.push_back(std::make_unique<cgre2::IndexBuffer>(dctx, indices));
                    counts.push_back(static_cast<uint32_t>(indices.size()));
                };
                buildPrim(mesh->m_pLines, 0.15f, 0.45f, 0.85f,
                          m_lineVertexBuffers, m_lineIndexBuffers, m_lineIndexCounts);
                buildPrim(mesh->m_pPoints, 0.90f, 0.20f, 0.20f,
                          m_pointVertexBuffers, m_pointIndexBuffers, m_pointIndexCounts);
            }

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
            // In curvature mode we shade purely from per-vertex colours (the
            // heatmap), so skip material colour/texture resolution entirely
            // (no texture bound → white fallback → colour = vertex colour).
            if (mat && !m_curvatureMode && !m_thicknessMode) {
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
            const bool useVertexColor = (m_curvatureMode || m_thicknessMode || mat == nullptr) && haveColors;

            cgre2::Logger::debug("Viewer",
                "submesh #" + std::to_string(m_vertexBuffers.size())
                + " matType=" + std::to_string(mat ? static_cast<int>(mat->GetType()) : -1)
                + " color=(" + std::to_string(matColor[0]) + "," + std::to_string(matColor[1])
                + "," + std::to_string(matColor[2]) + ")"
                + " tex=" + (submeshTex ? "1" : "0")
                + " uv=" + (haveTexCoords ? "1" : "0")
                + " vtxCol=" + (useVertexColor ? "1" : "0"));

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
        // NB: don't reset the camera framing (m_zoomFactor) here — this runs
        // on every rebuild, including curvature (re)colouring, which must not
        // disturb the user's view. Reframing happens in onMeshChanged on an
        // actual new-mesh load.
        cgre2::Logger::info("Viewer",
            "uploaded " + std::to_string(m_vertexBuffers.size()) + " sub-mesh(es) — "
            + std::to_string(totalVerts) + " verts, "
            + std::to_string(totalIndices) + " indices");
    } catch (const std::exception &e) {
        cgre2::Logger::error("Viewer", std::string("mesh upload failed: ") + e.what());
        m_vertexBuffers.clear();
        m_indexBuffers.clear();
        m_indexCounts.clear();
        m_materialSets.clear();
        m_submeshTextures.clear();
        m_lineVertexBuffers.clear();
        m_lineIndexBuffers.clear();
        m_lineIndexCounts.clear();
        m_pointVertexBuffers.clear();
        m_pointIndexBuffers.clear();
        m_pointIndexCounts.clear();
        m_uploadedMeshes = nullptr;
    }
}

void CgreQuickItem::onMeshChanged()
{
    // A freshly loaded mesh starts in normal (material) shading and is
    // reframed to the default zoom (rebuildMeshBuffers no longer does this,
    // so that recolouring keeps the current view).
    m_curvatureMode = false;
    m_thicknessMode = false;
    m_zoomFactor    = 1.0f;
    // Any picked points belong to the previous mesh's coordinate space — drop
    // them so a stale anchor/hover doesn't reproject onto the new model.
    clearAnchor();
    if (m_hoverValid) {
        m_hoverValid = false;
        emit hoverChanged();
    }
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
    if (!m_pipeline)
        return;
    // A pure lines/points mesh has no surface VBOs but still has primitives.
    if (m_vertexBuffers.empty() &&
        m_lineVertexBuffers.empty() && m_pointVertexBuffers.empty())
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

    // Line + point primitives (mesh 'l' / 'p'). Unlit pipelines, same camera
    // push constants, no descriptor sets. Drawn after the surface.
    auto drawPrims = [&](cgre2::Pipeline *pipe,
                         const std::vector<std::unique_ptr<cgre2::VertexBuffer>> &vbs,
                         const std::vector<std::unique_ptr<cgre2::IndexBuffer>> &ibs,
                         const std::vector<uint32_t> &counts) {
        if (!pipe || vbs.empty())
            return;
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe->getPipeline());
        vkCmdSetViewport(cmd, 0, 1, &vp);
        vkCmdSetScissor (cmd, 0, 1, &scissor);
        // Unlit shaders declare only mvp (first 64 bytes of PushConstants).
        vkCmdPushConstants(cmd, pipe->getPipelineLayout(),
                           VK_SHADER_STAGE_VERTEX_BIT, 0,
                           static_cast<uint32_t>(sizeof(pc.mvp)), &pc);
        for (std::size_t i = 0; i < vbs.size(); ++i) {
            VkBuffer     vbo    = vbs[i]->getBuffer();
            VkDeviceSize offset = 0;
            vkCmdBindVertexBuffers(cmd, 0, 1, &vbo, &offset);
            vkCmdBindIndexBuffer  (cmd, ibs[i]->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(cmd, counts[i], 1, 0, 0, 0);
        }
    };
    drawPrims(m_linePipeline.get(),  m_lineVertexBuffers,  m_lineIndexBuffers,  m_lineIndexCounts);
    drawPrims(m_pointPipeline.get(), m_pointVertexBuffers, m_pointIndexBuffers, m_pointIndexCounts);
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
    m_lineVertexBuffers.clear();
    m_lineIndexBuffers.clear();
    m_lineIndexCounts.clear();
    m_pointVertexBuffers.clear();
    m_pointIndexBuffers.clear();
    m_pointIndexCounts.clear();
    m_linePipeline.reset();
    m_pointPipeline.reset();
    m_pipeline.reset();
    m_descriptorLayouts.reset();
    m_unlitDescriptorLayouts.reset();
    m_shaderManager.reset();
    // Pool + textures hold a DeviceContext& — drop them before the adapter
    // that owns the context.
    m_descriptorPool.reset();
    m_textures.reset();
    m_adapter.reset();
    m_uploadedMeshes = nullptr;
}

} // namespace sulina
