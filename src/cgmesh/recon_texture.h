#pragma once
//
// Reconstruction — étape [10] : texturation projective.
//
#include "recon_interfaces.h"

class Mesh;

namespace recon {

// Pour chaque face : choisit la caméra la plus FRONTALE où la face se projette
// entièrement dans l'image, y projette les sommets (UV par face) et assigne l'image
// comme matériau-texture. Produit un mesh à matériaux + UV exportable en OBJ+MTL.
//
// Test d'occlusion (rayon d'ombre BVH barycentre->caméra) : une face n'est texturée
// que par une caméra qui la VOIT réellement, évitant le bleeding sur les concavités
// (intérieur, faces arrière, silhouettes). Faces sans caméra valide -> repli gris.
// v1 : pas encore de blending multi-vues (une seule caméra par face).
class ProjectiveTexturer : public ITexturer
{
public:
    bool occlusion = true;   // rejeter les caméras dont la face est occultée

    void texture(Mesh&                            mesh,
                 const std::vector<std::string>&  imagePaths,
                 const std::vector<CameraView>&   cameras) override;
};

} // namespace recon
