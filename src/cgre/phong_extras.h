#pragma once

#include "material_renderer.h"

namespace cgre
{

//
// CE QUE LE MODE « MATERIAUX SANS PBR » TIRE D'UN MATERIAU PBR, en plus de sa
// projection de Phong (cgpbr::toPhong).
//
// La projection est un materiau du pipeline fixe : elle ne sait porter qu'une
// couleur par canal et une texture diffuse. Tout ce qu'elle jette et que le
// programme de surface sait neanmoins rendre a peu de frais passe par ici, puis
// par les uniformes de sa variante « extras » (cf. PhongExtrasProgram) :
//
//   - l'EMISSION, facteur ET carte. La projection texturee la perdait
//     entierement (MaterialTexture n'a pas de canal d'emission) ;
//   - la carte d'OCCLUSION, appliquee aux seuls termes AMBIANTS -- c'est la
//     semantique de glTF : elle attenue la lumiere indirecte, pas la directe ;
//   - la carte de NORMALES, par une base tangente tiree des DERIVEES ECRAN
//     (aucun attribut tangente, donc aucun risque d'alias sur gl_Vertex) ;
//   - la DECOUPE ALPHA du mode `mask`.
//
// Fonction PURE, sans aucun appel GL : c'est ce qui la rend testable hors
// contexte, et ce qui garde les regles de decision a un seul endroit.
//
struct PhongExtras
{
	//! Facteur emissif LINEAIRE (glTF), borne a >= 0. Nul : pas d'emission.
	float emissive[3]       = { 0.f, 0.f, 0.f };
	bool  useEmissiveMap    = false;

	bool  useOcclusionMap   = false;
	float occlusionStrength = 0.f;   //!< dans [0,1]

	bool  useNormalMap      = false;
	float normalScale       = 1.f;

	//! Seuil de decoupe alpha ; NEGATIF quand le mode n'est pas `mask`.
	float alphaCutoff       = -1.f;
};

//! `hasTexCoords` : le maillage televerse-t-il un jeu d'UV ? Sans lui aucune
//! carte n'est echantillonnable. Un materiau non PBR rend l'etat neutre (tout
//! eteint) : il n'a rien de plus que ce que le programme de surface lit deja.
PhongExtras MakePhongExtras (const MaterialRenderer::MaterialPbrInfo& info, bool hasTexCoords);

} // namespace cgre
