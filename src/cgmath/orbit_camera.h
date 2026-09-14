#pragma once

#include "TMatrix4.h"
#include "TVector3.h"

/// Sphere circonscrite a une boite alignee sur les axes.
struct BoundingSphere {
    float center[3] = { 0.f, 0.f, 0.f };
    float radius    = 0.f;
};

/// Sphere circonscrite d'une boite : centre au MILIEU, rayon = demi-diagonale.
///
/// Les deux spheres du cadrage -- celle qui fixe la distance d'oeil et celle
/// qui fixe les plans de coupe -- passent toutes deux par ici. Ecrite deux
/// fois, la formule autoriserait les deux spheres a diverger sans qu'aucune
/// mesure ne le dise.
///
/// Une boite reduite a un point rend un rayon nul : c'est a l'appelant de
/// decider ce que cela veut dire chez lui, la fonction ne tranche pas.
BoundingSphere BoundingSphereOfBox(const float bboxMin[3], const float bboxMax[3]);

/// Camera d'orbite : pivot, distance et orientation comme etat coherent.
///
/// L'etat se reduit a quatre grandeurs -- pivot monde, distance oeil-pivot,
/// rotation, champ vertical. Tout le reste (matrice de vue, position d'oeil,
/// plans de coupe) en derive, donc aucun couple d'accesseurs ne peut diverger.
///
/// La matrice de vue est composee explicitement :
///
///     V = T(0, 0, -d) . R . T(-c)
///
/// ce qui garantit V.c = (0, 0, -d) QUELLE QUE SOIT R : le pivot est sur l'axe
/// de vue, a la distance d, par construction. La formulation `lookAt` ne
/// l'offre pas -- elle perd le roulis et devient singuliere des que la
/// direction de vue est colineaire a `up`, c'est-a-dire sur une vue de dessus
/// dans un monde Z-up.
///
/// Aucune dependance graphique : la classe est du calcul pur et se teste hors
/// contexte GL.
class OrbitCamera {
public:

    // ========================================================================
    // Constructeur
    // ========================================================================

    /// Pivot a l'origine, distance unitaire, rotation identite, champ de 45 deg.
    OrbitCamera();

    // ========================================================================
    // Etat
    // ========================================================================

    const TVector3<float>& GetPivot() const { return m_pivot; }
    void SetPivot(const TVector3<float>& pivot) { m_pivot = pivot; }
    void SetPivot(float x, float y, float z) { m_pivot.Set(x, y, z); }

    float GetDistance() const { return m_distance; }

    /// La distance est strictement positive : une valeur nulle ou negative
    /// placerait l'oeil sur le pivot ou derriere lui, et la matrice de vue
    /// cesserait d'etre celle d'une orbite. Elle est donc bornee par le bas.
    void SetDistance(float distance);

    float GetFovY() const { return m_fovY; }

    /// Champ vertical en RADIANS.
    void SetFovY(float fovYRadians);

    /// Rotation courante : 4x4 colonne-majeur, orthonormale, sans translation.
    const TMatrix4<float>& GetRotation() const { return m_rotation; }

    /// Impose l'orientation depuis un tableau de 16 flottants colonne-majeur.
    /// Requis par les harnais qui ecrivent l'orientation directement.
    void SetRotation(const float m[16]);

    // ========================================================================
    // Grandeurs derivees
    // ========================================================================

    /// Matrice de vue V = T(0, 0, -d) . R . T(-c).
    TMatrix4<float> GetViewMatrix() const;

    /// Position de l'oeil : e = c + d.r3, r3 etant la troisieme LIGNE de la
    /// partie 3x3 de R (direction monde de l'axe +Z de l'oeil).
    TVector3<float> GetEyePosition() const;

    /// Rayon monde passant par le point (ndcX, ndcY) du plan image, exprime en
    /// coordonnees normalisees : -1 a gauche / en BAS, +1 a droite / en HAUT.
    /// C'est la convention d'OpenGL ; convertir depuis des pixels est le travail
    /// de l'appelant, qui seul connait son repere d'ecran.
    ///
    /// Operation INVERSE de la projection. Elle est ecrite en forme fermee et
    /// non par inversion de P.V, pour deux raisons :
    ///
    ///  - un rayon ne depend NI de zNear NI de zFar. Le frustum etant
    ///    symetrique, le point de NDC (u,v) est, en espace oeil, la direction
    ///    (u.tan(fovY/2).aspect , v.tan(fovY/2) , -1) quels que soient les
    ///    plans de coupe. Passer par l'inverse de la matrice ferait entrer dans
    ///    le resultat un rapport far/near qui vaut parfois 1000 ;
    ///  - l'origine est l'OEIL, donc le rayon couvre aussi ce qui se trouve
    ///    entre l'oeil et le plan near. Un rayon partant du plan near ferait
    ///    dependre le picking d'un plancher de profondeur que la scene deplace.
    ///
    /// `direction` est normalisee. `aspectRatio` est largeur / hauteur.
    void RayFromNdc(float ndcX, float ndcY, float aspectRatio,
                    float origin[3], float direction[3]) const;

    // ========================================================================
    // Manipulations
    // ========================================================================

    /// Rotation incrementale d'angle `angleRad` autour de `axis` exprime en
    /// espace ecran : R <- Rot(axis, angle) . R. Un axe degenere ne fait rien.
    ///
    /// R est reorthonormalisee apres chaque composition : la simple precision
    /// ne tient pas l'orthonormalite sur des dizaines de milliers de pas.
    void Orbit(const float axis[3], float angleRad);

    /// Deplace LE PIVOT d'un deplacement souris exprime en pixels :
    ///
    ///     k  = 2.d.tan(fovY/2) / H        (unites monde par pixel au pivot)
    ///     c += Rt . ( -k.dx , +k.dy , 0 )
    ///
    /// `viewportHeight` sert aux DEUX axes : les pixels sont carres.
    void PanScreen(float dx, float dy, int viewportHeight);

    /// Cadre une boite englobante : pivot au centre, distance telle que la
    /// sphere circonscrite tienne dans le champ vertical, avec 20 % de marge.
    /// Une boite degeneree laisse l'etat inchange.
    void Frame(const float bboxMin[3], const float bboxMax[3]);

    // ========================================================================
    // Plans de coupe
    // ========================================================================

    /// Plans de coupe pour une sphere de profondeur (centre, rayon) QUI N'EST
    /// PAS le pivot : le pivot suit le point d'interet, la sphere englobe ce
    /// qui doit rester visible. Les confondre fait sortir du far tout element
    /// excentre des que le pivot bouge.
    ///
    /// La mesure part de l'OEIL :
    ///
    ///     zFar  = ||e - s|| + 1.2.r_s
    ///     zNear = max( ||e - s|| - 1.2.r_s , zFar.0.001 )
    void ClipPlanes(const float sceneCenter[3], float sceneRadius,
                    float* zNear, float* zFar) const;

private:
    TVector3<float> m_pivot;
    float m_distance;
    TMatrix4<float> m_rotation;
    float m_fovY;
};
