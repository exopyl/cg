#include "orbit_camera.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr float kPi = 3.14159265358979323846f;

/// Marge appliquee au rayon, au cadrage comme aux plans de coupe : la sphere
/// circonscrite d'un modele touche ses extremes, un cadrage exact les colle au
/// bord de l'ecran et au plan de coupe.
constexpr float kRadiusMargin = 1.2f;

/// En deca, un axe de rotation n'a plus de direction exploitable apres
/// normalisation.
constexpr float kMinAxisLength = 1e-8f;

/// Distance minimale oeil-pivot : borne l'invariant d > 0.
constexpr float kMinDistance = 1e-6f;

/// En deca, sin(fovY/2) ne permet plus de deduire une distance de cadrage.
constexpr float kMinSinHalfFov = 1e-4f;

/// Rapport zNear/zFar plancher : en deca, la precision du tampon de profondeur
/// s'effondre.
constexpr float kNearFarRatio = 0.001f;

/// Reorthonormalise la partie 3x3 par Gram-Schmidt modifie, la troisieme ligne
/// etant reconstruite par produit vectoriel (donc det = +1).
///
/// Appliquee a CHAQUE composition : sans elle, 10^4 rotations incrementales en
/// simple precision portent ||R.R^t - I||_inf a 1.10e-05, au-dela du budget de
/// 1e-05 (test tu_cgmath_orbit_camera). Son cout est negligeable devant le
/// produit 4x4 qui la precede.
void Reorthonormalize(TMatrix4<float>& r)
{
    float row0[3] = { r.at(0, 0), r.at(0, 1), r.at(0, 2) };
    float row1[3] = { r.at(1, 0), r.at(1, 1), r.at(1, 2) };

    const float len0 = std::sqrt(row0[0] * row0[0] + row0[1] * row0[1] + row0[2] * row0[2]);
    if (len0 < kMinAxisLength)
        return;
    for (int i = 0; i < 3; ++i)
        row0[i] /= len0;

    const float dot = row1[0] * row0[0] + row1[1] * row0[1] + row1[2] * row0[2];
    for (int i = 0; i < 3; ++i)
        row1[i] -= dot * row0[i];

    const float len1 = std::sqrt(row1[0] * row1[0] + row1[1] * row1[1] + row1[2] * row1[2]);
    if (len1 < kMinAxisLength)
        return;
    for (int i = 0; i < 3; ++i)
        row1[i] /= len1;

    const float row2[3] = {
        row0[1] * row1[2] - row0[2] * row1[1],
        row0[2] * row1[0] - row0[0] * row1[2],
        row0[0] * row1[1] - row0[1] * row1[0]
    };

    for (int i = 0; i < 3; ++i)
    {
        r.at(0, i) = row0[i];
        r.at(1, i) = row1[i];
        r.at(2, i) = row2[i];
    }
}

} // namespace

BoundingSphere BoundingSphereOfBox(const float bboxMin[3], const float bboxMax[3])
{
    BoundingSphere sphere;
    float sumOfSquares = 0.f;
    for (int i = 0; i < 3; ++i)
    {
        sphere.center[i] = 0.5f * (bboxMin[i] + bboxMax[i]);
        const float extent = bboxMax[i] - bboxMin[i];
        sumOfSquares += extent * extent;
    }
    sphere.radius = 0.5f * std::sqrt(sumOfSquares);
    return sphere;
}

OrbitCamera::OrbitCamera()
    : m_pivot(0.f, 0.f, 0.f)
    , m_distance(1.f)
    , m_rotation()
    , m_fovY(kPi / 4.f)
{}

void OrbitCamera::SetDistance(float distance)
{
    m_distance = std::max(distance, kMinDistance);
}

void OrbitCamera::SetFovY(float fovYRadians)
{
    m_fovY = fovYRadians;
}

void OrbitCamera::SetRotation(const float m[16])
{
    m_rotation = TMatrix4<float>(m);
}

TMatrix4<float> OrbitCamera::GetViewMatrix() const
{
    TMatrix4<float> view;

    // Partie 3x3 : R inchangee.
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 3; ++col)
            view.at(row, col) = m_rotation.at(row, col);

    // Translation : -R.c, puis le recul de d le long de -Z de l'oeil.
    for (int row = 0; row < 3; ++row)
    {
        view.at(row, 3) = -(m_rotation.at(row, 0) * m_pivot.x
                          + m_rotation.at(row, 1) * m_pivot.y
                          + m_rotation.at(row, 2) * m_pivot.z);
    }
    view.at(2, 3) -= m_distance;

    view.at(3, 0) = 0.f;
    view.at(3, 1) = 0.f;
    view.at(3, 2) = 0.f;
    view.at(3, 3) = 1.f;

    return view;
}

TVector3<float> OrbitCamera::GetEyePosition() const
{
    return TVector3<float>(
        m_pivot.x + m_distance * m_rotation.at(2, 0),
        m_pivot.y + m_distance * m_rotation.at(2, 1),
        m_pivot.z + m_distance * m_rotation.at(2, 2));
}

void OrbitCamera::RayFromNdc(float ndcX, float ndcY, float aspectRatio,
                             float origin[3], float direction[3]) const
{
    if (!origin || !direction)
        return;

    const TVector3<float> eye = GetEyePosition();
    origin[0] = eye.x;
    origin[1] = eye.y;
    origin[2] = eye.z;

    // Direction en espace OEIL, sur le plan z = -1.
    const float tanHalfFov = std::tan(m_fovY * 0.5f);
    const float dirEye[3] = { ndcX * tanHalfFov * aspectRatio,
                              ndcY * tanHalfFov,
                              -1.f };

    // Monde = R^t . dirEye (une direction ignore la translation de la vue) :
    // la composante i vaut somme_k R(k,i).dirEye[k].
    float dirWorld[3];
    for (int i = 0; i < 3; ++i)
    {
        dirWorld[i] = m_rotation.at(0, i) * dirEye[0]
                    + m_rotation.at(1, i) * dirEye[1]
                    + m_rotation.at(2, i) * dirEye[2];
    }

    // R est orthonormale et dirEye a une composante z de -1 : la norme vaut au
    // moins 1, la division ne peut pas degenerer.
    const float length = std::sqrt(dirWorld[0] * dirWorld[0]
                                 + dirWorld[1] * dirWorld[1]
                                 + dirWorld[2] * dirWorld[2]);
    for (int i = 0; i < 3; ++i)
        direction[i] = dirWorld[i] / length;
}

void OrbitCamera::Orbit(const float axis[3], float angleRad)
{
    const float length = std::sqrt(axis[0] * axis[0] + axis[1] * axis[1] + axis[2] * axis[2]);
    if (length < kMinAxisLength)
        return;

    const float x = axis[0] / length;
    const float y = axis[1] / length;
    const float z = axis[2] / length;

    const float c = std::cos(angleRad);
    const float s = std::sin(angleRad);
    const float t = 1.f - c;

    // Rodrigues : Rot = I.cos + (1-cos).aa^T + sin.[a]x
    TMatrix4<float> rot;
    rot.at(0, 0) = c + x * x * t;     rot.at(0, 1) = x * y * t - z * s; rot.at(0, 2) = x * z * t + y * s;
    rot.at(1, 0) = y * x * t + z * s; rot.at(1, 1) = c + y * y * t;     rot.at(1, 2) = y * z * t - x * s;
    rot.at(2, 0) = z * x * t - y * s; rot.at(2, 1) = z * y * t + x * s; rot.at(2, 2) = c + z * z * t;

    // Pre-multiplication : l'axe est lu en espace ecran, pas en espace monde.
    m_rotation = rot * m_rotation;

    Reorthonormalize(m_rotation);
}

void OrbitCamera::PanScreen(float dx, float dy, int viewportHeight)
{
    if (viewportHeight <= 0)
        return;

    const float k = 2.f * m_distance * std::tan(m_fovY * 0.5f)
                  / static_cast<float>(viewportHeight);

    const float sx = -k * dx;
    const float sy = k * dy;

    // R^t . (sx, sy, 0) : la composante i vaut somme_k R(k,i).screen[k].
    m_pivot.x += m_rotation.at(0, 0) * sx + m_rotation.at(1, 0) * sy;
    m_pivot.y += m_rotation.at(0, 1) * sx + m_rotation.at(1, 1) * sy;
    m_pivot.z += m_rotation.at(0, 2) * sx + m_rotation.at(1, 2) * sy;
}

void OrbitCamera::Frame(const float bboxMin[3], const float bboxMax[3])
{
    const BoundingSphere sphere = BoundingSphereOfBox(bboxMin, bboxMax);
    if (sphere.radius <= 0.f)
        return;

    m_pivot.Set(sphere.center[0], sphere.center[1], sphere.center[2]);

    const float sinHalfFov = std::sin(m_fovY * 0.5f);
    const float distance = (sinHalfFov > kMinSinHalfFov ? sphere.radius / sinHalfFov
                                                        : sphere.radius * 3.f);
    SetDistance(distance * kRadiusMargin);
}

void OrbitCamera::ClipPlanes(const float sceneCenter[3], float sceneRadius,
                             float* zNear, float* zFar) const
{
    if (!zNear || !zFar)
        return;

    const TVector3<float> eye = GetEyePosition();
    const float ex = eye.x - sceneCenter[0];
    const float ey = eye.y - sceneCenter[1];
    const float ez = eye.z - sceneCenter[2];
    const float distToScene = std::sqrt(ex * ex + ey * ey + ez * ez);

    const float margin = kRadiusMargin * std::fabs(sceneRadius);

    *zFar = distToScene + margin;
    *zNear = std::max(distToScene - margin, *zFar * kNearFarRatio);
}
