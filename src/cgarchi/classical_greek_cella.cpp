#include <cgarchi/classical_greek_cella.h>

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

[[noreturn]] void Fail (const char *field, const std::string &rule)
{
	throw std::invalid_argument (std::string ("Cella : 'cella.") + field + "' " + rule);
}

// Positions deduites des longueurs, repere du temple (CellaLayout). Sommes et
// differences seulement, et des moities exactes : aucun produit contractable.
struct Plan
{
	double w = 0.0, wo = 0.0, wt = 0.0;        // nus interieur et exterieur des murs longs, bord du toichobate
	double aIn = 0.0, aOut = 0.0;              // faces interieure et exterieure des antes
	double yF = 0.0, yB = 0.0;                 // faces avant et arriere du toichobate
	double cf0 = 0.0, cf1 = 0.0;               // mur de refend avant, nus avant et arriere
	double cb0 = 0.0, cb1 = 0.0;               // mur de refend arriere
	double af0 = 0.0, af1 = 0.0;               // antes avant, face et dos
	double ab0 = 0.0, ab1 = 0.0;               // antes arriere, dos et face
	double ef0 = 0.0, ef1 = 0.0;               // architrave avant, face et dos
	double eb0 = 0.0, eb1 = 0.0;               // architrave arriere, dos et face
	double door0 = 0.0, door1 = 0.0;           // jambages Left et Right
	double t0 = 0.0, t1 = 0.0;                 // bouts du seuil
	double ty0 = 0.0, ty1 = 0.0;               // devant et dos du seuil
	double zS = 0.0, zT = 0.0, zN = 0.0, zL = 0.0, zP = 0.0, zA = 0.0, zF = 0.0, zW = 0.0;
};

Plan PlanOf (const CellaSettings &s, const KrepisLayout &krepis)
{
	Plan p;
	const double a = krepis.steps.front ().halfDepth;
	p.w = s.longWallSpan / 2.0;
	p.wo = p.w + s.longWallThickness;
	p.wt = p.wo + s.toichobateOverhang;
	p.aIn = p.w - s.antaInnerProjection;
	p.aOut = p.wo + s.antaOuterProjection;
	p.yF = s.frontPteron - a;
	p.yB = a - s.backPteron;
	p.cf0 = p.yF + s.frontCrossWallPosition;
	p.cf1 = p.cf0 + s.frontCrossWallThickness;
	p.cb1 = p.yB - s.backCrossWallPosition;
	p.cb0 = p.cb1 - s.backCrossWallThickness;
	p.af0 = p.yF + s.frontAntaRetreat;
	p.af1 = p.af0 + s.antaHeadLength;
	p.ab1 = p.yB - s.backAntaRetreat;
	p.ab0 = p.ab1 - s.antaHeadLength;
	p.ef0 = -s.frontPorchAxis - s.porchEntablatureFront;
	p.ef1 = p.ef0 + s.porchEntablatureDepth;
	p.eb1 = s.backPorchAxis + s.porchEntablatureFront;
	p.eb0 = p.eb1 - s.porchEntablatureDepth;
	p.door0 = s.doorJambOffset - p.w;
	p.door1 = p.door0 + s.doorWidth;
	const double centre = (p.door0 + p.door1) / 2.0;
	p.t0 = centre - s.thresholdLength / 2.0;
	p.t1 = centre + s.thresholdLength / 2.0;
	p.ty0 = p.cf0 - s.thresholdProjection;
	p.ty1 = p.cf0 + s.thresholdDepth;
	p.zS = krepis.stylobateTop;
	p.zT = p.zS + s.toichobateRise;
	p.zN = p.zS + s.naosFloorHeight;
	p.zL = p.zN + s.lintelHeight;
	p.zP = static_cast<double> (static_cast<float> (s.porchColumnHeight)) + p.zT;
	p.zA = p.zP + s.porchArchitraveHeight;
	p.zF = p.zA + s.porchFriezeHeight;
	p.zW = p.zS + s.wallTop;
	return p;
}

void RequireLengths (const CellaSettings &s)
{
	const std::pair<const char *, double> lengths[] = {
		{"toichobateRise", s.toichobateRise},
		{"frontPteron", s.frontPteron},
		{"backPteron", s.backPteron},
		{"toichobateOverhang", s.toichobateOverhang},
		{"longWallThickness", s.longWallThickness},
		{"longWallSpan", s.longWallSpan},
		{"wallTop", s.wallTop},
		{"frontCrossWallPosition", s.frontCrossWallPosition},
		{"frontCrossWallThickness", s.frontCrossWallThickness},
		{"backCrossWallPosition", s.backCrossWallPosition},
		{"backCrossWallThickness", s.backCrossWallThickness},
		{"doorWidth", s.doorWidth},
		{"lintelHeight", s.lintelHeight},
		{"doorJambOffset", s.doorJambOffset},
		{"thresholdLength", s.thresholdLength},
		{"thresholdProjection", s.thresholdProjection},
		{"thresholdDepth", s.thresholdDepth},
		{"naosFloorHeight", s.naosFloorHeight},
		{"frontAntaRetreat", s.frontAntaRetreat},
		{"backAntaRetreat", s.backAntaRetreat},
		{"antaInnerProjection", s.antaInnerProjection},
		{"antaOuterProjection", s.antaOuterProjection},
		{"antaHeadLength", s.antaHeadLength},
		{"porchColumnHeight", s.porchColumnHeight},
		{"frontPorchAxis", s.frontPorchAxis},
		{"backPorchAxis", s.backPorchAxis},
		{"porchArchitraveHeight", s.porchArchitraveHeight},
		{"porchFriezeHeight", s.porchFriezeHeight},
		{"porchEntablatureDepth", s.porchEntablatureDepth},
		{"porchEntablatureFront", s.porchEntablatureFront},
	};
	for (const auto &[field, value] : lengths)
		if (!(std::isfinite (value) && value > 0.0))
			Fail (field, "n'est pas une longueur finie > 0");
	if (s.porches && !(std::isfinite (s.porches->offset) && s.porches->offset > 0.0))
		Fail ("porches.offset", "n'est pas une longueur finie > 0");
}

void RequirePlan (const CellaSettings &s, const KrepisLayout &krepis, const Plan &p)
{
	const KrepisBox &stylobate = krepis.steps.front ();
	if (!(p.yF > -stylobate.halfDepth))
		Fail ("frontPteron", "place la face avant du toichobate hors du stylobate");
	if (!(p.yB < stylobate.halfDepth && p.yB > p.yF))
		Fail ("backPteron", "place la face arriere du toichobate hors du stylobate ou devant sa face avant");
	if (!(p.wt <= stylobate.halfWidth))
		Fail ("toichobateOverhang", "sort le toichobate du dessus du stylobate");
	if (!(p.aOut <= p.wt))
		Fail ("antaOuterProjection", "sort la face exterieure des antes du toichobate");
	if (!(p.aIn > 0.0))
		Fail ("antaInnerProjection", "porte la face interieure des antes au-dela de l'axe longitudinal");

	// Le long de Y : antes, architrave, mur de refend, de chaque cote, sans contact.
	if (!(p.af1 < p.cf0 && p.ef1 < p.cf0))
		Fail ("frontCrossWallPosition", "place le mur de refend avant au contact des antes ou de l'architrave avant");
	if (!(p.cb1 < p.ab0 && p.cb1 < p.eb0))
		Fail ("backCrossWallPosition", "place le mur de refend arriere au contact des antes ou de l'architrave arriere");
	if (!(p.cf1 < p.cb0))
		Fail ("backCrossWallPosition", "fait chevaucher les deux murs de refend");
	if (!(p.ef0 < p.ef1 && p.eb0 < p.eb1 && p.ab1 <= p.yB))
		Fail ("porchEntablatureDepth", "ne laisse pas d'architrave entre les antes et le mur de refend");

	if (!(p.zN > p.zT))
		Fail ("naosFloorHeight", "ne depasse pas le dessus du toichobate");
	if (!(p.zL < p.zW))
		Fail ("lintelHeight", "place le dessous du linteau au-dessus des murs");
	if (!(p.zP < p.zW))
		Fail ("porchColumnHeight", "place le pied de l'architrave des porches au-dessus des murs");

	if (!(p.door0 > -p.w && p.door1 < p.w))
		Fail ("doorWidth", "et 'cella.doorJambOffset' placent la porte hors des nus interieurs des murs");
	if (!(p.t0 > -p.w && p.t1 < p.w && p.t0 < p.door0 && p.t1 > p.door1))
		Fail ("thresholdLength", "ne place pas le seuil entre les murs et au-dela des jambages");
	if (!(s.thresholdDepth == s.frontCrossWallThickness))
		Fail ("thresholdDepth", "differe de 'cella.frontCrossWallThickness' : le seuil traverse le mur de refend avant "
		                        "(registre CE-3)");
	if (!(p.ty0 > p.yF))
		Fail ("thresholdProjection", "sort le seuil du toichobate");
}

// Colonnes des porches : pieds sur le toichobate, colonnes disjointes entre
// elles, des antes, du seuil et des murs de refend (boites de demi-cote
// `overall`, comme TE4).
void RequirePorchColumns (const CellaSettings &s, const Plan &p, const GreekDoricColumnHalfExtents &e)
{
	const GreekPorchColumnsSettings &c = *s.porches;
	if (!(static_cast<float> (c.column.height) == static_cast<float> (s.porchColumnHeight)))
		Fail ("porches.column.height", "differe en float de 'cella.porchColumnHeight' : l'architrave des porches ne peut "
		                               "poser sur deux hauteurs");
	if (!(c.offset > e.overall))
		Fail ("porches.offset", "met les deux colonnes d'un porche au contact");
	if (!(c.offset + e.overall < p.aIn))
		Fail ("porches.offset", "met une colonne de porche au contact d'une ante");
	if (!(c.offset + e.foot <= p.wt))
		Fail ("porches.offset", "pose le pied d'une colonne de porche hors du toichobate");
	if (!(-s.frontPorchAxis - e.foot >= p.yF && -s.frontPorchAxis + e.overall < p.ty0))
		Fail ("frontPorchAxis", "pose une colonne du porche avant hors du toichobate ou au contact du seuil");
	if (!(s.backPorchAxis + e.foot <= p.yB && s.backPorchAxis - e.overall > p.cb1))
		Fail ("backPorchAxis", "pose une colonne du porche arriere hors du toichobate ou au contact du mur de refend");
}

using detail::BoxMesh;
using detail::Extrusion;
using detail::PrismMesh;

} // namespace

CellaLayout LayoutGreekCella (const CellaSettings &settings, const KrepisLayout &krepis)
{
	if (krepis.steps.empty ())
		throw std::invalid_argument ("Cella : krepis sans stylobate");
	RequireLengths (settings);
	const Plan p = PlanOf (settings, krepis);
	RequirePlan (settings, krepis, p);

	CellaLayout layout;
	layout.stylobateTop = p.zS;
	layout.stylobateHalfDepth = krepis.steps.front ().halfDepth;
	layout.toichobateTop = p.zT;
	layout.porchFoot = p.zP;
	layout.top = std::max (p.zW, p.zF);
	layout.halfWidth = p.wt;
	layout.front = p.yF;
	layout.back = p.yB;
	layout.naos = {p.w, p.wo, p.cf0, p.cf1, p.cb0, p.cb1, p.zN, p.zW};
	layout.porchEntablature = {p.ef0, p.ef1, p.eb0, p.eb1, p.aOut, p.zA, p.zF};
	if (settings.porches)
	{
		layout.porchColumnExtents = ComputeGreekDoricColumnHalfExtents (settings.porches->column);
		RequirePorchColumns (settings, p, layout.porchColumnExtents);
		const double o = settings.porches->offset;
		layout.porchColumns = {{PeristyleSide::Front, 0, -o, -settings.frontPorchAxis, p.zT},
		                       {PeristyleSide::Front, 1, o, -settings.frontPorchAxis, p.zT},
		                       {PeristyleSide::Back, 0, -o, settings.backPorchAxis, p.zT},
		                       {PeristyleSide::Back, 1, o, settings.backPorchAxis, p.zT}};
	}
	return layout;
}

std::vector<CellaPiece> BuildGreekCellaPieces (const CellaSettings &settings, const CellaLayout &layout)
{
	// Le plan ne lit du stylobate que sa demi-profondeur et la cote de son dessus.
	KrepisLayout krepis;
	krepis.stylobateTop = layout.stylobateTop;
	krepis.steps.push_back ({0.0, layout.stylobateHalfDepth, 0.0, layout.stylobateTop});
	const Plan p = PlanOf (settings, krepis);

	std::vector<CellaPiece> pieces;
	pieces.reserve (15);
	auto add = [&pieces] (CellaElement element, PeristyleSide side, int index, std::unique_ptr<Mesh> mesh) {
		pieces.push_back ({element, side, index, std::move (mesh)});
	};
	add (CellaElement::Toichobate, PeristyleSide::Front, 0, BoxMesh (-p.wt, p.wt, p.yF, p.yB, p.zS, p.zT));
	add (CellaElement::NaosFloor, PeristyleSide::Front, 0, BoxMesh (-p.w, p.w, p.cf1, p.cb0, p.zT, p.zN));
	// Mur long : du dos des antes jusqu'au pied de l'architrave des porches, puis du dos des architraves jusqu'au
	// dessus des murs.
	const std::vector<std::array<double, 2>> longWall = {{p.af1, p.zT}, {p.ab0, p.zT}, {p.ab0, p.zP}, {p.eb0, p.zP},
	                                                     {p.eb0, p.zW}, {p.ef1, p.zW}, {p.ef1, p.zP}, {p.af1, p.zP}};
	add (CellaElement::LongWall, PeristyleSide::Left, 0, PrismMesh (longWall, Extrusion::AlongX, -p.wo, -p.w));
	add (CellaElement::LongWall, PeristyleSide::Right, 0, PrismMesh (longWall, Extrusion::AlongX, p.w, p.wo));
	// Mur de refend avant : echancrure du seuil sur toute l'epaisseur, puis baie de la porte jusqu'au linteau.
	const std::vector<std::array<double, 2>> frontWall = {
		{-p.w, p.zT},     {p.t0, p.zT},    {p.t0, p.zN},    {p.door0, p.zN}, {p.door0, p.zL}, {p.door1, p.zL},
		{p.door1, p.zN}, {p.t1, p.zN},    {p.t1, p.zT},    {p.w, p.zT},     {p.w, p.zW},     {-p.w, p.zW}};
	add (CellaElement::CrossWall, PeristyleSide::Front, 0, PrismMesh (frontWall, Extrusion::AlongY, -p.cf1, -p.cf0));
	add (CellaElement::CrossWall, PeristyleSide::Back, 0, BoxMesh (-p.w, p.w, p.cb0, p.cb1, p.zT, p.zW));
	add (CellaElement::Threshold, PeristyleSide::Front, 0, BoxMesh (p.t0, p.t1, p.ty0, p.ty1, p.zT, p.zN));
	add (CellaElement::Anta, PeristyleSide::Front, 0, BoxMesh (-p.aOut, -p.aIn, p.af0, p.af1, p.zT, p.zP));
	add (CellaElement::Anta, PeristyleSide::Front, 1, BoxMesh (p.aIn, p.aOut, p.af0, p.af1, p.zT, p.zP));
	add (CellaElement::Anta, PeristyleSide::Back, 0, BoxMesh (-p.aOut, -p.aIn, p.ab0, p.ab1, p.zT, p.zP));
	add (CellaElement::Anta, PeristyleSide::Back, 1, BoxMesh (p.aIn, p.aOut, p.ab0, p.ab1, p.zT, p.zP));
	add (CellaElement::PorchArchitrave, PeristyleSide::Front, 0, BoxMesh (-p.aOut, p.aOut, p.ef0, p.ef1, p.zP, p.zA));
	add (CellaElement::PorchFrieze, PeristyleSide::Front, 0, BoxMesh (-p.aOut, p.aOut, p.ef0, p.ef1, p.zA, p.zF));
	add (CellaElement::PorchArchitrave, PeristyleSide::Back, 0, BoxMesh (-p.aOut, p.aOut, p.eb0, p.eb1, p.zP, p.zA));
	add (CellaElement::PorchFrieze, PeristyleSide::Back, 0, BoxMesh (-p.aOut, p.aOut, p.eb0, p.eb1, p.zA, p.zF));
	return pieces;
}

} // namespace classical
} // namespace cgarchi
