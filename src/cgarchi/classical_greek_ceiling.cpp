#include <cgarchi/classical_greek_ceiling.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <cgarchi/classical_greek_prism_detail.h>
#include "mesh.h"

namespace cgarchi
{
namespace classical
{

namespace
{

[[noreturn]] void FailEast (const char *field, const std::string &rule)
{
	throw std::invalid_argument (std::string ("Plafond est : 'eastCeiling.") + field + "' " + rule);
}

[[noreturn]] void FailFlank (const char *field, const std::string &rule)
{
	throw std::invalid_argument (std::string ("Plafonds des flancs : 'flankCeilings.") + field + "' " + rule);
}

// Produit arrondi en double avant toute somme : jamais contracte en FMA.
double Product (double a, double b)
{
	const volatile double p = a * b;
	return p;
}

// Lignes d'une grille de caissons sur un axe : bords [lo, hi], puis, pour chaque
// caisson, bords de l'ouverture et du fond ; `count` caissons centres dans
// [a, b]. kind, par intervalle : 0 plein, 1 ouverture hors du fond, 2 fond.
struct CofferAxis
{
	std::vector<double> lines;
	std::vector<int> kind;
};

CofferAxis CofferAxisOf (double lo, double hi, double a, double b, int count, const CofferSettings &c)
{
	CofferAxis axis;
	const double start = a + ((b - a) - Product (count, c.pitch)) / 2.0;
	const double half = c.opening / 2.0, inner = half - c.frame;
	axis.lines.push_back (lo);
	for (int k = 0; k < count; ++k)
	{
		const double centre = start + Product (k + 0.5, c.pitch);
		axis.lines.insert (axis.lines.end (), {centre - half, centre - inner, centre + inner, centre + half});
		axis.kind.insert (axis.kind.end (), {0, 1, 2, 1});
	}
	axis.lines.push_back (hi);
	axis.kind.push_back (0);
	return axis;
}

// Champ fautif si deux lignes voisines de la grille se confondent une fois
// arrondies en float : un fond ou un cadre ecrase ('cofferFrame'), ou un plein
// entre deux ouvertures ou au bord ('cofferOpening') ; nullptr sinon.
const char *CollapsedCofferField (const CofferAxis &axis)
{
	for (std::size_t i = 0; i + 1 < axis.lines.size (); ++i)
		if (!(static_cast<float> (axis.lines[i]) < static_cast<float> (axis.lines[i + 1])))
			return axis.kind[i] == 0 ? "cofferOpening" : "cofferFrame";
	return nullptr;
}

// Cotes des couches d'une dalle, strictement croissantes une fois arrondies en
// float : pied, mi-profondeur, fond des caissons (les deux ressauts), dessus.
bool SlabLayersDistinctInFloat (double foot, double top, double depth)
{
	const float z[] = {static_cast<float> (foot), static_cast<float> (foot + depth / 2.0),
	                   static_cast<float> (foot + depth), static_cast<float> (top)};
	return z[0] < z[1] && z[1] < z[2] && z[2] < z[3];
}

// Dalle a caissons (CE-37, CE-38) : couche basse evidee sous les ouvertures,
// couche moyenne sous les fonds, couche haute pleine.
std::unique_ptr<Mesh> SlabMesh (const CofferAxis &x, const CofferAxis &y, double foot, double top, double depth)
{
	const std::vector<double> zs = {foot, foot + depth / 2.0, foot + depth, top};
	return detail::GridSolidMesh (x.lines, y.lines, zs, [&x, &y] (std::size_t i, std::size_t j, std::size_t k) {
		const int kx = x.kind[i], ky = y.kind[j];
		if (k == 0)
			return !(kx >= 1 && ky >= 1);
		if (k == 1)
			return !(kx == 2 && ky == 2);
		return true;
	});
}

} // namespace

EastCeilingLayout LayoutGreekEastCeiling (const EastCeilingSettings &s, const EpicranitisLayout &crown,
                                         double ringInnerHalfWidth, double ringInnerHalfDepth)
{
	const std::pair<const char *, double> lengths[] = {
		{"beamSpacing", s.beamSpacing},           {"beamWidth", s.beamWidth},
		{"beamHeight", s.beamHeight},             {"beamWestBearing", s.beamWestBearing},
		{"endMassifWidth", s.endMassifWidth},     {"corniceHeight", s.cornice.height},
		{"corniceProjection", s.cornice.projection}, {"bandHeight", s.cornice.bandHeight},
		{"corniceDepth", s.cornice.depth},        {"cofferPitch", s.coffers.pitch},
		{"cofferOpening", s.coffers.opening},     {"cofferFrame", s.coffers.frame},
		{"cofferDepth", s.coffers.depth},
	};
	for (const auto &[field, value] : lengths)
		if (!(std::isfinite (value) && value > 0.0))
			FailEast (field, "n'est pas une longueur finie > 0");
	if (s.beamCount < 1)
		FailEast ("beamCount", "doit valoir au moins 1");
	if (s.cofferRows < 1)
		FailEast ("cofferRows", "doit valoir au moins 1");
	if (!(s.beamSpacing > s.beamWidth))
		FailEast ("beamSpacing", "ne depasse pas la largeur des poutres : les poutres se touchent");

	EastCeilingLayout l;
	l.crown = crown;
	const CellaPorchEntablature &p = crown.porch;
	const double halfBeam = s.beamWidth / 2.0;
	const double middle = (s.beamCount - 1) / 2.0;
	for (int k = 0; k < s.beamCount; ++k)
		l.beamAxes.push_back (Product (k - middle, s.beamSpacing));
	l.massif = ringInnerHalfWidth - s.endMassifWidth;
	if (!(static_cast<float> (l.beamAxes.back () + halfBeam) < static_cast<float> (l.massif)))
		FailEast ("beamSpacing", "porte la face exterieure de la poutre extreme au plan du massif de bout ou au-dela");
	if (!(l.massif > p.halfWidth))
		FailEast ("endMassifWidth", "porte le plan du massif de bout au plus a la face exterieure des antes");
	const double porchDepth = p.frontBack - p.front;
	if (s.beamWestBearing > porchDepth)
		FailEast ("beamWestBearing", "depasse l'epaisseur de l'entablement des porches");
	if (s.cornice.depth > porchDepth)
		FailEast ("corniceDepth", "depasse l'epaisseur de l'entablement des porches");
	if (!(s.beamHeight > s.cornice.height + s.cornice.bandHeight))
		FailEast ("beamHeight", "ne depasse pas la corniche et le bandeau : pas de dalle");

	l.bays.push_back ({-l.massif, l.beamAxes.front () - halfBeam});
	for (std::size_t k = 1; k < l.beamAxes.size (); ++k)
		l.bays.push_back ({l.beamAxes[k - 1] + halfBeam, l.beamAxes[k] - halfBeam});
	l.bays.push_back ({l.beamAxes.back () + halfBeam, l.massif});
	l.westEnd = p.front + s.beamWestBearing;
	l.eastEnd = -ringInnerHalfDepth;
	l.beamFoot = crown.top;
	l.beamTop = crown.top + s.beamHeight;
	l.corniceTop = crown.top + s.cornice.height;
	l.bandTop = l.corniceTop + s.cornice.bandHeight;

	if (!SlabLayersDistinctInFloat (l.bandTop, l.beamTop, s.coffers.depth))
		FailEast ("cofferDepth", "atteint l'epaisseur de la dalle, ou confond deux cotes des ressauts, en float");
	if (!(s.coffers.opening < s.coffers.pitch))
		FailEast ("cofferOpening", "atteint le pas des caissons");
	if (!(2.0 * s.coffers.frame < s.coffers.opening))
		FailEast ("cofferFrame", "deux fois, atteint l'ouverture : pas de fond");
	double narrowest = l.bays.front ()[1] - l.bays.front ()[0];
	for (const std::array<double, 2> &bay : l.bays)
		narrowest = std::min (narrowest, bay[1] - bay[0]);
	if (Product (s.cofferRows, s.coffers.pitch) > narrowest)
		FailEast ("cofferRows", "ne tient pas dans la travee la plus etroite");
	l.cofferColumns = static_cast<int> (std::floor ((p.front - l.eastEnd) / s.coffers.pitch));
	if (l.cofferColumns < 1)
		FailEast ("cofferPitch", "ne laisse aucun caisson dans la longueur libre");
	if (!(s.cornice.projection < p.front - l.eastEnd))
		FailEast ("corniceProjection", "atteint la longueur libre, du nu de la frise a la face interieure de l'anneau");
	std::vector<CofferAxis> axes {CofferAxisOf (l.eastEnd, l.westEnd, l.eastEnd, p.front, l.cofferColumns, s.coffers)};
	for (const std::array<double, 2> &bay : l.bays)
		axes.push_back (CofferAxisOf (bay[0], bay[1], bay[0], bay[1], s.cofferRows, s.coffers));
	for (const CofferAxis &axis : axes)
		if (const char *field = CollapsedCofferField (axis))
			FailEast (field, "confond deux lignes de la grille des caissons une fois arrondies en float");

	l.eastPocket = {-l.massif, l.massif, l.eastEnd, l.westEnd, l.beamTop};
	l.westPocket = {-p.halfWidth, p.halfWidth, p.back - s.cornice.depth, p.back + s.cornice.projection, l.bandTop};
	return l;
}

std::vector<CeilingPiece> BuildGreekEastCeilingPieces (const EastCeilingSettings &s, const EastCeilingLayout &l)
{
	const CellaPorchEntablature &p = l.crown.porch;
	const PorchCorniceSettings &c = s.cornice;
	const double halfBeam = s.beamWidth / 2.0;
	std::vector<CeilingPiece> pieces;
	pieces.reserve (l.beamAxes.size () + 3 * l.bays.size () + 2);
	using E = CeilingElement;
	using S = PeristyleSide;
	auto add = [&pieces] (E element, S side, std::size_t index, std::unique_ptr<Mesh> mesh) {
		pieces.push_back ({element, side, static_cast<int> (index), std::move (mesh)});
	};
	for (std::size_t k = 0; k < l.beamAxes.size (); ++k)
		add (E::Beam, S::Front, k,
		     detail::BoxMesh (l.beamAxes[k] - halfBeam, l.beamAxes[k] + halfBeam, l.eastEnd, l.westEnd, l.beamFoot, l.beamTop));
	for (std::size_t k = 0; k < l.bays.size (); ++k)
		add (E::Cornice, S::Front, k,
		     detail::BoxMesh (l.bays[k][0], l.bays[k][1], p.front - c.projection, p.front + c.depth, l.crown.top, l.corniceTop));
	for (std::size_t k = 0; k < l.bays.size (); ++k)
		add (E::Band, S::Front, k, detail::BoxMesh (l.bays[k][0], l.bays[k][1], p.front, p.front + c.depth, l.corniceTop, l.bandTop));
	add (E::Cornice, S::Back, 0,
	     detail::BoxMesh (-p.halfWidth, p.halfWidth, p.back - c.depth, p.back + c.projection, l.crown.top, l.corniceTop));
	add (E::Band, S::Back, 0, detail::BoxMesh (-p.halfWidth, p.halfWidth, p.back - c.depth, p.back, l.corniceTop, l.bandTop));
	const CofferAxis along = CofferAxisOf (l.eastEnd, l.westEnd, l.eastEnd, p.front, l.cofferColumns, s.coffers);
	for (std::size_t k = 0; k < l.bays.size (); ++k)
	{
		const std::array<double, 2> &bay = l.bays[k];
		const CofferAxis across = CofferAxisOf (bay[0], bay[1], bay[0], bay[1], s.cofferRows, s.coffers);
		add (E::Slab, S::Front, k, SlabMesh (across, along, l.bandTop, l.beamTop, s.coffers.depth));
	}
	return pieces;
}

FlankCeilingLayout LayoutGreekFlankCeilings (const FlankCeilingSettings &s, const EastCeilingSettings &east,
                                            const EastCeilingLayout &e, double ringInnerHalfWidth)
{
	const std::pair<const char *, double> lengths[] = {
		{"beamSpacing", s.beamSpacing}, {"firstBeamOffset", s.firstBeamOffset}, {"beamWidth", s.beamWidth},
		{"beamHeight", s.beamHeight},   {"peristyleGap", s.peristyleGap},
	};
	for (const auto &[field, value] : lengths)
		if (!(std::isfinite (value) && value > 0.0))
			FailFlank (field, "n'est pas une longueur finie > 0");
	const std::pair<const char *, int> counts[] = {
		{"eastBeamCount", s.frontBeamCount}, {"westBeamCount", s.backBeamCount}, {"cofferRows", s.cofferRows}};
	for (const auto &[field, value] : counts)
		if (value < 1)
			FailFlank (field, "doit valoir au moins 1");
	if (!(s.beamSpacing > s.beamWidth))
		FailFlank ("beamSpacing", "ne depasse pas la largeur des poutres : les poutres se touchent");
	const double halfBeam = s.beamWidth / 2.0;
	if (!(s.firstBeamOffset > halfBeam))
		FailFlank ("firstBeamOffset", "ne depasse pas la demi-largeur des poutres : les deux poutres centrales se touchent "
		                              "ou se recouvrent");

	FlankCeilingLayout l;
	for (int k = s.frontBeamCount - 1; k >= 0; --k)
		l.beamAxes.push_back (-(s.firstBeamOffset + Product (k, s.beamSpacing)));
	for (int k = 0; k < s.backBeamCount; ++k)
		l.beamAxes.push_back (s.firstBeamOffset + Product (k, s.beamSpacing));
	const CellaPorchEntablature &p = e.crown.porch;
	if (!(static_cast<float> (l.beamAxes.front () - halfBeam) > static_cast<float> (p.frontBack)))
		FailFlank ("eastBeamCount", "porte la face de la derniere poutre est au contact du dos de l'entablement du porche "
		                            "avant ou au-dela");
	if (!(static_cast<float> (l.beamAxes.back () + halfBeam) < static_cast<float> (p.backFront)))
		FailFlank ("westBeamCount", "porte la face de la derniere poutre ouest au contact du dos de l'entablement du porche "
		                            "arriere ou au-dela");
	l.wallEnd = e.crown.innerFace;
	l.naosOuter = e.crown.naos.outer;
	l.peristyleEnd = ringInnerHalfWidth - s.peristyleGap;
	if (!(static_cast<float> (l.peristyleEnd) > static_cast<float> (l.naosOuter)))
		FailFlank ("peristyleGap", "ne laisse pas de portee au-dela du nu exterieur des murs");
	l.beamFoot = e.crown.top;
	l.beamTop = e.crown.top + s.beamHeight;
	l.slabFoot = e.bandTop;
	if (!SlabLayersDistinctInFloat (l.slabFoot, l.beamTop, east.coffers.depth))
		FailFlank ("beamHeight", "met le dessus des poutres au plus a la profondeur des caissons au-dessus du bandeau du "
		                         "plafond est : pas de dalle");
	l.cofferColumns = static_cast<int> (std::floor ((l.peristyleEnd - l.naosOuter) / east.coffers.pitch));
	if (l.cofferColumns < 1)
		FailFlank ("peristyleGap", "ne laisse aucun caisson entre le nu exterieur des murs et le bout des poutres");
	if (Product (s.cofferRows, east.coffers.pitch) > std::min (s.beamSpacing, 2.0 * s.firstBeamOffset) - s.beamWidth)
		FailFlank ("cofferRows", "ne tient pas dans la travee la plus etroite entre deux poutres");
	const double a = l.naosOuter, b = l.peristyleEnd;
	std::vector<CofferAxis> axes {CofferAxisOf (-b, -l.wallEnd, -b, -a, l.cofferColumns, east.coffers),
	                              CofferAxisOf (l.wallEnd, b, a, b, l.cofferColumns, east.coffers)};
	for (std::size_t k = 0; k + 1 < l.beamAxes.size (); ++k)
	{
		const double y0 = l.beamAxes[k] + halfBeam, y1 = l.beamAxes[k + 1] - halfBeam;
		axes.push_back (CofferAxisOf (y0, y1, y0, y1, s.cofferRows, east.coffers));
	}
	for (const CofferAxis &axis : axes)
		if (const char *field = CollapsedCofferField (axis))
			FailFlank ("cofferRows", std::string ("confond deux lignes de la grille des caissons une fois arrondies en float "
			                                    "('eastCeiling.") + field + "')");

	const double y0 = l.beamAxes.front () - halfBeam, y1 = l.beamAxes.back () + halfBeam;
	l.pockets[0] = {-l.peristyleEnd, -l.wallEnd, y0, y1, l.beamTop};
	l.pockets[1] = {l.wallEnd, l.peristyleEnd, y0, y1, l.beamTop};
	return l;
}

std::vector<CeilingPiece> BuildGreekFlankCeilingPieces (const FlankCeilingSettings &s, const EastCeilingSettings &east,
                                                        const FlankCeilingLayout &l)
{
	const double halfBeam = s.beamWidth / 2.0;
	std::vector<CeilingPiece> pieces;
	pieces.reserve (2 * (2 * l.beamAxes.size () - 1));
	using E = CeilingElement;
	for (const PeristyleSide side : {PeristyleSide::Left, PeristyleSide::Right})
	{
		const bool left = side == PeristyleSide::Left;
		const double x0 = left ? -l.peristyleEnd : l.wallEnd, x1 = left ? -l.wallEnd : l.peristyleEnd;
		const double a = left ? -l.peristyleEnd : l.naosOuter, b = left ? -l.naosOuter : l.peristyleEnd;
		const CofferAxis along = CofferAxisOf (x0, x1, a, b, l.cofferColumns, east.coffers);
		for (std::size_t k = 0; k < l.beamAxes.size (); ++k)
			pieces.push_back ({E::Beam, side, static_cast<int> (k),
			                   detail::BoxMesh (x0, x1, l.beamAxes[k] - halfBeam, l.beamAxes[k] + halfBeam, l.beamFoot, l.beamTop)});
		for (std::size_t k = 0; k + 1 < l.beamAxes.size (); ++k)
		{
			const double y0 = l.beamAxes[k] + halfBeam, y1 = l.beamAxes[k + 1] - halfBeam;
			const CofferAxis across = CofferAxisOf (y0, y1, y0, y1, s.cofferRows, east.coffers);
			pieces.push_back ({E::Slab, side, static_cast<int> (k), SlabMesh (along, across, l.slabFoot, l.beamTop, east.coffers.depth)});
		}
	}
	return pieces;
}

} // namespace classical
} // namespace cgarchi
