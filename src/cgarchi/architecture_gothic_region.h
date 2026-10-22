#pragma once

#include <vector>

#include <cgmath/cgmath.h>

#include "extrude_contours.h"   // ExtrudeContour
#include "polygon2.h"

namespace cgarchi
{

//
// Region de pierre d'une baie gothique, calculee par UN SEUL booleen 2D :
//
//     pierre = cadre - (U vides  U  U_k (fenetre_k inter U sous-vides_k))
//
// Havemann (these, p. 246) : les anneaux d'un meme plan ne doivent ni se toucher
// ni partager un sommet ; les ecoincons sont « ce qui reste du plan de face une
// fois le cercle et les sous-arcs retires ». La pierre est donc la difference
// du cadre et de la reunion des vides, jamais une ouverture percee dans laquelle
// on reposerait des ilots de pierre : ces ilots partagent exactement les aretes
// de l'ouverture (appui, jambages), et le tessellateur y cree des sommets sans
// paroi.
//
// Entree :
//   - frame : silhouette exterieure, anneau simple, lu comme plein quel que
//     soit son sens ;
//   - voids : anneaux simples, chacun lu comme plein quel que soit son sens ;
//     ils peuvent se recouvrir, la reunion les fond ;
//   - clippedVoids : sous-vides d'une ouverture recursive. Seule la partie de
//     leur reunion interieure a la fenetre (l'ouverture de la lancette) est
//     videe ; la fenetre elle-meme n'est pas un contour de la sortie.
//
// Sortie : anneaux dans la convention de Clipper2 -- enveloppes en sens
// trigonometrique (aire positive), trous en sens horaire --, soit l'entree
// qu'attendent glutess (NONZERO) et ValidateContours (OuterCcwHoleCw). Le
// booleen supprime les aretes communes que la construction impose (ouverture
// et pierre reposee dedans), mais ne SEPARE pas des vides tangents en entree :
// deux anneaux peuvent alors partager un sommet, un anneau se pincer, et
// l'accrochage a la grille peut changer une quasi-tangence en croisement de
// deux trous. La disjonction n'est donc garantie que pour des vides disjoints
// d'au moins un pas de grille ; ValidateContours la verifie, et la garde
// morphologique (ci-dessous) traite ce qu'il signale, sans le garantir : une
// region peut rester invalide apres la garde. Une enveloppe
// posee dans un trou (ilot) est rendue comme une enveloppe : sa connexion au
// reste n'est pas garantie, c'est le compte de composantes du maillage qui la
// mesure.
//
// Precision : les coordonnees sont accrochees a la grille de ClipperD, dont le
// pas est la puissance de deux 2^-(ilogb(10^precision) + 1) : 2^-7 = 1/128 pour
// precision = 2, et non 10^-2 (InflatePaths, lui, accroche a 10^-precision).
// Les booleens appeles en amont par le generateur (ecoincons, rosaces,
// mouchettes) emploient la precision par defaut de Clipper2, 2 :
// kGothicClipperPrecision la reprend, pour que les sommets d'une baie vivent
// sur la meme grille. Ce pas n'est pas covariant avec l'echelle de la baie.
//
// Les sommets colineaires sont retires de la sortie : un sommet pose sur une
// arete droite donnerait a la tessellation un triangle d'aire nulle.
//
// Garde morphologique, a la demande : si la region calculee a un defaut de
// contour (ValidateContours, tolerance par defaut), elle est recalculee apres une
// ouverture puis une fermeture de la reunion des vides, de rayon guardGridSteps
// pas de grille, jonctions rondes, sur la grille du booleen. L'ouverture retire
// les cheveux et pointes de vide plus fins que deux rayons ; la fermeture
// rejoint les vides separes par moins de deux rayons de pierre, ce qui traite
// les contacts et pincements entre vides tangents. Elle arrondit aussi les
// angles saillants des vides (rayon guardGridSteps pas) : c'est pourquoi une
// region sans defaut sort inchangee.
//
// La region gardee est revalidee. Si elle garde un defaut, la fermeture seule
// est essayee : l'ouverture peut recreer un contact entre une pointe de vide et
// son voisin. Si la fermeture seule est valide, elle est retenue ; sinon la
// region de l'ouverture suivie de la fermeture est rendue, invalide (garde
// insuffisante : un vide tangent au cadre, par exemple, n'est decolle par
// aucune des deux).
//
constexpr int kGothicClipperPrecision = 2;
constexpr int kGothicGuardGridSteps   = 2;

// Issue de la garde morphologique.
enum class GothicGuardOutcome
{
	NotNeeded,         // region valide sans garde (ou garde desactivee, ou region vide)
	Applied,           // ouverture puis fermeture : region valide
	ClosingFallback,   // l'ouverture puis la fermeture laissaient un defaut ; fermeture seule, valide
	Insufficient,      // aucune des deux n'est valide ; ouverture puis fermeture rendue
};

using GothicRing = std::vector<Vector2d>;

struct GothicClippedVoids
{
	GothicRing              window;   // anneau simple, lu comme plein
	std::vector<GothicRing> voids;    // anneaux simples, lus comme pleins
};

struct GothicStoneInput
{
	GothicRing                      frame;
	std::vector<GothicRing>         voids;
	std::vector<GothicClippedVoids> clippedVoids;
};

// Leve std::invalid_argument si precision est hors de la plage de Clipper2
// ([-8, 8]) ou si guardGridSteps est negatif (0 : sans garde). Un anneau de
// moins de 3 points est ignore. Rend un jeu vide si le cadre est vide ou
// entierement evide. *guardApplied, s'il est fourni, dit si la garde a ete
// appliquee (toute issue autre que NotNeeded) ; *outcome, s'il est fourni, en
// donne l'issue.
std::vector<GothicRing> buildGothicStoneRegion (const GothicStoneInput &input,
                                                int precision = kGothicClipperPrecision,
                                                int guardGridSteps = kGothicGuardGridSteps,
                                                bool *guardApplied = nullptr,
                                                GothicGuardOutcome *outcome = nullptr);

// Adaptateurs vers les deux entrees d'extrusion du depot. L'ordre et le sens
// des anneaux sont conserves ; Polygon2 est en float.
Polygon2                    gothicRegionToPolygon2 (const std::vector<GothicRing> &region);
// isHole = aire signee negative, soit le role que lui donne la convention de
// sortie ci-dessus.
std::vector<ExtrudeContour> gothicRegionToContours (const std::vector<GothicRing> &region);

} // namespace cgarchi
