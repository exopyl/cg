#pragma once

// ============================================================================
//  Texte -> solide extrude
// ============================================================================
//
// Le dernier maillon de la chaine, et le seul qui vive en cgmesh : tout ce qui
// precede produit de la geometrie 2D et tient en cgmath.
//
//   font.h          police -> contours de glyphes, en unites de police
//   text_layout.h   UTF-8  -> glyphes places, en unites monde
//   ICI             mise a l'echelle -> aplatissement -> ExtrudeContour -> Mesh
//   bezier_flatten.h        (l'aplatissement proprement dit)
//
// Point de conception a ne pas rater : l'aplatissement a lieu APRES la mise a
// l'echelle, sur les coordonnees monde finales. `flattenTol` s'exprime donc
// dans les unites du maillage produit, et la finesse des courbes est la meme
// quelle que soit la police -- em de 1000 ou de 2048 -- et quelle que soit la
// taille demandee. import_svg fait l'inverse (tolerance en pixels du document),
// ce qui rend la finesse dependante de l'echelle du fichier source.
//
// ============================================================================

#include <string>

#include "../cgmath/text_layout.h"   // TextAlign

class Font;
class Mesh;

struct TextExtrudeOptions
{
	// Hauteur d'em en unites monde : le corps typographique, pas la hauteur des
	// capitales. C'est le facteur d'echelle applique aux unites de police.
	float size = 1.f;

	// Epaisseur extrudee, de z = 0 a z = depth.
	float depth = 0.2f;

	// Tolerance d'aplatissement des courbes, en UNITES MONDE (cf. l'en-tete).
	// A 1/100e du corps, un contour de glyphe est visuellement lisse.
	float flattenTol = 0.01f;

	float     lineSpacing   = 1.f;
	float     letterSpacing = 0.f;
	TextAlign align         = TextAlign::Left;
	bool      kerning       = true;

	// Passe booleenne Clipper2 sur l'ENSEMBLE des glyphes avant tessellation.
	// Desactivee par defaut : elle coute cher et ne sert que quand les glyphes
	// se recouvrent reellement (interlettrage tres negatif, scripts cursifs).
	// Elle a un cout fonctionnel en plus du cout machine : les glyphes fondus en
	// une seule region ne peuvent plus porter de materiau distinct.
	bool unionOverlaps = false;

	// Recentre l'emprise typographique sur l'origine. Utile a une interface qui
	// place l'objet dans une scene ; laisse a false, l'origine du maillage est
	// celle de la premiere ligne de base.
	bool centerOnOrigin = false;

	// Estampille chaque face (cf. Mesh::Material_Add). Le defaut correspond a
	// MaterialType::MATERIAL_NONE.
	unsigned int materialId = (unsigned int)-1;
};

// Maillage alloue sur le tas (l'appelant en devient proprietaire), normales
// calculees. nullptr quand rien n'a pu etre produit : police invalide, texte
// vide, ou texte entierement compose de glyphes blancs.
//
// Un glyphe absent de la police degrade proprement -- il occupe son avance et
// n'emet aucun contour, comme une espace.
Mesh* text_to_extruded_mesh (const Font& font, const std::string& utf8,
                             const TextExtrudeOptions& opt);
