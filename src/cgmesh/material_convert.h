#pragma once

//
// Conversions de modeles de materiau, en EXEMPLAIRE UNIQUE.
//
// La projection metallic-roughness -> Phong existait deja, ecrite a la main dans
// l'importeur 3DM (mesh_io_3dm.cpp). Toute deuxieme copie derive : c'est ici, et
// nulle part ailleurs, que la formule se lit et se corrige.
//
// DEUX ESPACES COLORIMETRIQUES, ET C'EST ICI QUE PASSE LEUR FRONTIERE
// -------------------------------------------------------------------
// MaterialPbr est le conteneur du format glTF : ses facteurs sont LINEAIRES,
// c'est ce que dit la specification et c'est ce qu'attend un moteur PBR
// (cgre2). Les materiaux Phong du depot, eux, sont en sRGB : MaterialColor
// porte des octets d'ecran, et l'ecrivain GLB applique srgbToLinear a leurs
// canaux avant de les ecrire (mesh_io_gltf.cpp).
//
// Les deux fonctions ci-dessous traversent donc cette frontiere, et portent la
// conversion. Sans elle, un aller-retour GLB assombrit les couleurs : l'export
// linearise, l'import ne delinearisait pas.
//

#include "material.h"
#include "material_pbr.h"

#include <memory>

namespace cgpbr {

// PBR -> Phong. IMPLEMENTATION UNIQUE de la projection.
//
// Sur les canaux, EN ESPACE LINEAIRE :
//
//   diffuse   = base * (1 - metallic)
//   specular  = base * metallic + 0.04 * (1 - metallic)     [F0 de Schlick]
//   ambient   = 0.2 * base
//
// ou `metallic` et `roughness` sont les facteurs EFFECTIFS : le facteur du
// materiau, multiplie par la moyenne de la carte metallic-roughness quand elle
// est presente (canal B pour le metallique, canal G pour la rugosite, comme
// dans le format). Un fichier qui omet les deux facteurs les laisse a 1 et
// confie tout a la carte -- le facteur seul y donnerait une diffuse NULLE. En
// l'absence de carte, les facteurs traversent inchanges.
//
// La moyenne porte sur TOUS les texels de la carte et se recalcule A CHAQUE
// APPEL. Un appelant qui projette souvent doit mettre le resultat en cache ;
// cette fonction n'en garde aucun.
//
// puis chacun est converti en sRGB par linearToSrgb. L'ALPHA et la brillance ne
// sont pas des signaux lumineux : ils traversent sans conversion.
//
//   shininess = (1 - roughness)^2
//
// /!\ shininess est une FRACTION dans [0,1], PAS un exposant OpenGL. Le rendu la
// multiplie par 128 (MaterialRenderer::GlShininess, src/cgre/material_renderer.cpp),
// et glMaterialf rend GL_INVALID_VALUE au-dela de 128 -- l'appel est alors IGNORE
// et l'objet herite de l'exposant du materiau precedent. Y ecrire un exposant
// reintroduirait exactement ce defaut.
//
// TYPE RENDU : MaterialTexture si la carte base_color est presente, sinon
// MaterialColorExt. Rendre inconditionnellement un MaterialColorExt PERDRAIT
// l'image -- un Duck.glb importe sortirait sans sa texture chez les quatre
// consommateurs de la projection. L'image est PARTAGEE, pas dupliquee.
//
// CE QUE LA PROJECTION JETTE, dans les deux cas, faute de champ ou l'ecrire :
//
//   - les cartes normal, occlusion et emissive, et la carte metallic_roughness
//     en tant qu'IMAGE -- seule sa moyenne survit, dans les scalaires ;
//   - alphaMode, alphaCutoff, doubleSided ;
//   - normalScale, occlusionStrength ;
//   - le jeu d'UV (uvSet) de la carte de couleur de base ;
//   - dans la branche MaterialTexture SEULEMENT, l'emission : la classe ne
//     porte pas de canal d'emission, la seule a n'en pas avoir.
//
// Aucun de ces attributs n'a de representation dans le modele de Phong du
// depot. Un consommateur qui en a besoin doit lire le MaterialPbr lui-meme.
std::unique_ptr<Material> toPhong (const MaterialPbr& src);

// Phong -> PBR. Inverse EXACT de toPhong sur les deux equations qui le
// permettent (base depuis la diffuse, rugosite depuis la brillance), approche
// par la luminance pour le facteur metallique -- exact aux deux extremites
// (dielectrique pur, metal pur), approche entre les deux.
//
// CETTE EXACTITUDE SUPPOSE UN MATERIAU SANS CARTE metallic-roughness. Quand la
// carte est la, toPhong projette les scalaires EFFECTIFS -- facteur multiplie
// par la moyenne de la carte -- et cette fonction les reconstruit tels quels :
// elle rend le scalaire effectif, PAS le facteur du fichier, et la carte
// elle-meme est perdue. Un aller-retour sur un tel materiau ne redonne donc
// pas la source.
//
// IDEMPOTENTE sur un MaterialPbr : la source en est alors une COPIE. Sans cette
// garde, un MaterialPbr ne serait reconnu par aucune des trois branches de
// lecture Phong et ressortirait NEUTRE -- diffuse blanche, metallic 0,
// roughness 1 -- en silence.
//
// L'IMAGE d'un MaterialTexture n'est PAS reportee : la remonter demanderait de
// decider a quel emplacement PBR elle se branche et dans quel espace
// colorimetrique, ce qu'un materiau Phong ne dit pas. Seuls les facteurs
// traversent.
std::unique_ptr<MaterialPbr> fromPhong (const Material& src);

// Courbes sRGB <-> lineaire de la SPECIFICATION (et non l'approximation en
// puissance 2,2). baseColorFactor et emissiveFactor sont definis dans l'espace
// LINEAIRE ; les couleurs de materiau du depot sont en sRGB.
double srgbToLinear (double c);
double linearToSrgb (double c);

} // namespace cgpbr
