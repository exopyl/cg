#include <cgarchi/classical_greek_naos_interior.h>

#include <algorithm>
#include <array>
#include <cmath>
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
	throw std::invalid_argument (std::string ("Interieur : 'interior.") + field + "' " + rule);
}

// Produit arrondi en double avant toute somme : jamais contracte en FMA, meme
// position sur toute cible.
double Product (double a, double b)
{
	const volatile double p = a * b;
	return p;
}

// Ecart relatif admis entre une longueur et la somme de travees qui la ferme :
// au-dessus du bruit de sommes en double, en deca de toute cote ecrite.
constexpr double kClosureTolerance = 1e-9;

void RequireLengths (const NaosInteriorSettings &s)
{
	const std::pair<const char *, double> lengths[] = {
		{"spacing", s.spacing},
		{"frontAxisOffset", s.frontAxisOffset},
		{"backAxisOffset", s.backAxisOffset},
		{"wallAxisOffset", s.wallAxisOffset},
		{"naveLevel", s.naveLevel},
		{"lowerEpistyleHeight", s.lowerEpistyleHeight},
		{"lowerEpistyleSoffit", s.lowerEpistyleSoffit},
		{"upperEpistyleHeight", s.upperEpistyleHeight},
		{"upperEpistyleSoffit", s.upperEpistyleSoffit},
		{"courseHeight", s.courseHeight},
		{"courseThickness", s.courseThickness},
		{"corniceHeight", s.corniceHeight},
		{"corniceProjection", s.corniceProjection},
		{"pocketCeiling", s.pocketCeiling},
		{"pocketMargin", s.pocketMargin},
	};
	for (const auto &[field, value] : lengths)
		if (!(std::isfinite (value) && value > 0.0))
			Fail (field, "n'est pas une longueur finie > 0");
}

// Nombre entier de travees `pitch` qui ferme `length` a kClosureTolerance pres ;
// 0 si aucun.
long ClosingBays (double length, double pitch)
{
	const double bays = length / pitch;
	if (!(std::isfinite (bays) && bays >= 0.5 && bays < 1e6))
		return 0;
	const long n = std::lround (bays);
	return std::fabs (Product (static_cast<double> (n), pitch) - length) <= kClosureTolerance * length ? n : 0;
}

// Trois prismes en Pi (Left, Right, Back) de la ligne des axes X = -+a, Y = yW
// (fond), partant de Y = yE : face cote nef a hn de la ligne des axes, face cote
// mur ou cote fond a hb ; onglets a 45 degres au retour du fond.
std::array<std::unique_ptr<Mesh>, 3> PiPrisms (double a, double yE, double yW, double hn, double hb, double z0, double z1)
{
	using detail::Extrusion;
	using detail::PrismMesh;
	const double xn = a - hn, xb = a + hb, yn = yW - hn, yb = yW + hb;
	return {PrismMesh ({{-xn, yE}, {-xn, yn}, {-xb, yb}, {-xb, yE}}, Extrusion::AlongZ, z0, z1),
	        PrismMesh ({{xn, yE}, {xb, yE}, {xb, yb}, {xn, yn}}, Extrusion::AlongZ, z0, z1),
	        PrismMesh ({{-xn, yn}, {xn, yn}, {xb, yb}, {-xb, yb}}, Extrusion::AlongZ, z0, z1)};
}

} // namespace

NaosInteriorLayout LayoutGreekNaosInterior (const NaosInteriorSettings &s, const CellaLayout &cella)
{
	RequireLengths (s);
	const CellaNaos &naos = cella.naos;
	NaosInteriorLayout l;
	l.naos = naos;
	l.lowerFoot = cella.stylobateTop + s.naveLevel;
	if (!(l.lowerFoot == naos.floor))
		Fail ("naveLevel", "differe du sol du naos : la nef abaissee n'est pas construite (registre CE-15)");

	const double length = naos.back - naos.front;
	const long bays = ClosingBays ((length - s.frontAxisOffset) - s.backAxisOffset, s.spacing);
	if (bays < 1)
		Fail ("backAxisOffset", "et 'interior.frontAxisOffset' ne laissent pas un nombre entier de travees "
		                        "'interior.spacing' dans la longueur du naos");
	l.axisX = naos.inner - s.wallAxisOffset;
	if (!(l.axisX > 0.0))
		Fail ("wallAxisOffset", "porte les axes des flancs au-dela de l'axe longitudinal");
	const long backBays = ClosingBays (2.0 * l.axisX, s.spacing);
	if (backBays < 1)
		Fail ("wallAxisOffset", "ne laisse pas un nombre entier de travees 'interior.spacing' entre les flancs");
	l.frontAxisY = naos.front + s.frontAxisOffset;
	l.backAxisY = l.frontAxisY + Product (static_cast<double> (bays), s.spacing);

	l.lowerExtents = ComputeGreekDoricColumnHalfExtents (s.lowerColumn);
	l.upperExtents = ComputeGreekDoricColumnHalfExtents (s.upperColumn);
	const double e = std::max (l.lowerExtents.overall, l.upperExtents.overall);
	const double backPitch = 2.0 * l.axisX / static_cast<double> (backBays);
	if (!(l.axisX + e < naos.inner))
		Fail ("wallAxisOffset", "met une colonne au contact d'un mur long");
	if (!(l.frontAxisY - e > naos.front))
		Fail ("frontAxisOffset", "met une colonne au contact du mur de refend avant");
	if (!(l.backAxisY + e < naos.back))
		Fail ("backAxisOffset", "met une colonne au contact du mur de refend arriere");
	if (!(s.spacing > 2.0 * e && backPitch > 2.0 * e))
		Fail ("spacing", "met deux colonnes voisines au contact");

	const double hLower = s.lowerEpistyleSoffit / 2.0, hUpper = s.upperEpistyleSoffit / 2.0;
	if (!(l.axisX + hLower < naos.inner && l.axisX - hLower > 0.0 && l.backAxisY + hLower < naos.back))
		Fail ("lowerEpistyleSoffit", "met l'epistyle inferieur au contact d'un mur ou au-dela de l'axe longitudinal");
	if (!(l.axisX + hUpper < naos.inner && l.axisX - hUpper > 0.0 && l.backAxisY + hUpper < naos.back))
		Fail ("upperEpistyleSoffit", "met l'epistyle superieur au contact d'un mur ou au-dela de l'axe longitudinal");
	if (!(l.axisX - hUpper - s.corniceProjection > 0.0))
		Fail ("corniceProjection", "porte la corniche au-dela de l'axe longitudinal");
	if (!detail::InnerCourseWithinWalls (naos, detail::InnerCourseFaces (naos, s.courseThickness)))
		Fail ("courseThickness", "sort l'assise XVII de l'epaisseur d'un mur");

	l.lowerEpistyleFoot = static_cast<double> (static_cast<float> (s.lowerColumn.height)) + l.lowerFoot;
	l.upperFoot = l.lowerEpistyleFoot + s.lowerEpistyleHeight;
	l.upperEpistyleFoot = static_cast<double> (static_cast<float> (s.upperColumn.height)) + l.upperFoot;
	l.corniceFoot = l.upperEpistyleFoot + s.upperEpistyleHeight;
	l.corniceTop = l.corniceFoot + s.corniceHeight;
	l.courseTop = naos.wallTop + s.courseHeight;
	l.top = std::max (l.corniceTop, l.courseTop);

	if (!(s.pocketMargin >= s.courseThickness))
		Fail ("pocketMargin", "est plus etroit que 'interior.courseThickness' : l'assise XVII traverserait le comble");
	l.pocket.x0 = -(naos.inner + s.pocketMargin);
	l.pocket.x1 = naos.inner + s.pocketMargin;
	l.pocket.y0 = naos.front - s.pocketMargin;
	l.pocket.y1 = naos.back + s.pocketMargin;
	l.pocket.zTop = cella.stylobateTop + s.pocketCeiling;
	if (!(l.pocket.zTop > l.top))
		Fail ("pocketCeiling", "n'est pas au-dessus de la cote la plus haute de l'interieur");

	const long perFlank = bays + 1;
	l.columns.reserve (static_cast<std::size_t> (2 * (2 * perFlank + backBays - 1)));
	for (const NaosInteriorStorey storey : {NaosInteriorStorey::Lower, NaosInteriorStorey::Upper})
	{
		const double z = storey == NaosInteriorStorey::Lower ? l.lowerFoot : l.upperFoot;
		for (long k = 0; k < perFlank; ++k)
			l.columns.push_back ({storey, PeristyleSide::Left, static_cast<int> (k), -l.axisX,
			                      l.frontAxisY + Product (static_cast<double> (k), s.spacing), z});
		for (long j = 1; j < backBays; ++j)
			l.columns.push_back (
				{storey, PeristyleSide::Back, static_cast<int> (j - 1), -l.axisX + Product (static_cast<double> (j), backPitch), l.backAxisY, z});
		for (long k = 0; k < perFlank; ++k)
			l.columns.push_back ({storey, PeristyleSide::Right, static_cast<int> (k), l.axisX,
			                      l.frontAxisY + Product (static_cast<double> (k), s.spacing), z});
	}
	return l;
}

std::vector<NaosInteriorPiece> BuildGreekNaosInteriorPieces (const NaosInteriorSettings &s, const NaosInteriorLayout &l)
{
	using detail::BoxMesh;
	const CellaNaos &n = l.naos;
	std::vector<NaosInteriorPiece> pieces;
	pieces.reserve (13);
	auto add = [&pieces] (NaosInteriorElement element, PeristyleSide side, std::unique_ptr<Mesh> mesh) {
		pieces.push_back ({element, side, 0, std::move (mesh)});
	};
	// Assise XVII : flancs sur toute la longueur, coins compris, puis avant et arriere entre eux.
	const detail::InnerCourse course = detail::InnerCourseFaces (n, s.courseThickness);
	const double xo = course.side, yf = course.front, yb = course.back;
	add (NaosInteriorElement::Course, PeristyleSide::Left, BoxMesh (-xo, -n.inner, yf, yb, n.wallTop, l.courseTop));
	add (NaosInteriorElement::Course, PeristyleSide::Right, BoxMesh (n.inner, xo, yf, yb, n.wallTop, l.courseTop));
	add (NaosInteriorElement::Course, PeristyleSide::Front, BoxMesh (-n.inner, n.inner, yf, n.front, n.wallTop, l.courseTop));
	add (NaosInteriorElement::Course, PeristyleSide::Back, BoxMesh (-n.inner, n.inner, n.back, yb, n.wallTop, l.courseTop));

	const PeristyleSide sides[3] = {PeristyleSide::Left, PeristyleSide::Right, PeristyleSide::Back};
	const double hLower = s.lowerEpistyleSoffit / 2.0, hUpper = s.upperEpistyleSoffit / 2.0;
	const struct
	{
		NaosInteriorElement element;
		double hn, hb, z0, z1;
	} rows[] = {
		{NaosInteriorElement::LowerEpistyle, hLower, hLower, l.lowerEpistyleFoot, l.upperFoot},
		{NaosInteriorElement::UpperEpistyle, hUpper, hUpper, l.upperEpistyleFoot, l.corniceFoot},
		{NaosInteriorElement::Cornice, hUpper + s.corniceProjection, hUpper, l.corniceFoot, l.corniceTop},
	};
	for (const auto &row : rows)
	{
		std::array<std::unique_ptr<Mesh>, 3> pi = PiPrisms (l.axisX, n.front, l.backAxisY, row.hn, row.hb, row.z0, row.z1);
		for (int k = 0; k < 3; ++k)
			add (row.element, sides[k], std::move (pi[static_cast<std::size_t> (k)]));
	}
	return pieces;
}

} // namespace classical
} // namespace cgarchi
