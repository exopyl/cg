#pragma once
//
// Reconstruction 3D à partir d'images — TYPES DE DONNÉES PARTAGÉS (Phase 0).
//
// Contrats uniquement : aucune logique d'algorithme ici. Tout le code de
// reconstruction vit dans cgmesh (pas de module séparé). Voir reconstruction.md.
//
// Ces types sont les « valeurs » qui transitent entre les étapes du pipeline ;
// les étapes elles-mêmes sont déclarées dans recon_interfaces.h.
//
#include <cstddef>
#include <vector>

namespace recon {

// Carte de profondeur dense, une par vue. Indexation : z[y*w + x].
struct DepthMap
{
    int                w = 0;
    int                h = 0;
    std::vector<float> z;                 // profondeur ou disparité (cf. isDisparity)
    bool               isDisparity = true; // true : valeur ~ 1/profondeur (Depth Anything)
};

// Caméra calibrée (modèle sténopé) + pose monde->caméra.
// Adaptable depuis cgmesh/BundleCamera (f_pxl, R, T, k1, k2).
struct CameraView
{
    int   w = 0, h = 0;                    // dimensions image (px)
    float fx = 0.f, fy = 0.f;              // focales (px)
    float cx = 0.f, cy = 0.f;              // point principal (px)
    float k1 = 0.f, k2 = 0.f;              // distorsion radiale
    float R[9] = {1,0,0, 0,1,0, 0,0,1};    // rotation monde->caméra (row-major)
    float T[3] = {0,0,0};                  // translation monde->caméra
};

// Nuage de points. Conteneur propre : PointSet n'expose pas de setter
// normales/couleurs (cf. reconstruction.md §6). normals/colors sont vides ou
// de taille 3*size().
struct PointCloud
{
    std::vector<float> positions;          // 3*n : x,y,z
    std::vector<float> normals;            // 0 ou 3*n
    std::vector<float> colors;             // 0 ou 3*n : r,g,b dans [0,1]

    // Nuage « organisé » (grille) : width*height == size(), points en row-major
    // (index = gy*width + gx), validité par point (vide = tous valides). Permet une
    // reconstruction par grille (heightmap). Un nuage non organisé laisse width=height=0.
    int                        width = 0, height = 0;
    std::vector<unsigned char> valid;

    std::size_t size() const { return positions.size() / 3; }
    bool        hasNormals() const { return normals.size() == positions.size() && !normals.empty(); }
    bool        hasColors()  const { return colors.size()  == positions.size() && !colors.empty(); }
    bool        organized() const { return width > 0 && height > 0 && (std::size_t)width * height == size(); }
    bool        isValid(std::size_t i) const { return valid.empty() || (i < valid.size() && valid[i]); }
};

// Résultat de l'étape d'évaluation (étape 11).
struct Metrics
{
    float hausdorff         = 0.f;   // max symétrique (distance de Hausdorff)
    float hausdorffRelative = 0.f;   // hausdorff / diagonale bbox (sans échelle)
    float meanError         = 0.f;   // erreur moyenne symétrique
    float rmsError          = 0.f;   // erreur RMS symétrique
    float p95Error          = 0.f;   // 95e percentile (robuste aux aberrations)
};

} // namespace recon
