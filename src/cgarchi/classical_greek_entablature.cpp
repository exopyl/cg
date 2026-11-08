#include <cgarchi/classical_greek_entablature.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <cgarchi/classical_mesh.h>
#include <cgarchi/classical_partition.h>
#include "mesh.h"

namespace cgarchi
{
namespace classical
{

namespace
{

[[noreturn]] void Fail (const std::string &field, const std::string &rule)
{
	throw std::invalid_argument ("Entablement : '" + field + "' " + rule);
}

[[noreturn]] void FailRing (const std::string &rule)
{
	throw std::invalid_argument ("Anneau : " + rule);
}

bool FinitePositive (double v)
{
	return std::isfinite (v) && v > 0.0;
}

std::string BandField (std::size_t k, const char *member)
{
	return "bands[" + std::to_string (k) + "]." + member;
}

void RequireCount (const char *field, int value, int lo, int hi)
{
	if (value < lo || value > hi)
		Fail (field, "= " + std::to_string (value) + " hors de [" + std::to_string (lo) + ", " + std::to_string (hi) + "]");
}

// p q - r s en evaluation stricte : chaque produit est arrondi en double avant
// la difference (lecture volatile), jamais contracte en FMA, quels que soient
// le compilateur et ses drapeaux. Les tests d'alignement exact (== 0) des
// orientations donnent donc le meme verdict sur toute cible : les flancs de
// canaux a 45 degres des deux bras d'une piece d'angle sont alignes et
// disjoints, et une orientation contractee y laisse un residu de signe
// arbitraire, lu comme une traversee.
double StrictCrossDifference (double p, double q, double r, double s)
{
	const volatile double left = p * q;
	const volatile double right = r * s;
	return left - right;
}

// Orientation de (a, b, c) : > 0 si le tour est direct.
double Orient (const RingSectionPoint &a, const RingSectionPoint &b, const RingSectionPoint &c)
{
	return StrictCrossDifference (b.u - a.u, c.z - a.z, b.z - a.z, c.u - a.u);
}

// c, colineaire a [a, b], est dans la boite du segment.
bool WithinBox (const RingSectionPoint &a, const RingSectionPoint &b, const RingSectionPoint &c)
{
	return std::min (a.u, b.u) <= c.u && c.u <= std::max (a.u, b.u) && std::min (a.z, b.z) <= c.z &&
	       c.z <= std::max (a.z, b.z);
}

// Deux segments fermes se touchent ou se coupent.
bool SegmentsMeet (const RingSectionPoint &p1, const RingSectionPoint &p2, const RingSectionPoint &p3,
                   const RingSectionPoint &p4)
{
	const double d1 = Orient (p3, p4, p1), d2 = Orient (p3, p4, p2), d3 = Orient (p1, p2, p3), d4 = Orient (p1, p2, p4);
	if (((d1 > 0.0 && d2 < 0.0) || (d1 < 0.0 && d2 > 0.0)) && ((d3 > 0.0 && d4 < 0.0) || (d3 < 0.0 && d4 > 0.0)))
		return true;
	return (d1 == 0.0 && WithinBox (p3, p4, p1)) || (d2 == 0.0 && WithinBox (p3, p4, p2)) ||
	       (d3 == 0.0 && WithinBox (p1, p2, p3)) || (d4 == 0.0 && WithinBox (p1, p2, p4));
}

// Polygone simple : aretes non voisines disjointes, aretes voisines sans
// repli (deux aretes colineaires de sens opposes se recouvrent).
bool IsSimple (const std::vector<RingSectionPoint> &s)
{
	const std::size_t n = s.size ();
	for (std::size_t i = 0; i < n; ++i)
	{
		const RingSectionPoint &a = s[i], &b = s[(i + 1) % n], &c = s[(i + 2) % n];
		const double du0 = b.u - a.u, dz0 = b.z - a.z, du1 = c.u - b.u, dz1 = c.z - b.z;
		if (StrictCrossDifference (du0, dz1, dz0, du1) == 0.0 && du0 * du1 + dz0 * dz1 < 0.0)
			return false;
		for (std::size_t j = i + 2; j < n; ++j)
		{
			if ((j + 1) % n == i)
				continue;
			if (SegmentsMeet (a, b, s[j], s[(j + 1) % n]))
				return false;
		}
	}
	return true;
}

// Deux fois l'aire signee (formule du lacet).
double TwiceSignedArea (const std::vector<RingSectionPoint> &s)
{
	double sum = 0.0;
	for (std::size_t i = 0; i < s.size (); ++i)
	{
		const RingSectionPoint &a = s[i], &b = s[(i + 1) % s.size ()];
		sum += a.u * b.z - b.u * a.z;
	}
	return sum;
}

// Profil de face du triglyphe en (a, d), a croissant : canaux en V ouverts sur
// la face (face), demi-canaux aux bords. `bounds` : bornes des divisions.
std::vector<std::array<double, 2>> TriglyphProfile (const GreekTriglyphSettings &t, const std::vector<double> &bounds,
                                                    double face)
{
	std::vector<std::array<double, 2>> s;
	const std::size_t n = t.divisionWidths.size ();
	auto push = [&] (double u, double v) {
		if (s.empty () || std::fabs (s.back ()[0] - u) > 1e-12 || std::fabs (s.back ()[1] - v) > 1e-12)
			s.push_back ({u, v});
	};
	for (std::size_t i = 0; i < n; ++i)
	{
		const double a = bounds[i], next = bounds[i + 1];
		if (!t.divisionIsChannel[i])
		{
			push (a, face);
			push (next, face);
		}
		else if (i == 0)
		{
			push (a, face - t.halfChannelDepth);
			push (next, face);
		}
		else if (i + 1 == n)
			push (next, face - t.halfChannelDepth);
		else
		{
			push (0.5 * (a + next), face - t.channelDepth);
			push (next, face);
		}
	}
	return s;
}

// Prisme vertical d'une section monotone en a : fond droit en d = back, profil
// `top` (a strictement croissant, d > back) ; `map` envoie (a, d) sur (x, y)
// monde. Faces planes triangulees en bandes entre le fond et le profil.
template <class Map>
void PrismSection (const std::vector<std::array<double, 2>> &top, double back, Map map, double z0, double z1,
                   std::vector<std::array<double, 3>> &positions, std::vector<std::array<unsigned, 3>> &triangles)
{
	const unsigned n = static_cast<unsigned> (top.size ());
	// Sens de l'application (a, d) -> (x, y) : un determinant negatif retourne
	// les faces.
	const std::array<double, 2> o = map (0.0, 0.0), ea = map (1.0, 0.0), ed = map (0.0, 1.0);
	const bool flip = (ea[0] - o[0]) * (ed[1] - o[1]) - (ea[1] - o[1]) * (ed[0] - o[0]) < 0.0;
	auto tri = [&] (unsigned a, unsigned b, unsigned c) {
		if (flip)
			triangles.push_back ({a, c, b});
		else
			triangles.push_back ({a, b, c});
	};
	const unsigned base = static_cast<unsigned> (positions.size ());
	// Par point i : fond bas, profil bas, fond haut, profil haut.
	for (const std::array<double, 2> &q : top)
	{
		const std::array<double, 2> f = map (q[0], back), t = map (q[0], q[1]);
		positions.push_back ({f[0], f[1], z0});
		positions.push_back ({t[0], t[1], z0});
		positions.push_back ({f[0], f[1], z1});
		positions.push_back ({t[0], t[1], z1});
	}
	auto B = [&] (unsigned i, int level) { return base + 4 * i + (level ? 2u : 0u); };
	auto T = [&] (unsigned i, int level) { return base + 4 * i + (level ? 3u : 1u); };
	for (unsigned i = 0; i + 1 < n; ++i)
	{
		// Dessus (z1) : (B_i, B_i+1, T_i+1, T_i) en sens direct dans (a, d).
		tri (B (i, 1), B (i + 1, 1), T (i + 1, 1));
		tri (B (i, 1), T (i + 1, 1), T (i, 1));
		// Dessous (z0) : sens inverse.
		tri (B (i, 0), T (i + 1, 0), B (i + 1, 0));
		tri (B (i, 0), T (i, 0), T (i + 1, 0));
		// Parois du fond et du profil.
		tri (B (i, 0), B (i + 1, 0), B (i + 1, 1));
		tri (B (i, 0), B (i + 1, 1), B (i, 1));
		tri (T (i + 1, 0), T (i, 0), T (i, 1));
		tri (T (i + 1, 0), T (i, 1), T (i + 1, 1));
	}
	// Parois des deux bouts.
	tri (T (0, 0), B (0, 0), B (0, 1));
	tri (T (0, 0), B (0, 1), T (0, 1));
	tri (B (n - 1, 0), T (n - 1, 0), T (n - 1, 1));
	tri (B (n - 1, 0), T (n - 1, 1), B (n - 1, 1));
}

// Axes d'un cote dans son sens de parcours : ses colonnes, puis l'angle qui
// ouvre le cote suivant ; coordonnee le long du cote (X ou Y).
std::vector<double> SideAxes (const PeristyleLayout &p, PeristyleSide side)
{
	const bool alongX = side == PeristyleSide::Front || side == PeristyleSide::Back;
	const PeristyleSide next = static_cast<PeristyleSide> ((static_cast<int> (side) + 1) % 4);
	std::vector<double> axes;
	const PeristyleColumn *nextCorner = nullptr;
	for (const PeristyleColumn &c : p.columns)
	{
		if (c.side == side)
			axes.push_back (alongX ? c.x : c.y);
		else if (c.side == next && c.corner)
			nextCorner = &c;
	}
	if (!nextCorner)
		throw std::logic_error ("Entablement : angle du cote suivant absent du peristyle");
	axes.push_back (alongX ? nextCorner->x : nextCorner->y);
	return axes;
}

bool AlongX (PeristyleSide side)
{
	return side == PeristyleSide::Front || side == PeristyleSide::Back;
}

// Cote parcouru a croissant (Front, Right) : son angle ouvrant est a son
// extremite negative, son angle fermant a son extremite positive.
bool Ascending (PeristyleSide side)
{
	return side == PeristyleSide::Front || side == PeristyleSide::Right;
}

PeristyleSide Previous (PeristyleSide side)
{
	return static_cast<PeristyleSide> ((static_cast<int> (side) + 3) % 4);
}

// (a, d) -> (x, y) d'un cote, par les expressions de l'anneau (SweepRectangularRing).
std::array<double, 2> PlanPoint (PeristyleSide side, double hx, double hy, double a, double d)
{
	switch (side)
	{
	case PeristyleSide::Front: return {a, -(hy + d)};
	case PeristyleSide::Right: return {hx + d, a};
	case PeristyleSide::Back:  return {a, hy + d};
	case PeristyleSide::Left:  return {-(hx + d), a};
	}
	return {};
}

struct CornerArms
{
	double a0 = 0.0, a1 = 0.0;     // bras du cote ouvert
	double ra0 = 0.0, ra1 = 0.0;   // bras de retour, sur le cote precedent
};

// Bras de la piece d'angle qui ouvre `side` (EN14) : bout exterieur a
// +-(H + face), au plan des faces des triglyphes du cote voisin, l'autre bout a
// w en deca. `along` : demi-distance des axes le long du cote ; `across` : celle
// du cote precedent, qui est la demi-distance perpendiculaire au cote. Ecritures
// sans produit : les deux bouts d'une rangee sont des negations exactes.
CornerArms CornerTriglyphArms (PeristyleSide side, double along, double across, double face, double width)
{
	CornerArms arms;
	const double outer = along + face, inner = outer - width;
	if (Ascending (side))
	{
		arms.a0 = -outer;
		arms.a1 = -inner;
	}
	else
	{
		arms.a0 = inner;
		arms.a1 = outer;
	}
	const double returnOuter = across + face, returnInner = returnOuter - width;
	if (Ascending (Previous (side)))
	{
		arms.ra0 = returnInner;
		arms.ra1 = returnOuter;
	}
	else
	{
		arms.ra0 = -returnOuter;
		arms.ra1 = -returnInner;
	}
	return arms;
}

struct StraightPlacements
{
	std::vector<double> centres;   // triglyphes droits d'un cote, a croissant
	double lo = 0.0, hi = 0.0;     // bornes extremes des metopes du cote
};

// Centres des triglyphes droits d'un cote (axes a croissant, angles compris) et
// bornes extremes de ses metopes. AxisTriglyph : chaque axe et le milieu de
// chaque travee, metopes jusqu'au coin du nu des metopes (+-(along + back)).
// Strategies d'angle : les axes interieurs et un intermediaire par travee, la
// piece d'angle a part ; metopes jusqu'aux bouts interieurs des bras d'angle,
// +-((along + face) - width).
StraightPlacements StraightCentres (DoricCornerStrategy strategy, const std::vector<double> &axes, double along,
                                    double back, double face, double width)
{
	StraightPlacements out;
	std::vector<double> &centres = out.centres;
	centres.reserve (2 * axes.size () - 1);
	const double half = 0.5 * width;
	if (strategy == DoricCornerStrategy::AxisTriglyph)
	{
		for (std::size_t i = 0; i + 1 < axes.size (); ++i)
		{
			centres.push_back (axes[i]);
			centres.push_back (EqualParts (axes[i], axes[i + 1], 2, "peristyle")[1]);
		}
		centres.push_back (axes.back ());
		const double corner = along + back;
		out.lo = -corner;
		out.hi = corner;
		return out;
	}
	const double outer = along + face, inner = outer - width, armCentre = outer - half;
	const std::size_t n = axes.size ();
	for (std::size_t i = 0; i + 1 < n; ++i)
	{
		if (i > 0)
			centres.push_back (axes[i]);
		double from = axes[i], to = axes[i + 1];
		if (strategy == DoricCornerStrategy::CornerTriglyphEqualMetopes)
		{
			if (i == 0)
				from = -armCentre;
			if (i + 2 == n)
				to = armCentre;
		}
		centres.push_back (EqualParts (from, to, 2, "peristyle")[1]);
	}
	out.lo = -inner;
	out.hi = inner;
	return out;
}

// Profil de face d'un bras [a0, a1] : celui d'un triglyphe droit a cette place.
std::vector<std::array<double, 2>> ArmProfile (const GreekTriglyphSettings &t, double a0, double a1, double face)
{
	std::vector<double> bounds = CumulativeBounds (a0, t.divisionWidths, "triglyph.divisionWidths");
	bounds.back () = a1;
	return TriglyphProfile (t, bounds, face);
}

// Section en L de la piece d'angle qui ouvre `side` (EN14), en (x, y) du plan :
// dos du bras ouvert ; profil de ce bras du bout interieur vers l'arete, sans
// le point d'arete ; profil du retour depuis l'arete, sans le point d'arete ;
// dos du retour ; coin du nu des metopes. Le segment entre les deux profils est
// le chanfrein commun, d'une bande extreme a l'autre.
std::vector<std::array<double, 2>> CornerSection (const GreekTriglyphSettings &t, double back, double face,
                                                  PeristyleSide side, double hx, double hy, const CornerArms &arms)
{
	const PeristyleSide previous = Previous (side);
	std::vector<std::array<double, 2>> open = ArmProfile (t, arms.a0, arms.a1, face);
	if (Ascending (side))
		std::reverse (open.begin (), open.end ());
	std::vector<std::array<double, 2>> ret = ArmProfile (t, arms.ra0, arms.ra1, face);
	if (Ascending (previous))
		std::reverse (ret.begin (), ret.end ());
	const double openInner = Ascending (side) ? arms.a1 : arms.a0;
	const double returnInner = Ascending (previous) ? arms.ra0 : arms.ra1;
	const double along = AlongX (side) ? hx : hy;
	const double cornerA = Ascending (side) ? -(along + back) : along + back;

	std::vector<std::array<double, 2>> s;
	s.reserve (open.size () + ret.size () + 1);
	s.push_back (PlanPoint (side, hx, hy, openInner, back));
	for (std::size_t i = 0; i + 1 < open.size (); ++i)
		s.push_back (PlanPoint (side, hx, hy, open[i][0], open[i][1]));
	for (std::size_t i = 1; i < ret.size (); ++i)
		s.push_back (PlanPoint (previous, hx, hy, ret[i][0], ret[i][1]));
	s.push_back (PlanPoint (previous, hx, hy, returnInner, back));
	s.push_back (PlanPoint (side, hx, hy, cornerA, back));
	return s;
}

std::vector<RingSectionPoint> AsRingPoints (const std::vector<std::array<double, 2>> &polygon)
{
	std::vector<RingSectionPoint> out;
	out.reserve (polygon.size ());
	for (const std::array<double, 2> &q : polygon)
		out.push_back ({q[0], q[1]});
	return out;
}

double Cross (const std::array<double, 2> &a, const std::array<double, 2> &b, const std::array<double, 2> &c)
{
	return StrictCrossDifference (b[0] - a[0], c[1] - a[1], b[1] - a[1], c[0] - a[0]);
}

// Decoupe d'oreilles d'un polygone simple de sens direct, en double : P - 2
// triangles de sens direct (indices des sommets). Une oreille est un sommet
// strictement convexe dont le triangle ne contient, bord compris, aucun autre
// sommet restant. Vide si une etape ne trouve aucune oreille.
std::vector<std::array<unsigned, 3>> EarClip (const std::vector<std::array<double, 2>> &polygon)
{
	std::vector<unsigned> left;
	left.reserve (polygon.size ());
	for (std::size_t i = 0; i < polygon.size (); ++i)
		left.push_back (static_cast<unsigned> (i));
	std::vector<std::array<unsigned, 3>> triangles;
	triangles.reserve (polygon.size ());
	while (left.size () > 3)
	{
		const std::size_t n = left.size ();
		bool clipped = false;
		for (std::size_t i = 0; i < n && !clipped; ++i)
		{
			const unsigned a = left[(i + n - 1) % n], b = left[i], c = left[(i + 1) % n];
			if (!(Cross (polygon[a], polygon[b], polygon[c]) > 0.0))
				continue;
			bool empty = true;
			for (std::size_t k = 0; k < n && empty; ++k)
			{
				const unsigned v = left[k];
				if (v == a || v == b || v == c)
					continue;
				empty = !(Cross (polygon[a], polygon[b], polygon[v]) >= 0.0 && Cross (polygon[b], polygon[c], polygon[v]) >= 0.0 &&
				          Cross (polygon[c], polygon[a], polygon[v]) >= 0.0);
			}
			if (!empty)
				continue;
			triangles.push_back ({a, b, c});
			left.erase (left.begin () + static_cast<std::ptrdiff_t> (i));
			clipped = true;
		}
		if (!clipped)
			return {};
	}
	triangles.push_back ({left[0], left[1], left[2]});
	return triangles;
}

enum class PolygonPrismStatus { Built, NotSimple, FloatCollapse, NotTriangulable };

// Prisme vertical d'un polygone simple en (x, y), ajoute a `positions` et
// `triangles` : P sommets a z0, P a z1, parois en quads, couvercles par
// decoupe d'oreilles en double, normales sortantes. Rien n'est ajoute si le
// polygone n'est pas simple, si deux sommets voisins sont confondus une fois
// arrondis en float, ou si la decoupe est incomplete.
PolygonPrismStatus AppendPolygonPrism (std::vector<std::array<double, 2>> section, double z0, double z1,
                                       std::vector<std::array<double, 3>> &positions,
                                       std::vector<std::array<unsigned, 3>> &triangles)
{
	const std::vector<RingSectionPoint> ring = AsRingPoints (section);
	if (!IsSimple (ring))
		return PolygonPrismStatus::NotSimple;
	if (TwiceSignedArea (ring) < 0.0)
		std::reverse (section.begin (), section.end ());
	const std::size_t n = section.size ();
	for (std::size_t i = 0; i < n; ++i)
	{
		const std::array<double, 2> &q = section[i], &r = section[(i + 1) % n];
		if (static_cast<float> (q[0]) == static_cast<float> (r[0]) && static_cast<float> (q[1]) == static_cast<float> (r[1]))
			return PolygonPrismStatus::FloatCollapse;
	}
	const std::vector<std::array<unsigned, 3>> caps = EarClip (section);
	if (caps.size () + 2 != n)
		return PolygonPrismStatus::NotTriangulable;

	const unsigned base = static_cast<unsigned> (positions.size ()), count = static_cast<unsigned> (n);
	for (const double z : {z0, z1})
		for (const std::array<double, 2> &q : section)
			positions.push_back ({q[0], q[1], z});
	// Parois : sommet i en bas, count + i en haut ; normale sortante pour une
	// section de sens direct.
	for (unsigned i = 0; i < count; ++i)
	{
		const unsigned j = (i + 1) % count;
		triangles.push_back ({base + i, base + j, base + count + j});
		triangles.push_back ({base + i, base + count + j, base + count + i});
	}
	for (const std::array<unsigned, 3> &c : caps)
	{
		triangles.push_back ({base + count + c[0], base + count + c[1], base + count + c[2]});
		triangles.push_back ({base + c[0], base + c[2], base + c[1]});
	}
	return PolygonPrismStatus::Built;
}

// Prisme vertical de la section en L d'une piece d'angle (EN14) : P sommets en
// bas, P en haut ; parois en quads, couvercles par decoupe d'oreilles.
std::unique_ptr<Mesh> BuildCornerTriglyphMesh (const GreekEntablatureSettings &s, const PeristyleLayout &p,
                                               const FriezePiece &piece, double zFoot)
{
	const GreekTriglyphSettings &t = s.triglyph;
	const double hx = p.halfWidth, hy = p.halfDepth;
	// Gardes float du prisme droit, sur chaque bras : la demi-distance
	// perpendiculaire au bras ouvert, puis celle perpendiculaire au retour.
	const bool alongX = AlongX (piece.side);
	for (const double across : {alongX ? hy : hx, alongX ? hx : hy})
	{
		const float backPlane = static_cast<float> (across + piece.d0);
		if (!(static_cast<float> (across + (piece.d1 - t.channelDepth)) > backPlane))
			throw std::invalid_argument ("Frise : 'triglyph.channelDepth' met le fond du canal sur le dos une fois arrondi en float");
		if (!(static_cast<float> (across + (piece.d1 - t.halfChannelDepth)) > backPlane))
			throw std::invalid_argument ("Frise : 'triglyph.halfChannelDepth' met le fond du demi-canal sur le dos une fois arrondi en float");
	}

	const std::vector<std::array<double, 2>> section =
		CornerSection (t, piece.d0, piece.d1, piece.side, hx, hy, {piece.a0, piece.a1, piece.ra0, piece.ra1});
	const std::size_t n = section.size ();
	std::vector<std::array<double, 3>> positions;
	positions.reserve (2 * n);
	std::vector<std::array<unsigned, 3>> triangles;
	triangles.reserve (4 * n - 4);
	switch (AppendPolygonPrism (section, zFoot + piece.z0, zFoot + piece.z1, positions, triangles))
	{
	case PolygonPrismStatus::NotSimple:
		throw std::invalid_argument ("Frise : section de la piece d'angle non simple");
	case PolygonPrismStatus::FloatCollapse:
		throw std::invalid_argument ("Frise : 'triglyph.divisionWidths' confond deux sommets de la section d'angle une fois arrondie en float");
	case PolygonPrismStatus::NotTriangulable:
		throw std::invalid_argument ("Frise : section de la piece d'angle non triangulable");
	case PolygonPrismStatus::Built:
		break;
	}
	return MakeTriangleMesh (positions, triangles);
}

// Cotes des jonctions de bandes, sans validation.
std::vector<double> BandBounds (const GreekEntablatureSettings &s)
{
	std::vector<double> heights;
	heights.reserve (s.bands.size ());
	for (const EntablatureBand &b : s.bands)
		heights.push_back (b.height);
	return CumulativeBounds (0.0, heights, "bands");
}

// Section en bandes (contrat de GreekEntablatureSection), reglages supposes
// valides hors la simplicite de la sous-face inclinee ; z : BandBounds.
std::vector<RingSectionPoint> SectionOf (const GreekEntablatureSettings &s, const std::vector<double> &z)
{
	const std::size_t n = s.bands.size ();
	const bool inclined = s.mutules && s.mutules->rise > 0.0;
	const std::size_t mb = inclined ? static_cast<std::size_t> (s.mutules->band) : n;
	std::vector<RingSectionPoint> out;
	out.reserve (2 * n + 3);
	out.push_back ({s.bands.front ().projection, z.front ()});
	for (std::size_t k = 0; k < n; ++k)
	{
		const EntablatureBand &band = s.bands[k];
		if (k == mb)
		{
			// Sous-face inclinee (EN22) : du dos, a la hauteur du dessus du mutule
			// au dos, a la face des mutules, au pied de la bande suivante.
			const double face = s.mutules->face;
			out.push_back ({band.topProjection, z[k + 1] + s.mutules->rise});
			out.push_back ({face, z[k + 1]});
			if (s.bands[k + 1].projection != face)
				out.push_back ({s.bands[k + 1].projection, z[k + 1]});
			continue;
		}
		out.push_back ({band.topProjection, z[k + 1]});
		if (k + 1 == n)
			break;
		const EntablatureBand &next = s.bands[k + 1];
		if (next.projection != band.topProjection)
			out.push_back ({next.projection, z[k + 1]});
		else if (band.projection == band.topProjection && next.projection == next.topProjection)
			out.pop_back ();   // jonction alignee entre deux faces verticales
	}
	out.push_back ({-s.innerFace, z.back ()});
	out.push_back ({-s.innerFace, z.front ()});
	return out;
}

// {diametre de tete, vide, ..., vide, diametre de tete} des rangs de gouttes
// d'un mutule (EN23).
std::vector<double> MutuleGuttaSeries (const GreekMutuleGuttaSettings &g)
{
	std::vector<double> series;
	series.reserve (2 * g.rowDiameters.size ());
	for (std::size_t i = 0; i < g.rowDiameters.size (); ++i)
	{
		if (i > 0)
			series.push_back (g.rowGaps[i - 1]);
		series.push_back (g.rowDiameters[i]);
	}
	return series;
}

// M de la garde du pied : plus forte saillie de ce qui est sous la bande des
// mutules (dos, bandes 0 a k - 1, face des triglyphes et des regulae).
double MutuleFootLimit (const GreekEntablatureSettings &s)
{
	const std::size_t k = static_cast<std::size_t> (s.mutules->band);
	double limit = std::max (s.bands[k].projection, s.triglyph.face);
	// La validation borne regulae.face par la saillie de la tenia, deja dans la
	// boucle : ce terme ne change M que si cette borne change.
	if (s.regulae)
		limit = std::max (limit, s.regulae->face);
	for (std::size_t b = 0; b < k; ++b)
		limit = std::max ({limit, s.bands[b].projection, s.bands[b].topProjection});
	return limit;
}

// Refus des gouttes des mutules, dans l'ordre du contrat ; reglages des bandes
// et des mutules deja valides.
void ValidateMutuleGuttae (const GreekEntablatureSettings &s)
{
	const GreekMutuleSettings &m = *s.mutules;
	const GreekMutuleGuttaSettings &g = *m.guttae;
	const std::size_t rows = g.rowDiameters.size ();
	if (rows < 1 || rows > static_cast<std::size_t> (kGreekMaxGuttae))
		Fail ("mutules.guttae.rowDiameters", "compte " + std::to_string (rows) + " rang(s), hors de [1, " +
		                                     std::to_string (kGreekMaxGuttae) + "]");
	double widest = 0.0;
	for (std::size_t i = 0; i < rows; ++i)
	{
		if (!FinitePositive (g.rowDiameters[i]))
			Fail ("mutules.guttae.rowDiameters", "doit etre fini et strictement positif a chaque rang");
		widest = std::max (widest, g.rowDiameters[i]);
	}
	if (g.rowGaps.size () + 1 != rows)
		Fail ("mutules.guttae.rowGaps", "doit compter un vide de moins que de rangs");
	for (const double gap : g.rowGaps)
		if (!FinitePositive (gap))
			Fail ("mutules.guttae.rowGaps", "doit etre fini et strictement positif a chaque vide");
	const double c = g.conicity;
	bool flareFits = std::isfinite (c) && c >= 0.0;
	for (const double gap : g.rowGaps)
		flareFits = flareFits && c + c < gap;
	if (!flareFits)
		Fail ("mutules.guttae.conicity", "doit etre finie, >= 0, et laisser chaque vide entre tetes plus large que 2 conicity : deux pieds voisins se toucheraient");
	RequireCount ("mutules.guttae.perRow", g.perRow, 2, kGreekMaxGuttae);
	if (rows * static_cast<std::size_t> (g.perRow) > static_cast<std::size_t> (kGreekMaxGuttae))
		Fail ("mutules.guttae.perRow", "donne rangs x perRow = " + std::to_string (rows * static_cast<std::size_t> (g.perRow)) +
		                               " gouttes par mutule, au-dela de " + std::to_string (kGreekMaxGuttae));
	if (!FinitePositive (g.height))
		Fail ("mutules.guttae.height", "doit etre finie et strictement positive");
	const double foot = widest + c + c;
	if (!(foot < g.edgeAxis + g.edgeAxis))
		Fail ("mutules.guttae.edgeAxis", "met le pied des gouttes extremes au bord du mutule ou au-dela : diametre de pied >= 2 edgeAxis");
	const std::vector<double> pitches (static_cast<std::size_t> (g.perRow - 1), foot);
	if (!(CumulativeBounds (0.0, pitches, "mutules.guttae.perRow").back () < m.width - (g.edgeAxis + g.edgeAxis)))
		Fail ("mutules.guttae.perRow", "fait se toucher les pieds de deux gouttes voisines d'un rang : (perRow - 1) pieds >= width - 2 edgeAxis");
	RequireCount ("mutules.guttae.sides", g.sides, 4, kGreekMaxGuttaSides);
	if (g.sides % 4 != 0)
		Fail ("mutules.guttae.sides", "= " + std::to_string (g.sides) + " n'est pas un multiple de 4");

	// Garde du pied (D3-70) : l'arriere du pied de la derniere goutte, a
	// (S' D + H rise) / L de la face, reste devant M ; le point le plus bas des
	// pieds, avant du pied de la goutte avant a (H D + c rise) / L sous z_k,
	// reste au-dessus du pied de l'entablement. Devant M, aucune piece de
	// l'entablement ne descend sous ce pied ; dessous, seuls les chapiteaux.
	const std::size_t k = static_cast<std::size_t> (m.band);
	const double depth = m.face - s.bands[k].projection, clear = m.face - MutuleFootLimit (s);
	const double reach = CumulativeBounds (0.0, MutuleGuttaSeries (g), "mutules.guttae").back () + c;
	const double b = StrictCrossDifference (reach, depth, -g.height, m.rise);
	const double slope = StrictCrossDifference (depth, depth, -m.rise, m.rise);
	if (!(clear > 0.0 && StrictCrossDifference (clear * clear, slope, b, b) > 0.0))
		Fail ("mutules.guttae", "met l'arriere du pied de la derniere goutte au droit d'une bande, d'un triglyphe ou d'une regula sous les mutules, ou derriere");
	double bottom = 0.0;   // z_k, somme courante des hauteurs (BandBounds)
	for (std::size_t i = 0; i < k; ++i)
		bottom += s.bands[i].height;
	const double drop = StrictCrossDifference (g.height, depth, -c, m.rise);
	if (!(StrictCrossDifference (bottom * bottom, slope, drop, drop) > 0.0))
		Fail ("mutules.guttae", "met le point le plus bas des pieds au niveau du pied de l'entablement ou dessous");
}

// {diametre, vide, ..., vide, diametre} des gouttes d'une regula.
std::vector<double> GuttaSeries (const GreekRegulaSettings &r)
{
	std::vector<double> series;
	series.reserve (2 * static_cast<std::size_t> (r.guttaCount));
	for (int k = 0; k < r.guttaCount; ++k)
	{
		if (k > 0)
			series.push_back (r.guttaGap);
		series.push_back (r.gutta.footDiameter);
	}
	return series;
}

// Envergure des gouttes, par somme cumulee (jamais count * diametre).
double GuttaSpan (const GreekRegulaSettings &r)
{
	return CumulativeBounds (0.0, GuttaSeries (r), "regulae.guttaCount").back ();
}

// Marge de chaque bout (EN20) : (width - envergure) / 2.
double GuttaMargin (const GreekRegulaSettings &r, double width)
{
	return 0.5 * (width - GuttaSpan (r));
}

// Refus des ornements, reglages seuls (contrat de ValidateGreekEntablatureSettings).
void ValidateOrnaments (const GreekEntablatureSettings &s)
{
	const GreekTriglyphSettings &t = s.triglyph;
	const std::size_t f = static_cast<std::size_t> (s.friezeBand);
	if (s.regulae)
	{
		const GreekRegulaSettings &r = *s.regulae;
		if (f < 3)
			Fail ("regulae", "exige friezeBand >= 3 : la regula pend sous la tenia (bande friezeBand - 1), devant la bande "
			                 "friezeBand - 2, et le pied de ses gouttes au-dessus de la bande friezeBand - 3");
		const EntablatureBand &below = s.bands[f - 2];
		if (below.topProjection != below.projection)
			Fail (BandField (f - 2, "topProjection"), "doit egaler la saillie du pied : le dos des regulae est vertical");
		if (!(std::isfinite (r.height) && r.height > 0.0 && r.height < below.height))
			Fail ("regulae.height", "doit etre dans ]0, hauteur de la bande friezeBand - 2[");
		const double back = below.projection;
		if (!(std::isfinite (r.face) && back < r.face && r.face <= s.bands[f - 1].projection))
			Fail ("regulae.face", "doit etre dans ]face de la bande friezeBand - 2, saillie de la tenia]");
		RequireCount ("regulae.guttaCount", r.guttaCount, 1, kGreekMaxGuttae);
		if (!FinitePositive (r.guttaGap))
			Fail ("regulae.guttaGap", "doit etre fini et strictement positif");
		const GreekGuttaSettings &g = r.gutta;
		if (!FinitePositive (g.footDiameter))
			Fail ("regulae.gutta.footDiameter", "doit etre fini et strictement positif");
		if (!(std::isfinite (g.footProjection) && g.footProjection <= r.face))
			Fail ("regulae.gutta.footProjection", "doit etre finie et au plus la face de la regula");
		if (!(back < g.footProjection - g.footDiameter))
			Fail ("regulae.gutta.footDiameter", "met l'arriere de la goutte sur la face de la bande friezeBand - 2 ou derriere");
		const double conicity = g.footProjection - g.topProjection;
		if (!(std::isfinite (g.topProjection) && g.topProjection <= g.footProjection && conicity + conicity < g.footDiameter))
			Fail ("regulae.gutta.topProjection", "doit etre finie, au plus la saillie du pied, et laisser un diametre de tete > 0");
		RequireCount ("regulae.gutta.sides", g.sides, 4, kGreekMaxGuttaSides);
		if (g.sides % 4 != 0)
			Fail ("regulae.gutta.sides", "= " + std::to_string (g.sides) + " n'est pas un multiple de 4");
		if (!(s.bands[f - 3].topProjection < g.footProjection - g.footDiameter))
			Fail (BandField (f - 3, "topProjection"), "atteint l'arriere du pied des gouttes : elles poseraient sur le ressaut de cette bande");
		if (!(GuttaSpan (r) < t.width))
			Fail ("regulae.guttaCount", "donne une envergure des gouttes (diametres et vides) >= 'triglyph.width'");
		if (s.corner == DoricCornerStrategy::AxisTriglyph)
		{
			if (!(0.5 * t.width < back))
				Fail ("triglyph.width", "fait toucher les regulae d'angle de deux cotes voisins : width / 2 >= face de la bande friezeBand - 2");
		}
		else
		{
			if (!(t.face - t.width < back))
				Fail ("triglyph.width", "rend la regula d'angle degeneree : face - width >= face de la bande friezeBand - 2");
			if (!(t.face - GuttaMargin (r, t.width) < g.footProjection - g.footDiameter))
				Fail ("regulae.guttaGap", "fait se toucher les gouttes d'angle des deux bras : face - marge >= footProjection - footDiameter");
		}
	}
	if (s.mutules)
	{
		const GreekMutuleSettings &m = *s.mutules;
		if (!(m.band >= s.friezeBand + 1 && static_cast<std::size_t> (m.band) + 2 <= s.bands.size ()))
			Fail ("mutules.band", "doit designer une bande au-dessus de la frise, ni la derniere, dans [friezeBand + 1, " +
			                      std::to_string (s.bands.size ()) + " - 2]");
		const std::size_t k = static_cast<std::size_t> (m.band);
		if (s.bands[k].topProjection != s.bands[k].projection)
			Fail (BandField (k, "topProjection"), "doit egaler la saillie du pied : le dos des mutules est vertical");
		if (!FinitePositive (m.width))
			Fail ("mutules.width", "doit etre fini et strictement positif");
		if (!(std::isfinite (m.face) && s.bands[k].projection < m.face && m.face <= s.bands[k + 1].projection))
			Fail ("mutules.face", "doit etre dans ]saillie de sa bande, saillie de la bande suivante]");
		if (!(std::isfinite (m.rise) && m.rise >= 0.0))
			Fail ("mutules.rise", "doit etre fini et >= 0");
		if (m.rise > 0.0 && !IsSimple (SectionOf (s, BandBounds (s))))
			Fail ("mutules.rise", "rend la section de l'anneau non simple : la sous-face inclinee traverse ou touche une autre face");
		if (m.guttae)
			ValidateMutuleGuttae (s);
	}
}

// Placements des ornements sur une frise deja placee, sans validation.
OrnamentLayout OrnamentsOf (const GreekEntablatureSettings &s, const PeristyleLayout &p, const FriezeLayout &frieze)
{
	OrnamentLayout out;
	if (!s.regulae && !s.mutules)
		return out;
	const std::vector<double> z = BandBounds (s);
	const std::size_t f = static_cast<std::size_t> (s.friezeBand);
	const double hx = p.halfWidth, hy = p.halfDepth;
	if (s.regulae)
	{
		const GreekRegulaSettings &r = *s.regulae;
		const double back = s.bands[f - 2].projection, top = z[f - 1], bottom = top - r.height;
		const double axis = r.gutta.footProjection - 0.5 * r.gutta.footDiameter;
		const double margin = GuttaMargin (r, s.triglyph.width);
		std::vector<double> parts {margin};
		const std::vector<double> series = GuttaSeries (r);
		parts.insert (parts.end (), series.begin (), series.end ());
		parts.push_back (margin);
		auto addGuttae = [&] (PeristyleSide side, double a0, double a1) {
			std::vector<double> b = CumulativeBounds (a0, parts, "regulae.guttaCount");
			b.back () = a1;
			for (int k = 0; k < r.guttaCount; ++k)
			{
				GuttaPlacement g;
				g.side = side;
				g.a = 0.5 * (b[static_cast<std::size_t> (1 + 2 * k)] + b[static_cast<std::size_t> (2 + 2 * k)]);
				g.axis = axis;
				g.z0 = z[f - 2];
				g.z1 = bottom;
				out.guttae.push_back (g);
			}
		};
		out.regulae.reserve (frieze.triglyphs.size ());
		for (const FriezePiece &q : frieze.triglyphs)
		{
			OrnamentPiece o;
			o.side = q.side;
			o.rank = q.rank;
			o.d0 = back;
			o.d1 = r.face;
			o.z0 = bottom;
			o.z1 = top;
			o.firstGutta = static_cast<int> (out.guttae.size ());
			if (q.kind == FriezeElement::CornerTriglyph)
			{
				o.kind = OrnamentElement::CornerRegula;
				const double along = AlongX (q.side) ? hx : hy, across = AlongX (q.side) ? hy : hx;
				if (Ascending (q.side))
				{
					o.a0 = -(along + r.face);
					o.a1 = q.a1;
				}
				else
				{
					o.a0 = q.a0;
					o.a1 = along + r.face;
				}
				if (Ascending (Previous (q.side)))
				{
					o.ra0 = q.ra0;
					o.ra1 = across + r.face;
				}
				else
				{
					o.ra0 = -(across + r.face);
					o.ra1 = q.ra1;
				}
				addGuttae (q.side, q.a0, q.a1);
				addGuttae (Previous (q.side), q.ra0, q.ra1);
			}
			else
			{
				o.kind = OrnamentElement::Regula;
				o.a0 = q.a0;
				o.a1 = q.a1;
				addGuttae (q.side, q.a0, q.a1);
			}
			o.guttaCount = static_cast<int> (out.guttae.size ()) - o.firstGutta;
			out.regulae.push_back (o);
		}
	}
	if (s.mutules)
	{
		const GreekMutuleSettings &m = *s.mutules;
		const std::size_t k = static_cast<std::size_t> (m.band);
		const double back = s.bands[k].projection, half = 0.5 * m.width;
		// Gouttes des mutules (EN23) : axes le long de la sous-face, communs a
		// tous les mutules.
		std::vector<double> alongs;
		if (m.guttae)
		{
			const std::vector<double> b = CumulativeBounds (0.0, MutuleGuttaSeries (*m.guttae), "mutules.guttae");
			alongs.reserve (m.guttae->rowDiameters.size ());
			for (std::size_t i = 0; i < m.guttae->rowDiameters.size (); ++i)
				alongs.push_back (0.5 * (b[2 * i] + b[2 * i + 1]));
			// Au plus un mutule par placement de frise, deux par piece d'angle.
			const std::size_t mutuleBound = 2 * frieze.triglyphs.size () + frieze.metopes.size ();
			out.mutuleGuttae.reserve (mutuleBound * alongs.size () * static_cast<std::size_t> (m.guttae->perRow));
		}
		const PeristyleSide sides[] = {PeristyleSide::Front, PeristyleSide::Right, PeristyleSide::Back, PeristyleSide::Left};
		for (const PeristyleSide side : sides)
		{
			// Centres a croissant : triglyphes droits et bras d'angle portes par
			// cette face (le bras ouvert de sa piece d'angle, le retour de celle
			// du cote suivant), puis metopes.
			std::vector<double> centres;
			for (const FriezePiece &q : frieze.triglyphs)
			{
				if (q.side == side)
					centres.push_back (0.5 * (q.a0 + q.a1));
				if (q.kind == FriezeElement::CornerTriglyph && Previous (q.side) == side)
					centres.push_back (0.5 * (q.ra0 + q.ra1));
			}
			for (const FriezePiece &q : frieze.metopes)
				if (q.side == side)
					centres.push_back (0.5 * (q.a0 + q.a1));
			std::sort (centres.begin (), centres.end ());
			for (std::size_t i = 0; i + 1 < centres.size (); ++i)
				if (!(centres[i] + half < centres[i + 1] - half))
					Fail ("mutules.width", "ne laisse aucune via entre deux mutules voisins");
			const double limit = (AlongX (side) ? hx : hy) + back;
			if (!(centres.front () - half > -limit && centres.back () + half < limit))
				Fail ("mutules.width", "fait atteindre au mutule extreme le plan des dos des mutules du cote voisin : les mutules se recouvrent a l'angle");
			const int n = static_cast<int> (centres.size ());
			for (int rank = 0; rank < n; ++rank)
			{
				const double c = centres[static_cast<std::size_t> (Ascending (side) ? rank : n - 1 - rank)];
				OrnamentPiece o;
				o.kind = OrnamentElement::Mutule;
				o.side = side;
				o.rank = rank;
				o.a0 = c - half;
				o.a1 = c + half;
				o.d0 = back;
				o.d1 = m.face;
				o.z0 = z[k];
				o.z1 = z[k + 1];
				o.rise = m.rise;
				o.firstGutta = static_cast<int> (out.mutuleGuttae.size ());
				if (m.guttae)
				{
					const std::vector<double> across =
						EqualParts (o.a0 + m.guttae->edgeAxis, o.a1 - m.guttae->edgeAxis, m.guttae->perRow - 1, "mutules.guttae.perRow");
					for (std::size_t row = 0; row < alongs.size (); ++row)
						for (const double a : across)
						{
							MutuleGuttaPlacement g;
							g.side = side;
							g.row = static_cast<int> (row);
							g.a = a;
							g.along = alongs[row];
							out.mutuleGuttae.push_back (g);
						}
				}
				o.guttaCount = static_cast<int> (out.mutuleGuttae.size ()) - o.firstGutta;
				out.mutules.push_back (o);
			}
		}
	}
	return out;
}

bool SameFloatPoint (const std::array<double, 3> &a, const std::array<double, 3> &b)
{
	return static_cast<float> (a[0]) == static_cast<float> (b[0]) && static_cast<float> (a[1]) == static_cast<float> (b[1]) &&
	       static_cast<float> (a[2]) == static_cast<float> (b[2]);
}

// Goutte (EN19) : tronc de cone de `sides` cotes, axe vertical en (g.a, g.axis)
// du cote g.side ; anneau du pied a zFoot + g.z0, de la tete a zFoot + g.z1 ;
// eventails depuis le centre de chaque base. Dans (a, d), l'anneau part de
// l'avant (d maximal) vers +a : sens indirect ; il est direct dans (x, y) quand
// la correspondance du cote renverse le sens, et les triangles sont retournes
// sinon. Faux si deux sommets voisins d'un anneau se confondent en float.
bool AppendGutta (const GuttaPlacement &g, const GreekGuttaSettings &gs, double hx, double hy, double zFoot,
                  std::vector<std::array<double, 3>> &positions, std::vector<std::array<unsigned, 3>> &triangles)
{
	const double kPi = 3.14159265358979323846;
	const unsigned n = static_cast<unsigned> (gs.sides);
	const double rb = 0.5 * gs.footDiameter, rt = rb - (gs.footProjection - gs.topProjection);
	const double z0 = zFoot + g.z0, z1 = zFoot + g.z1;
	const unsigned base = static_cast<unsigned> (positions.size ());
	for (unsigned level = 0; level < 2; ++level)
	{
		const double r = level ? rt : rb, z = level ? z1 : z0;
		for (unsigned i = 0; i < n; ++i)
		{
			double a = g.a, d = g.axis;
			if (i == 0)
				d = level ? gs.topProjection : gs.footProjection;
			else if (4 * i == n)
				a = g.a + r;
			else if (2 * i == n)
				d = level ? g.axis - rt : gs.footProjection - gs.footDiameter;
			else if (4 * i == 3 * n)
				a = g.a - r;
			else
			{
				const double t = 2.0 * kPi * i / n;
				a = g.a + r * std::sin (t);
				d = g.axis + r * std::cos (t);
			}
			const std::array<double, 2> q = PlanPoint (g.side, hx, hy, a, d);
			positions.push_back ({q[0], q[1], z});
		}
		for (unsigned i = 0; i < n; ++i)
			if (SameFloatPoint (positions[base + level * n + i], positions[base + level * n + (i + 1) % n]))
				return false;
	}
	const std::array<double, 2> c = PlanPoint (g.side, hx, hy, g.a, g.axis);
	positions.push_back ({c[0], c[1], z0});
	positions.push_back ({c[0], c[1], z1});
	const std::array<double, 2> o = PlanPoint (g.side, hx, hy, 0.0, 0.0), ea = PlanPoint (g.side, hx, hy, 1.0, 0.0),
	                            ed = PlanPoint (g.side, hx, hy, 0.0, 1.0);
	const bool reverses = StrictCrossDifference (ea[0] - o[0], ed[1] - o[1], ea[1] - o[1], ed[0] - o[0]) < 0.0;
	auto tri = [&] (unsigned a, unsigned b, unsigned d) {
		if (reverses)
			triangles.push_back ({a, b, d});
		else
			triangles.push_back ({a, d, b});
	};
	const unsigned bottom = base + 2 * n, top = base + 2 * n + 1;
	for (unsigned i = 0; i < n; ++i)
	{
		const unsigned j = (i + 1) % n;
		tri (base + i, base + j, base + n + j);
		tri (base + i, base + n + j, base + n + i);
		tri (bottom, base + j, base + i);
		tri (top, base + n + i, base + n + j);
	}
	return true;
}

// Vrai si la correspondance (a, d) -> (x, y) du cote renverse le sens.
bool Reverses (PeristyleSide side, double hx, double hy)
{
	const std::array<double, 2> o = PlanPoint (side, hx, hy, 0.0, 0.0), ea = PlanPoint (side, hx, hy, 1.0, 0.0),
	                            ed = PlanPoint (side, hx, hy, 0.0, 1.0);
	return StrictCrossDifference (ea[0] - o[0], ed[1] - o[1], ea[1] - o[1], ed[0] - o[0]) < 0.0;
}

// Mutule (EN21, EN22) : prisme du parallelogramme (d0, z0 + rise), (d1, z0),
// (d1, z1), (d0, z1 + rise), de a0 a a1. Sommets et triangles dans l'ordre de
// PrismSection pour le profil {(a0, d1), (a1, d1)} sur le fond d0 : par bout,
// dos bas, face basse, dos haut, face haute. Le dessus au dos est a
// zFoot + (z1 + rise), l'expression du sommet de l'anneau au meme endroit.
void AppendMutulePrism (const OrnamentPiece &m, double hx, double hy, double zFoot, std::vector<std::array<double, 3>> &positions,
                        std::vector<std::array<unsigned, 3>> &triangles)
{
	const double zLow = zFoot + m.z0, zHigh = zFoot + m.z1;
	const double zBackLow = zFoot + (m.z0 + m.rise), zBackHigh = zFoot + (m.z1 + m.rise);
	const unsigned base = static_cast<unsigned> (positions.size ());
	for (const double a : {m.a0, m.a1})
	{
		const std::array<double, 2> back = PlanPoint (m.side, hx, hy, a, m.d0), face = PlanPoint (m.side, hx, hy, a, m.d1);
		positions.push_back ({back[0], back[1], zBackLow});
		positions.push_back ({face[0], face[1], zLow});
		positions.push_back ({back[0], back[1], zBackHigh});
		positions.push_back ({face[0], face[1], zHigh});
	}
	// Dessus, dessous, dos, face, puis les deux bouts, en sens direct dans (a, d).
	static const unsigned kTriangles[12][3] = {{2, 6, 7}, {2, 7, 3}, {0, 5, 4}, {0, 1, 5}, {0, 4, 6}, {0, 6, 2},
	                                           {5, 1, 3}, {5, 3, 7}, {1, 0, 2}, {1, 2, 3}, {4, 5, 7}, {4, 7, 6}};
	const bool flip = Reverses (m.side, hx, hy);
	for (const auto &t : kTriangles)
		triangles.push_back (flip ? std::array<unsigned, 3> {base + t[0], base + t[2], base + t[1]}
		                          : std::array<unsigned, 3> {base + t[0], base + t[1], base + t[2]});
}

// Goutte de mutule (EN24) : tronc de cone de `sides` cotes, d'axe normal a la
// sous-face du mutule m ; tete de diametre Ø (rowDiameters[g.row]) centree sur
// la sous-face a `along` de l'arete avant-basse, pied de diametre Ø + 2c a
// `height` sous elle. Dans le plan (d, z) d'un cote : sous-face de pente
// (c, -s) vers l'avant, axe vers le bas (-s, -c). Sommets avant, arriere et
// lateraux poses sans trigonometrie ; meme ordre et memes triangles que
// AppendGutta. Faux si deux sommets voisins d'un anneau se confondent en float.
bool AppendInclinedGutta (const MutuleGuttaPlacement &g, const GreekMutuleGuttaSettings &gs, const OrnamentPiece &m, double hx,
                          double hy, double zFoot, std::vector<std::array<double, 3>> &positions,
                          std::vector<std::array<unsigned, 3>> &triangles)
{
	const double kPi = 3.14159265358979323846;
	const unsigned n = static_cast<unsigned> (gs.sides);
	const double depth = m.d1 - m.d0, length = std::hypot (depth, m.rise), c = depth / length, s = m.rise / length;
	const double rt = 0.5 * gs.rowDiameters[static_cast<std::size_t> (g.row)], rb = rt + gs.conicity;
	const double dHead = m.d1 - g.along * c, zHead = m.z0 + g.along * s;
	const double dFoot = dHead - gs.height * s, zFootAxis = zHead - gs.height * c;
	const unsigned base = static_cast<unsigned> (positions.size ());
	for (unsigned level = 0; level < 2; ++level)
	{
		const double r = level ? rt : rb, dc = level ? dHead : dFoot, zc = level ? zHead : zFootAxis;
		for (unsigned i = 0; i < n; ++i)
		{
			double a = g.a, d = dc, z = zc;
			if (i == 0)
			{
				d = dc + r * c;
				z = zc - r * s;
			}
			else if (4 * i == n)
				a = g.a + r;
			else if (2 * i == n)
			{
				d = dc - r * c;
				z = zc + r * s;
			}
			else if (4 * i == 3 * n)
				a = g.a - r;
			else
			{
				const double t = 2.0 * kPi * i / n, radial = r * std::cos (t);
				a = g.a + r * std::sin (t);
				d = dc + radial * c;
				z = zc - radial * s;
			}
			const std::array<double, 2> q = PlanPoint (m.side, hx, hy, a, d);
			positions.push_back ({q[0], q[1], zFoot + z});
		}
		for (unsigned i = 0; i < n; ++i)
			if (SameFloatPoint (positions[base + level * n + i], positions[base + level * n + (i + 1) % n]))
				return false;
	}
	const std::array<double, 2> footCentre = PlanPoint (m.side, hx, hy, g.a, dFoot), headCentre = PlanPoint (m.side, hx, hy, g.a, dHead);
	positions.push_back ({footCentre[0], footCentre[1], zFoot + zFootAxis});
	positions.push_back ({headCentre[0], headCentre[1], zFoot + zHead});
	const bool reverses = Reverses (m.side, hx, hy);
	auto tri = [&] (unsigned a, unsigned b, unsigned d) {
		if (reverses)
			triangles.push_back ({a, b, d});
		else
			triangles.push_back ({a, d, b});
	};
	const unsigned bottom = base + 2 * n, top = base + 2 * n + 1;
	for (unsigned i = 0; i < n; ++i)
	{
		const unsigned j = (i + 1) % n;
		tri (base + i, base + j, base + n + j);
		tri (base + i, base + n + j, base + n + i);
		tri (bottom, base + j, base + i);
		tri (top, base + n + i, base + n + j);
	}
	return true;
}

// Prisme d'un placement de triglyphe, reglages supposes valides (contrat de
// BuildTriglyphMesh hors validation).
std::unique_ptr<Mesh> TriglyphMeshOf (const GreekEntablatureSettings &s, const PeristyleLayout &p,
                                      const FriezePiece &piece, double zFoot)
{
	if (piece.kind == FriezeElement::Metope)
		throw std::invalid_argument ("Frise : une metope est la face de l'anneau, sans maillage propre");
	if (!(std::isfinite (piece.a0) && std::isfinite (piece.a1) && piece.a0 < piece.a1 && std::isfinite (zFoot) &&
	      std::isfinite (piece.z0) && std::isfinite (piece.z1) && piece.z0 < piece.z1))
		throw std::invalid_argument ("Frise : placement de triglyphe vide ou non fini");
	if (piece.kind == FriezeElement::CornerTriglyph)
	{
		if (s.corner == DoricCornerStrategy::AxisTriglyph)
			throw std::invalid_argument ("Frise : piece d'angle sous la strategie AxisTriglyph, sans triglyphe d'angle");
		if (!(std::isfinite (piece.ra0) && std::isfinite (piece.ra1) && piece.ra0 < piece.ra1))
			throw std::invalid_argument ("Frise : bras de retour du triglyphe d'angle vide ou non fini");
		return BuildCornerTriglyphMesh (s, p, piece, zFoot);
	}

	const GreekTriglyphSettings &t = s.triglyph;
	// Non-degenerescence apres l'arrondi float des positions, par les expressions
	// des positions elles-memes : `across` porte le dos et les fonds (distance de
	// la ligne des axes au plan de frise du cote), `along` le coin de la frise.
	const bool alongX = piece.side == PeristyleSide::Front || piece.side == PeristyleSide::Back;
	const double across = alongX ? p.halfDepth : p.halfWidth, along = alongX ? p.halfWidth : p.halfDepth;
	const float backPlane = static_cast<float> (across + piece.d0);
	if (!(static_cast<float> (across + (piece.d1 - t.channelDepth)) > backPlane))
		throw std::invalid_argument ("Frise : 'triglyph.channelDepth' met le fond du canal sur le dos une fois arrondi en float");
	if (!(static_cast<float> (across + (piece.d1 - t.halfChannelDepth)) > backPlane))
		throw std::invalid_argument ("Frise : 'triglyph.halfChannelDepth' met le fond du demi-canal sur le dos une fois arrondi en float");
	if (!(static_cast<float> (piece.a1) < static_cast<float> (along + piece.d0) &&
	      static_cast<float> (piece.a0) > static_cast<float> (-(along + piece.d0))))
		throw std::invalid_argument ("Frise : 'triglyph.width' met le triglyphe au contact, en float, du plan de frise d'un cote voisin");
	std::vector<double> bounds = CumulativeBounds (piece.a0, t.divisionWidths, "triglyph.divisionWidths");
	bounds.back () = piece.a1;
	const std::vector<std::array<double, 2>> top = TriglyphProfile (t, bounds, piece.d1);

	const double hx = p.halfWidth, hy = p.halfDepth;
	const PeristyleSide side = piece.side;
	auto map = [&] (double a, double d) { return PlanPoint (side, hx, hy, a, d); };
	std::vector<std::array<double, 3>> positions;
	std::vector<std::array<unsigned, 3>> triangles;
	positions.reserve (4 * top.size ());
	triangles.reserve (8 * top.size ());
	PrismSection (top, piece.d0, map, zFoot + piece.z0, zFoot + piece.z1, positions, triangles);
	return MakeTriangleMesh (positions, triangles);
}

// Regula avec ses gouttes, ou mutule, reglages supposes valides.
std::unique_ptr<Mesh> OrnamentMeshOf (const GreekEntablatureSettings &s, const PeristyleLayout &p,
                                      const OrnamentLayout &ornaments, const OrnamentPiece &piece, double zFoot)
{
	auto finite = [] (double a, double b) { return std::isfinite (a) && std::isfinite (b) && a < b; };
	if (!(finite (piece.a0, piece.a1) && finite (piece.d0, piece.d1) && finite (piece.z0, piece.z1) && std::isfinite (zFoot)))
		throw std::invalid_argument ("Ornement : piece vide ou non finie");
	const bool regula = piece.kind != OrnamentElement::Mutule;
	if (!regula && piece.guttaCount != 0 && !(s.mutules && s.mutules->guttae))
		throw std::invalid_argument ("Ornement : mutule portant des gouttes (guttaCount != 0) sans reglages 'mutules.guttae'");
	if (regula && !s.regulae)
		throw std::invalid_argument ("Ornement : regula sans reglages 'regulae'");
	if (!regula && !s.mutules)
		throw std::invalid_argument ("Ornement : mutule sans reglages 'mutules'");
	if (!regula && !(std::isfinite (piece.rise) && piece.rise >= 0.0))
		throw std::invalid_argument ("Ornement : piece vide ou non finie (montee du mutule)");
	const std::size_t placements = regula ? ornaments.guttae.size () : ornaments.mutuleGuttae.size ();
	if (!(piece.firstGutta >= 0 && piece.guttaCount >= 0 &&
	      static_cast<std::size_t> (piece.firstGutta) + static_cast<std::size_t> (piece.guttaCount) <= placements))
		throw std::invalid_argument ("Ornement : plage de gouttes hors des placements");
	const double hx = p.halfWidth, hy = p.halfDepth, z0 = zFoot + piece.z0, z1 = zFoot + piece.z1;
	std::vector<std::array<double, 3>> positions;
	std::vector<std::array<unsigned, 3>> triangles;
	const int sides = regula ? s.regulae->gutta.sides : (piece.guttaCount != 0 ? s.mutules->guttae->sides : 0);
	const std::size_t guttaVertices = sides ? 2 * static_cast<std::size_t> (sides) + 2 : 0;
	positions.reserve (12 + static_cast<std::size_t> (piece.guttaCount) * guttaVertices);
	triangles.reserve (20 + static_cast<std::size_t> (piece.guttaCount) * 2 * guttaVertices);
	if (piece.kind == OrnamentElement::CornerRegula)
	{
		if (s.corner == DoricCornerStrategy::AxisTriglyph)
			throw std::invalid_argument ("Ornement : regula d'angle sous la strategie AxisTriglyph, sans triglyphe d'angle");
		if (!finite (piece.ra0, piece.ra1))
			throw std::invalid_argument ("Ornement : bras de retour de la regula d'angle vide ou non fini");
		const PeristyleSide side = piece.side, previous = Previous (side);
		const double openInner = Ascending (side) ? piece.a1 : piece.a0, openCorner = Ascending (side) ? piece.a0 : piece.a1;
		const double returnInner = Ascending (previous) ? piece.ra0 : piece.ra1;
		const double along = AlongX (side) ? hx : hy;
		const double backCorner = Ascending (side) ? -(along + piece.d0) : along + piece.d0;
		const std::vector<std::array<double, 2>> section = {
			PlanPoint (side, hx, hy, openInner, piece.d0),       PlanPoint (side, hx, hy, openInner, piece.d1),
			PlanPoint (side, hx, hy, openCorner, piece.d1),      PlanPoint (previous, hx, hy, returnInner, piece.d1),
			PlanPoint (previous, hx, hy, returnInner, piece.d0), PlanPoint (side, hx, hy, backCorner, piece.d0)};
		switch (AppendPolygonPrism (section, z0, z1, positions, triangles))
		{
		case PolygonPrismStatus::NotSimple:
			throw std::invalid_argument ("Ornement : section de la regula d'angle non simple");
		case PolygonPrismStatus::FloatCollapse:
			throw std::invalid_argument ("Ornement : deux sommets de la section de la regula d'angle confondus une fois arrondis en float");
		case PolygonPrismStatus::NotTriangulable:
			throw std::invalid_argument ("Ornement : section de la regula d'angle non triangulable");
		case PolygonPrismStatus::Built:
			break;
		}
	}
	else
	{
		const PeristyleSide side = piece.side;
		if (piece.kind == OrnamentElement::Regula)
		{
			// Bout d'une regula droite contre le plan de dos des regulae d'un cote
			// voisin, par les expressions des positions.
			const double along = AlongX (side) ? hx : hy;
			if (!(static_cast<float> (piece.a1) < static_cast<float> (along + piece.d0) &&
			      static_cast<float> (piece.a0) > static_cast<float> (-(along + piece.d0))))
				throw std::invalid_argument ("Ornement : 'triglyph.width' met la regula au contact, en float, du plan de dos des regulae d'un cote voisin");
		}
		if (piece.kind == OrnamentElement::Mutule)
			AppendMutulePrism (piece, hx, hy, zFoot, positions, triangles);
		else
		{
			auto map = [&] (double a, double d) { return PlanPoint (side, hx, hy, a, d); };
			PrismSection ({{piece.a0, piece.d1}, {piece.a1, piece.d1}}, piece.d0, map, z0, z1, positions, triangles);
		}
		// Sommet 0 (dos, bas, premier bout) contre ses trois voisins : face (1),
		// haut (2) et second bout (4).
		if (SameFloatPoint (positions[0], positions[1]) || SameFloatPoint (positions[0], positions[4]) ||
		    SameFloatPoint (positions[0], positions[2]))
			throw std::invalid_argument ("Ornement : deux sommets du prisme confondus une fois arrondis en float");
	}
	if (!regula)
	{
		// Pieds contre M et contre le pied de l'entablement, par les positions
		// elles-memes : contacts que la validation exclut en double et que
		// l'arrondi float retablirait.
		const double limit = piece.guttaCount != 0 ? MutuleFootLimit (s) : 0.0;
		const bool alongX = AlongX (piece.side);
		const double across = alongX ? hy : hx;
		for (int k = 0; k < piece.guttaCount; ++k)
		{
			const MutuleGuttaPlacement &g = ornaments.mutuleGuttae[static_cast<std::size_t> (piece.firstGutta + k)];
			const GreekMutuleGuttaSettings &gs = *s.mutules->guttae;
			if (!(g.row >= 0 && static_cast<std::size_t> (g.row) < gs.rowDiameters.size ()))
				throw std::invalid_argument ("Ornement : goutte de mutule d'un rang hors des reglages 'mutules.guttae'");
			const std::size_t base = positions.size ();
			if (!AppendInclinedGutta (g, gs, piece, hx, hy, zFoot, positions, triangles))
				throw std::invalid_argument ("Ornement : deux sommets d'une goutte de mutule confondus une fois arrondis en float");
			const std::array<double, 3> &footBack = positions[base + static_cast<std::size_t> (gs.sides / 2)], &footFront = positions[base];
			if (!(static_cast<float> (std::fabs (alongX ? footBack[1] : footBack[0])) > static_cast<float> (across + limit)))
				throw std::invalid_argument ("Ornement : 'mutules.guttae' met l'arriere du pied d'une goutte au contact, en float, d'une piece sous les mutules");
			if (!(static_cast<float> (footFront[2]) > static_cast<float> (zFoot)))
				throw std::invalid_argument ("Ornement : 'mutules.guttae' met le pied d'une goutte au contact, en float, du pied de l'entablement");
		}
		return MakeTriangleMesh (positions, triangles);
	}
	for (int k = 0; k < piece.guttaCount; ++k)
	{
		const GuttaPlacement &g = ornaments.guttae[static_cast<std::size_t> (piece.firstGutta + k)];
		const GreekGuttaSettings &gs = s.regulae->gutta;
		// Arriere du pied de la goutte contre la face de la bande f - 2 et le haut
		// de la bande f - 3, par l'expression du sommet arriere : refus des
		// contacts que la validation exclut en double et que l'arrondi float
		// retablirait.
		const std::size_t f = static_cast<std::size_t> (s.friezeBand);
		const double across = AlongX (g.side) ? hy : hx;
		const float footBack = static_cast<float> (across + (gs.footProjection - gs.footDiameter));
		if (!(footBack > static_cast<float> (across + s.bands[f - 2].projection)))
			throw std::invalid_argument ("Ornement : 'regulae.gutta.footDiameter' met l'arriere du pied d'une goutte au contact, en float, de la face de la bande friezeBand - 2");
		if (!(footBack > static_cast<float> (across + s.bands[f - 3].topProjection)))
			throw std::invalid_argument ("Ornement : '" + BandField (f - 3, "topProjection") +
			                             "' met le pied d'une goutte au contact, en float, du haut de cette bande");
		if (!AppendGutta (g, gs, hx, hy, zFoot, positions, triangles))
			throw std::invalid_argument ("Ornement : deux sommets d'une goutte confondus une fois arrondis en float");
	}
	return MakeTriangleMesh (positions, triangles);
}

} // namespace

std::unique_ptr<Mesh> SweepRectangularRing (const std::vector<RingSectionPoint> &section, double halfX, double halfY,
                                            double z0)
{
	const std::size_t n = section.size ();
	if (n < 3)
		FailRing ("section de moins de 3 sommets");
	if (!(std::isfinite (halfX) && std::isfinite (halfY) && std::isfinite (z0)))
		FailRing ("demi-longueur ou cote du pied non finie");
	for (std::size_t i = 0; i < n; ++i)
	{
		const RingSectionPoint &q = section[i];
		if (!(std::isfinite (q.u) && std::isfinite (q.z)))
			FailRing ("sommet " + std::to_string (i) + " non fini");
		if (!(halfX + q.u > 0.0 && halfY + q.u > 0.0))
			FailRing ("sommet " + std::to_string (i) + " au-dela de la ligne des axes opposee (halfX + u <= 0 ou halfY + u <= 0)");
		const RingSectionPoint &r = section[(i + 1) % n];
		if (q.u == r.u && q.z == r.z)
			FailRing ("sommets " + std::to_string (i) + " et " + std::to_string ((i + 1) % n) + " confondus");
	}
	if (!IsSimple (section))
		FailRing ("section non simple");
	if (!(TwiceSignedArea (section) > 0.0))
		FailRing ("section de sens indirect dans (u, z)");

	std::vector<std::array<double, 3>> positions;
	std::vector<std::array<unsigned, 3>> triangles;
	positions.reserve (4 * n);
	triangles.reserve (8 * n);
	for (const RingSectionPoint &q : section)
	{
		const double x = halfX + q.u, y = halfY + q.u, z = z0 + q.z;
		// Sens trigonometrique vu de dessus, depuis l'angle (-x, -y).
		positions.push_back ({-x, -y, z});
		positions.push_back ({x, -y, z});
		positions.push_back ({x, y, z});
		positions.push_back ({-x, y, z});
	}
	for (std::size_t i = 0; i < n; ++i)
	{
		const unsigned a0 = static_cast<unsigned> (4 * i), a1 = static_cast<unsigned> (4 * ((i + 1) % n));
		for (unsigned k = 0; k < 4; ++k)
		{
			const unsigned a = a0 + k, b = a0 + (k + 1) % 4, c = a1 + (k + 1) % 4, d = a1 + k;
			triangles.push_back ({a, b, c});
			triangles.push_back ({a, c, d});
		}
	}
	return MakeTriangleMesh (positions, triangles);
}

double RectangularRingVolume (const std::vector<RingSectionPoint> &section, double halfX, double halfY)
{
	double twiceArea = 0.0, moment = 0.0;   // 2 A ; 6 A uG
	for (std::size_t i = 0; i < section.size (); ++i)
	{
		const RingSectionPoint &a = section[i], &b = section[(i + 1) % section.size ()];
		const double cross = a.u * b.z - b.u * a.z;
		twiceArea += cross;
		moment += (a.u + b.u) * cross;
	}
	// A (4 (halfX + halfY) + 8 uG), avec A uG = moment / 6.
	return 2.0 * twiceArea * (halfX + halfY) + 8.0 * moment / 6.0;
}

void ValidateGreekEntablatureSettings (const GreekEntablatureSettings &s)
{
	if (s.bands.size () < 2)
		Fail ("bands", "compte " + std::to_string (s.bands.size ()) + " bande(s), au moins 2 attendues");
	for (std::size_t k = 0; k < s.bands.size (); ++k)
	{
		const EntablatureBand &b = s.bands[k];
		if (!FinitePositive (b.height))
			Fail (BandField (k, "height"), "doit etre fini et strictement positif");
		if (!FinitePositive (b.projection))
			Fail (BandField (k, "projection"), "doit etre fini et strictement positif");
		if (!FinitePositive (b.topProjection))
			Fail (BandField (k, "topProjection"), "doit etre fini et strictement positif");
	}
	if (!FinitePositive (s.innerFace))
		Fail ("innerFace", "doit etre fini et strictement positif");
	if (!(s.friezeBand >= 1 && static_cast<std::size_t> (s.friezeBand) + 2 <= s.bands.size ()))
		Fail ("friezeBand", "doit designer une bande ni premiere ni derniere, dans [1, " +
		                    std::to_string (s.bands.size ()) + " - 2]");
	const std::size_t f = static_cast<std::size_t> (s.friezeBand);
	if (s.bands[f].topProjection != s.bands[f].projection)
		Fail (BandField (f, "topProjection"), "doit egaler la saillie du pied : la face de la frise est verticale");

	const GreekTriglyphSettings &t = s.triglyph;
	if (!FinitePositive (t.width))
		Fail ("triglyph.width", "doit etre fini et strictement positif");
	if (t.divisionWidths.size () < 2)
		Fail ("triglyph.divisionWidths", "compte moins de 2 divisions");
	if (t.divisionIsChannel.size () != t.divisionWidths.size ())
		Fail ("triglyph.divisionIsChannel", "n'a pas la taille de 'triglyph.divisionWidths'");
	double sum = 0.0;
	for (std::size_t i = 0; i < t.divisionWidths.size (); ++i)
	{
		if (!FinitePositive (t.divisionWidths[i]))
			Fail ("triglyph.divisionWidths[" + std::to_string (i) + "]", "doit etre fini et strictement positif");
		sum += t.divisionWidths[i];
	}
	if (!(std::fabs (sum - t.width) <= 1e-12 * t.width))
		Fail ("triglyph.divisionWidths", "a une somme differente de 'triglyph.width' (1e-12 relatif)");
	if (!(t.divisionIsChannel.front () && t.divisionIsChannel.back ()))
		Fail ("triglyph.divisionIsChannel", "doit commencer et finir par un demi-canal");

	const double back = s.bands[f].projection;
	if (!(std::isfinite (t.face) && back < t.face))
		Fail ("triglyph.face", "doit etre finie et saillir sur le nu des metopes (bands[friezeBand].projection)");
	if (!(t.channelDepth > 0.0 && t.channelDepth < t.face - back))
		Fail ("triglyph.channelDepth", "doit etre dans ]0, face - nu des metopes[");
	if (!(t.halfChannelDepth > 0.0 && t.halfChannelDepth < t.face - back))
		Fail ("triglyph.halfChannelDepth", "doit etre dans ]0, face - nu des metopes[");

	if (!(s.corner == DoricCornerStrategy::AxisTriglyph || s.corner == DoricCornerStrategy::CornerTriglyphEqualMetopes ||
	      s.corner == DoricCornerStrategy::CornerTriglyphMidBay))
		Fail ("corner", "hors de DoricCornerStrategy");
	if (s.corner != DoricCornerStrategy::AxisTriglyph)
	{
		if (!(t.width > t.face - back))
			Fail ("triglyph.width", "doit depasser face - nu des metopes : chaque bras du triglyphe d'angle repose sur une face de frise");
		// Section en L aux quatre angles, sur des demi-distances nulles : la
		// simplicite ne depend pas de la translation, mais du sens de la division
		// sur chaque bras.
		const PeristyleSide sides[] = {PeristyleSide::Front, PeristyleSide::Right, PeristyleSide::Back, PeristyleSide::Left};
		for (const PeristyleSide side : sides)
		{
			const CornerArms arms = CornerTriglyphArms (side, 0.0, 0.0, t.face, t.width);
			if (!IsSimple (AsRingPoints (CornerSection (t, back, t.face, side, 0.0, 0.0, arms))))
				Fail ("triglyph.divisionWidths",
				      "rend la section du triglyphe d'angle non simple : le chanfrein d'arete passe derriere les nus des metopes");
		}
	}
	ValidateOrnaments (s);
}

std::vector<double> GreekEntablatureBandBounds (const GreekEntablatureSettings &s)
{
	ValidateGreekEntablatureSettings (s);
	return BandBounds (s);
}

std::vector<RingSectionPoint> GreekEntablatureSection (const GreekEntablatureSettings &s)
{
	return SectionOf (s, GreekEntablatureBandBounds (s));
}

FriezeLayout LayoutGreekFrieze (const GreekEntablatureSettings &s, const PeristyleLayout &p)
{
	const std::vector<double> z = GreekEntablatureBandBounds (s);
	if (!(s.innerFace < std::min (p.halfWidth, p.halfDepth)))
		Fail ("innerFace", "atteint la ligne des axes opposee : innerFace >= min (halfWidth, halfDepth)");
	const std::size_t f = static_cast<std::size_t> (s.friezeBand);
	const double back = s.bands[f].projection, width = s.triglyph.width, half = 0.5 * width;
	const bool axisCorner = s.corner == DoricCornerStrategy::AxisTriglyph;
	if (axisCorner && !(half < back))
		Fail ("triglyph.width", "fait toucher les triglyphes d'angle de deux cotes voisins : width / 2 >= nu des metopes");

	FriezeLayout layout;
	const PeristyleSide sides[] = {PeristyleSide::Front, PeristyleSide::Right, PeristyleSide::Back, PeristyleSide::Left};
	for (const PeristyleSide side : sides)
	{
		const bool alongX = side == PeristyleSide::Front || side == PeristyleSide::Back;
		const bool descending = side == PeristyleSide::Back || side == PeristyleSide::Left;
		std::vector<double> axes = SideAxes (p, side);
		if (descending)
			std::reverse (axes.begin (), axes.end ());
		const double along = alongX ? p.halfWidth : p.halfDepth, across = alongX ? p.halfDepth : p.halfWidth;

		const StraightPlacements straight = StraightCentres (s.corner, axes, along, back, s.triglyph.face, width);
		const std::vector<double> &centres = straight.centres;
		const double lo = straight.lo, hi = straight.hi;
		CornerArms arms;
		if (!axisCorner)
			arms = CornerTriglyphArms (side, along, across, s.triglyph.face, width);

		std::vector<Interval> gaps;
		try
		{
			gaps = GapsBetweenCentred (centres, width, "triglyph.width");
		}
		catch (const std::invalid_argument &e)
		{
			Fail ("triglyph.width", std::string ("laisse une metope vide (") + e.what () + ")");
		}
		const Interval first {lo, centres.front () - half}, last {centres.back () + half, hi};
		if (!(first.lo < first.hi && last.lo < last.hi))
			Fail ("triglyph.width", axisCorner ? "laisse une metope d'angle vide"
			                                   : "laisse une metope vide contre un bras de triglyphe d'angle");

		std::vector<Interval> metopes;
		metopes.reserve (gaps.size () + 2);
		metopes.push_back (first);
		metopes.insert (metopes.end (), gaps.begin (), gaps.end ());
		metopes.push_back (last);

		auto piece = [&] (FriezeElement kind, int rank, double a0, double a1) {
			FriezePiece q;
			q.kind = kind;
			q.side = side;
			q.rank = rank;
			q.a0 = a0;
			q.a1 = a1;
			q.d0 = back;
			q.d1 = kind == FriezeElement::Metope ? back : s.triglyph.face;
			q.z0 = z[f];
			q.z1 = z[f + 1];
			return q;
		};
		const int nt = static_cast<int> (centres.size ()), nm = static_cast<int> (metopes.size ());
		const int firstStraight = axisCorner ? 0 : 1;
		if (!axisCorner)
		{
			FriezePiece q = piece (FriezeElement::CornerTriglyph, 0, arms.a0, arms.a1);
			q.ra0 = arms.ra0;
			q.ra1 = arms.ra1;
			layout.triglyphs.push_back (q);
		}
		for (int r = 0; r < nt; ++r)
		{
			const double c = centres[static_cast<std::size_t> (descending ? nt - 1 - r : r)];
			layout.triglyphs.push_back (piece (FriezeElement::Triglyph, firstStraight + r, c - half, c + half));
		}
		for (int r = 0; r < nm; ++r)
		{
			const Interval &m = metopes[static_cast<std::size_t> (descending ? nm - 1 - r : r)];
			layout.metopes.push_back (piece (FriezeElement::Metope, r, m.lo, m.hi));
		}
	}
	return layout;
}

std::unique_ptr<Mesh> BuildTriglyphMesh (const GreekEntablatureSettings &s, const PeristyleLayout &p,
                                         const FriezePiece &piece, double zFoot)
{
	if (piece.kind == FriezeElement::Metope)
		throw std::invalid_argument ("Frise : une metope est la face de l'anneau, sans maillage propre");
	ValidateGreekEntablatureSettings (s);
	return TriglyphMeshOf (s, p, piece, zFoot);
}

double CornerTriglyphOffset (const GreekEntablatureSettings &s)
{
	ValidateGreekEntablatureSettings (s);
	return s.triglyph.face - 0.5 * s.triglyph.width;
}

std::optional<double> ComputedCornerSpacing (const GreekEntablatureSettings &s, double spacing)
{
	ValidateGreekEntablatureSettings (s);
	if (!FinitePositive (spacing))
		Fail ("spacing", "doit etre fini et strictement positif");
	switch (s.corner)
	{
	case DoricCornerStrategy::AxisTriglyph:
		return spacing;
	case DoricCornerStrategy::CornerTriglyphEqualMetopes:
	{
		const double computed = spacing - CornerTriglyphOffset (s);
		if (!(computed > 0.0))
			Fail ("triglyph.width", "donne un entraxe d'angle calcule <= 0 : spacing <= face - width / 2");
		return computed;
	}
	case DoricCornerStrategy::CornerTriglyphMidBay:
		return std::nullopt;
	}
	return std::nullopt;
}

std::array<DoricCornerReport, 4> ReportDoricCorners (const GreekEntablatureSettings &s, const PeristyleLayout &p)
{
	const FriezeLayout layout = LayoutGreekFrieze (s, p);
	const bool axisCorner = s.corner == DoricCornerStrategy::AxisTriglyph;
	// Rangs, dans le sens de parcours, des deux metopes de la travee d'angle de
	// l'angle ouvrant : sous AxisTriglyph, la demi-metope d'angle a le rang 0.
	const int outerRank = axisCorner ? 1 : 0, innerRank = outerRank + 1;
	const PeristyleSide sides[] = {PeristyleSide::Front, PeristyleSide::Right, PeristyleSide::Back, PeristyleSide::Left};
	std::array<DoricCornerReport, 4> reports;
	for (std::size_t k = 0; k < reports.size (); ++k)
	{
		const PeristyleSide side = sides[k];
		const bool alongX = AlongX (side);
		DoricCornerReport &r = reports[k];
		r.side = side;
		r.spacing = alongX ? p.frontSpacing : p.flankSpacing;
		r.cornerSpacing = alongX ? p.frontCornerSpacing : p.flankCornerSpacing;
		r.currentMetope = 0.5 * r.spacing - s.triglyph.width;
		const FriezePiece *outer = nullptr, *inner = nullptr, *intermediate = nullptr, *corner = nullptr;
		for (const FriezePiece &q : layout.metopes)
			if (q.side == side)
			{
				if (q.rank == outerRank)
					outer = &q;
				if (q.rank == innerRank)
					inner = &q;
			}
		for (const FriezePiece &q : layout.triglyphs)
			if (q.side == side)
			{
				if (q.rank == 1)
					intermediate = &q;
				if (q.rank == 0 && q.kind == FriezeElement::CornerTriglyph)
					corner = &q;
			}
		if (!(outer && inner && intermediate && (axisCorner || corner)))
			throw std::logic_error ("Entablement : travee d'angle absente des placements de la frise");
		r.outerMetope = outer->a1 - outer->a0;
		r.innerMetope = inner->a1 - inner->a0;
		// Travee d'angle : axe de l'angle ouvrant (premier dans le sens de
		// parcours) et son voisin.
		const std::vector<double> axes = SideAxes (p, side);
		const double middle = 0.5 * (axes[0] + axes[1]);
		const double centre = 0.5 * (intermediate->a0 + intermediate->a1);
		r.intermediateShift = Ascending (side) ? middle - centre : centre - middle;
		// Centre du bras ouvert de la piece placee, au-dela de l'axe d'angle.
		if (corner)
		{
			const double armCentre = 0.5 * (corner->a0 + corner->a1);
			r.cornerTriglyphOffset = Ascending (side) ? axes[0] - armCentre : armCentre - axes[0];
		}
		// Contraction calculee : absente si elle serait <= 0 (spacing <= c), que
		// ComputedCornerSpacing refuse.
		if (s.corner != DoricCornerStrategy::CornerTriglyphEqualMetopes || r.spacing > CornerTriglyphOffset (s))
			r.computedCornerSpacing = ComputedCornerSpacing (s, r.spacing);
		if (r.computedCornerSpacing)
			r.cornerSpacingGap = r.cornerSpacing - *r.computedCornerSpacing;
	}
	return reports;
}

OrnamentLayout LayoutGreekOrnaments (const GreekEntablatureSettings &s, const PeristyleLayout &p)
{
	const FriezeLayout frieze = LayoutGreekFrieze (s, p);
	return OrnamentsOf (s, p, frieze);
}

std::unique_ptr<Mesh> BuildOrnamentMesh (const GreekEntablatureSettings &s, const PeristyleLayout &p,
                                         const OrnamentLayout &ornaments, const OrnamentPiece &piece, double zFoot)
{
	ValidateGreekEntablatureSettings (s);
	return OrnamentMeshOf (s, p, ornaments, piece, zFoot);
}

std::vector<std::unique_ptr<Mesh>> BuildEntablaturePieceMeshes (const GreekEntablatureSettings &s, const PeristyleLayout &p,
                                                                const FriezeLayout &frieze, const OrnamentLayout &ornaments,
                                                                double zFoot, const std::function<void (const Mesh &)> &afterEach)
{
	ValidateGreekEntablatureSettings (s);
	std::vector<std::unique_ptr<Mesh>> meshes;
	meshes.reserve (frieze.triglyphs.size () + ornaments.regulae.size () + ornaments.mutules.size ());
	auto keep = [&] (std::unique_ptr<Mesh> mesh) {
		if (afterEach)
			afterEach (*mesh);
		meshes.push_back (std::move (mesh));
	};
	for (const FriezePiece &q : frieze.triglyphs)
		keep (TriglyphMeshOf (s, p, q, zFoot));
	for (const auto *list : {&ornaments.regulae, &ornaments.mutules})
		for (const OrnamentPiece &o : *list)
			keep (OrnamentMeshOf (s, p, ornaments, o, zFoot));
	return meshes;
}

} // namespace classical
} // namespace cgarchi
