#pragma once
//
// Reconstruction — étape [8], variante : maillage heightmap (mono-vue).
//
#include "recon_interfaces.h"

class Mesh;

namespace recon {

// Reconstruit un mesh depuis un nuage ORGANISÉ (grille width×height) : 2 triangles
// par cellule, sauf cellules à coin invalide ou à discontinuité de profondeur (arête
// bien plus longue que la médiane → bord objet/fond). Préserve le détail, contrairement
// au champ implicite — idéal pour une seule vue. Renvoie nullptr si le nuage n'est pas
// organisé.
class HeightmapReconstructor : public ISurfaceReconstructor
{
public:
    float edgeFactor = 4.f;   // coupe une cellule si une arête > edgeFactor × médiane

    Mesh* reconstruct(const PointCloud& cloud) override;
};

} // namespace recon
