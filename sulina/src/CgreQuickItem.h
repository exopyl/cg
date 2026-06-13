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

namespace sulina {

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
    // Current view orientation (camera rotation × trackball rotation). Maps a
    // world-axis direction into eye space; the QML axis gizmo reads it to spin
    // with the model. Changes whenever the trackball rotation changes.
    Q_PROPERTY(QMatrix4x4 axisTransform READ axisTransform NOTIFY axisTransformChanged)

    // Surface-point picking on hover (driven by the "Mesure / Point" tool).
    // When enabled, hoverMoveEvent ray-casts the cursor against the mesh and
    // publishes the hit point (in mesh coordinates) for a QML readout.
    Q_PROPERTY(bool      pickingEnabled READ pickingEnabled WRITE setPickingEnabled NOTIFY pickingEnabledChanged)
    Q_PROPERTY(bool      hoverValid     READ hoverValid  NOTIFY hoverChanged)
    Q_PROPERTY(QVector3D hoverPoint     READ hoverPoint  NOTIFY hoverChanged)
    Q_PROPERTY(QPointF   hoverScreen    READ hoverScreen NOTIFY hoverChanged)

    // Anchored "first point" for distance measurement. A left click while
    // picking is enabled snaps the current hover hit as the anchor; the QML
    // distance readout then measures from here to the live hover point.
    Q_PROPERTY(bool      anchorValid    READ anchorValid NOTIFY anchorChanged)
    Q_PROPERTY(QVector3D anchorPoint    READ anchorPoint NOTIFY anchorChanged)
    // Anchor projected to item-local pixels with the current camera, for the
    // 2D measurement overlay. (-1,-1) when invalid or behind the camera.
    // Re-emitted via anchorChanged whenever the camera moves.
    Q_PROPERTY(QPointF   anchorScreen   READ anchorScreen NOTIFY anchorChanged)

public:
    explicit CgreQuickItem(QQuickItem *parent = nullptr);
    ~CgreQuickItem() override;

    MeshModel *meshModel() const { return m_meshModel; }
    void setMeshModel(MeshModel *model);

    /// View orientation = camera rotation × trackball rotation. Applying its
    /// upper 3×3 to a world-axis unit vector yields that axis' direction in
    /// eye space (x right, y up, z toward viewer) — what the gizmo projects.
    QMatrix4x4 axisTransform() const;

    bool      pickingEnabled() const { return m_pickingEnabled; }
    void      setPickingEnabled(bool on);
    bool      hoverValid() const { return m_hoverValid; }
    QVector3D hoverPoint() const { return m_hoverPoint; }
    QPointF   hoverScreen() const { return m_hoverScreen; }

    bool      anchorValid() const { return m_anchorValid; }
    QVector3D anchorPoint() const { return m_anchorPoint; }
    QPointF   anchorScreen() const;

    /// Drop the anchored distance "first point" (back to no measurement).
    /// Wired to the measure panel's reset and cleared when picking is off.
    Q_INVOKABLE void clearAnchor();

    /// Reset the camera framing — clears any accumulated trackball rotation
    /// and restores the default zoom. Wired to the dock's "Recadrer" button.
    Q_INVOKABLE void resetView();

    /// Compute per-vertex curvature on the loaded mesh using the Desbrun
    /// method (Meyer/Desbrun/Schröder/Barr discrete operators, via cgmesh's
    /// MeshAlgoTensorEvaluator) and display it as a jet heatmap on the model.
    /// `type` is one of "Gaussienne" / "Moyenne" / "Minimale" / "Maximale".
    /// Returns true if at least one sub-mesh was coloured. Wired to the
    /// curvature panel's "Évaluer" button.
    Q_INVOKABLE bool evaluateCurvature(const QString &type);

    /// Re-map the already-computed curvature tensors to a different curvature
    /// type and recolour the model — cheap, no re-evaluation. No-op unless a
    /// curvature heatmap is currently shown. Wired to the curvature type
    /// drop-down so the heatmap updates live when the type changes.
    Q_INVOKABLE bool recolorCurvature(const QString &type);

    /// Compute per-vertex thickness on the loaded mesh and display it as a
    /// heatmap (THIN = red, THICK = blue). `method` selects the cgmesh
    /// MeshAlgoThickness algorithm:
    ///   - "Rayon normal"  → single inward-normal ray (fast, M2);
    ///   - anything else (e.g. "SDF (cône)") → robust cone-of-rays Shape
    ///     Diameter Function (default, M1).
    /// Colour scale: when `autoScale` is true the field's own min/max is used;
    /// otherwise it is normalised over [scaleMin, scaleMax] (model units).
    /// Returns true if at least one sub-mesh was coloured. Wired to the
    /// thickness panel's "Évaluer" button.
    Q_INVOKABLE bool evaluateThickness(const QString &method = QString(),
                                       bool   autoScale = true,
                                       double scaleMin  = -1.0,
                                       double scaleMax  = -1.0);

    /// Drop any analysis colouring and restore normal material/vertex
    /// shading. Wired to the panel's "Réinitialiser".
    Q_INVOKABLE void clearAnalysis();

protected:
    void mousePressEvent  (QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent   (QMouseEvent *event) override;
    void wheelEvent       (QWheelEvent *event) override;
    void hoverMoveEvent   (QHoverEvent *event) override;
    void hoverLeaveEvent  (QHoverEvent *event) override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

signals:
    void meshModelChanged();
    void axisTransformChanged();
    void pickingEnabledChanged();
    void hoverChanged();
    void anchorChanged();

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

    // Analysis colouring: when true, rebuildMeshBuffers ignores materials
    // and shades each sub-mesh with its per-vertex colours (the curvature
    // heatmap written by evaluateCurvature). Reset on each new mesh load.
    bool m_curvatureMode = false;

    // Same idea for the wall-thickness heatmap (MeshAlgoThickness). Mutually
    // exclusive with m_curvatureMode; either one routes rendering through the
    // per-vertex colours. Reset on each new mesh load.
    bool m_thicknessMode = false;

    // -- hover picking ("Mesure / Point") ---------------------------------
    // Rebuild the proj / view / model matrices used by the renderer so the
    // picker can unproject the cursor with the exact same camera.
    void cameraMatrices(QMatrix4x4 &proj, QMatrix4x4 &view, QMatrix4x4 &model) const;
    // Ray-cast the cursor (item-local pixels) against the mesh; update hover*.
    void updatePick(const QPointF &posItem);

    bool      m_pickingEnabled = false;
    bool      m_hoverValid     = false;
    QVector3D m_hoverPoint;     // surface point, in mesh coordinates
    QPointF   m_hoverScreen;    // cursor position, item-local pixels

    bool      m_anchorValid    = false;
    QVector3D m_anchorPoint;    // distance "first point", in mesh coordinates
};

} // namespace sulina
