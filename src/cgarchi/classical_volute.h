#pragma once

#include <optional>
#include <vector>

#include <cgarchi/classical_orders.h>

namespace cgarchi
{
namespace classical
{

// ============================================================================
//  Volute ionique a douze centres (Vignole, pl. XX, figure A) : trace 2D
// ============================================================================
//
// Repere de la volute : u horizontal, positif vers l'exterieur de la colonne,
// v vertical vers le haut, origine au centre de l'oeil ; unites : parties.
// Les arcs tournent dans le sens horaire.
//
// Centres : e = rayon de l'oeil ; a1 = e/2, a2 = e/3, a3 = e/6 ; pour le tour
// j, centres (-aj, aj), (aj, aj), (aj, -aj), (-aj, -aj). Le premier arc part du
// sommet de la volute, (-a1, centre_depuis_haut).
//
// Listel : courbe interieure aux centres a'j = aj - a1/12, partant de
// (-a'1, r_top - w), w = quart de l'intervalle laisse au-dessus du premier
// tour ; un segment horizontal la prolonge jusqu'a la cathete du premier
// centre, (-a1, r_top - w).

enum class VoluteJunction
{
	// L'arc k s'arrete sur la demi-droite issue de son centre dans la
	// direction du centre suivant ; le rayon suivant est diminue de la distance
	// entre les deux centres. Raccords tangents (G1).
	CentreLine,
	// L'arc k s'arrete sur l'axe horizontal ou vertical de son centre, l'arc
	// suivant repart de ce point (lecture de la section 5.7). Cassures de
	// tangente aux changements de tour, et le 12e arc s'acheve hors de l'oeil
	// (a 1,023 du centre) : BuildVoluteSection refuse ce trace (« n'entre pas
	// dans l'oeil »).
	Axis
};

struct VolutePoint
{
	double u = 0.0;
	double v = 0.0;
};

// Arc de cercle horaire : de l'angle a0 a l'angle a1 < a0 (radians, depuis +u).
struct VoluteArc
{
	VolutePoint centre;
	double radius = 0.0;
	double a0 = 0.0;
	double a1 = 0.0;

	VolutePoint At (double angle) const;
	VolutePoint Start () const { return At (a0); }
	VolutePoint End () const { return At (a1); }
};

struct VoluteCurve
{
	std::optional<VolutePoint> prefixStart;   // segment rectiligne jusqu'au premier arc
	std::vector<VoluteArc> arcs;
};

struct VoluteSpiral
{
	VoluteCurve outer;         // contour exterieur
	VoluteCurve inner;         // bord interieur du listel
	double eyeRadius = 0.0;
	double listelWidth = 0.0;  // w
};

// `innerCentreShift` : decalage des centres interieurs vers le centre de
// l'oeil, a1/12 par defaut ; une valeur negative les eloigne. Leve
// std::invalid_argument si le decalage atteint e/6, si deux centres
// consecutifs sont confondus, si un rayon n'est pas positif ou si un arc ne
// tient pas dans un demi-tour.
VoluteSpiral BuildTwelveCentreVolute (const IonicVoluteData &data,
                                      VoluteJunction junction = VoluteJunction::CentreLine,
                                      std::optional<double> innerCentreShift = std::nullopt);

// Points de la courbe, prefixe compris ; `segmentsPerQuarter` segments par
// quart de cercle (au moins un segment par arc). Les extremites des arcs sont
// des points de la liste, sans doublon.
std::vector<VolutePoint> SampleCurve (const VoluteCurve &curve, int segmentsPerQuarter);

// Plus grand angle entre tangentes a un raccord d'arcs (radians).
double MaxTangentBreak (const VoluteCurve &curve);

struct VoluteBox
{
	double uMin = 0.0, uMax = 0.0, vMin = 0.0, vMax = 0.0;
};

// Boite exacte des `arcCount` premiers arcs (extremites et extrema d'axe
// atteints dans chaque arc), prefixe exclu.
VoluteBox CurveExtents (const VoluteCurve &curve, std::size_t arcCount);

// ---------------------------------------------------------------------------
//  Section du coussinet
// ---------------------------------------------------------------------------
//
// S (corps) : interieur du premier tour exterieur, ferme par le segment
// u = closureU entre le point de retour du premier tour et le sommet.
// R (relief) : listel (entre contour exterieur et bord interieur) et oeil.
// C (canal) : S prive de R.
// Polygones en indices dans `points`, sens trigonometrique ; les bords
// communs partagent leurs points. Le contour exterieur et le bord interieur
// sont coupes a leur entree dans l'oeil ; l'oeil est borde par l'arc de son
// cercle qui va d'une entree a l'autre en suivant le sens de la spirale.
//
// Leve std::invalid_argument si le sommet de la volute n'est pas sur
// u = closureU, si une courbe n'entre pas dans l'oeil, si le premier tour ne
// recoupe pas u = closureU, ou si la section echoue a ValidateVoluteSection.
struct VoluteSection
{
	std::vector<VolutePoint> points;
	std::vector<unsigned> body;     // S
	std::vector<unsigned> raised;   // R
	std::vector<unsigned> canal;    // C
};

VoluteSection BuildVoluteSection (const VoluteSpiral &spiral, double closureU, int segmentsPerQuarter);

// Leve std::invalid_argument si un polygone a moins de trois points, un
// indice hors bornes, une aire non positive ou deux aretes non voisines qui se
// touchent, ou si |A(C) + A(R) - A(S)| > 1e-9 A(S).
void ValidateVoluteSection (const VoluteSection &section);

// Aire signee d'un polygone d'indices (positive en sens trigonometrique).
double PolygonArea (const std::vector<VolutePoint> &points, const std::vector<unsigned> &polygon);

// Vrai si deux aretes non voisines du polygone se coupent ou se touchent
// (sommet pose sur une arete compris). Balayage en O(n log n + k).
bool PolygonSelfIntersects (const std::vector<VolutePoint> &points, const std::vector<unsigned> &polygon);

} // namespace classical
} // namespace cgarchi
