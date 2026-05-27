#include "MeshModel.h"

#include "mesh.h"
#include "vmeshes.h"

#include <QElapsedTimer>
#include <QFileInfo>
#include <QUrl>

#include <algorithm>
#include <cmath>

MeshModel::MeshModel(QObject *parent)
    : QObject(parent)
{
}

MeshModel::~MeshModel() = default;

bool MeshModel::load(const QString &filename)
{
    QElapsedTimer timer;
    timer.start();

    auto fresh = std::make_unique<VMeshes>();
    const QByteArray local = filename.toLocal8Bit();

    if (!fresh->load(local.constData()) || fresh->GetNVertices() == 0)
    {
        m_lastError  = QStringLiteral("Failed to load '%1' (no vertices read)").arg(filename);
        m_source     = filename;
        m_loadTimeMs = timer.elapsed();
        m_meshes.reset();
        m_bboxMin[0] = m_bboxMin[1] = m_bboxMin[2] = 0.0f;
        m_bboxMax[0] = m_bboxMax[1] = m_bboxMax[2] = 0.0f;
        m_diagonal   = 0.0f;
        emit meshChanged();
        return false;
    }

    // Compute aggregate bbox by unioning sub-mesh bboxes.
    bool first = true;
    for (Mesh *mesh : fresh->GetMeshes()) {
        if (!mesh || mesh->GetNVertices() == 0)
            continue;
        mesh->computebbox();
        const auto &bb = mesh->bbox();
        if (first) {
            m_bboxMin[0] = bb.GetMinX(); m_bboxMin[1] = bb.GetMinY(); m_bboxMin[2] = bb.GetMinZ();
            m_bboxMax[0] = bb.GetMaxX(); m_bboxMax[1] = bb.GetMaxY(); m_bboxMax[2] = bb.GetMaxZ();
            first = false;
        } else {
            m_bboxMin[0] = std::min(m_bboxMin[0], bb.GetMinX());
            m_bboxMin[1] = std::min(m_bboxMin[1], bb.GetMinY());
            m_bboxMin[2] = std::min(m_bboxMin[2], bb.GetMinZ());
            m_bboxMax[0] = std::max(m_bboxMax[0], bb.GetMaxX());
            m_bboxMax[1] = std::max(m_bboxMax[1], bb.GetMaxY());
            m_bboxMax[2] = std::max(m_bboxMax[2], bb.GetMaxZ());
        }
    }
    const float dx = m_bboxMax[0] - m_bboxMin[0];
    const float dy = m_bboxMax[1] - m_bboxMin[1];
    const float dz = m_bboxMax[2] - m_bboxMin[2];
    m_diagonal = std::sqrt(dx * dx + dy * dy + dz * dz);

    m_meshes     = std::move(fresh);
    m_source     = filename;
    m_lastError.clear();
    m_loadTimeMs = timer.elapsed();
    emit meshChanged();
    return true;
}

bool MeshModel::loadUrl(const QUrl &url)
{
    return load(url.isLocalFile() ? url.toLocalFile() : url.toString());
}

void MeshModel::clear()
{
    if (!m_meshes && m_source.isEmpty() && m_lastError.isEmpty())
        return;
    m_meshes.reset();
    m_source.clear();
    m_lastError.clear();
    m_loadTimeMs = 0;
    m_bboxMin[0] = m_bboxMin[1] = m_bboxMin[2] = 0.0f;
    m_bboxMax[0] = m_bboxMax[1] = m_bboxMax[2] = 0.0f;
    m_diagonal   = 0.0f;
    emit meshChanged();
}

QString MeshModel::name() const
{
    return m_meshes ? QFileInfo(m_source).completeBaseName() : QString();
}

int MeshModel::vertexCount() const
{
    return m_meshes ? static_cast<int>(m_meshes->GetNVertices()) : 0;
}

int MeshModel::faceCount() const
{
    return m_meshes ? static_cast<int>(m_meshes->GetNFaces()) : 0;
}

int MeshModel::meshCount() const
{
    return m_meshes ? static_cast<int>(m_meshes->GetNMeshes()) : 0;
}

bool MeshModel::isTriangleMesh() const
{
    return m_meshes ? m_meshes->IsTriangleMesh() : false;
}

QVector3D MeshModel::bboxMin() const
{
    if (!m_meshes) return {};
    return { m_bboxMin[0], m_bboxMin[1], m_bboxMin[2] };
}

QVector3D MeshModel::bboxMax() const
{
    if (!m_meshes) return {};
    return { m_bboxMax[0], m_bboxMax[1], m_bboxMax[2] };
}

QVector3D MeshModel::bboxCenter() const
{
    if (!m_meshes) return {};
    return { 0.5f * (m_bboxMin[0] + m_bboxMax[0]),
             0.5f * (m_bboxMin[1] + m_bboxMax[1]),
             0.5f * (m_bboxMin[2] + m_bboxMax[2]) };
}

float MeshModel::bboxDiagonal() const
{
    return m_meshes ? m_diagonal : 0.0f;
}
