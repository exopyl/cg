#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <cgarchi/classical_acanthus.h>
#include <cgarchi/classical_acanthus_detail.h>
#include <cgarchi/classical_acanthus_template.h>
#include <cgarchi/classical_orders.h>
#include <cgarchi/classical_profile.h>
#include "../src/cgmesh/mesh.h"
#include "acanthus_test_support.h"

using namespace cgarchi::classical;

// ===========================================================================
//  Feuilles D dans l'ornement : K16 a K21 pour les 16 D
// ===========================================================================
//
// Pieces 24 a 39 : tige k croissante, D interieur puis D exterieur (registre AC-46). Axes ecrits d'apres les planches
// XXV et XXVI, non lus dans le produit : tige a pi/8 + k pi/4 ; pointe du D i. a 17,0 degres de la tige vers l'axe de
// face, pointe du D e. a 19,0 degres vers la corne.

namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg = kPi / 180.0;
constexpr double kTau = 1e-4;
constexpr double kCell = 1e-3;  // p : maille de la grille de hachage de K18
constexpr std::size_t kFirstMinor = 24;
constexpr std::size_t kEndMinor = kFirstMinor + 16;   // puis 8 volutes et 8 helices (registre AC-46)
using Point = std::array<double, 3>;
using Side = detail::MinorLeafSide;

const char *LevelName (OrnamentDetail d)
{
	return d == OrnamentDetail::Light ? "Light" : "Heavy";
}

struct Case
{
	OrnamentDetail detail;
	double scale;
	detail::CapitalEnvelope env;
	CapitalOrnament ornament;
};

const std::vector<Case> &Cases ()
{
	static const std::vector<Case> cases = [] {
		std::vector<Case> out;
		const OrderData &order = ClassicalOrders ().Get (Order::Corinthian);
		ColumnOptions options;
		options.order = Order::Corinthian;
		const ColumnProfile profile = BuildColumnProfile (order, options);
		for (const OrnamentDetail detail : {OrnamentDetail::Light, OrnamentDetail::Heavy})
			for (const double scale : {1.0, 2.0})
			{
				AcanthusOptions o;
				o.detail = detail;
				o.scale = scale;
				out.push_back ({detail, scale, detail::ReadCapitalEnvelope (order, profile, scale), BuildCorinthianOrnament (order, profile, o)});
			}
		return out;
	}();
	return cases;
}

Point Parts (const Mesh &mesh, const detail::CapitalEnvelope &env, unsigned v)
{
	float p[3] {};
	mesh.GetVertex (v, p);
	return {p[0] / env.scale, p[1] / env.scale, p[2] / env.scale - env.foot};
}

double StemAxis (int k)
{
	return kPi / 8.0 + k * kPi / 4.0;
}

// Sens des theta croissants vers l'axe de face le plus proche de la tige k.
double TowardFace (int k)
{
	return std::remainder (StemAxis (k), 0.5 * kPi) > 0.0 ? -1.0 : 1.0;
}

Side SideOf (std::size_t i)
{
	return (i - kFirstMinor) % 2 == 0 ? Side::Inner : Side::Outer;
}

int StemOf (std::size_t i)
{
	return static_cast<int> ((i - kFirstMinor) / 2);
}

detail::LeafSheet SheetAt (const Case &c, std::size_t i)
{
	const Side side = SideOf (i);
	return detail::BuildLeafSheet (c.env, detail::DefaultMinorLeafSpec (c.detail, side),
	                               detail::DefaultMinorLeafPlacement (c.env, side, StemAxis (StemOf (i))),
	                               detail::LeafTemplateOf (detail::LeafStyle::OliveMinor, c.detail));
}

unsigned ApexIndex (const detail::LeafSheet &s)
{
	unsigned apex = 0;
	for (unsigned v = 0; v < s.faceVertices; ++v)
		if (s.planar[v][1] > s.planar[apex][1])
			apex = v;
	return apex;
}

// Les 60 pieces et les 16 D a leur place (registre AC-46), avant toute boucle sur les D : une boucle vide ne prouve rien.
::testing::AssertionResult HasMinorLeaves (const Case &c)
{
	if (c.ornament.pieces.size () != kEndMinor + 16u + detail::kFleurons)
		return ::testing::AssertionFailure () << c.ornament.pieces.size () << " pieces, 60 attendues";
	for (std::size_t i = kFirstMinor; i < kEndMinor; ++i)
		if (c.ornament.pieces[i].kind != CapitalPieceKind::MinorLeaf)
			return ::testing::AssertionFailure () << "piece " << i << " : pas une feuille D";
	return ::testing::AssertionSuccess ();
}

// Distance du point p au triangle (a, b, c) (point le plus proche, regions de Voronoi du triangle).
double PointTriangle (const Point &p, const Point &a, const Point &b, const Point &c)
{
	const auto sub = [] (const Point &u, const Point &v) { return Point {u[0] - v[0], u[1] - v[1], u[2] - v[2]}; };
	const auto dot = [] (const Point &u, const Point &v) { return u[0] * v[0] + u[1] * v[1] + u[2] * v[2]; };
	const auto at = [] (const Point &o, const Point &u, double s, const Point &v, double t) {
		return Point {o[0] + s * u[0] + t * v[0], o[1] + s * u[1] + t * v[1], o[2] + s * u[2] + t * v[2]};
	};
	const Point ab = sub (b, a), ac = sub (c, a), ap = sub (p, a);
	const double d1 = dot (ab, ap), d2 = dot (ac, ap);
	Point q;
	if (d1 <= 0.0 && d2 <= 0.0)
		q = a;
	else
	{
		const Point bp = sub (p, b);
		const double d3 = dot (ab, bp), d4 = dot (ac, bp);
		const Point cp = sub (p, c);
		const double d5 = dot (ab, cp), d6 = dot (ac, cp);
		const double vc = d1 * d4 - d3 * d2, vb = d5 * d2 - d1 * d6, va = d3 * d6 - d5 * d4;
		if (d3 >= 0.0 && d4 <= d3)
			q = b;
		else if (vc <= 0.0 && d1 >= 0.0 && d3 <= 0.0)
			q = at (a, ab, d1 / (d1 - d3), ac, 0.0);
		else if (d6 >= 0.0 && d5 <= d6)
			q = c;
		else if (vb <= 0.0 && d2 >= 0.0 && d6 <= 0.0)
			q = at (a, ab, 0.0, ac, d2 / (d2 - d6));
		else if (va <= 0.0 && (d4 - d3) >= 0.0 && (d5 - d6) >= 0.0)
		{
			const double w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
			q = Point {b[0] + w * (c[0] - b[0]), b[1] + w * (c[1] - b[1]), b[2] + w * (c[2] - b[2])};
		}
		else
		{
			const double den = 1.0 / (va + vb + vc);
			q = at (a, ab, vb * den, ac, vc * den);
		}
	}
	return std::hypot (p[0] - q[0], p[1] - q[1], p[2] - q[2]);
}

// Distance minimale entre les sommets de la piece `from` et les triangles de la piece `to`, sommets a moins de
// `near` p de la boite de `to` seulement.
double PieceDistance (const Mesh &mesh, const detail::CapitalEnvelope &env, const CapitalPiece &from, const CapitalPiece &to, double near)
{
	Point lo {1e9, 1e9, 1e9}, hi {-1e9, -1e9, -1e9};
	for (unsigned v = to.vertexBegin; v < to.vertexEnd; ++v)
	{
		const Point q = Parts (mesh, env, v);
		for (int k = 0; k < 3; ++k)
		{
			lo[k] = std::min (lo[k], q[k]);
			hi[k] = std::max (hi[k], q[k]);
		}
	}
	double best = std::numeric_limits<double>::infinity ();
	for (unsigned v = from.vertexBegin; v < from.vertexEnd; ++v)
	{
		const Point p = Parts (mesh, env, v);
		bool close = true;
		for (int k = 0; k < 3; ++k)
			close = close && p[k] > lo[k] - near && p[k] < hi[k] + near;
		if (!close)
			continue;
		for (unsigned f = to.faceBegin; f < to.faceEnd; ++f)
			best = std::min (best, PointTriangle (p, Parts (mesh, env, static_cast<unsigned> (mesh.GetFaceVertex (f, 0))),
			                                      Parts (mesh, env, static_cast<unsigned> (mesh.GetFaceVertex (f, 1))),
			                                      Parts (mesh, env, static_cast<unsigned> (mesh.GetFaceVertex (f, 2)))));
	}
	return best;
}

#define CASE_TRACE(c) SCOPED_TRACE (std::string (LevelName ((c).detail)) + ", echelle " + std::to_string ((c).scale))

} // namespace

// K16 : phase. La pointe (apex du gabarit) de chaque D est sur son azimut releve.
TEST (TEST_cgarchi_classical_acanthus_minor_ornament, the_minor_leaves_stand_on_their_plate_azimuths)
{
	for (const Case &c : Cases ())
	{
		CASE_TRACE (c);
		ASSERT_TRUE (HasMinorLeaves (c));
		const unsigned apex = ApexIndex (SheetAt (c, kFirstMinor));
		ASSERT_EQ (apex, ApexIndex (SheetAt (c, kFirstMinor + 1)));
		double worst = 0.0;
		for (std::size_t i = kFirstMinor; i < kEndMinor; ++i)
		{
			const CapitalPiece &p = c.ornament.pieces[i];
			ASSERT_EQ (p.kind, CapitalPieceKind::MinorLeaf) << i;
			const int k = StemOf (i);
			const double expected = StemAxis (k) + (SideOf (i) == Side::Inner ? TowardFace (k) * 17.0 : -TowardFace (k) * 19.0) * kDeg;
			const Point q = Parts (*c.ornament.mesh, c.env, p.vertexBegin + apex);
			worst = std::max (worst, std::fabs (std::remainder (std::atan2 (q[1], q[0]) - expected, 2.0 * kPi)));
		}
		EXPECT_LE (worst, 1e-4);
		std::printf ("%s, echelle %g : K16 pointes des D, ecart maximal a l'azimut releve %.2e rad\n", LevelName (c.detail), c.scale, worst);
	}
}

// K17 : copie = construction. La D i est, indice par indice, la feuille posee directement a sa tige (4 ulp float).
TEST (TEST_cgarchi_classical_acanthus_minor_ornament, each_minor_copy_is_the_leaf_built_at_its_place)
{
	for (const Case &c : Cases ())
	{
		CASE_TRACE (c);
		ASSERT_TRUE (HasMinorLeaves (c));
		const Mesh &mesh = *c.ornament.mesh;
		double worst = 0.0;
		for (std::size_t i = kFirstMinor; i < kEndMinor; ++i)
		{
			const CapitalPiece &piece = c.ornament.pieces[i];
			const detail::LeafSheet sheet = SheetAt (c, i);
			ASSERT_EQ (piece.vertexEnd - piece.vertexBegin, static_cast<unsigned> (sheet.mesh->GetNVertices ())) << i;
			ASSERT_EQ (piece.faceEnd - piece.faceBegin, static_cast<unsigned> (sheet.mesh->GetNFaces ())) << i;
			for (unsigned v = 0; v < static_cast<unsigned> (sheet.mesh->GetNVertices ()); ++v)
			{
				const Point a = Parts (mesh, c.env, piece.vertexBegin + v), b = Parts (*sheet.mesh, c.env, v);
				worst = std::max ({worst, std::fabs (a[0] - b[0]), std::fabs (a[1] - b[1]), std::fabs (a[2] - b[2])});
			}
			bool same = true;
			for (unsigned f = 0; f < static_cast<unsigned> (sheet.mesh->GetNFaces ()); ++f)
				for (unsigned j = 0; j < 3; ++j)
					same = same && static_cast<unsigned> (mesh.GetFaceVertex (piece.faceBegin + f, j)) ==
					                   piece.vertexBegin + static_cast<unsigned> (sheet.mesh->GetFaceVertex (f, j));
			EXPECT_TRUE (same) << i;
		}
		EXPECT_LE (worst, 8e-6);
		std::printf ("%s, echelle %g : K17 ecart maximal copie / construction des D %.2e p\n", LevelName (c.detail), c.scale, worst);
	}
}

// K18 : l'ornement entier a la symetrie D4 de l'abaque (rotation de pi/2, reflexions d'axes 0 et pi/4), sans les
// indices ; il n'a pas celle de la rotation de pi/4 (controle negatif : les D i. et D e. y echangent leurs places).
TEST (TEST_cgarchi_classical_acanthus_minor_ornament, the_ornament_has_the_symmetry_of_the_abacus)
{
	const auto key = [] (long long i, long long j, long long k) {
		return static_cast<std::uint64_t> (i * 73856093LL ^ j * 19349663LL ^ k * 83492791LL);
	};
	for (const Case &c : Cases ())
	{
		CASE_TRACE (c);
		const Mesh &mesh = *c.ornament.mesh;
		const unsigned n = static_cast<unsigned> (mesh.GetNVertices ());
		std::vector<Point> points (n);
		std::unordered_multimap<std::uint64_t, unsigned> grid;
		grid.reserve (n);
		const auto cellOf = [] (double x) { return static_cast<long long> (std::floor (x / kCell)); };
		for (unsigned v = 0; v < n; ++v)
		{
			points[v] = Parts (mesh, c.env, v);
			grid.emplace (key (cellOf (points[v][0]), cellOf (points[v][1]), cellOf (points[v][2])), v);
		}
		// Distance au plus proche sommet, bornee a 1,5 p (au-dela de la grille : 1,5).
		const auto nearest = [&] (const Point &q) {
			double best = 1.5;
			const long long i = cellOf (q[0]), j = cellOf (q[1]), k = cellOf (q[2]);
			for (long long di = -1; di <= 1; ++di)
				for (long long dj = -1; dj <= 1; ++dj)
					for (long long dk = -1; dk <= 1; ++dk)
					{
						const auto range = grid.equal_range (key (i + di, j + dj, k + dk));
						for (auto it = range.first; it != range.second; ++it)
						{
							const Point &p = points[it->second];
							best = std::min (best, std::hypot (p[0] - q[0], p[1] - q[1], p[2] - q[2]));
						}
					}
			return best;
		};
		const double cr = std::cos (kPi / 4.0), sr = std::sin (kPi / 4.0);
		double mirror0 = 0.0, mirror45 = 0.0, quarter = 0.0;
		for (const Point &p : points)
		{
			mirror0 = std::max (mirror0, nearest ({p[0], -p[1], p[2]}));
			mirror45 = std::max (mirror45, nearest ({p[1], p[0], p[2]}));
			quarter = std::max (quarter, nearest ({-p[1], p[0], p[2]}));
		}
		// Controle negatif mesure sans plafond : pointe de chaque D tournee de pi/4, distance exacte au plus proche
		// sommet de l'ornement (recherche exhaustive).
		ASSERT_TRUE (HasMinorLeaves (c));
		const unsigned apex = ApexIndex (SheetAt (c, kFirstMinor));
		double eighth = 0.0;
		for (std::size_t i = kFirstMinor; i < kEndMinor; ++i)
		{
			const Point &p = points[c.ornament.pieces[i].vertexBegin + apex];
			const Point q {cr * p[0] - sr * p[1], sr * p[0] + cr * p[1], p[2]};
			double best = std::numeric_limits<double>::infinity ();
			for (const Point &s : points)
				best = std::min (best, std::hypot (s[0] - q[0], s[1] - q[1], s[2] - q[2]));
			eighth = std::max (eighth, best);
		}
		EXPECT_LE (mirror0, 1e-5);
		EXPECT_LE (mirror45, 1e-5);
		EXPECT_LE (quarter, 1e-5);
		EXPECT_GT (eighth, 1.0);
		std::printf ("%s, echelle %g : K18 D4 : reflexions %.2e / %.2e p, rotation de pi/2 %.2e p ; rotation de pi/4 %.3f p (negatif)\n",
		             LevelName (c.detail), c.scale, mirror0, mirror45, quarter, eighth);
	}
}

// K19, K20 : jeu au noyau des D (>= c) et borne des D : le cone (registre AC-2) ; l'ecart a l'abaque est journalise.
TEST (TEST_cgarchi_classical_acanthus_minor_ornament, the_minor_leaves_keep_off_the_bell_and_under_the_cone)
{
	for (const Case &c : Cases ())
	{
		CASE_TRACE (c);
		ASSERT_TRUE (HasMinorLeaves (c));
		double clearance = std::numeric_limits<double>::infinity (), excess = -clearance, abacus = -clearance;
		for (std::size_t i = kFirstMinor; i < kEndMinor; ++i)
		{
			const CapitalPiece &p = c.ornament.pieces[i];
			clearance = std::min (clearance, detail::CoreClearance (*c.ornament.mesh, c.env, p));
			excess = std::max (excess, detail::EnvelopeExcess (*c.ornament.mesh, c.env, p));
			abacus = std::max (abacus, detail::EnvelopeExcess (*c.ornament.mesh, c.env, p.vertexBegin, p.vertexEnd));
		}
		EXPECT_GE (clearance, 0.05 - kTau);
		EXPECT_LE (excess, kTau);
		std::printf ("%s, echelle %g : K19 CoreClearance des D %.4f p ; K20 contre le cone %+.4f p ; contre l'abaque (journal) %+.4f p\n",
		             LevelName (c.detail), c.scale, clearance, excess, abacus);
	}
}

// K21 : ordre radial des D (registre AC-51). Grille (theta, h) des faces : D i. et D e. derriere les E
// voisines (face interieure de E - c - face exterieure de D >= 0) ; D i. a 0,5 degre au moins de son axe de face ;
// D e. a 0,5 degre au moins de sa corne ; D au-dessus de la tige : distance entre les sommets d'une piece et les triangles
// de l'autre, dans les deux sens, >= c.
TEST (TEST_cgarchi_classical_acanthus_minor_ornament, the_minor_leaves_stand_behind_E_and_clear_of_the_stems)
{
	using acanthus_test::RadiusGrid;
	for (const Case &c : Cases ())
	{
		CASE_TRACE (c);
		ASSERT_TRUE (HasMinorLeaves (c));
		const Mesh &mesh = *c.ornament.mesh;
		const std::vector<CapitalPiece> &P = c.ornament.pieces;
		double behind = std::numeric_limits<double>::infinity (), toFace = behind, toHorn = behind, toStem = behind;
		unsigned common = 0;
		for (std::size_t i = kFirstMinor; i < kEndMinor; ++i)
		{
			const int k = StemOf (i);
			const double thetaC = StemAxis (k), face = thetaC + TowardFace (k) * kPi / 8.0, horn = thetaC - TowardFace (k) * kPi / 8.0;
			RadiusGrid dOut;
			acanthus_test::Rasterize (dOut, mesh, c.env, P[i].faceBegin, P[i].faceEnd, true, thetaC);
			for (const double eAxis : {face, horn})
			{
				const int ke = static_cast<int> (std::lround (eAxis / (kPi / 4.0))) % 8;
				const CapitalPiece &e = P[static_cast<std::size_t> (8 + (ke + 8) % 8)];
				ASSERT_EQ (e.kind, CapitalPieceKind::MiddleLeaf);
				RadiusGrid eIn;
				acanthus_test::Rasterize (eIn, mesh, c.env, e.faceBegin, e.faceEnd, false, thetaC);
				for (int j = 0; j < RadiusGrid::kNH; ++j)
					for (int ii = 0; ii < RadiusGrid::kNTheta; ++ii)
						if (!std::isnan (dOut.At (ii, j)) && !std::isnan (eIn.At (ii, j)))
						{
							++common;
							behind = std::min (behind, eIn.At (ii, j) - 0.05 - dOut.At (ii, j));
						}
			}
			for (unsigned v = P[i].vertexBegin; v < P[i].vertexEnd; ++v)
			{
				const Point q = Parts (mesh, c.env, v);
				const double t = std::atan2 (q[1], q[0]);
				if (SideOf (i) == Side::Inner)
					toFace = std::min (toFace, std::fabs (std::remainder (t - face, 2.0 * kPi)) / kDeg);
				else
					toHorn = std::min (toHorn, std::fabs (std::remainder (t - horn, 2.0 * kPi)) / kDeg);
			}
			const CapitalPiece &stemPiece = P[static_cast<std::size_t> (16 + k)];
			ASSERT_EQ (stemPiece.kind, CapitalPieceKind::Cauliculus);
			toStem = std::min ({toStem, PieceDistance (mesh, c.env, P[i], stemPiece, 1.0), PieceDistance (mesh, c.env, stemPiece, P[i], 1.0)});
		}
		EXPECT_GT (common, 0u);
		EXPECT_GE (behind, 0.0);
		EXPECT_GE (toFace, 0.5);
		EXPECT_GE (toHorn, 0.5);
		EXPECT_GE (toStem, 0.05 - kTau);
		std::printf ("%s, echelle %g : K21 D derriere E : %u noeuds, marge %+.4f p ; D i. a %.3f deg de l'axe de face ; D e. a %.3f deg de la "
		             "corne ; D / tige %.4f p\n",
		             LevelName (c.detail), c.scale, common, behind, toFace, toHorn, toStem);
	}
}

// K24 (plancher 0,78, registre AC-7) et K24' (journal : rouge connu, dette ; registre AC-7) sur l'ornement complet.
TEST (TEST_cgarchi_classical_acanthus_minor_ornament, the_ornament_covers_the_bell)
{
	for (const Case &c : Cases ())
	{
		CASE_TRACE (c);
		ASSERT_TRUE (HasMinorLeaves (c));
		const double coverage = detail::BellCoverage (*c.ornament.mesh, c.ornament.pieces, c.env, 1.0, 23.0);
		const double visible = detail::VisibleBellFraction (*c.ornament.mesh, c.ornament.pieces, c.env, 10.0, 1.0, 23.0);
		EXPECT_GE (coverage, 0.78);
		std::printf ("%s, echelle %g : K24 couverture angulaire %.4f ; K24' corbeille visible en elevation %.4f (V-1 0,0557, ecart %+.4f)\n",
		             LevelName (c.detail), c.scale, coverage, visible, visible - 0.0557);
	}
}
