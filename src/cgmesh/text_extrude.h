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

#include <cstddef>
#include <string>
#include <vector>

// Pour ExtrudeContour, que text_to_contours rend. L'en-tete ne tire que les
// types de contour, pas le constructeur de maillage.
#include "extrude_contours.h"

#include <cgmath/text_layout.h>   // TextAlign

class Font;
class Mesh;

struct TextExtrudeOptions
{
	// Ce que `size` MESURE. Deux references, et elles ne coincident pour aucune
	// police : l'em vaut ~1,4 fois la hauteur de capitale.
	//
	//   Em        -- le corps typographique. Deux polices au meme `size` NE
	//                rendent PAS des lettres de meme hauteur.
	//   CapHeight -- la hauteur des capitales, mesuree depuis la ligne de base.
	//                C'est la cote qu'un humain designe quand il dit « lettres de
	//                30 mm », et la seule qui se verifie a la regle sur la piece
	//                imprimee.
	//
	// La conversion a lieu ICI et pas dans text_layout, qui ne connait que
	// IGlyphMetrics -- contrat minimal, sans hauteur de capitale (cf. font.h).
	// Cette couche tient une vraie police, donc elle peut convertir ; le
	// compositeur, non.
	//
	// ⚠ DEGRADATION : une police dont aucune capitale a sommet plat n'est
	// lisible rend `Font::capHeight() == 0`. Le mode retombe alors sur l'em --
	// la cote demandee n'est plus honoree, et rien d'autre ne pouvait l'etre.
	enum class SizeMode { Em, CapHeight };
	SizeMode sizeMode = SizeMode::Em;

	// La cote demandee en unites monde, interpretee selon `sizeMode`.
	float size = 1.f;

	// Epaisseur extrudee, de z = 0 a z = depth.
	float depth = 0.2f;

	// Tolerance d'aplatissement des courbes, en UNITES MONDE (cf. l'en-tete).
	// A 1/100e du corps, un contour de glyphe est visuellement lisse.
	//
	// ZERO (ou negatif) = AUTOMATIQUE : `size / 600`. C'est la seule valeur qui
	// n'avait aucun sens -- une tolerance nulle subdiviserait sans fin -- d'ou sa
	// reaffectation plutot qu'un drapeau de plus.
	//
	// Le 600 n'est pas arbitraire : a une cote de 30 mm il donne 0,05 mm d'ecart
	// de corde, soit largement sous la buse (0,4 mm) et sous la couche (0,2 mm)
	// d'une imprimante FDM -- donc invisible sur la piece --, pour un budget
	// mesure de ~3 500 triangles sur neuf glyphes. Une tolerance absolue, elle,
	// ne peut pas suivre la cote : le defaut de 0,01 convenait a un corps de 1 et
	// produisait des contours inutilement denses a 30.
	float flattenTol = 0.f;

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

	// --- plaque de SUPPORT ---------------------------------------------------
	//
	// Un contour de PLUS dans l'union ci-dessus, jamais un second volume a
	// recoller : le depot n'a aucun booleen 3D sur maillages, et il n'en faut
	// pas ici. Demander un support ACTIVE l'union, meme si unionOverlaps est
	// faux -- sans elle le support et les lettres se recouvriraient sans
	// fusionner, et le solide ne serait pas etanche.
	//
	// ⚠ CONTRAINTE, et elle n'est pas adoucie : support et texte partagent la
	// profondeur d'extrusion. Il n'y a qu'un champ `depth`, et l'union 2D
	// produit une region PLANE unique, portee de z = 0 a z = depth. Un socle
	// plus epais que les lettres exige un booleen 3D, c'est-a-dire une capacite
	// nouvelle -- pas un reglage de plus.
	//
	// ⚠ CONSEQUENCE MESUREE, et il faut la connaitre avant de choisir :
	// `Plate` contient l'emprise du texte, donc a profondeur egale l'union rend
	// la SILHOUETTE de la plaque et les lettres disparaissent dedans. C'est
	// exact, ce n'est pas une panne, et c'est ce que « meme profondeur » veut
	// dire. Les deux formes qui gardent les lettres lisibles sont celles qui ne
	// les couvrent pas : `Bar` (un bandeau qui mord le bas des lettres et les
	// relie en une seule piece) et `Frame` (un cadre qui les entoure).
	enum class Support
	{
		None,
		Plate,    // rectangle plein sous le texte
		Bar,      // bandeau horizontal, mordant le bas des lettres
		Frame     // cadre rectangulaire, matiere sur son seul pourtour
	};
	Support support = Support::None;

	// Ecart entre l'emprise du texte et le bord du support, en unites monde.
	float supportMargin = 0.f;

	// Bar / Frame : epaisseur de matiere, en unites monde. Sans effet sur Plate,
	// qui est plein.
	float supportThickness = 0.1f;

	// Bar : hauteur a laquelle le bandeau MORD dans les lettres, en unites
	// monde, mesuree au-dessus du bas de l'emprise. Zero laisse le bandeau
	// tangent -- donc une union qui ne fusionne rien de fiable.
	float supportOverlap = 0.02f;

	// Plate / Frame : rayon d'arrondi des coins EXTERIEURS. Zero = coins vifs.
	float supportCornerRadius = 0.f;

	// Recentre l'emprise typographique sur l'origine. Utile a une interface qui
	// place l'objet dans une scene ; laisse a false, l'origine du maillage est
	// celle de la premiere ligne de base.
	bool centerOnOrigin = false;

	// Estampille chaque face (cf. Mesh::Material_Add). Le defaut correspond a
	// MaterialType::MATERIAL_NONE.
	unsigned int materialId = (unsigned int)-1;
};

// Declare par cgmath, a la base de la chaine : voir cgmath/context.h.
class Context;

// Ce que la passe a REELLEMENT execute. `glyphsFlattened` compte les
// aplatissements, un par glyphe DISTINCT : c'est la memoisation par glyphe qui
// se mesure ici, et rien d'autre. « MISSISSIPPI » place onze glyphes et n'en
// aplatit que quatre.
struct TextExtrudeStats
{
	std::size_t glyphsPlaced = 0;
	std::size_t glyphsFlattened = 0;
};

// Maillage alloue sur le tas (l'appelant en devient proprietaire), normales
// calculees. nullptr quand rien n'a pu etre produit : police invalide, texte
// vide, ou texte entierement compose de glyphes blancs.
//
// Un glyphe absent de la police degrade proprement -- il occupe son avance et
// n'emet aucun contour, comme une espace.
//
// stats optionnel : rempli quand la mise en page a produit au moins un glyphe.
//
// ctx optionnel, en DERNIER parametre : la boucle de placement des glyphes teste
// le jeton d'annulation et rend nullptr sans rien construire. Un appelant qui
// n'annule rien compile inchange.
// Contours 2D du texte, en unités monde, prêts à extruder — l'étage que
// text_to_extruded_mesh enchaîne en interne, rendu accessible pour qu'un autre
// consommateur puisse s'y brancher (un nœud de graphe, typiquement).
//
// ⚠ Les glyphes sont TOUJOURS fusionnés en une région unique (Clipper2,
// NonZero), contrairement à text_to_extruded_mesh qui ne le fait que sur demande
// ou quand un support l'impose. Une liste plate de contours ne peut pas porter
// le découpage par glyphe, et extruder d'un seul tenant des lettres non
// fusionnées laisserait des murs internes là où elles se touchent.
//
// `depth` et `materialId` ne sont pas lus : ce sont des réglages d'extrusion.
// Rend false sur police invalide, texte vide, contours vides ou annulation.
bool text_to_contours (const Font& font, const std::string& utf8,
                       const TextExtrudeOptions& opt,
                       std::vector<ExtrudeContour>& out,
                       TextExtrudeStats* stats = nullptr,
                       const Context* ctx = nullptr);

Mesh* text_to_extruded_mesh (const Font& font, const std::string& utf8,
                             const TextExtrudeOptions& opt,
                             TextExtrudeStats* stats = nullptr,
                             const Context* ctx = nullptr);
