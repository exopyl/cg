#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <cfloat>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <cgarchi/classical_acanthus.h>
#include <cgarchi/classical_acanthus_detail.h>
#include <cgarchi/classical_mesh.h>
#include <cgarchi/classical_orders.h>
#include <cgarchi/classical_profile.h>
#include <cgarchi/classical_volute.h>
#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/mesh_validation.h"
#include "acanthus_test_support.h"

using namespace cgarchi::classical;

// ===========================================================================
//  Helices (C : helice) dans l'ornement : K29 a K44 pour les 8 H
// ===========================================================================
//
// Pieces 48 a 55 : tige k croissante (registre AC-46). Cotes ecrites d'apres le releve de la pl. XXVI (registre AC-59
// a AC-63), non lues dans le produit : spirale de Vignole a 12 centres x 0,36 (V 5,5 a 6,0) ; oeil a 3,45 de l'axe
// de face (vu de face : 2,9 a 3,5), h 30,05 (29,8 a 30,3), a 21,5 en avant de l'axe (profondeur de convention) ; plan
// vertical a 45 degres du plan de la face ; largeur du ruban 0,90 / 0,44 / 0,38 p au sommet des tours, bornee par
// l'oeil ; tige nee a h 25,5, cachee derriere le D interieur ; jour de la paire sur l'axe de face 2,2 +- 0,6 p.

namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg = kPi / 180.0;
constexpr double kTau = 1e-4;
constexpr double kC = 0.05;
constexpr std::size_t kFirstHelix = 48;
constexpr std::size_t kHelices = 8;
constexpr double kEyeX = 21.5, kEyeY = 3.45, kEyeH = 30.05;
using Point = std::array<double, 3>;

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

#define CASE_TRACE(c) SCOPED_TRACE (std::string (LevelName ((c).detail)) + ", echelle " + std::to_string ((c).scale))

Point Parts (const Mesh &mesh, const detail::CapitalEnvelope &env, unsigned v)
{
	float p[3] {};
	mesh.GetVertex (v, p);
	return {p[0] / env.scale, p[1] / env.scale, p[2] / env.scale - env.foot};
}

Point PartsOf (const std::array<double, 3> &p, const detail::CapitalEnvelope &env)
{
	return {p[0] / env.scale, p[1] / env.scale, p[2] / env.scale - env.foot};
}

// Axe de face vers lequel va l'helice de la tige k, et cote de la tige par rapport a cet axe (+1 : theta croissants).
double FaceOf (std::size_t k)
{
	return 0.5 * kPi * static_cast<double> ((k + 1) / 2);
}

double SideOf (std::size_t k)
{
	return k % 2 == 0 ? 1.0 : -1.0;
}

::testing::AssertionResult HasHelices (const Case &c)
{
	if (c.ornament.pieces.size () != kFirstHelix + kHelices + detail::kFleurons)
		return ::testing::AssertionFailure () << c.ornament.pieces.size () << " pieces, 60 attendues";
	for (std::size_t i = kFirstHelix; i < kFirstHelix + kHelices; ++i)
		if (c.ornament.pieces[i].kind != CapitalPieceKind::Helix)
			return ::testing::AssertionFailure () << "piece " << i << " : pas une helice";
	return ::testing::AssertionSuccess ();
}

void Eye (const Case &c, std::size_t i, unsigned ne, Point &centre, Point &axis)
{
	const CapitalPiece &p = c.ornament.pieces[i];
	ASSERT_GE (p.vertexEnd - p.vertexBegin, 2 * ne);
	const unsigned e0 = p.vertexEnd - 2 * ne;
	Point front {}, back {};
	for (unsigned j = 0; j < ne; ++j)
	{
		const Point a = Parts (*c.ornament.mesh, c.env, e0 + j), b = Parts (*c.ornament.mesh, c.env, e0 + ne + j);
		for (int q = 0; q < 3; ++q)
		{
			front[q] += a[q] / ne;
			back[q] += b[q] / ne;
		}
	}
	for (int q = 0; q < 3; ++q)
		centre[q] = 0.5 * (front[q] + back[q]);
	const double len = std::hypot (front[0] - back[0], front[1] - back[1], front[2] - back[2]);
	axis = {(front[0] - back[0]) / len, (front[1] - back[1]) / len, (front[2] - back[2]) / len};
}

void ExpectInvalid (const std::function<void ()> &call, const std::string &fragment)
{
	try
	{
		call ();
		ADD_FAILURE () << "aucune exception, attendu : " << fragment;
	}
	catch (const std::invalid_argument &e)
	{
		EXPECT_NE (std::string (e.what ()).find (fragment), std::string::npos) << e.what ();
	}
}

// Repere du plan de la representante de la tige 0 (axe de face 0) : u vers l'axe de face et vers l'avant, v vers le haut.
struct SpiralPlane
{
	Point eye, u;
	double U (const Point &p) const { return (p[0] - eye[0]) * u[0] + (p[1] - eye[1]) * u[1]; }
	double V (const Point &p) const { return p[2] - eye[2]; }
};

SpiralPlane PlaneZero ()
{
	return {{kEyeX, kEyeY, kEyeH}, {std::sqrt (0.5), -std::sqrt (0.5), 0.0}};
}

// Jour entre deux pieces : sommets contre triangles, dans les deux sens (boite elargie de 1 p).
double Distance (const Mesh &mesh, const detail::CapitalEnvelope &env, const CapitalPiece &a, const CapitalPiece &b)
{
	return acanthus_test::PieceGap (mesh, env, a, b, 1.0);
}

} // namespace

// K29 : comptes attendus : Light 14 + 33 stations, n_q 9, n_e 12 ; Heavy 28 + 85, 15, 32 ; deux coques ;
// ornement : 8 F, 8 E, 8 C, 16 D, 8 V, puis 8 H.
TEST (TEST_cgarchi_classical_acanthus_helix, the_helices_have_their_counts_and_their_place_in_the_ornament)
{
	struct Expected
	{
		OrnamentDetail detail;
		unsigned stem, spiral, nq, ne, faces, vertices;
	};
	for (const Expected &e : {Expected {OrnamentDetail::Light, 14, 33, 9, 12, 886, 447}, Expected {OrnamentDetail::Heavy, 28, 85, 15, 32, 3510, 1759}})
	{
		SCOPED_TRACE (LevelName (e.detail));
		const Case &c = Cases ()[e.detail == OrnamentDetail::Light ? 0 : 2];
		for (const int stem : {0, 1})
		{
			const detail::VolutePiece v = detail::BuildVoluteRibbon (c.env, detail::DefaultHelixSpec (e.detail), stem);
			EXPECT_EQ (v.stemStations, e.stem);
			EXPECT_EQ (v.spiralStations, e.spiral);
			EXPECT_EQ (v.sectionPoints, e.nq);
			const unsigned ns = v.stemStations + v.spiralStations;
			EXPECT_EQ (v.ribbonTriangles, 2 * (ns - 1) * v.sectionPoints + 2 * (v.sectionPoints - 2));
			EXPECT_EQ (v.triangles.size () - v.ribbonTriangles, 4u * e.ne - 4u);
			EXPECT_EQ (v.triangles.size (), e.faces);
			EXPECT_EQ (v.positions.size (), e.vertices);
			MeshValidationOptions o;
			o.expectedComponents = 2u;
			const MeshValidationReport r = ValidateMesh (*MakeTriangleMesh (v.positions, v.triangles), o);
			EXPECT_TRUE (r.IsValid ());
		}
		ASSERT_TRUE (HasHelices (c));
		for (std::size_t i = kFirstHelix; i < kFirstHelix + kHelices; ++i)
		{
			EXPECT_EQ (c.ornament.pieces[i].faceEnd - c.ornament.pieces[i].faceBegin, e.faces) << i;
			EXPECT_EQ (c.ornament.pieces[i].vertexEnd - c.ornament.pieces[i].vertexBegin, e.vertices) << i;
		}
	}
}

// K30 : oeil de chaque H a (21,5 ; 3,45) dans le repere de son axe de face, du cote de sa tige, h 30,05 ; axe de l'oeil
// a 45 degres de la normale de la face.
TEST (TEST_cgarchi_classical_acanthus_helix, each_helix_eye_and_plane_stand_on_their_values)
{
	for (const Case &c : Cases ())
	{
		CASE_TRACE (c);
		ASSERT_TRUE (HasHelices (c));
		const unsigned ne = c.detail == OrnamentDetail::Light ? 12u : 32u;
		double worstP = 0.0, worstA = 0.0;
		for (std::size_t k = 0; k < kHelices; ++k)
		{
			Point centre {}, axis {};
			Eye (c, kFirstHelix + k, ne, centre, axis);
			const double f = FaceOf (k), cf = std::cos (f), sf = std::sin (f);
			const double along = centre[0] * cf + centre[1] * sf, across = (-centre[0] * sf + centre[1] * cf) * SideOf (k);
			worstP = std::max ({worstP, std::fabs (along - kEyeX), std::fabs (across - kEyeY), std::fabs (centre[2] - kEyeH)});
			worstA = std::max (worstA, std::fabs (std::acos (std::min (1.0, std::fabs (axis[0] * cf + axis[1] * sf))) - 45.0 * kDeg));
		}
		EXPECT_LE (worstP, 2e-5);
		EXPECT_LE (worstA, 1e-4);
		std::printf ("%s, echelle %g : K30 oeil des H %.1e p ; plan %.1e rad\n", LevelName (c.detail), c.scale, worstP, worstA);
	}
}

// K31 : copie = construction (4 ulp float a la cote du tour) ; K32 : symetrie D4 des H, controle negatif pi/4.
TEST (TEST_cgarchi_classical_acanthus_helix, the_helix_copies_and_symmetry)
{
	const double cr = std::cos (kPi / 4.0), sr = std::sin (kPi / 4.0);
	for (const Case &c : Cases ())
	{
		CASE_TRACE (c);
		ASSERT_TRUE (HasHelices (c));
		double worst = 0.0;
		bool exact = true;
		for (std::size_t k = 0; k < kHelices; ++k)
		{
			const detail::VolutePiece v = detail::BuildVoluteRibbon (c.env, detail::DefaultHelixSpec (c.detail), static_cast<int> (k % 2));
			const CapitalPiece &piece = c.ornament.pieces[kFirstHelix + k];
			ASSERT_EQ (piece.vertexEnd - piece.vertexBegin, v.positions.size ());
			const double a = 0.5 * kPi * static_cast<double> (k / 2), ca = std::cos (a), sa = std::sin (a);
			for (std::size_t j = 0; j < v.positions.size (); ++j)
			{
				float q[3] {};
				c.ornament.mesh->GetVertex (piece.vertexBegin + static_cast<unsigned> (j), q);
				const std::array<double, 3> &p = v.positions[j];
				const double x = k < 2 ? p[0] : ca * p[0] - sa * p[1], y = k < 2 ? p[1] : sa * p[0] + ca * p[1];
				if (k < 2)
					exact = exact && q[0] == static_cast<float> (x) && q[1] == static_cast<float> (y) && q[2] == static_cast<float> (p[2]);
				worst = std::max (worst, std::hypot (q[0] - x, q[1] - y, q[2] - p[2]) / c.env.scale);
			}
		}
		EXPECT_TRUE (exact);
		EXPECT_LE (worst, 4.0 * std::numeric_limits<float>::epsilon () * (c.env.foot + 36.0));
		std::vector<Point> pts;
		for (unsigned v = c.ornament.pieces[kFirstHelix].vertexBegin; v < c.ornament.pieces[kFirstHelix + kHelices - 1].vertexEnd; ++v)
			pts.push_back (Parts (*c.ornament.mesh, c.env, v));
		const auto nearest = [&] (const Point &q) {
			double best = std::numeric_limits<double>::infinity ();
			for (const Point &p : pts)
				best = std::min (best, std::hypot (p[0] - q[0], p[1] - q[1], p[2] - q[2]));
			return best;
		};
		double m0 = 0.0, m45 = 0.0, quarter = 0.0, eighth = 0.0;
		for (std::size_t i = 0; i < pts.size (); i += c.detail == OrnamentDetail::Light ? 1 : 7)
		{
			const Point &p = pts[i];
			m0 = std::max (m0, nearest ({p[0], -p[1], p[2]}));
			m45 = std::max (m45, nearest ({p[1], p[0], p[2]}));
			quarter = std::max (quarter, nearest ({-p[1], p[0], p[2]}));
		}
		const unsigned ne = c.detail == OrnamentDetail::Light ? 12u : 32u;
		for (std::size_t k = 0; k < kHelices; ++k)
		{
			Point centre {}, axis {};
			Eye (c, kFirstHelix + k, ne, centre, axis);
			eighth = std::max (eighth, nearest ({cr * centre[0] - sr * centre[1], sr * centre[0] + cr * centre[1], centre[2]}));
		}
		EXPECT_LE (m0, 1e-5);
		EXPECT_LE (m45, 1e-5);
		EXPECT_LE (quarter, 1e-5);
		EXPECT_GT (eighth, 1.0);
		std::printf ("%s, echelle %g : K31 copie %.2e p ; K32 D4 des H %.2e / %.2e / %.2e p, pi/4 %.3f p (negatif)\n", LevelName (c.detail),
		             c.scale, worst, m0, m45, quarter, eighth);
	}
}

// K33 : tige G1 a la jonction (echantillonnage fin : angle a la jonction du meme ordre que chez les voisines).
TEST (TEST_cgarchi_classical_acanthus_helix, the_helix_stem_joins_its_spiral_smoothly)
{
	const Case &c = Cases ()[0];
	detail::VoluteSpec spec = detail::DefaultHelixSpec (OrnamentDetail::Light);
	spec.samplesPerQuarter = 64;
	spec.bendSegments = 400;
	const detail::VolutePiece v = detail::BuildVoluteRibbon (c.env, spec, 0);
	const unsigned nq = v.sectionPoints;
	const auto centre = [&] (unsigned station) {
		Point m {};
		for (unsigned i = 0; i < nq; ++i)
			for (int q = 0; q < 3; ++q)
				m[q] += PartsOf (v.positions[station * nq + i], c.env)[q] / nq;
		return m;
	};
	const auto turning = [&] (unsigned j) {
		const Point a = centre (j - 1), b = centre (j), d = centre (j + 1);
		const Point u {b[0] - a[0], b[1] - a[1], b[2] - a[2]}, w {d[0] - b[0], d[1] - b[1], d[2] - b[2]};
		const double cosA = (u[0] * w[0] + u[1] * w[1] + u[2] * w[2]) / (std::hypot (u[0], u[1], u[2]) * std::hypot (w[0], w[1], w[2]));
		return std::acos (std::clamp (cosA, -1.0, 1.0));
	};
	ASSERT_GE (v.stemStations, 2u);
	ASSERT_GE (v.spiralStations, 3u);
	ASSERT_GE (v.positions.size (), static_cast<std::size_t> (v.stemStations + v.spiralStations) * nq);
	const unsigned j = v.stemStations;
	const double at = turning (j), before = turning (j - 1), after = turning (j + 1);
	EXPECT_LE (at, std::max (before, after) + 0.5 * kDeg);
	// Spirale du ruban G1, raccords entre arcs compris : le bord exterieur tourne a chaque station de l'angle d'un pas
	// d'arc ; au raccord de deux arcs, de la demi-somme des pas voisins, donc entre les angles des deux stations
	// voisines. Une cassure (G0) s'y ajoute : depassement de l'angle d'une station hors de l'intervalle de ses voisines.
	const auto outerAt = [&] (unsigned station) { return PartsOf (v.positions[station * nq], c.env); };
	std::vector<double> turn;
	for (unsigned s = v.stemStations + 1; s + 1 < v.stemStations + v.spiralStations; ++s)
	{
		const Point a = outerAt (s - 1), b = outerAt (s), d = outerAt (s + 1);
		const Point u {b[0] - a[0], b[1] - a[1], b[2] - a[2]}, w {d[0] - b[0], d[1] - b[1], d[2] - b[2]};
		const double cosA = (u[0] * w[0] + u[1] * w[1] + u[2] * w[2]) / (std::hypot (u[0], u[1], u[2]) * std::hypot (w[0], w[1], w[2]));
		turn.push_back (std::acos (std::clamp (cosA, -1.0, 1.0)));
	}
	ASSERT_GE (turn.size (), 3u);
	double spiralBreak = 0.0;
	for (std::size_t s = 1; s + 1 < turn.size (); ++s)
		spiralBreak = std::max ({spiralBreak, turn[s] - std::max (turn[s - 1], turn[s + 1]), std::min (turn[s - 1], turn[s + 1]) - turn[s]});
	EXPECT_LE (spiralBreak, 1e-6);
	std::printf ("K33 helice, jonction tige / spirale : %.3f deg (voisines %.3f / %.3f) ; spirale du ruban : cassure %.1e rad\n",
	             at / kDeg, before / kDeg, after / kDeg, spiralBreak);
}

// K34, K35 : hors de la corbeille (jeu >= c), sous l'abaque (h <= 36 - c), dans le cone (garde stricte) ; ecart a
// l'abaque (P4) journalise : exception a AC-2, du type de celle du D interieur.
TEST (TEST_cgarchi_classical_acanthus_helix, the_helices_keep_off_the_bell_under_the_abacus_and_in_the_cone)
{
	for (const Case &c : Cases ())
	{
		CASE_TRACE (c);
		ASSERT_TRUE (HasHelices (c));
		double clearance = std::numeric_limits<double>::infinity (), cone = -clearance, abacus = -clearance, top = -clearance;
		for (std::size_t i = kFirstHelix; i < kFirstHelix + kHelices; ++i)
		{
			const CapitalPiece &p = c.ornament.pieces[i];
			clearance = std::min (clearance, detail::CoreClearance (*c.ornament.mesh, c.env, p));
			cone = std::max (cone, detail::EnvelopeExcess (*c.ornament.mesh, c.env, p));
			abacus = std::max (abacus, detail::EnvelopeExcess (*c.ornament.mesh, c.env, p.vertexBegin, p.vertexEnd));
			for (unsigned v = p.vertexBegin; v < p.vertexEnd; ++v)
				top = std::max (top, Parts (*c.ornament.mesh, c.env, v)[2]);
		}
		EXPECT_GE (clearance, kC - kTau);
		EXPECT_LT (cone, 0.0);
		EXPECT_LE (top, 36.0 - kC + kTau);
		std::printf ("%s, echelle %g : K34 jeu au noyau %.4f p, h max %.4f ; K35 cone %.4f p, abaque (P4, journal) %+.4f p\n", LevelName (c.detail),
		             c.scale, clearance, top, cone, abacus);
	}
}

// K36 : la tige de chaque H passe derriere le D interieur de sa tige de caulicole ; jour de la paire sur l'axe de face
// 2,2 +- 0,6 p ; jour entre chaque H et les autres pieces >= c.
TEST (TEST_cgarchi_classical_acanthus_helix, each_helix_stem_passes_behind_its_inner_minor_leaf_and_the_pair_leaves_its_gap)
{
	using acanthus_test::RadiusGrid;
	for (const Case &c : Cases ())
	{
		if (c.scale != 1.0)
			continue;
		CASE_TRACE (c);
		ASSERT_TRUE (HasHelices (c));
		const Mesh &mesh = *c.ornament.mesh;
		const std::vector<CapitalPiece> &P = c.ornament.pieces;
		double behind = std::numeric_limits<double>::infinity ();
		unsigned common = 0;
		for (std::size_t k = 0; k < kHelices; ++k)
		{
			const CapitalPiece &innerD = P[24 + 2 * k];
			ASSERT_EQ (innerD.kind, CapitalPieceKind::MinorLeaf);
			const double stemAxis = kPi / 8.0 + k * kPi / 4.0;
			RadiusGrid hOut, dIn;
			acanthus_test::Rasterize (hOut, mesh, c.env, P[kFirstHelix + k].faceBegin, P[kFirstHelix + k].faceEnd, true, stemAxis);
			acanthus_test::Rasterize (dIn, mesh, c.env, innerD.faceBegin, innerD.faceEnd, false, stemAxis);
			for (int j = 0; j < RadiusGrid::kNH; ++j)
				for (int ii = 0; ii < RadiusGrid::kNTheta; ++ii)
					if (!std::isnan (hOut.At (ii, j)) && !std::isnan (dIn.At (ii, j)))
					{
						++common;
						behind = std::min (behind, dIn.At (ii, j) - kC - hOut.At (ii, j));
					}
		}
		EXPECT_GT (common, 0u);
		EXPECT_GE (behind, 0.0);
		// jour sur l'axe de face 0 : pieces des tiges 0 et 7
		double gap = std::numeric_limits<double>::infinity ();
		for (const std::size_t k : {std::size_t {0}, std::size_t {7}})
			for (unsigned v = P[kFirstHelix + k].vertexBegin; v < P[kFirstHelix + k].vertexEnd; ++v)
				gap = std::min (gap, 2.0 * std::fabs (Parts (mesh, c.env, v)[1]));
		EXPECT_NEAR (gap, 2.2, 0.6);
		double others = std::numeric_limits<double>::infinity ();
		std::size_t closest = 0;
		// F, E, C, D, V, et l'helice de la tige 7, l'autre de la paire sur l'axe de face 0
		for (std::size_t i = 0; i <= kFirstHelix + 7; ++i)
		{
			if (i >= kFirstHelix && i < kFirstHelix + 7)
				continue;
			const double d = Distance (mesh, c.env, P[kFirstHelix], P[i]);
			if (d < others)
			{
				others = d;
				closest = i;
			}
		}
		EXPECT_GE (others, kC) << "piece " << closest;
		std::printf ("%s : K36 tige derriere le D i. %.4f p (%u noeuds) ; jour sur l'axe de face %.4f p ; H / autres pieces %.4f p (piece %zu, sommets contre triangles)\n",
		             LevelName (c.detail), behind, common, gap, others, closest);
	}
}

// K39 : diametre vertical V = 16 k = 5,76 (releve 5,5 a 6,0, +-1) ; raccourci vu de face de l'helice 0,6 a 0,9 (plan a
// 45 degres : 0,71) ; plus bas sommet de la tige a h 25,5 (+0,05).
TEST (TEST_cgarchi_classical_acanthus_helix, the_helix_meets_the_plate_dimensions)
{
	for (const Case &c : Cases ())
	{
		CASE_TRACE (c);
		const detail::VolutePiece v = detail::BuildVoluteRibbon (c.env, detail::DefaultHelixSpec (c.detail), 0);
		const SpiralPlane plane = PlaneZero ();
		double uLo = 1e9, uHi = -1e9, vLo = 1e9, vHi = -1e9, yLo = 1e9, yHi = -1e9, low = 1e9;
		ASSERT_LE (v.ribbonVertices, v.positions.size ());
		for (std::size_t j = 0; j < v.ribbonVertices; ++j)
		{
			const Point p = PartsOf (v.positions[j], c.env);
			low = std::min (low, p[2]);
			if (j < static_cast<std::size_t> (v.stemStations) * v.sectionPoints)
				continue;
			uLo = std::min (uLo, plane.U (p));
			uHi = std::max (uHi, plane.U (p));
			vLo = std::min (vLo, plane.V (p));
			vHi = std::max (vHi, plane.V (p));
			yLo = std::min (yLo, p[1]);
			yHi = std::max (yHi, p[1]);
		}
		const double H = uHi - uLo, V = vHi - vLo, front = (yHi - yLo) / H;
		EXPECT_NEAR (V, 5.75, 1.0);
		EXPECT_GE (front, 0.6);
		EXPECT_LE (front, 0.9);
		EXPECT_GE (low, 25.5 - kTau);
		EXPECT_LE (low, 25.55);
		std::printf ("%s, echelle %g : K39 helice H %.3f, V %.3f p ; vue de face %.3f ; tige des h %.3f\n", LevelName (c.detail), c.scale, H, V,
		             front, low);
	}
}

// K40 : le prefixe F + E + C + D + V de l'ornement (63 488 / 271 808 premieres faces, 31 856 / 136 016 premiers
// sommets) a ces empreintes. Topologie (comptes, arite et indices de chaque face) a l'egalite ; sommets : somme
// ponderee de i |v_i|^2 a 1e-8 pres (relatif) et boite a 4 ulp float ; avec MSVC, sommets float a l'octet (FNV-1a ;
// un autre compilateur ou une autre bibliotheque mathematique peut en changer un dernier bit).
TEST (TEST_cgarchi_classical_acanthus_helix, the_leaves_stems_and_volutes_are_unchanged)
{
	struct Expected
	{
		OrnamentDetail detail;
		double scale;
		unsigned vertices, faces;
		std::uint64_t topology, bits;
		double weighted;
		std::array<double, 3> lo, hi;
	};
	const Expected expected[] = {{OrnamentDetail::Light, 1.0, 31856, 63488, 0x88b43121403d53acULL, 0xee6756fb4ea426d6ULL, 59015050193722.672, {-26.9386864, -26.9386864, 318.049988}, {26.9386864, 26.9386864, 353.730011}},
	                             {OrnamentDetail::Light, 2.0, 31856, 63488, 0x88b43121403d53acULL, 0x16b59ba5bbb4bc63ULL, 236060200774890.69, {-53.8773727, -53.8773727, 636.099976}, {53.8773727, 53.8773727, 707.460022}},
	                             {OrnamentDetail::Heavy, 1.0, 136016, 271808, 0xd04d284de7cd0056ULL, 0x19588f8c2feaea77ULL, 1064607313149432.9, {-26.99617, -26.99617, 318.049988}, {26.99617, 26.99617, 353.730011}},
	                             {OrnamentDetail::Heavy, 2.0, 136016, 271808, 0xd04d284de7cd0056ULL, 0x2329ce9236a12cd6ULL, 4258429252597731.5, {-53.9923401, -53.9923401, 636.099976}, {53.9923401, 53.9923401, 707.460022}}};
	for (const Expected &e : expected)
	{
		const Case *c = nullptr;
		for (const Case &k : Cases ())
			if (k.detail == e.detail && k.scale == e.scale)
				c = &k;
		ASSERT_NE (c, nullptr);
		CASE_TRACE (*c);
		ASSERT_TRUE (HasHelices (*c));
		EXPECT_EQ (c->ornament.pieces[kFirstHelix].vertexBegin, e.vertices);
		EXPECT_EQ (c->ornament.pieces[kFirstHelix].faceBegin, e.faces);
		ASSERT_GE (c->ornament.mesh->GetNVertices (), static_cast<int> (e.vertices));
		ASSERT_GE (c->ornament.mesh->GetNFaces (), static_cast<int> (e.faces));
		const acanthus_test::PrefixPrint p = acanthus_test::MeasurePrefix (*c->ornament.mesh, e.vertices, e.faces);
		EXPECT_EQ (p.topology, e.topology);
		EXPECT_NEAR (p.weighted, e.weighted, 1e-8 * e.weighted);
		for (std::size_t k = 0; k < 3; ++k)
		{
			EXPECT_NEAR (p.lo[k], e.lo[k], 4.0 * FLT_EPSILON * std::fabs (e.lo[k])) << k;
			EXPECT_NEAR (p.hi[k], e.hi[k], 4.0 * FLT_EPSILON * std::fabs (e.hi[k])) << k;
		}
#if defined (_MSC_VER)
		EXPECT_EQ (p.bits, e.bits);
#endif
		std::printf ("K40 helix %s %.1f topo=%016llx bits=%016llx weighted=%.17g lo=%.9g,%.9g,%.9g hi=%.9g,%.9g,%.9g\n", LevelName (e.detail), e.scale,
		             static_cast<unsigned long long> (p.topology), static_cast<unsigned long long> (p.bits), p.weighted, p.lo[0], p.lo[1], p.lo[2],
		             p.hi[0], p.hi[1], p.hi[2]);
	}
}

// K43 : gardes propres aux helices (domaine du plan, ecart de la tige).
TEST (TEST_cgarchi_classical_acanthus_helix, the_helix_guards_are_seen)
{
	const detail::CapitalEnvelope &env = Cases ()[0].env;
	detail::VoluteSpec s = detail::DefaultHelixSpec (OrnamentDetail::Light);
	s.planeAngle = 0.5 * kPi;
	ExpectInvalid ([&] { detail::BuildVoluteRibbon (env, s, 0); }, "planeAngle");
	s = detail::DefaultHelixSpec (OrnamentDetail::Light);
	s.stemLift = -0.1;
	ExpectInvalid ([&] { detail::BuildVoluteRibbon (env, s, 0); }, "stemLift");
	s = detail::DefaultHelixSpec (OrnamentDetail::Light);
	s.stemLift = 0.0;
	ExpectInvalid ([&] { detail::BuildVoluteRibbon (env, s, 0); }, "dans la corbeille");
}

// K44 : bord exterieur sur le contour (12 centres x 0,36) a 1e-9 ; largeur au sommet des tours 0,90 / 0,44 / 0,38 +- 0,3 ;
// jour entre tours >= c.
TEST (TEST_cgarchi_classical_acanthus_helix, the_helix_ribbon_follows_the_contour_with_its_widths_and_gaps)
{
	IonicVoluteData data;
	data.eyeDiameter = 2.0 * 0.36;
	data.centreFromTop = 9.0 * 0.36;
	const VoluteSpiral spiral = BuildTwelveCentreVolute (data, VoluteJunction::CentreLine);
	for (const Case &c : Cases ())
	{
		CASE_TRACE (c);
		const detail::VolutePiece v = detail::BuildVoluteRibbon (c.env, detail::DefaultHelixSpec (c.detail), 0);
		const SpiralPlane plane = PlaneZero ();
		const unsigned nq = v.sectionPoints, nf = nq - 2, first = v.stemStations;
		ASSERT_GE (nq, 3u);
		ASSERT_GE (v.positions.size (), static_cast<std::size_t> (first + v.spiralStations) * nq);
		std::vector<std::array<double, 2>> outer, inner;
		for (unsigned j = first; j < first + v.spiralStations; ++j)
		{
			const Point o = PartsOf (v.positions[j * nq], c.env), i = PartsOf (v.positions[j * nq + nf - 1], c.env);
			outer.push_back ({plane.U (o), plane.V (o)});
			inner.push_back ({plane.U (i), plane.V (i)});
		}
		double onContour = 0.0;
		for (const std::array<double, 2> &o : outer)
		{
			double best = std::numeric_limits<double>::infinity ();
			for (const VoluteArc &a : spiral.outer.arcs)
				best = std::min (best, std::fabs (std::hypot (o[0] - a.centre.u, o[1] - a.centre.v) - a.radius));
			onContour = std::max (onContour, best);
		}
		EXPECT_LE (onContour, 1e-8);
		const auto widthAt = [&] (std::size_t j) { return std::hypot (outer[j][0] - inner[j][0], outer[j][1] - inner[j][1]); };
		std::vector<double> widths {widthAt (0)};
		for (std::size_t j = 1; j + 1 < outer.size (); ++j)
			if (outer[j][1] > 0.0 && outer[j][0] <= 0.0 && outer[j + 1][0] > 0.0)
				widths.push_back (widthAt (std::fabs (outer[j][0]) <= std::fabs (outer[j + 1][0]) ? j : j + 1));
		ASSERT_GE (widths.size (), 3u);
		EXPECT_NEAR (widths[0], 0.90, 0.3);
		EXPECT_NEAR (widths[1], 0.44, 0.3);
		EXPECT_NEAR (widths[2], 0.38, 0.3);
		const std::size_t lag = static_cast<std::size_t> (3 * detail::DefaultHelixSpec (c.detail).samplesPerQuarter);
		double gap = std::numeric_limits<double>::infinity ();
		for (std::size_t j = 0; j < inner.size (); ++j)
			for (std::size_t q = j + lag; q + 1 < outer.size (); ++q)
			{
				const double ax = outer[q][0], ay = outer[q][1], bx = outer[q + 1][0], by = outer[q + 1][1];
				const double dx = bx - ax, dy = by - ay, L = dx * dx + dy * dy;
				const double t = L > 0.0 ? std::clamp (((inner[j][0] - ax) * dx + (inner[j][1] - ay) * dy) / L, 0.0, 1.0) : 0.0;
				gap = std::min (gap, std::hypot (inner[j][0] - ax - t * dx, inner[j][1] - ay - t * dy));
			}
		EXPECT_GE (gap, kC - kTau);
		std::printf ("%s, echelle %g : K44 helice contour %.1e p ; largeurs %.3f / %.3f / %.3f p ; jour entre tours %.4f p ; %.3f tours\n",
		             LevelName (c.detail), c.scale, onContour, widths[0], widths[1], widths[2], gap, v.turns);
	}
}
