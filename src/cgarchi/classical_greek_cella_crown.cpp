#include <cgarchi/classical_greek_cella_crown.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <cgarchi/classical_greek_cella_detail.h>
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
	throw std::invalid_argument (std::string ("Couronnement : 'epicranitis.") + field + "' " + rule);
}

[[noreturn]] void FailExtension (const std::string &field, const std::string &rule)
{
	throw std::invalid_argument ("Prolongement : '" + field + "' " + rule);
}

using detail::BoxMesh;
using detail::StripAxis;
using detail::StripMesh;
using detail::StripOutward;

// a avant b une fois arrondis en float, comme les sommets d'une bande (StripMesh).
bool BeforeInFloat (double a, double b)
{
	return static_cast<float> (a) < static_cast<float> (b);
}

// Profil d'une face moulee, (saillie, cote), de bas en haut : bande de z0 a step,
// puis filet et listel a la saillie du listel jusqu'a top ; d = 0 contre le nu.
std::vector<std::array<double, 2>> Profile (const EpicranitisSettings &s, const EpicranitisLayout &l, double z0)
{
	return {{0.0, z0},
	        {s.bandProjection, z0},
	        {s.bandProjection, l.step},
	        {s.listelProjection, l.step},
	        {s.listelProjection, l.top},
	        {0.0, l.top}};
}

} // namespace

EpicranitisLayout LayoutGreekEpicranitis (const EpicranitisSettings &s, const CellaLayout &cella)
{
	const std::pair<const char *, double> lengths[] = {
		{"height", s.height},
		{"innerOffset", s.innerOffset},
		{"listelHeight", s.listelHeight},
		{"filletHeight", s.filletHeight},
		{"listelProjection", s.listelProjection},
		{"bandProjection", s.bandProjection},
	};
	for (const auto &[field, value] : lengths)
		if (!(std::isfinite (value) && value > 0.0))
			Fail (field, "n'est pas une longueur finie > 0");

	EpicranitisLayout l;
	l.naos = cella.naos;
	l.porch = cella.porchEntablature;
	const CellaNaos &n = l.naos;
	const CellaPorchEntablature &p = l.porch;
	const detail::InnerCourse course = detail::InnerCourseFaces (n, s.innerOffset);
	l.innerFace = course.side;
	l.frontCourse = course.front;
	l.backCourse = course.back;
	if (!detail::InnerCourseWithinWalls (n, course))
		Fail ("innerOffset", "sort la face interieure de l'epicranitis de l'epaisseur d'un mur");

	l.top = n.wallTop + s.height;
	if (!(l.top > p.friezeTop))
		Fail ("height", "ne depasse pas le dessus de la frise des porches");
	l.step = l.top - (s.listelHeight + s.filletHeight);
	if (!(l.step > std::max (n.wallTop, p.friezeTop)))
		Fail ("listelHeight", "et 'epicranitis.filletHeight' ne laissent pas de bande a meandre sur les murs ou sur la frise");

	// Bandes, par les expressions de leurs bouts et en float, comme StripMesh : au dos du couronnement, hors des murs,
	// le bout d'onglet reste avant la face des antes ; dans chaque porche, deux bouts d'onglet opposes restent l'un
	// avant l'autre (dos du couronnement et mur de refend le long de X, murs longs le long de Y).
	const bool bandWider = s.bandProjection >= s.listelProjection;
	const double reach = bandWider ? s.bandProjection : s.listelProjection;
	const char *widest = bandWider ? "bandProjection" : "listelProjection";
	if (!BeforeInFloat (n.outer + reach, p.halfWidth))
		Fail (widest, "atteint la face exterieure des antes au dos du couronnement");
	if (!(BeforeInFloat (-n.inner + reach, n.inner - reach) && BeforeInFloat (p.frontBack + reach, n.frontWall - reach) &&
	      BeforeInFloat (n.backWall + reach, p.backFront - reach)))
		Fail (widest, "croise deux bandes d'onglet d'un porche");

	l.halfWidth = std::max (p.halfWidth, n.outer + reach);
	l.front = p.front - reach;
	l.back = p.back + reach;
	return l;
}

std::vector<EpicranitisPiece> BuildGreekEpicranitisPieces (const EpicranitisSettings &s, const EpicranitisLayout &l)
{
	const CellaNaos &n = l.naos;
	const CellaPorchEntablature &p = l.porch;
	const double w = n.inner, wo = n.outer, xi = l.innerFace, a = p.halfWidth;
	const double zW = n.wallTop, zF = p.friezeTop, top = l.top;
	std::vector<EpicranitisPiece> pieces;
	pieces.reserve (26);
	auto add = [&pieces] (EpicranitisElement element, PeristyleSide side, int index, std::unique_ptr<Mesh> mesh) {
		pieces.push_back ({element, side, index, std::move (mesh)});
	};
	using E = EpicranitisElement;
	using S = PeristyleSide;
	add (E::Course, S::Left, 0, BoxMesh (-wo, -xi, p.frontBack, p.backFront, zW, top));
	add (E::Course, S::Right, 0, BoxMesh (xi, wo, p.frontBack, p.backFront, zW, top));
	add (E::PorchCourse, S::Front, 0, BoxMesh (-xi, -w, p.frontBack, l.frontCourse, zW, top));
	add (E::PorchCourse, S::Front, 1, BoxMesh (w, xi, p.frontBack, l.frontCourse, zW, top));
	add (E::PorchCourse, S::Back, 0, BoxMesh (-xi, -w, l.backCourse, p.backFront, zW, top));
	add (E::PorchCourse, S::Back, 1, BoxMesh (w, xi, l.backCourse, p.backFront, zW, top));
	add (E::CrossCourse, S::Front, 0, BoxMesh (-w, w, n.frontWall, l.frontCourse, zW, top));
	add (E::CrossCourse, S::Back, 0, BoxMesh (-w, w, l.backCourse, n.backWall, zW, top));
	add (E::Crown, S::Front, 0, BoxMesh (-a, a, p.front, p.frontBack, zF, top));
	add (E::Crown, S::Back, 0, BoxMesh (-a, a, p.backFront, p.back, zF, top));

	const std::vector<std::array<double, 2>> wall = Profile (s, l, zW), crown = Profile (s, l, zF);
	// Nus exterieurs des murs longs, d'onglet contre le dos des couronnements.
	add (E::Moulding, S::Left, 0,
	     StripMesh (wall, StripAxis::AlongY, -wo, StripOutward::Decreasing, p.frontBack, true, p.backFront, true));
	add (E::Moulding, S::Right, 0,
	     StripMesh (wall, StripAxis::AlongY, wo, StripOutward::Increasing, p.frontBack, true, p.backFront, true));
	// Porches : la face exterieure du couronnement, puis le pourtour du porche (dos du couronnement, mur de refend,
	// murs longs), d'onglet a ses angles rentrants, puis le dos du couronnement hors des murs. `inward` : sens de Y
	// du couronnement vers l'interieur du porche ; `outward`, le sens oppose.
	struct Porch
	{
		S side;
		double face, back, wall;   // face exterieure et dos du couronnement, face cote porche du mur de refend
		StripOutward inward, outward;
		double y0, y1;             // murs longs du porche, le long de Y
	};
	const Porch porches[] = {
		{S::Front, p.front, p.frontBack, n.frontWall, StripOutward::Increasing, StripOutward::Decreasing, p.frontBack, n.frontWall},
		{S::Back, p.back, p.backFront, n.backWall, StripOutward::Decreasing, StripOutward::Increasing, n.backWall, p.backFront}};
	for (const Porch &q : porches)
	{
		add (E::Moulding, q.side, 0, StripMesh (crown, StripAxis::AlongX, q.face, q.outward, -a, false, a, false));
		add (E::Moulding, q.side, 1, StripMesh (crown, StripAxis::AlongX, q.back, q.inward, -w, true, w, true));
		add (E::Moulding, q.side, 2, StripMesh (wall, StripAxis::AlongX, q.wall, q.outward, -w, true, w, true));
		add (E::Moulding, q.side, 3, StripMesh (wall, StripAxis::AlongY, -w, StripOutward::Increasing, q.y0, true, q.y1, true));
		add (E::Moulding, q.side, 4, StripMesh (wall, StripAxis::AlongY, w, StripOutward::Decreasing, q.y0, true, q.y1, true));
		add (E::Moulding, q.side, 5, StripMesh (crown, StripAxis::AlongX, q.back, q.inward, -a, false, -wo, true));
		add (E::Moulding, q.side, 6, StripMesh (crown, StripAxis::AlongX, q.back, q.inward, wo, true, a, false));
	}
	return pieces;
}

PorchExtensionLayout LayoutGreekPorchExtension (const PorchExtensionSettings &settings, const EpicranitisLayout &crown,
                                               double ringInnerHalfWidth, double entablatureFoot)
{
	const CellaPorchEntablature &p = crown.porch;
	if (!(std::isfinite (settings.eastSpan) && settings.eastSpan > 0.0))
		FailExtension ("porchExtension.eastSpan", "n'est pas une longueur finie > 0");
	if (!BeforeInFloat (p.halfWidth, ringInnerHalfWidth))
		FailExtension ("porchExtension", "ne trouve pas la face interieure de l'anneau au-dela de la face exterieure des antes");
	if (!(std::fabs ((p.halfWidth + settings.eastSpan) - ringInnerHalfWidth) <= kPorchExtensionClosure))
	{
		char closure[32];
		std::snprintf (closure, sizeof closure, "%g", kPorchExtensionClosure);
		FailExtension ("porchExtension.eastSpan", std::string ("ne ferme pas l'ecart de la face exterieure des antes a la "
		                                                       "face interieure de l'anneau a ") + closure + " m pres");
	}
	if (!(entablatureFoot < p.architraveTop))
		FailExtension ("porchExtension", "pose l'architrave prolongee au-dessus de l'architrave des porches");
	return {p.halfWidth, ringInnerHalfWidth, entablatureFoot};
}

std::vector<PorchExtensionPiece> BuildGreekPorchExtensionPieces (const EpicranitisSettings &s, const EpicranitisLayout &crown,
                                                                 const PorchExtensionLayout &l)
{
	const CellaPorchEntablature &p = crown.porch;
	const std::vector<std::array<double, 2>> profile = Profile (s, crown, p.friezeTop);
	std::vector<PorchExtensionPiece> pieces;
	pieces.reserve (10);
	using E = PorchExtensionElement;
	for (const PeristyleSide side : {PeristyleSide::Left, PeristyleSide::Right})
	{
		const double x0 = side == PeristyleSide::Left ? -l.outer : l.inner;
		const double x1 = side == PeristyleSide::Left ? -l.inner : l.outer;
		auto add = [&pieces, side] (E element, int index, std::unique_ptr<Mesh> mesh) {
			pieces.push_back ({element, side, index, std::move (mesh)});
		};
		add (E::Architrave, 0, BoxMesh (x0, x1, p.front, p.frontBack, l.foot, p.architraveTop));
		add (E::Frieze, 0, BoxMesh (x0, x1, p.front, p.frontBack, p.architraveTop, p.friezeTop));
		add (E::Crown, 0, BoxMesh (x0, x1, p.front, p.frontBack, p.friezeTop, crown.top));
		add (E::Moulding, 0, StripMesh (profile, StripAxis::AlongX, p.front, StripOutward::Decreasing, x0, false, x1, false));
		add (E::Moulding, 1, StripMesh (profile, StripAxis::AlongX, p.frontBack, StripOutward::Increasing, x0, false, x1, false));
	}
	return pieces;
}

} // namespace classical
} // namespace cgarchi
