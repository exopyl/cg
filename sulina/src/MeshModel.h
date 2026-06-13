#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVector3D>
#include <QtQmlIntegration/qqmlintegration.h>

#include <memory>

class VMeshes;

class MeshModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString name           READ name           NOTIFY meshChanged)
    Q_PROPERTY(QString source         READ source         NOTIFY meshChanged)
    Q_PROPERTY(int     vertexCount    READ vertexCount    NOTIFY meshChanged)
    Q_PROPERTY(int     faceCount      READ faceCount      NOTIFY meshChanged)
    Q_PROPERTY(int     meshCount      READ meshCount      NOTIFY meshChanged)
    Q_PROPERTY(bool    isTriangleMesh READ isTriangleMesh NOTIFY meshChanged)
    Q_PROPERTY(QVector3D bboxMin      READ bboxMin        NOTIFY meshChanged)
    Q_PROPERTY(QVector3D bboxMax      READ bboxMax        NOTIFY meshChanged)
    Q_PROPERTY(QVector3D bboxCenter   READ bboxCenter     NOTIFY meshChanged)
    Q_PROPERTY(float   bboxDiagonal   READ bboxDiagonal   NOTIFY meshChanged)
    Q_PROPERTY(bool    loaded         READ loaded         NOTIFY meshChanged)
    Q_PROPERTY(QString lastError      READ lastError      NOTIFY meshChanged)
    Q_PROPERTY(qint64  loadTimeMs     READ loadTimeMs     NOTIFY meshChanged)

public:
    explicit MeshModel(QObject *parent = nullptr);
    ~MeshModel() override;

    Q_INVOKABLE bool load(const QString &filename);
    Q_INVOKABLE bool loadUrl(const QUrl &url);
    Q_INVOKABLE void clear();

    QString name() const;
    QString source() const { return m_source; }
    int vertexCount() const;
    int faceCount() const;
    int meshCount() const;
    bool isTriangleMesh() const;
    QVector3D bboxMin() const;
    QVector3D bboxMax() const;
    QVector3D bboxCenter() const;
    float bboxDiagonal() const;
    bool loaded() const { return m_meshes != nullptr; }
    QString lastError() const { return m_lastError; }
    qint64 loadTimeMs() const { return m_loadTimeMs; }

    /// Internal access to the underlying cgmesh::VMeshes — used by
    /// CgreQuickItem to iterate sub-meshes and upload one VBO+IBO per
    /// Mesh. Returns nullptr when no file is loaded.
    VMeshes *internalMeshes() const noexcept { return m_meshes.get(); }

signals:
    void meshChanged();

private:
    std::unique_ptr<VMeshes> m_meshes;
    QString m_source;
    QString m_lastError;
    qint64  m_loadTimeMs = 0;

    // Aggregate bbox cached at load time — VMeshes does not expose a
    // per-collection bbox, so we union the sub-mesh bboxes ourselves.
    float m_bboxMin[3] = { 0.0f, 0.0f, 0.0f };
    float m_bboxMax[3] = { 0.0f, 0.0f, 0.0f };
    float m_diagonal   = 0.0f;
};
