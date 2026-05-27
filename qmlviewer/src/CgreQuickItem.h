#pragma once

#include "MeshModel.h"  // Q_PROPERTY pointer types need fully-defined types in Qt 6.11
#include "cgre2/Vertex.hpp"
#include "cgre2/Trackball.hpp"

class VMeshes;
class MaterialTexture;

#include <QQuickItem>
#include <QQuickWindow>  // moc registers QQuickWindow* metatype for slots
#include <QtQmlIntegration/qqmlintegration.h>

#include <memory>
#include <vector>

namespace cgre2 {
class Pipeline;
class ShaderManager;
class DescriptorLayouts;
class DescriptorPool;
class TextureManager;
class Texture;
class VertexBuffer;
class IndexBuffer;
}

namespace qmlviewer {

class QtDeviceAdapter;

/// QML item that records cgre2 Vulkan draw commands into Qt's scene graph
/// frame, using the "Vulkan-Under-QML" pattern. Shares Qt's VkInstance /
/// VkDevice / VkQueue via QSGRendererInterface; never creates its own.
///
/// When a `meshModel` is bound, every sub-mesh of its VMeshes is uploaded
/// as a separate (VertexBuffer + IndexBuffer) pair. Each frame, all
/// sub-meshes are drawn in sequence in `beforeRenderPassRecording`.
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
    void ensureDeviceResources();      // texture cache + descriptor pool
    // Upload a MaterialTexture's albedo into m_textures; nullptr on failure
    // or empty image. Resolves relative filenames against the model's dir.
    cgre2::Texture *loadAlbedo(MaterialTexture *mt);
    void ensurePipeline();             // lazy build on first frame
    void ensureMaterialSets();         // one descriptor set per sub-mesh
    void rebuildMeshBuffers();         // when meshModel data changes
    void releaseGpuResources();        // sceneGraphInvalidated

    MeshModel *m_meshModel = nullptr;

    // GPU side — owned, lifetime tied to the QtDeviceAdapter (Qt's device).
    std::unique_ptr<QtDeviceAdapter>          m_adapter;
    std::unique_ptr<cgre2::ShaderManager>     m_shaderManager;
    std::unique_ptr<cgre2::DescriptorLayouts> m_descriptorLayouts;
    std::unique_ptr<cgre2::Pipeline>          m_pipeline;

    // Texture cache + descriptor pool for per-sub-mesh material sets. In
    // the minimal scope every set binds the 1x1 white fallback (see
    // ensureMaterialSets); m_materialSets is aligned with m_vertexBuffers.
    std::unique_ptr<cgre2::TextureManager>    m_textures;
    std::unique_ptr<cgre2::DescriptorPool>    m_descriptorPool;
    std::vector<VkDescriptorSet>              m_materialSets;

    // Per-sub-mesh albedo texture (owned by m_textures' cache), aligned
    // with m_vertexBuffers. nullptr → bind the 1x1 white fallback.
    std::vector<cgre2::Texture *>             m_submeshTextures;

    // One pair of VBO/IBO per sub-mesh in the loaded VMeshes. Indices in
    // these three vectors are aligned (i-th vbo + i-th ibo + i-th count).
    std::vector<std::unique_ptr<cgre2::VertexBuffer>> m_vertexBuffers;
    std::vector<std::unique_ptr<cgre2::IndexBuffer>>  m_indexBuffers;
    std::vector<uint32_t>                              m_indexCounts;

    // Pipeline needs a stable VertexLayout pointer for the duration of
    // the ctor call — keep it as a member so the lifetime is well-defined.
    cgre2::VertexLayout m_vertexLayout;

    // Track the last VMeshes instance uploaded so we skip redundant
    // rebuilds when QML re-evaluates bindings or meshChanged fires more
    // than once.
    VMeshes *m_uploadedMeshes = nullptr;

    // Trackball — left-mouse drag rotates around the loaded mesh's bbox
    // center.
    cgre2::Trackball m_trackball;

    // Wheel-driven zoom factor. 1.0 = default framing; <1 closer, >1
    // farther. Reset to 1.0 on each new mesh load.
    float m_zoomFactor = 1.0f;
};

} // namespace qmlviewer
