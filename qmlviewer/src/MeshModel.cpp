#include "MeshModel.h"

#include "mesh.h"

#include <QElapsedTimer>
#include <QFileInfo>
#include <QUrl>

MeshModel::MeshModel(QObject *parent)
    : QObject(parent)
{
}

MeshModel::~MeshModel() = default;

bool MeshModel::load(const QString &filename)
{
    QElapsedTimer timer;
    timer.start();

    auto fresh = std::make_unique<Mesh>();
    fresh->m_name = QFileInfo(filename).completeBaseName().toStdString();

    const QByteArray local = filename.toLocal8Bit();
    fresh->load(local.constData());

    if (fresh->GetNVertices() == 0)
    {
        m_lastError  = QStringLiteral("Failed to load '%1' (no vertices read)").arg(filename);
        m_source     = filename;
        m_loadTimeMs = timer.elapsed();
        m_mesh.reset();
        emit meshChanged();
        return false;
    }

    fresh->computebbox();

    m_mesh       = std::move(fresh);
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
    if (!m_mesh && m_source.isEmpty() && m_lastError.isEmpty())
        return;
    m_mesh.reset();
    m_source.clear();
    m_lastError.clear();
    m_loadTimeMs = 0;
    emit meshChanged();
}

QString MeshModel::name() const
{
    return m_mesh ? QString::fromStdString(m_mesh->m_name) : QString();
}

int MeshModel::vertexCount() const
{
    return m_mesh ? static_cast<int>(m_mesh->GetNVertices()) : 0;
}

int MeshModel::faceCount() const
{
    return m_mesh ? static_cast<int>(m_mesh->GetNFaces()) : 0;
}

bool MeshModel::isTriangleMesh() const
{
    return m_mesh ? m_mesh->IsTriangleMesh() : false;
}

QVector3D MeshModel::bboxMin() const
{
    if (!m_mesh) return {};
    const auto &bb = m_mesh->bbox();
    return { bb.GetMinX(), bb.GetMinY(), bb.GetMinZ() };
}

QVector3D MeshModel::bboxMax() const
{
    if (!m_mesh) return {};
    const auto &bb = m_mesh->bbox();
    return { bb.GetMaxX(), bb.GetMaxY(), bb.GetMaxZ() };
}

QVector3D MeshModel::bboxCenter() const
{
    if (!m_mesh) return {};
    float c[3] = {0.f, 0.f, 0.f};
    m_mesh->bbox().GetCenter(c);
    return { c[0], c[1], c[2] };
}

float MeshModel::bboxDiagonal() const
{
    return m_mesh ? m_mesh->bbox().GetDiagonalLength() : 0.f;
}
