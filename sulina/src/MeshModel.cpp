// ⚠ LES EN-TETES WIN32 VIENNENT EN PREMIER, et ce n'est pas cosmetique.
//
// `vmeshes.h` porte un `using namespace std;` au perimetre global (releve dans
// debt_cgmesh.md). Une fois cette declaration active, le `byte` que <rpcndr.h>
// et <wtypes.h> definissent devient ambigu avec std::byte, et ce sont les
// en-tetes du SDK Windows qui refusent de compiler -- pas notre code. Les
// inclure avant suffit : le typedef de Windows est alors deja pose.
//
// WIN32_LEAN_AND_MEAN et NOMINMAX pour la raison habituelle : ne pas tirer
// winsock ni les macros min/max dans une unite qui manipule des maillages.
#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  define NOMINMAX
#  include <windows.h>
#  include <commdlg.h>
#endif

#include "MeshModel.h"

#include "mesh.h"
#include "vmeshes.h"
#include "vmeshes_io.h"

#include <QDir>
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

    if (!VMeshesIO::load(*fresh, local.constData()) || fresh->GetNVertices() == 0)
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

// ---------------------------------------------------------------------------
//  Selecteur de fichier NATIF
// ---------------------------------------------------------------------------
//
// Ecrit a la main plutot que via QFileDialog : celui-ci vit dans QtWidgets, que
// sulina ne lie pas et qui pese 6,3 Mo a lui seul -- soit plus que le module
// QtQuick.Dialogs qu'on retire. Passer par Widgets aurait deplace le poids, pas
// supprime.
//
// ⚠ PORTEE : implante pour Win32 seulement. sulina est aujourd'hui une cible
// Windows (ENABLE_SULINA est Off dans tous les presets, la CI Linux ne le
// construit pas, et le deploiement passe par windeployqt). Ailleurs, la fonction
// rend false sans rien ouvrir plutot que de ne pas compiler : le jour ou une
// cible Linux apparait, c'est ici qu'il faut brancher GTK/portal, et l'absence
// se verra a l'usage, pas au lien.

bool MeshModel::loadFromFileDialog(const QString &startDir)
{
#ifdef _WIN32
    wchar_t file[MAX_PATH] = { 0 };

    const QString initial = QDir::toNativeSeparators(startDir);
    std::wstring initialDir = initial.toStdWString();

    OPENFILENAMEW ofn = {};
    ofn.lStructSize  = sizeof(ofn);
    ofn.hwndOwner    = nullptr;
    ofn.lpstrFilter  = L"Mesh files (*.obj;*.ply;*.stl;*.off;*.3ds;*.glb)\0"
                       L"*.obj;*.ply;*.stl;*.off;*.3ds;*.glb\0"
                       L"All files (*.*)\0*.*\0";
    ofn.lpstrFile    = file;
    ofn.nMaxFile     = MAX_PATH;
    ofn.lpstrTitle   = L"Open a mesh file";
    ofn.lpstrInitialDir = initialDir.empty() ? nullptr : initialDir.c_str();
    // EXPLORER pour la boite moderne ; FILEMUSTEXIST parce qu'on va la lire ;
    // NOCHANGEDIR parce qu'un selecteur n'a pas a deplacer le repertoire courant
    // du processus -- les tests et les chemins relatifs en dependent.
    ofn.Flags        = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST
                     | OFN_NOCHANGEDIR;

    if (!GetOpenFileNameW(&ofn))
        return false;   // annulation : lastError reste tel quel

    return load(QString::fromWCharArray(file));
#else
    Q_UNUSED(startDir);
    return false;
#endif
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
