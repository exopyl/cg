#pragma once

#include "MeshModel.h"  // Q_PROPERTY pointer types need fully-defined types in Qt 6.11
#include "cgre2/Vertex.hpp"
#include "cgre2/Trackball.hpp"

class Mesh;

#include <QQuickItem>
#include <QQuickWindow>  // moc registers QQuickWindow* metatype for slots
#include <QtQmlIntegration/qqmlintegration.h>

#include <memory>

namespace cgre2 {
class Pipeline;
class ShaderManager;
class DescriptorLayouts;
class VertexBuffer;
class IndexBuffer;
}

namespace qmlviewer {

class QtDeviceAdapter;

/// QML item that records cgre2 Vulkan draw commands into Qt's scene graph
/// frame, using the "Vulkan-Under-QML" pattern. Shares Qt's VkInstance /
/// VkDevice / VkQueue via QSGRendererInterface; never creates its own.
///
/// When a `meshModel` is bound, its loaded mesh is uploaded once to a
/// cgre2::VertexBuffer + IndexBuffer (re-uploaded on each meshChanged
/// signal). Each frame, `beforeRenderPassRecording` binds the pipeline,
/// pushes a camera-derived MVP, and issues vkCmdDrawIndexed.
class CgreQuickItem : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(MeshModel *meshModel READ meshModel WRITE setMeshModel NOTIFY meshModelChanged)

public:
    explicit CgreQuickItem(QQuickItem *parent = nullptr);
    ~CgreQuickItem() override;

    MeshModel *meshModel() const { return m_meshModel; }
    void setMeshModel(MeshModel *model);

protected:
    void mousePressEvent  (QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent   (QMouseEvent *event) override;
    void wheelEvent       (QWheelEvent *event) override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

signals:
    void meshModelChanged();

private slots:
    void onWindowChanged(QQuickWindow *window);
    void onSceneGraphInitialized();
    void onBeforeRenderPassRecording();
    void onSceneGraphInvalidated();
    void onMeshChanged();

private:
    void ensurePipeline();             // lazy build on first frame
    void rebuildMeshBuffers();         // when meshModel data changes
    void releaseGpuResources();        // sceneGraphInvalidated

    // Connection-target object (m_meshModel) — kept for the disconnect side.
    MeshModel *m_meshModel = nullptr;

    // GPU side — owned, lifetime tied to the QtDeviceAdapter (Qt's device).
    std::unique_ptr<QtDeviceAdapter>      m_adapter;
    std::unique_ptr<cgre2::ShaderManager> m_shaderManager;
    std::unique_ptr<cgre2::DescriptorLayouts> m_descriptorLayouts;
    std::unique_ptr<cgre2::Pipeline>      m_pipeline;
    std::unique_ptr<cgre2::VertexBuffer>  m_vertexBuffer;
    std::unique_ptr<cgre2::IndexBuffer>   m_indexBuffer;

    // Pipeline needs a stable VertexLayout pointer for the duration of the
    // ctor call — keep it as a member so the lifetime is well-defined.
    cgre2::VertexLayout m_vertexLayout;

    uint32_t m_indexCount = 0;

    // Track the last mesh pointer uploaded so we skip redundant rebuilds
    // when QML re-evaluates bindings or meshChanged fires more than once.
    Mesh *m_uploadedMesh = nullptr;

    // Trackball — left-mouse drag rotates the mesh around its bbox center.
    cgre2::Trackball m_trackball;

    // Wheel-driven zoom factor. 1.0 = default framing; <1 closer, >1 farther.
    // Reset to 1.0 on each new mesh load (in rebuildMeshBuffers).
    float m_zoomFactor = 1.0f;
};

} // namespace qmlviewer
