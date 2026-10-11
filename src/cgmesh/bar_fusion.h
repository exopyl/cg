#pragma once

#include <cstdint>
#include <vector>

#include <cgmath/TVector2.h>

#include "profile2d.h"

class Mesh;
class Polygon2;

// ============================================================================
//  Fusion des barres de moulure -- niveaux 2D
// ============================================================================
//
// Une barre est un profil ferme balaye le long d'un contour d'ouverture
// (buildBayMoulding). Quand la coupe du profil a chaque cote u est une reunion
// d'intervalles de v, la plaque et ses barres forment un solide 2,5D : la
// section a la cote h est la reunion, sur les barres, des points dont la
// distance signee au contour est dans la coupe du profil a h. Les niveaux sont
// ces sections, calculees en entiers par Clipper2 ; la couture 3D les relie.
//
// REPERE. Profil : u = cote au-dessus de la face, u_min = pied de la barre ;
// v = decalage lateral, positif a DROITE du sens de parcours du contour -- vers
// la pierre pour une ouverture en sens trigonometrique, comme la normale
// (T.y, -T.x) du balayage de buildBayMoulding. Une barre d'echelle f porte le
// profil (u_min + (u - u_min) f, v f).
//
// GRILLE. Coordonnees entieres au pas 1 / kBarFusionGridScale = 1/128, celui
// de la region de pierre (buildGothicStoneRegion, precision 2) : les contours
// des barres et ceux de la pierre vivent sur les memes entiers. Les seuils
// ci-dessous sont en pas de cette grille.
//
// HAUTEURS. Cotes distinctes des sommets des profils mis a l'echelle, u_min
// compris ; un ecart plus grand que (u_haut - u_min) / kBarFusionMinLevelSteps
// est subdivise regulierement (u_haut : crete de la plus grande barre). La
// hauteur de barre compte donc au moins kBarFusionMinLevelSteps pas, sans
// reglage. Une seule tolerance de cote, 1e-6 fois la hauteur de la plus grande
// barre, vaut pour toutes les barres : deux cotes plus proches sont confondues
// (la plus basse est gardee). Le niveau 0 est l'empreinte (coupe juste
// au-dessus de u_min) ; une barre dont la crete est a cette tolerance du niveau
// y donne un ruban de +/- kBarFusionApexHalfWidthSteps autour des v de ses
// sommets de crete (cote mise a l'echelle a cette meme tolerance de la crete),
// meme si sa crete a ete confondue avec une cote voisine.
//
// CONSTRUCTION, dans l'ordre :
//   1. bandes : pour chaque intervalle [a, c] de la coupe, decalage rond du
//      contour a c f moins decalage a a f ; reunion des bandes de toutes les
//      barres ;
//   2. enveloppe, du haut vers le bas : R_k = R_k U R_k+1. Un profil a
//      contre-depouille est rempli vers la face ;
//   3. nettoyage de chaque niveau : simplification de Ramer-Douglas-Peucker a
//      kBarFusionSimplifySteps (chaque sommet retire est a au plus cette
//      distance de la corde gardee), puis ouverture morphologique de rayon
//      kBarFusionOpeningSteps ;
//   4. imbrication : R_k+1 = R_k+1 inter (R_k erode de kBarFusionNestingSteps,
//      tolerance d'arc 0,1 pas). Chaque niveau, R_0 compris, est noeude contre
//      lui-meme (au plus kBarFusionNodePasses passes) : un croisement de deux
//      aretes (arrondi a la grille) ou un sommet pose sur une autre arete
//      devient un sommet commun. Puis CONTROLE : R_k+1 n'a plus ni croisement
//      ni sommet pose sur une autre arete et, dilate de
//      kBarFusionNestedMarginSteps, reste dans R_k. Les sommets de l'erosion et
//      de l'intersection sont arrondis a la grille, et l'erosion suit mal les
//      crans d'un pas que ces arrondis laissent : la marge de l'erosion seule
//      n'est pas tenue. Un niveau qui echoue au controle est reintersecte avec
//      R_k erode de kBarFusionRepairNestingSteps, puis, s'il echoue encore,
//      retire (le suivant est imbrique dans R_k).
// Les niveaux vides au sommet sont retires : le dernier rendu est le plus haut
// non vide.
//
// GARANTIE DE SORTIE (ce que la couture peut supposer) : aucune arete d'un
// niveau au-dessus de R_0 n'en croise une autre ni ne porte un sommet dans son
// interieur (controle sur la grille) ; deux aretes peuvent partager un sommet.
// R_0 est noeude sans nouveau controle. Chaque niveau, dilate
// de kBarFusionNestedMarginSteps (decalage rond, tolerance d'arc 0,1 pas),
// reste dans le precedent, au sens de Clipper2 sur la grille : la difference
// calculee est vide. Le dilate et la difference arrondissent leurs sommets a la
// grille ; la distance vraie entre les bords de deux niveaux consecutifs est
// donc d'environ kBarFusionNestedMarginSteps - 1 pas au moins. Cette derniere
// borne est observee, non demontree.
//
constexpr double kBarFusionGridScale          = 128.0;
constexpr int    kBarFusionMinLevelSteps      = 4;
constexpr int    kBarFusionApexHalfWidthSteps = 4;
constexpr int    kBarFusionSimplifySteps      = 2;
constexpr int    kBarFusionOpeningSteps       = 2;
constexpr int    kBarFusionNestingSteps       = 4;
constexpr int    kBarFusionNestedMarginSteps  = 3;
constexpr int    kBarFusionRepairNestingSteps = 6;

struct FusedBar
{
	std::vector<Vector2d> outline;   // contour ferme, sens quelconque
	double                scale = 1.0;
};

struct BarGridPoint
{
	std::int64_t x = 0;
	std::int64_t y = 0;
};

using BarGridRing = std::vector<BarGridPoint>;

struct BarFusionLevel
{
	double                   height = 0.0;   // cote u du profil, avant placement sur la face
	std::vector<BarGridRing> region;         // enveloppes en sens trigonometrique, trous en sens horaire
};

// Hauteurs des niveaux pour ces echelles de barre, croissantes, la premiere
// valant u_min. Leve std::invalid_argument si le profil a moins de 3 points,
// une coordonnee non finie ou une hauteur nulle (u_max = u_min), ou si une
// echelle n'est pas finie et strictement positive. Aucune echelle : vide.
std::vector<double> barFusionLevelHeights (const Profile2D &profile, const std::vector<double> &scales);

// Issue du controle d'imbrication.
struct BarFusionReport
{
	int repairedLevels = 0;   // reintersectes avec l'erosion de kBarFusionRepairNestingSteps
	int droppedLevels  = 0;   // retires apres reparation insuffisante
};

// Niveaux de la reunion des barres, du pied a la crete (voir CONSTRUCTION et
// GARANTIE DE SORTIE). Memes refus que barFusionLevelHeights. Un contour de
// moins de 3 points distincts sur la grille, ou d'aire nulle, ne donne aucune
// bande ; sans bande, le resultat est vide. Leve std::runtime_error si un
// booleen de Clipper2 echoue. *report, s'il est fourni, recoit l'issue du
// controle d'imbrication. Les etapes independantes tournent sur plusieurs
// fils (bar_fusion_parallel.h) ; le resultat et l'exception levee n'en
// dependent pas.
std::vector<BarFusionLevel> buildBarFusionLevels (const std::vector<FusedBar> &bars,
                                                  const Profile2D &profile,
                                                  BarFusionReport *report = nullptr);

// ============================================================================
//  Assemblage : plaque et barres en une seule coque fermee
// ============================================================================
//
// La plaque P (contours de la pierre, repere des barres) est posee sur la
// grille et extrudee de z = 0 a zTop ; elle est REMPLIE sous l'emprise R_0 des
// barres : le solide de base est D = P U R_0. Au-dessus de zTop, les niveaux
// sont relies par des gradins inclines : l'anneau R_k \ R_k+1 est tessele,
// ses sommets portes a leur cote (R_0 a zTop, R_k a zTop + h_k pour k >= 1,
// h_k cote u du profil), et le dernier niveau est ferme par un capot plat.
//
//   - noeud : les croisements de P et de R_0 (arrondis a la grille) et leurs
//     sommets poses sur l'autre bord sont inseres dans les deux, jusqu'a ce
//     qu'il n'en reste plus (au plus kBarFusionNodePasses passes) ;
//   - fond (z = 0) : tessellation Positive de P U R_0, tournee vers le bas ;
//     parois : bord Positive de P U R_0, de 0 a zTop ;
//   - plan zTop : tessellation Positive de P et de R_0 inverse (P \ R_0) ;
//   - toute tessellation : glutess (triangles plats bascules), puis
//     delaunayFlips ; elle est refusee si elle cree un sommet ailleurs qu'en
//     un point de ses contours (position absente, ou hors de la grille), ou
//     si deux contours de cotes differentes partagent un point ;
//   - garde d'anneau : un anneau dont les deux bords se touchent ou se
//     croisent (noeud non vide), ou dont la tessellation est refusee, est
//     remplace par l'anneau vers le niveau suivant (le niveau est saute) ;
//   - soudure : un sommet par (x, y entiers de la grille, cote).
//
// Echec (rendu avec `out` vide) : noeud inacheve ; une cote de niveau non
// finie ou au plus a 0 (profil enfonce plus profond que la plaque) ; fond,
// plan zTop ou capot refuse ; bord de P U R_0 que glutess ne rend pas, ou hors
// de la grille.
constexpr int kBarFusionNodePasses = 6;

struct FusedBarsResult
{
	BarFusionReport nesting;                 // controle d'imbrication des niveaux
	int             levels           = 0;    // niveaux poses au-dessus de zTop, R_0 compris
	int             skippedLevels    = 0;    // niveaux sautes par la garde d'anneau
	int             nodeInsertions   = 0;    // points inseres par le noeud P x R_0
	bool            delaunayComplete = true; // toutes les passes de delaunayFlips completes
	bool            failed           = false;
	const char     *failure          = nullptr; // cause de l'echec, en anglais ; nullptr sans echec
};

// Leve comme buildBarFusionLevels. `out` est remplace. Comme pour les
// niveaux, le maillage et l'exception levee ne dependent pas du nombre de
// fils.
FusedBarsResult fuseBarsOnPlate (const Polygon2 &plate, const std::vector<FusedBar> &bars,
                                 const Profile2D &barProfile, double zTop, Mesh &out);

// Meme assemblage a partir de niveaux deja calcules (R_0 d'abord, hauteurs
// croissantes en indice). Precondition non verifiee : chaque niveau est dans
// le precedent (un niveau hors du precedent donne une coque ouverte). La garde
// d'anneau saute un niveau qui touche ou croise le precedent, ou dont l'anneau
// creerait un sommet ; un R_0 qui se croise fait echouer l'assemblage (fond
// refuse). `nesting` du resultat reste vide. `out` est remplace.
FusedBarsResult assembleFusedLevels (const Polygon2 &plate, const std::vector<BarFusionLevel> &levels,
                                     double zTop, Mesh &out);
