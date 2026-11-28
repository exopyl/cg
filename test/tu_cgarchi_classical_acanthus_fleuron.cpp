#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include <cgarchi/classical_acanthus.h>
#include <cgarchi/classical_acanthus_detail.h>
#include <cgarchi/classical_lathe.h>
#include <cgarchi/classical_mesh.h>
#include <cgarchi/classical_orders.h>
#include <cgarchi/classical_profile.h>
#include <cgarchi/parameterized_classical.h>
#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/mesh_validation.h"
#include "acanthus_test_support.h"

using namespace cgarchi::classical;

// Fleuron G (lot A5) : tour du chapiteau lu comme LatheColumnProfile le maille, rosace seule. Cotes en parties,
// h depuis le pied du chapiteau, face theta = 0 (x radial, y le long de la face).

namespace
{

constexpr double kPi = 3.14159265358979323846;
constexpr double kC = 0.05;
constexpr double kTau = 1e-6;
// Sommets du tour et de la rosace stockes en float : ulp 2e-6 p a r 20, deux maillages.
constexpr double kFloat = 1e-5;

using Point = std::array<double, 3>;

const char *LevelName (OrnamentDetail d)
{
	return d == OrnamentDetail::Light ? "Light" : "Heavy";
}

// Reglage de colonne : nombre de secteurs du tour, cannelures (grille des cannelures au lieu de radialSegments).
struct Setting
{
	int radialSegments;
	bool flutes;
	const char *name;
};

const std::vector<Setting> &Settings ()
{
	static const std::vector<Setting> s {{8, false, "8 secteurs"}, {48, false, "48 secteurs"}, {256, false, "256 secteurs"},
	                                     {48, true, "48 secteurs, cannelures"}};
	return s;
}

struct Column
{
	ColumnProfile profile;
	detail::CapitalEnvelope env;
	detail::CapitalTour tour;
	std::unique_ptr<Mesh> lathe;
};

Column MakeColumn (const Setting &setting, double scale = 1.0)
{
	const OrderData &order = ClassicalOrders ().Get (Order::Corinthian);
	ColumnOptions options;
	options.order = Order::Corinthian;
	options.flutes = setting.flutes;
	Column c;
	c.profile = BuildColumnProfile (order, options);
	c.env = detail::ReadCapitalEnvelope (order, c.profile, scale);
	LatheOptions lathe;
	lathe.radialSegments = setting.radialSegments;
	lathe.scale = scale;
	c.tour = detail::ReadCapitalTour (c.profile, LatheAngles (c.profile, lathe), c.env);
	c.lathe = LatheColumnProfile (c.profile, lathe);
	return c;
}

const Column &DefaultColumn ()
{
	static const Column c = MakeColumn (Settings ()[1]);
	return c;
}

Point Parts (const Point &p, const detail::CapitalEnvelope &env)
{
	return {p[0] / env.scale, p[1] / env.scale, p[2] / env.scale - env.foot};
}

// Plus grand x d'un triangle dans la boite (h, y), par decoupage : instrument independant du tour, valide par la valeur
// connue du test de la bosse (the_stem_back_clears_a_tour_bulging_between_its_rings : bosse de 0,3 p vue a sa cote).
double ClipMaxX (std::vector<Point> poly, double hLo, double hHi, double yLo, double yHi)
{
	const auto clip = [] (const std::vector<Point> &in, int axis, double value, double s) {
		std::vector<Point> out;
		for (std::size_t k = 0; k < in.size (); ++k)
		{
			const Point &a = in[k], &b = in[(k + 1) % in.size ()];
			const double da = s * (a[axis] - value), db = s * (b[axis] - value);
			if (da >= 0.0)
				out.push_back (a);
			if ((da >= 0.0) != (db >= 0.0))
			{
				const double t = da / (da - db);
				out.push_back ({a[0] + t * (b[0] - a[0]), a[1] + t * (b[1] - a[1]), a[2] + t * (b[2] - a[2])});
			}
		}
		return out;
	};
	poly = clip (poly, 2, hLo, 1.0);
	poly = clip (poly, 2, hHi, -1.0);
	poly = clip (poly, 1, yLo, 1.0);
	poly = clip (poly, 1, yHi, -1.0);
	double best = -std::numeric_limits<double>::infinity ();
	for (const Point &p : poly)
		best = std::max (best, p[0]);
	return best;
}

// Triangles du maillage du tour de `c`, en parties, dont le plus grand x depasse xMin et dont l'etendue en h coupe
// [hLo, hHi].
std::vector<std::array<Point, 3>> LatheTriangles (const Column &c, double xMin, double hLo, double hHi)
{
	std::vector<std::array<Point, 3>> tris;
	for (unsigned f = 0; f < static_cast<unsigned> (c.lathe->GetNFaces ()); ++f)
	{
		std::array<Point, 3> t;
		for (unsigned e = 0; e < 3; ++e)
		{
			float q[3] {};
			c.lathe->GetVertex (static_cast<unsigned> (c.lathe->GetFaceVertex (f, e)), q);
			t[e] = Parts ({q[0], q[1], q[2]}, c.env);
		}
		if (std::max ({t[0][0], t[1][0], t[2][0]}) > xMin && std::max ({t[0][2], t[1][2], t[2][2]}) >= hLo &&
		    std::min ({t[0][2], t[1][2], t[2][2]}) <= hHi)
			tris.push_back (t);
	}
	return tris;
}

struct Rosette
{
	detail::FleuronSpec spec;
	detail::FleuronPiece piece;
	std::vector<Point> parts;   // sommets en parties
};

Rosette MakeRosette (OrnamentDetail level, const Column &column = DefaultColumn ())
{
	Rosette r;
	r.spec = detail::DefaultFleuronSpec (level);
	r.piece = detail::BuildFleuron (column.env, column.tour, r.spec);
	for (const Point &p : r.piece.positions)
		r.parts.push_back (Parts (p, column.env));
	return r;
}

std::size_t Grid (const detail::FleuronSpec &s)
{
	return static_cast<std::size_t> (1 + s.petals * s.samplesPerPetal * s.rings);
}

// Maxima locaux stricts d'une suite circulaire.
int CircularMaxima (const std::vector<double> &v)
{
	int count = 0;
	for (std::size_t i = 0; i < v.size (); ++i)
		if (v[i] > v[(i + v.size () - 1) % v.size ()] && v[i] > v[(i + 1) % v.size ()])
			++count;
	return count;
}

} // namespace

// TourDepthMax contre le maillage de LatheColumnProfile, decoupe triangle par triangle (sommets en float), a chaque
// reglage, sur des boites minces (0,001) et epaisses dans chaque zone du tour derriere la rosace : cone de la
// corbeille, marche h 36 (cone / B), B, marche h 39 (B / A), listel de A, talon, haut du chapiteau ; y de -4,6 a 4,7.
TEST (TEST_cgarchi_classical_acanthus_fleuron, the_tour_depth_equals_the_lathe_mesh)
{
	for (const Setting &s : Settings ())
	{
		SCOPED_TRACE (s.name);
		const Column c = MakeColumn (s);
		const std::vector<std::array<Point, 3>> tris = LatheTriangles (c, 0.0, 33.0, 43.0);
		ASSERT_GT (tris.size (), 0u);
		double worst = 0.0;
		int boxes = 0;
		for (const double hLo : {33.6, 34.8, 35.9, 35.98, 36.0, 37.2, 38.9, 38.98, 39.0, 39.5, 40.2, 40.9, 41.6, 41.95})
			for (const double yLo : {-4.6, -2.9, -1.1, 0.0, 1.3, 3.1, 4.7})
				for (const double dh : {0.001, 0.3})
					for (const double dy : {0.001, 0.4})
					{
						double ref = -std::numeric_limits<double>::infinity ();
						for (const auto &t : tris)
						{
							const double tyLo = std::min ({t[0][1], t[1][1], t[2][1]}), tyHi = std::max ({t[0][1], t[1][1], t[2][1]});
							const double thLo = std::min ({t[0][2], t[1][2], t[2][2]}), thHi = std::max ({t[0][2], t[1][2], t[2][2]});
							if (tyHi < yLo || tyLo > yLo + dy || thHi < hLo || thLo > hLo + dh)
								continue;
							ref = std::max (ref, ClipMaxX ({t[0], t[1], t[2]}, hLo, hLo + dh, yLo, yLo + dy));
						}
						const double got = detail::TourDepthMax (c.tour, hLo, hLo + dh, yLo, yLo + dy);
						ASSERT_EQ (std::isfinite (ref), std::isfinite (got)) << "h " << hLo << " y " << yLo;
						if (std::isfinite (ref))
						{
							worst = std::max (worst, std::fabs (got - ref));
							++boxes;
						}
					}
		EXPECT_EQ (boxes, 14 * 7 * 4);
		EXPECT_LE (worst, kFloat);
		std::printf ("%s : tour contre maillage, %d boites, ecart max %.2e p\n", s.name, boxes, worst);
	}
}

// Entre deux secteurs, le tour est la corde : sur la face concave de B, elle passe devant l'anneau (8 secteurs : de
// plus de 0,1 p a y 4 ; 48 secteurs : de plus de 0,005 p).
TEST (TEST_cgarchi_classical_acanthus_fleuron, the_tour_is_the_chord_not_the_ring)
{
	for (const auto &[setting, least] : {std::make_pair (Settings ()[0], 0.1), std::make_pair (Settings ()[1], 0.005)})
	{
		const Column c = MakeColumn (setting);
		const double y = 4.0;
		double x = 17.6;
		for (int it = 0; it < 50; ++it)
		{
			const double r = c.env.abacus.RadiusAt (std::atan2 (y, x));
			x = std::sqrt (r * r - y * y);
		}
		const double chord = detail::TourDepthMax (c.tour, 37.5, 37.5, y, y);
		EXPECT_GE (chord - x, least) << setting.name;
		std::printf ("%s : B a h 37,5, y 4 : corde %.4f, anneau %.4f, ecart %.4f p\n", setting.name, chord, x, chord - x);
	}
}

// K45 : rosace 4 n R triangles, 2 (1 + n R) sommets (n = petals samplesPerPetal, R = rings) ; tige 2 q (z - 1) +
// 2 (q - 2) triangles, q z sommets (q = stemSection, z = stemRings), apres la rosace.
TEST (TEST_cgarchi_classical_acanthus_fleuron, the_rosette_counts_follow_the_formula)
{
	for (const auto &[level, faces, vertices, stemFaces, stemVertices] :
	     {std::make_tuple (OrnamentDetail::Light, 2164u, 1086u, 116u, 60u), std::make_tuple (OrnamentDetail::Heavy, 7484u, 3746u, 572u, 288u)})
	{
		const Rosette r = MakeRosette (level);
		const unsigned n = static_cast<unsigned> (r.spec.petals * r.spec.samplesPerPetal), R = static_cast<unsigned> (r.spec.rings);
		const unsigned q = static_cast<unsigned> (r.spec.stemSection), z = static_cast<unsigned> (r.spec.stemRings);
		EXPECT_EQ (r.piece.rosetteTriangles, 4u * n * R) << LevelName (level);
		EXPECT_EQ (r.piece.rosetteVertices, 2u * (1u + n * R)) << LevelName (level);
		EXPECT_EQ (r.piece.triangles.size () - r.piece.rosetteTriangles, 2u * q * (z - 1u) + 2u * (q - 2u)) << LevelName (level);
		EXPECT_EQ (r.piece.positions.size () - r.piece.rosetteVertices, q * z) << LevelName (level);
		EXPECT_EQ (r.piece.triangles.size (), faces) << LevelName (level);
		EXPECT_EQ (r.piece.positions.size (), vertices) << LevelName (level);
		EXPECT_EQ (r.piece.triangles.size () - r.piece.rosetteTriangles, stemFaces) << LevelName (level);
		EXPECT_EQ (r.piece.positions.size () - r.piece.rosetteVertices, stemVertices) << LevelName (level);
		for (std::size_t t = 0; t < r.piece.triangles.size (); ++t)
			for (const unsigned v : r.piece.triangles[t])
				ASSERT_EQ (t < r.piece.rosetteTriangles, v < r.piece.rosetteVertices) << t;
	}
}

// K51 : deux coques fermees (rosace, puis tige), orientees vers l'exterieur, sans auto-intersection ; chacune valide
// seule.
TEST (TEST_cgarchi_classical_acanthus_fleuron, the_rosette_is_a_closed_outward_shell)
{
	for (const OrnamentDetail level : {OrnamentDetail::Light, OrnamentDetail::Heavy})
	{
		SCOPED_TRACE (LevelName (level));
		const Rosette r = MakeRosette (level);
		MeshValidationOptions o;
		o.expectedComponents = 2u;
		const MeshValidationReport v = ValidateMesh (*MakeTriangleMesh (r.piece.positions, r.piece.triangles), o);
		EXPECT_TRUE (v.IsValid ());
		EXPECT_EQ (v.components, 2u);
		EXPECT_GT (v.signedVolume, 0.0);
		const unsigned rv = r.piece.rosetteVertices, rt = r.piece.rosetteTriangles;
		const std::vector<Point> rosettePos (r.piece.positions.begin (), r.piece.positions.begin () + rv), stemPos (r.piece.positions.begin () + rv, r.piece.positions.end ());
		const std::vector<std::array<unsigned, 3>> rosetteTri (r.piece.triangles.begin (), r.piece.triangles.begin () + rt);
		std::vector<std::array<unsigned, 3>> stemTri;
		for (std::size_t t = rt; t < r.piece.triangles.size (); ++t)
			stemTri.push_back ({r.piece.triangles[t][0] - rv, r.piece.triangles[t][1] - rv, r.piece.triangles[t][2] - rv});
		o.expectedComponents = 1u;
		const std::vector<std::array<unsigned, 3>> &stemTriangles = stemTri;
		for (const auto &[pos, tri, name] : {std::make_tuple (&rosettePos, &rosetteTri, "rosace"), std::make_tuple (&stemPos, &stemTriangles, "tige")})
		{
			const MeshValidationReport part = ValidateMesh (*MakeTriangleMesh (*pos, *tri), o);
			EXPECT_TRUE (part.IsValid ()) << name;
			EXPECT_GT (part.signedVolume, 0.0) << name;
		}
	}
}

// K48 : chaque sommet a son image par y -> -y, et chaque triangle l'image de ses sommets (appariement par position,
// pas par indice) ; controle negatif : la rosace decalee de 0,1 p en y n'est pas son image.
TEST (TEST_cgarchi_classical_acanthus_fleuron, the_rosette_is_its_own_mirror)
{
	for (const OrnamentDetail level : {OrnamentDetail::Light, OrnamentDetail::Heavy})
	{
		const Rosette r = MakeRosette (level);
		const auto image = [&] (double shift) {
			std::vector<int> m (r.parts.size (), -1);
			for (std::size_t v = 0; v < r.parts.size (); ++v)
				for (std::size_t w = 0; w < r.parts.size (); ++w)
				{
					const Point &p = r.parts[v], &q = r.parts[w];
					if (std::fabs (p[0] - q[0]) <= 1e-9 && std::fabs (p[1] + shift + q[1] + shift) <= 1e-9 && std::fabs (p[2] - q[2]) <= 1e-9)
					{
						m[v] = static_cast<int> (w);
						break;
					}
				}
			return m;
		};
		const std::vector<int> m = image (0.0);
		EXPECT_EQ (std::count (m.begin (), m.end (), -1), 0) << LevelName (level);
		const std::vector<int> shifted = image (0.1);
		EXPECT_GT (std::count (shifted.begin (), shifted.end (), -1), 0) << LevelName (level);
		if (std::count (m.begin (), m.end (), -1) != 0)
			continue;
		std::vector<std::array<unsigned, 3>> sorted;
		for (auto t : r.piece.triangles)
		{
			std::sort (t.begin (), t.end ());
			sorted.push_back (t);
		}
		std::sort (sorted.begin (), sorted.end ());
		int orphans = 0;
		for (const auto &t : r.piece.triangles)
		{
			std::array<unsigned, 3> im {static_cast<unsigned> (m[t[0]]), static_cast<unsigned> (m[t[1]]), static_cast<unsigned> (m[t[2]])};
			std::sort (im.begin (), im.end ());
			orphans += std::binary_search (sorted.begin (), sorted.end (), im) ? 0 : 1;
		}
		EXPECT_EQ (orphans, 0) << LevelName (level);
	}
}

// K46 : centre de la couronne sur l'axe de face, a haut du chapiteau - c - tipRadius ; pointe haute sur l'axe, a
// haut - c.
TEST (TEST_cgarchi_classical_acanthus_fleuron, the_rosette_sits_on_the_face_axis)
{
	const Column &c = DefaultColumn ();
	for (const OrnamentDetail level : {OrnamentDetail::Light, OrnamentDetail::Heavy})
	{
		const Rosette r = MakeRosette (level);
		EXPECT_NEAR (r.parts[0][1], 0.0, 1e-9);
		EXPECT_NEAR (r.parts[0][2], c.tour.top - kC - r.spec.tipRadius, kTau);
		const auto top = std::max_element (r.parts.begin (), r.parts.end (), [] (const Point &a, const Point &b) { return a[2] < b[2]; });
		EXPECT_NEAR ((*top)[1], 0.0, 1e-9);
		EXPECT_NEAR ((*top)[2], c.tour.top - kC, kTau);
	}
}

// K52 : cotes du releve (A5 S0), relatives au haut du chapiteau (abaque) et au haut de la corbeille (bas de B),
// tolerance du calage des planches (+-0,4 p) : centre 3,9 p sous le haut ; bas 1,65 p sous le haut de la corbeille ;
// Ø vertical 7,9, horizontal dans [7,5 ; 9,0] ; Ø du bouton 2,5 ; face avant (bord du bouton) r 21,69.
TEST (TEST_cgarchi_classical_acanthus_fleuron, the_rosette_meets_the_plate_dimensions)
{
	const Column &c = DefaultColumn ();
	for (const OrnamentDetail level : {OrnamentDetail::Light, OrnamentDetail::Heavy})
	{
		SCOPED_TRACE (LevelName (level));
		const Rosette r = MakeRosette (level);
		const std::size_t grid = Grid (r.spec);
		ASSERT_GE (r.parts.size (), 2 * grid) << LevelName (level);
		double hLo = 1e9, hHi = -1e9, yLo = 1e9, yHi = -1e9;
		for (std::size_t v = 0; v < grid; ++v)
		{
			const Point &p = r.parts[v];
			hLo = std::min (hLo, p[2]);
			hHi = std::max (hHi, p[2]);
			yLo = std::min (yLo, p[1]);
			yHi = std::max (yHi, p[1]);
		}
		// bord du bouton : dernier anneau de relief positif ou nul au-dessus de la face avant
		const int n = r.spec.petals * r.spec.samplesPerPetal;
		int buttonRings = 0;
		while (r.parts[static_cast<std::size_t> (1 + buttonRings * n)][0] >= r.spec.front - kTau && buttonRings < r.spec.rings)
			++buttonRings;
		ASSERT_GE (buttonRings, 1);
		const Point &edge = r.parts[static_cast<std::size_t> (1 + (buttonRings - 1) * n)];
		const double button = 2.0 * std::hypot (edge[1], edge[2] - r.parts[0][2]);
		EXPECT_NEAR (c.tour.top - r.parts[0][2], 3.9, 0.4);
		EXPECT_NEAR (c.env.BandTop () - hLo, 1.65, 0.4);
		EXPECT_NEAR (hHi - hLo, 7.9, 0.4);
		EXPECT_GE (yHi - yLo, 7.5);
		EXPECT_LE (yHi - yLo, 9.0);
		EXPECT_NEAR (button, 2.5, 0.4);
		EXPECT_NEAR (edge[0], 21.69, 0.4);
		std::printf ("%s : K52 centre %.3f sous le haut ; bas %.3f sous la corbeille ; Ø %.3f x %.3f ; bord du bouton r %.3f\n",
		             LevelName (level), c.tour.top - r.parts[0][2], c.env.BandTop () - hLo, hHi - hLo, yHi - yLo, edge[0]);
	}
}

// K53 : un maximum du contour par petale ; encoches profondes de plus de 1 p ; un echantillon a l'encoche entre
// chaque paire de petales voisins (jour descendu jusqu'a notchRadius) ; controle negatif : un disque n'a ni maximum
// ni encoche.
TEST (TEST_cgarchi_classical_acanthus_fleuron, the_contour_has_one_maximum_per_petal)
{
	EXPECT_EQ (CircularMaxima (std::vector<double> (64, 3.95)), 0);
	for (const OrnamentDetail level : {OrnamentDetail::Light, OrnamentDetail::Heavy})
	{
		const Rosette r = MakeRosette (level);
		const int n = r.spec.petals * r.spec.samplesPerPetal;
		std::vector<double> contour;
		for (int i = 0; i < n; ++i)
		{
			const Point &p = r.parts[static_cast<std::size_t> (1 + (r.spec.rings - 1) * n + i)];
			contour.push_back (std::hypot (p[1], p[2] - r.parts[0][2]));
		}
		EXPECT_EQ (CircularMaxima (contour), 16) << LevelName (level);
		EXPECT_GE (*std::max_element (contour.begin (), contour.end ()) - *std::min_element (contour.begin (), contour.end ()), 1.0);
		const auto notches = [&] (const std::vector<double> &v) {
			return std::count_if (v.begin (), v.end (), [&] (double rho) { return std::fabs (rho - r.spec.notchRadius) <= kTau; });
		};
		EXPECT_EQ (notches (contour), 16) << LevelName (level);
		EXPECT_EQ (notches (std::vector<double> (64, 3.95)), 0);
	}
}

// Profil : les petales sont un tambour de petalThickness (gravure, profil de la pl. XXV : 2,17 a 2,51) et non des
// flancs qui suivent le tour ; aucun petale plus profond, et le tambour atteint devant B et A (h >= 36,5).
TEST (TEST_cgarchi_classical_acanthus_fleuron, the_petals_are_a_thin_drum)
{
	for (const OrnamentDetail level : {OrnamentDetail::Light, OrnamentDetail::Heavy})
	{
		const Rosette r = MakeRosette (level);
		const std::size_t grid = Grid (r.spec);
		ASSERT_GE (r.parts.size (), 2 * grid) << LevelName (level);
		const std::size_t petals = static_cast<std::size_t> (1 + r.spec.petals * r.spec.samplesPerPetal);
		double deepest = 0.0;
		int drum = 0;
		for (std::size_t v = petals; v < grid; ++v)
		{
			const double depth = r.spec.front - r.parts[v + grid][0];
			deepest = std::max (deepest, depth);
			if (r.parts[v][2] >= 36.5 && std::fabs (depth - r.spec.petalThickness) <= kTau)
				++drum;
		}
		EXPECT_LE (deepest, r.spec.petalThickness + kTau) << LevelName (level);
		EXPECT_GE (deepest, 2.17) << LevelName (level);
		EXPECT_LE (deepest, 2.51) << LevelName (level);
		EXPECT_GT (drum, 0) << LevelName (level);
		std::printf ("%s : tambour des petales %.4f p sous la face (%d sommets du dos sur le tambour)\n", LevelName (level), deepest, drum);
	}
}

// Le noyau (centre et anneau 1 du dos) s'appuie sur le tour : son plus proche sommet est a c (+ 0,01) du maillage du
// tour, et aucun n'en est a plus de 0,49 p (marche du listel de A sur B, 0,39, + c + 0,05 : la boite du centre
// couvre tout l'anneau du noyau, jusqu'a h 39).
TEST (TEST_cgarchi_classical_acanthus_fleuron, the_core_rests_on_the_tour)
{
	const Column &c = DefaultColumn ();
	const std::vector<std::array<Point, 3>> tris = LatheTriangles (c, 15.0, 35.0, 41.0);
	for (const OrnamentDetail level : {OrnamentDetail::Light, OrnamentDetail::Heavy})
	{
		const Rosette r = MakeRosette (level);
		const std::size_t grid = Grid (r.spec);
		ASSERT_GE (r.parts.size (), 2 * grid) << LevelName (level);
		const std::size_t core = static_cast<std::size_t> (1 + r.spec.petals * r.spec.samplesPerPetal);
		double farthest = 0.0, nearest = 1e9;
		for (std::size_t v = grid; v < grid + core; ++v)
		{
			double d = 1e9;
			for (const auto &t : tris)
				d = std::min (d, acanthus_test::PointTriangleDistance (r.parts[v], t[0], t[1], t[2]));
			farthest = std::max (farthest, d);
			nearest = std::min (nearest, d);
		}
		EXPECT_LE (nearest, kC + 0.01) << LevelName (level);
		EXPECT_LE (farthest, 0.49) << LevelName (level);
		std::printf ("%s : noyau, ecart au tour de %.4f a %.4f p\n", LevelName (level), nearest, farthest);
	}
}

// Vue de face, le noyau reste derriere le bouton : devant B et A (h >= 36,5), tout sommet du dos plus profond que le
// tambour est a rho <= buttonRadius.
TEST (TEST_cgarchi_classical_acanthus_fleuron, the_core_hides_behind_the_button)
{
	for (const OrnamentDetail level : {OrnamentDetail::Light, OrnamentDetail::Heavy})
	{
		const Rosette r = MakeRosette (level);
		const std::size_t grid = Grid (r.spec);
		ASSERT_GE (r.parts.size (), 2 * grid) << LevelName (level);
		int deep = 0;
		double widest = 0.0;
		for (std::size_t v = 0; v < grid; ++v)
		{
			const Point &b = r.parts[v + grid];
			if (b[2] >= 36.5 && b[0] < r.spec.front - r.spec.petalThickness - kTau)
			{
				++deep;
				widest = std::max (widest, std::hypot (b[1], b[2] - r.parts[0][2]));
			}
		}
		EXPECT_GT (deep, 0) << LevelName (level);
		EXPECT_LE (widest, r.spec.buttonRadius + kTau) << LevelName (level);
		std::printf ("%s : noyau, %d sommets, rayon de face max %.4f p (bouton %.2f)\n", LevelName (level), deep, widest, r.spec.buttonRadius);
	}
}

// K50 : dans le carre des cornes de l'abaque ; debord sur la face concave de B dans [4,06 - 1 ; 4,06 + 0,5] (gravure,
// profil de la pl. XXV) ; sous le haut du chapiteau - c.
TEST (TEST_cgarchi_classical_acanthus_fleuron, the_rosette_stays_within_the_engraved_bounds)
{
	const Column &c = DefaultColumn ();
	for (const OrnamentDetail level : {OrnamentDetail::Light, OrnamentDetail::Heavy})
	{
		SCOPED_TRACE (LevelName (level));
		const Rosette r = MakeRosette (level);
		double x = -1e9, excess = -1e9, h = -1e9;
		for (const Point &p : r.parts)
		{
			x = std::max (x, p[0]);
			excess = std::max (excess, std::hypot (p[0], p[1]) - c.env.abacus.RadiusAt (std::atan2 (p[1], p[0])));
			h = std::max (h, p[2]);
		}
		EXPECT_LE (x, c.env.abacus.radius / std::sqrt (2.0) - kC);
		EXPECT_LE (excess, 4.56);
		EXPECT_GE (excess, 3.06);
		EXPECT_LE (h, c.tour.top - kC + kTau);
		std::printf ("%s : K50 x max %.4f (carre %.4f) ; debord sur B %.4f ; h max %.4f\n", LevelName (level), x,
		             c.env.abacus.radius / std::sqrt (2.0) - kC, excess, h);
	}
}

// Epaisseur avant - dos >= minThickness en chaque sommet.
TEST (TEST_cgarchi_classical_acanthus_fleuron, the_rosette_is_thick_enough)
{
	for (const OrnamentDetail level : {OrnamentDetail::Light, OrnamentDetail::Heavy})
	{
		const Rosette r = MakeRosette (level);
		const std::size_t grid = Grid (r.spec);
		ASSERT_GE (r.parts.size (), 2 * grid) << LevelName (level);
		double thin = 1e9, at = 0.0;
		for (std::size_t v = 0; v < grid; ++v)
			if (r.parts[v][0] - r.parts[v + grid][0] < thin)
			{
				thin = r.parts[v][0] - r.parts[v + grid][0];
				at = r.parts[v][2];
			}
		EXPECT_GE (thin, 0.3 - kTau) << LevelName (level);   // minThickness du registre
		std::printf ("%s : epaisseur minimale %.4f p a h %.3f\n", LevelName (level), thin, at);
	}
}

// K49 (tour) : jour >= c entre la rosace et le maillage du tour, sommets contre triangles dans les deux sens, a
// chaque reglage (8, 48, 256 secteurs ; cannelures), le dos lu au meme reglage.
TEST (TEST_cgarchi_classical_acanthus_fleuron, the_rosette_keeps_c_from_the_tour_mesh)
{
	for (const OrnamentDetail level : {OrnamentDetail::Light, OrnamentDetail::Heavy})
		for (const Setting &s : Settings ())
		{
			const Column c = MakeColumn (s);
			const Rosette r = MakeRosette (level, c);
			std::vector<Point> pos;
			std::vector<std::array<unsigned, 3>> tri;
			for (unsigned v = 0; v < static_cast<unsigned> (c.lathe->GetNVertices ()); ++v)
			{
				float q[3] {};
				c.lathe->GetVertex (v, q);
				pos.push_back ({q[0], q[1], q[2]});
			}
			for (unsigned f = 0; f < static_cast<unsigned> (c.lathe->GetNFaces ()); ++f)
				tri.push_back ({static_cast<unsigned> (c.lathe->GetFaceVertex (f, 0)), static_cast<unsigned> (c.lathe->GetFaceVertex (f, 1)),
				                static_cast<unsigned> (c.lathe->GetFaceVertex (f, 2))});
			CapitalPiece tour, rosette;
			tour.faceEnd = static_cast<unsigned> (tri.size ());
			tour.vertexEnd = static_cast<unsigned> (pos.size ());
			rosette.kind = CapitalPieceKind::Fleuron;
			rosette.vertexBegin = tour.vertexEnd;
			rosette.faceBegin = tour.faceEnd;
			for (const Point &p : r.piece.positions)
				pos.push_back (p);
			for (const auto &t : r.piece.triangles)
				tri.push_back ({t[0] + rosette.vertexBegin, t[1] + rosette.vertexBegin, t[2] + rosette.vertexBegin});
			rosette.vertexEnd = static_cast<unsigned> (pos.size ());
			rosette.faceEnd = static_cast<unsigned> (tri.size ());
			std::array<double, 3> where {};
			const double gap = acanthus_test::PieceGap (*MakeTriangleMesh (pos, tri), c.env, rosette, tour, 1.0, &where);
			EXPECT_GE (gap, kC - kFloat) << LevelName (level) << ", " << s.name;
			std::printf ("%s, %s : jour au tour %.5f p (sommet %.3f, %.3f, h %.3f)\n", LevelName (level), s.name, gap, where[0], where[1], where[2]);
		}
}

namespace
{

// Plus grande distance, sur les anneaux de h dans [hLo, hHi], d'un point du tour a l'image par y -> -y du point de
// l'angle oppose (angles reguliers : l'oppose de l'indice k est (n - k) mod n).
double TourPointMirrorGap (const detail::CapitalTour &tour, double hLo, double hHi)
{
	const std::size_t na = tour.angles.size ();
	double gap = 0.0;
	for (std::size_t j = 0; j < tour.h.size (); ++j)
	{
		if (tour.h[j] < hLo || tour.h[j] > hHi)
			continue;
		for (std::size_t k = 0; k < na; ++k)
		{
			const std::size_t o = (na - k) % na;
			EXPECT_NEAR (std::remainder (tour.angles[k] + tour.angles[o], 2.0 * kPi), 0.0, 1e-12);
			const std::array<double, 2> &p = tour.points[j][k], &q = tour.points[j][o];
			gap = std::max (gap, std::hypot (p[0] - q[0], p[1] + q[1]));
		}
	}
	return gap;
}

} // namespace

// K63 : le dos d'une paire de sommets symetriques de la rosace est le plus grand de leurs deux dos lus, chacun sur la
// boite du sommet. Tour rendu asymetrique : rayon + 0,1 p des points d'un seul cote (y < 0, puis y > 0) des anneaux de
// h 33 a 43 (controles : le tour est alors a 0,1 p au moins de son miroir, 0 avant ; le dos construit sur ce tour
// s'ecarte de plus de 1e-6 p de celui du tour de la colonne). Sur ces tours, jour >= c entre la rosace et le maillage du
// tour (sommets contre triangles), et le dos reste son propre miroir : un sommet du dos et son symetrique ont le meme x
// et des y opposes.
TEST (TEST_cgarchi_classical_acanthus_fleuron, the_rosette_back_holds_on_an_asymmetric_tour)
{
	const Column &c = DefaultColumn ();
	EXPECT_LE (TourPointMirrorGap (c.tour, 33.0, 43.0), 1e-9);
	for (const double side : {-1.0, 1.0})
	{
		detail::CapitalTour tour = c.tour;
		for (std::size_t j = 0; j < tour.h.size (); ++j)
			if (tour.h[j] >= 33.0 && tour.h[j] <= 43.0)
				for (std::array<double, 2> &q : tour.points[j])
					if (side * q[1] > 1e-9)
					{
						const double r = std::hypot (q[0], q[1]);
						q = {q[0] * (r + 0.1) / r, q[1] * (r + 0.1) / r};
					}
		EXPECT_GE (TourPointMirrorGap (tour, 33.0, 43.0), 0.1 - 1e-9) << side;
		std::vector<Point> pos;
		std::vector<std::array<unsigned, 3>> tri;
		const unsigned na = static_cast<unsigned> (tour.angles.size ());
		for (std::size_t j = 0; j < tour.h.size (); ++j)
			for (const std::array<double, 2> &q : tour.points[j])
				pos.push_back ({q[0] * c.env.scale, q[1] * c.env.scale, (tour.h[j] + c.env.foot) * c.env.scale});
		for (unsigned j = 0; j + 1 < static_cast<unsigned> (tour.h.size ()); ++j)
			for (unsigned k = 0; k < na; ++k)
			{
				const unsigned a = j * na + k, b = j * na + (k + 1) % na, d = (j + 1) * na + (k + 1) % na, e = (j + 1) * na + k;
				tri.push_back ({a, b, d});
				tri.push_back ({a, d, e});
			}
		CapitalPiece tourPiece, rosette;
		tourPiece.faceEnd = static_cast<unsigned> (tri.size ());
		tourPiece.vertexEnd = static_cast<unsigned> (pos.size ());
		for (const OrnamentDetail level : {OrnamentDetail::Light, OrnamentDetail::Heavy})
		{
			const detail::FleuronSpec spec = detail::DefaultFleuronSpec (level);
			const detail::FleuronPiece piece = detail::BuildFleuron (c.env, tour, spec);
			ASSERT_LE (piece.rosetteTriangles, piece.triangles.size ()) << LevelName (level);
			ASSERT_GE (piece.positions.size (), std::max<std::size_t> (piece.rosetteVertices, 2 * Grid (spec))) << LevelName (level);
			std::vector<Point> all = pos;
			std::vector<std::array<unsigned, 3>> allTri = tri;
			rosette.kind = CapitalPieceKind::Fleuron;
			rosette.vertexBegin = tourPiece.vertexEnd;
			rosette.faceBegin = tourPiece.faceEnd;
			for (const Point &p : piece.positions)
				all.push_back (p);
			for (unsigned f = 0; f < piece.rosetteTriangles; ++f)
			{
				const std::array<unsigned, 3> &t = piece.triangles[f];
				allTri.push_back ({t[0] + rosette.vertexBegin, t[1] + rosette.vertexBegin, t[2] + rosette.vertexBegin});
			}
			rosette.vertexEnd = rosette.vertexBegin + piece.rosetteVertices;
			rosette.faceEnd = static_cast<unsigned> (allTri.size ());
			std::array<double, 3> where {};
			const double toTour = acanthus_test::PieceGap (*MakeTriangleMesh (all, allTri), c.env, rosette, tourPiece, 1.0, &where);
			EXPECT_GE (toTour, kC - kFloat) << LevelName (level) << ", cote " << side;

			const std::size_t n = static_cast<std::size_t> (spec.petals * spec.samplesPerPetal), grid = Grid (spec);
			double worst = 0.0;
			for (std::size_t j = 1; j <= static_cast<std::size_t> (spec.rings); ++j)
				for (std::size_t i = 0; i < n; ++i)
				{
					const Point &v = piece.positions[grid + 1 + (j - 1) * n + i], &w = piece.positions[grid + 1 + (j - 1) * n + (n - i) % n];
					worst = std::max ({worst, std::fabs (v[0] - w[0]), std::fabs (v[1] + w[1])});
				}
			EXPECT_LE (worst, 1e-12) << LevelName (level) << ", cote " << side;

			const detail::FleuronPiece plain = detail::BuildFleuron (c.env, c.tour, spec);
			ASSERT_EQ (plain.positions.size (), piece.positions.size ()) << LevelName (level);
			double moved = 0.0;
			for (std::size_t v = grid; v < 2 * grid; ++v)
				moved = std::max (moved, std::hypot (piece.positions[v][0] - plain.positions[v][0], piece.positions[v][1] - plain.positions[v][1],
				                                     piece.positions[v][2] - plain.positions[v][2]));
			EXPECT_GT (moved, 1e-6) << LevelName (level) << ", cote " << side;
			std::printf ("%s, tour bombe de 0,1 p du cote y %s 0 : jour au tour %.5f p (sommet %.3f, %.3f, h %.3f) ; dos miroir a %.1e p ; "
			             "dos deplace de %.4f p au plus\n",
			             LevelName (level), side < 0.0 ? "<" : ">", toTour, where[0], where[1], where[2], worst, moved);
		}
	}
}

// K58 (echelle) : a l'echelle 2, les sommets sont le double de ceux de l'echelle 1.
TEST (TEST_cgarchi_classical_acanthus_fleuron, the_rosette_follows_the_scale)
{
	const Column one = MakeColumn (Settings ()[1], 1.0), two = MakeColumn (Settings ()[1], 2.0);
	const Rosette a = MakeRosette (OrnamentDetail::Light, one), b = MakeRosette (OrnamentDetail::Light, two);
	ASSERT_EQ (a.piece.positions.size (), b.piece.positions.size ());
	double worst = 0.0;
	for (std::size_t v = 0; v < a.piece.positions.size (); ++v)
		for (int k = 0; k < 3; ++k)
			worst = std::max (worst, std::fabs (2.0 * a.piece.positions[v][static_cast<std::size_t> (k)] - b.piece.positions[v][static_cast<std::size_t> (k)]));
	EXPECT_LE (worst, 1e-9);
}

// K55 : chaque garde nomme sa cause.
TEST (TEST_cgarchi_classical_acanthus_fleuron, the_guards_name_their_cause)
{
	const Column &c = DefaultColumn ();
	const auto message = [&] (const detail::FleuronSpec &s, const detail::CapitalTour &tour) {
		try
		{
			detail::BuildFleuron (c.env, tour, s);
		}
		catch (const std::invalid_argument &e)
		{
			return std::string (e.what ());
		}
		return std::string ();
	};
	const auto expect = [&] (const detail::FleuronSpec &s, const char *fragment, const detail::CapitalTour *tour = nullptr) {
		const std::string m = message (s, tour ? *tour : c.tour);
		EXPECT_NE (m.find ("BuildFleuron : "), std::string::npos) << m;
		EXPECT_NE (m.find (fragment), std::string::npos) << "attendu « " << fragment << " », recu « " << m << " »";
	};
	const detail::FleuronSpec base = detail::DefaultFleuronSpec (OrnamentDetail::Light);
	EXPECT_EQ (message (base, c.tour), "");
	detail::FleuronSpec s = base;
	s.petals = 15;
	expect (s, "petals pair");
	s = base;
	s.petals = 2;
	expect (s, "petals pair");
	s = base;
	s.samplesPerPetal = 3;
	expect (s, "samplesPerPetal");
	s = base;
	s.rings = 2;
	expect (s, "rings >= 3");
	s = base;
	s.front = std::nan ("");
	expect (s, "forme hors domaine");
	s = base;
	s.notchRadius = s.buttonRadius;
	expect (s, "encoche sous le bouton");
	s = base;
	s.coreRadius = s.buttonRadius;
	expect (s, "noyau hors du bouton");
	s = base;
	s.petalGap = 1.6;
	expect (s, "plus large que leur pas");
	s = base;
	s.notchRadius = 3.5;
	expect (s, "au-dela du flanc");
	s = base;
	s.front = 20.0;
	expect (s, "epaisseur");
	s = base;
	s.front = 23.0;
	expect (s, "carre des cornes");
	s = base;
	s.petalGap = -0.1;
	expect (s, "jour entre petales, reliefs et jeu finis et >= 0");
	s = base;
	s.clearance = -0.05;
	expect (s, "jour entre petales, reliefs et jeu finis et >= 0");
	s = base;
	s.stemSection = 5;
	expect (s, "stemSection pair et >= 4");
	s = base;
	s.stemSection = 2;
	expect (s, "stemSection pair et >= 4");
	s = base;
	s.stemRings = 1;
	expect (s, "stemRings >= 2");
	s = base;
	s.cornetBottom = 23.0;
	expect (s, "stemFoot < cornetBottom < cornetTop");
	s = base;
	s.cornetTop = 30.0;
	expect (s, "stemFoot < cornetBottom < cornetTop");
	s = base;
	s.cornetTop = 34.5;
	expect (s, "tige au contact de la rosace");
	detail::CapitalTour low = c.tour;
	const std::size_t keep = static_cast<std::size_t> (std::find_if (low.h.begin (), low.h.end (), [] (double h) { return h >= 30.0; }) - low.h.begin ());
	low.h.resize (keep);
	low.points.resize (keep);
	expect (base, "tour absent derriere la rosace", &low);
	// tour tronque sous h 24 (premier anneau a h 28), complet au-dessus : la rosace se construit, la tige non
	detail::CapitalTour high = c.tour;
	const std::size_t drop = static_cast<std::size_t> (std::find_if (high.h.begin (), high.h.end (), [] (double h) { return h > 24.0; }) - high.h.begin ());
	ASSERT_LT (drop, high.h.size ());
	ASSERT_GT (high.h[drop], 24.0 + base.clearance);
	high.h.erase (high.h.begin (), high.h.begin () + static_cast<std::ptrdiff_t> (drop));
	high.points.erase (high.points.begin (), high.points.begin () + static_cast<std::ptrdiff_t> (drop));
	expect (base, "tour absent derriere la tige", &high);
}

// Enveloppe d'ornement (ColumnOptions::ornamentEnvelope) : les bandes F, E, D, C du tour sont l'enveloppe, volume
// schematique ; le tour du fleuron les lit au noyau, et la rosace est celle du profil a noyau, a l'egalite.
TEST (TEST_cgarchi_classical_acanthus_fleuron, the_envelope_mode_keeps_the_core_rosette)
{
	const OrderData &order = ClassicalOrders ().Get (Order::Corinthian);
	ColumnOptions options;
	options.order = Order::Corinthian;
	options.ornamentEnvelope = true;
	const ColumnProfile profile = BuildColumnProfile (order, options);
	const detail::CapitalEnvelope env = detail::ReadCapitalEnvelope (order, profile);
	const detail::CapitalTour tour = detail::ReadCapitalTour (profile, LatheAngles (profile, LatheOptions {}), env);
	for (const OrnamentDetail level : {OrnamentDetail::Light, OrnamentDetail::Heavy})
	{
		const detail::FleuronPiece enveloped = detail::BuildFleuron (env, tour, detail::DefaultFleuronSpec (level));
		const Rosette core = MakeRosette (level);
		ASSERT_EQ (enveloped.positions.size (), core.piece.positions.size ());
		double worst = 0.0;
		for (std::size_t v = 0; v < core.piece.positions.size (); ++v)
			for (std::size_t k = 0; k < 3; ++k)
				worst = std::max (worst, std::fabs (enveloped.positions[v][k] - core.piece.positions[v][k]));
		EXPECT_LE (worst, 1e-12) << LevelName (level);
		EXPECT_EQ (enveloped.triangles, core.piece.triangles) << LevelName (level);
	}
}

// Tige sur un tour non convexe en h : une bosse de 0,3 p (h 24,45 a 24,55, sommet a 24,5) entre deux anneaux de la tige
// (Light 24,0 et 25,0 ; Heavy 24,39 et 24,78), hors des boites des anneaux (h +- c). Le dos de la tige, droit entre
// deux anneaux, passe a c au moins devant le tour en tout h (64 hauteurs par intervalle, boite de 0,002 p sur la
// largeur de la tige ; plus grand x du tour par decoupage de ses triangles, instrument du test).
TEST (TEST_cgarchi_classical_acanthus_fleuron, the_stem_back_clears_a_tour_bulging_between_its_rings)
{
	const Column &c = DefaultColumn ();
	const detail::CapitalTour &tour = c.tour;
	ASSERT_GE (tour.h.size (), 2u);
	ASSERT_EQ (tour.points.size (), tour.h.size ());
	const auto ringAt = [&] (double h) {
		std::size_t j = 0;
		while (j + 2 < tour.h.size () && tour.h[j + 1] <= h)
			++j;
		const double t = (h - tour.h[j]) / (tour.h[j + 1] - tour.h[j]);
		std::vector<std::array<double, 2>> ring (tour.points[j].size ());
		for (std::size_t k = 0; k < ring.size (); ++k)
			for (std::size_t q = 0; q < 2; ++q)
				ring[k][q] = tour.points[j][k][q] + t * (tour.points[j + 1][k][q] - tour.points[j][k][q]);
		return ring;
	};
	// repere du tour en parties : a h 24, au secteur theta = 0, le tour est le noyau de la corbeille
	ASSERT_NEAR (tour.angles[0], 0.0, 1e-12);
	EXPECT_NEAR (ringAt (24.0)[0][0], c.env.CoreRadius (24.0), 1e-9);
	detail::CapitalTour bulged = tour;
	for (const auto &[h, rise] : {std::make_pair (24.45, 0.0), std::make_pair (24.5, 0.3), std::make_pair (24.55, 0.0)})
	{
		std::vector<std::array<double, 2>> ring = ringAt (h);
		for (std::array<double, 2> &q : ring)
		{
			const double r = std::hypot (q[0], q[1]);
			q = {q[0] * (r + rise) / r, q[1] * (r + rise) / r};
		}
		const std::size_t at = static_cast<std::size_t> (std::upper_bound (bulged.h.begin (), bulged.h.end (), h) - bulged.h.begin ());
		bulged.h.insert (bulged.h.begin () + static_cast<std::ptrdiff_t> (at), h);
		bulged.points.insert (bulged.points.begin () + static_cast<std::ptrdiff_t> (at), ring);
	}
	std::vector<std::vector<Point>> tris;
	for (std::size_t j = 0; j + 1 < bulged.h.size (); ++j)
	{
		if (bulged.h[j + 1] < 23.5 || bulged.h[j] > 33.5)
			continue;
		const std::size_t n = bulged.points[j].size ();
		for (std::size_t k = 0; k < n; ++k)
		{
			const std::size_t k1 = (k + 1) % n;
			const Point a {bulged.points[j][k][0], bulged.points[j][k][1], bulged.h[j]}, b {bulged.points[j][k1][0], bulged.points[j][k1][1], bulged.h[j]};
			const Point d {bulged.points[j + 1][k][0], bulged.points[j + 1][k][1], bulged.h[j + 1]},
			    e {bulged.points[j + 1][k1][0], bulged.points[j + 1][k1][1], bulged.h[j + 1]};
			if (std::max ({a[0], b[0], d[0], e[0]}) <= 0.0 || std::min ({a[1], b[1], d[1], e[1]}) > 2.0 || std::max ({a[1], b[1], d[1], e[1]}) < -2.0)
				continue;
			tris.push_back ({a, b, e});
			tris.push_back ({a, e, d});
		}
	}
	const auto tourX = [&] (double h, double half) {
		double x = -std::numeric_limits<double>::infinity ();
		for (const std::vector<Point> &t : tris)
			x = std::max (x, ClipMaxX (t, h - 1e-3, h + 1e-3, -half, half));
		return x;
	};
	// la bosse existe : 0,3 p devant le tour non bombe a h 24,5
	EXPECT_NEAR (tourX (24.5, 0.1) - ringAt (24.5)[0][0], 0.3, 1e-3);
	for (const OrnamentDetail level : {OrnamentDetail::Light, OrnamentDetail::Heavy})
	{
		const detail::FleuronSpec spec = detail::DefaultFleuronSpec (level);
		const detail::FleuronPiece piece = detail::BuildFleuron (c.env, bulged, spec);
		const std::size_t q = static_cast<std::size_t> (spec.stemSection), nf = q - 2;
		ASSERT_GE (q, 4u);
		std::vector<std::vector<Point>> rings;
		for (std::size_t v = piece.rosetteVertices; v + q <= piece.positions.size (); v += q)
		{
			rings.emplace_back ();
			for (std::size_t k = 0; k < q; ++k)
				rings.back ().push_back (Parts (piece.positions[v + k], c.env));
		}
		ASSERT_GE (rings.size (), 2u);
		bool straddled = false;
		double worst = std::numeric_limits<double>::infinity ();
		for (std::size_t i = 0; i + 1 < rings.size (); ++i)
		{
			const std::vector<Point> &a = rings[i], &b = rings[i + 1];
			const double ha = a[nf + 1][2], hb = b[nf + 1][2];
			straddled = straddled || (ha + kC < 24.45 && hb - kC > 24.55);
			const auto back = [] (const std::vector<Point> &ring, std::size_t f) { return std::min ({ring[0][0], ring[f][0], ring[f + 1][0]}); };
			double half = 0.0;
			for (const Point &pt : a)
				half = std::max (half, std::fabs (pt[1]));
			for (const Point &pt : b)
				half = std::max (half, std::fabs (pt[1]));
			for (int s = 0; s <= 64; ++s)
			{
				const double t = s / 64.0, h = ha + (hb - ha) * t;
				worst = std::min (worst, back (a, nf) + (back (b, nf) - back (a, nf)) * t - tourX (h, half));
			}
		}
		EXPECT_TRUE (straddled) << LevelName (level);
		EXPECT_GE (worst, kC - 1e-9) << LevelName (level);
		std::printf ("%s : tige sur un tour bombe entre deux anneaux : jour minimal du dos %.4f p\n", LevelName (level), worst);
	}
}

namespace
{

// Sommets de la tige en parties, par anneau (stemSection points : demi-ellipse de y > 0 a y < 0, puis milieu du dos).
std::vector<std::vector<Point>> StemRings (const Rosette &r)
{
	std::vector<std::vector<Point>> rings;
	const std::size_t q = static_cast<std::size_t> (r.spec.stemSection);
	for (std::size_t v = r.piece.rosetteVertices; v + q <= r.parts.size (); v += q)
		rings.emplace_back (r.parts.begin () + static_cast<std::ptrdiff_t> (v), r.parts.begin () + static_cast<std::ptrdiff_t> (v + q));
	return rings;
}

} // namespace

// AC-71, Q-G4 (a') et releve S0 (pl. XXVI) : tige de h 24,0 a 33,0, largeur 0,67 jusqu'a la pointe du cornet (30,8),
// evasee jusqu'a 0,94 au haut, croissante ; centree sur l'axe de face ; fleche stemDepth au milieu ; cotes de la
// planche : largeur 0,69 +- 0,1, cornet 0,94 +- 0,1, pointe du cornet 30,78 +- 0,4, haut 33,02 +- 0,4.
TEST (TEST_cgarchi_classical_acanthus_fleuron, the_stem_meets_the_plate_dimensions)
{
	for (const OrnamentDetail level : {OrnamentDetail::Light, OrnamentDetail::Heavy})
	{
		SCOPED_TRACE (LevelName (level));
		const Rosette r = MakeRosette (level);
		const std::vector<std::vector<Point>> rings = StemRings (r);
		ASSERT_EQ (rings.size (), static_cast<std::size_t> (r.spec.stemRings));
		const std::size_t nf = static_cast<std::size_t> (r.spec.stemSection - 2);
		double previous = 0.0, shaft = 0.0, top = 0.0;
		for (const std::vector<Point> &ring : rings)
		{
			const double h = ring[0][2];
			double yLo = 1e9, yHi = -1e9;
			for (const Point &p : ring)
			{
				EXPECT_NEAR (p[2], h, kTau);
				yLo = std::min (yLo, p[1]);
				yHi = std::max (yHi, p[1]);
			}
			const double w = yHi - yLo;
			EXPECT_NEAR (yHi + yLo, 0.0, 1e-9);
			EXPECT_GE (w, previous - kTau);
			EXPECT_NEAR (ring[nf / 2][0] - ring[nf + 1][0], r.spec.stemDepth, kTau);
			if (h <= 30.8)
			{
				EXPECT_NEAR (w, 0.67, kTau) << h;
				shaft = w;
			}
			previous = w;
			top = w;
		}
		EXPECT_NEAR (rings.front ()[0][2], 24.0, kTau);
		EXPECT_NEAR (rings.back ()[0][2], 33.0, kTau);
		EXPECT_NEAR (top, 0.94, kTau);
		EXPECT_NEAR (shaft, 0.69, 0.1);
		EXPECT_NEAR (top, 0.94, 0.1);
		EXPECT_NEAR (r.spec.cornetBottom, 30.78, 0.4);
		EXPECT_NEAR (rings.back ()[0][2], 33.02, 0.4);
	}
}

// La tige repose sur la corbeille : dos de chaque anneau a c (+ 0,2 : pente du cone 0,5625 sur un huitieme d'intervalle
// entre anneaux, 1,0 p en Light, et sur la dilatation c) du maillage du tour, jamais plus pres que c.
TEST (TEST_cgarchi_classical_acanthus_fleuron, the_stem_lies_on_the_bell)
{
	const Column &c = DefaultColumn ();
	const std::vector<std::array<Point, 3>> tris = LatheTriangles (c, 10.0, 23.0, 34.0);
	for (const OrnamentDetail level : {OrnamentDetail::Light, OrnamentDetail::Heavy})
	{
		const Rosette r = MakeRosette (level);
		const std::size_t nf = static_cast<std::size_t> (r.spec.stemSection - 2);
		double nearest = 1e9, farthest = 0.0;
		for (const std::vector<Point> &ring : StemRings (r))
			for (const std::size_t k : {std::size_t {0}, nf, nf + 1})
			{
				double d = 1e9;
				for (const auto &t : tris)
					d = std::min (d, acanthus_test::PointTriangleDistance (ring[k], t[0], t[1], t[2]));
				nearest = std::min (nearest, d);
				farthest = std::max (farthest, d);
			}
		EXPECT_GE (nearest, kC - kFloat) << LevelName (level);
		EXPECT_LE (farthest, kC + 0.2) << LevelName (level);
		std::printf ("%s : dos de la tige a %.4f a %.4f p du tour\n", LevelName (level), nearest, farthest);
	}
}

// Rosace contre tige dans la piece G : jour >= c (PieceGap, deux sens).
TEST (TEST_cgarchi_classical_acanthus_fleuron, the_stem_keeps_c_from_its_rosette)
{
	const Column &col = DefaultColumn ();
	for (const OrnamentDetail level : {OrnamentDetail::Light, OrnamentDetail::Heavy})
	{
		const Rosette r = MakeRosette (level);
		CapitalPiece rosette, stem;
		rosette.vertexEnd = r.piece.rosetteVertices;
		rosette.faceEnd = r.piece.rosetteTriangles;
		stem.vertexBegin = r.piece.rosetteVertices;
		stem.vertexEnd = static_cast<unsigned> (r.piece.positions.size ());
		stem.faceBegin = r.piece.rosetteTriangles;
		stem.faceEnd = static_cast<unsigned> (r.piece.triangles.size ());
		std::array<double, 3> at {};
		const double gap = acanthus_test::PieceGap (*MakeTriangleMesh (r.piece.positions, r.piece.triangles), col.env, rosette, stem, 3.0, &at);
		EXPECT_GE (gap, kC - kFloat) << LevelName (level);
		EXPECT_LT (gap, 3.0) << LevelName (level);
		std::printf ("%s : jour rosace / tige %.4f p (sommet %.3f, %.3f, h %.3f)\n", LevelName (level), gap, at[0], at[1], at[2]);
	}
}

TEST (TEST_cgarchi_classical_acanthus_fleuron, the_tour_reader_checks_its_input)
{
	const Column &c = DefaultColumn ();
	const auto fails = [&c] (const ColumnProfile &p, const std::vector<double> &angles, const char *fragment) {
		try
		{
			detail::ReadCapitalTour (p, angles, c.env);
		}
		catch (const std::invalid_argument &e)
		{
			EXPECT_NE (std::string (e.what ()).find (fragment), std::string::npos) << e.what ();
			return;
		}
		ADD_FAILURE () << "aucune exception, attendu « " << fragment << " »";
	};
	fails (c.profile, {}, "aucun angle");
	fails (c.profile, {0.5, 0.25}, "non croissants");
	fails (c.profile, {0.0, 7.0}, "non croissants");
	ColumnProfile noCapital = c.profile;
	noCapital.members.clear ();
	fails (noCapital, {0.0}, "chapiteau absent");
	ColumnProfile single = c.profile;
	single.vertices.resize (1);
	fails (single, {0.0}, "profil de moins de deux sommets");
	EXPECT_NEAR (c.tour.top, 42.0, kTau);
	double foot = std::numeric_limits<double>::infinity ();
	for (const ProfileMember &m : c.profile.members)
		if (m.block == "chapiteau")
			foot = std::min (foot, m.y0);
	EXPECT_EQ (c.env.foot, foot);
}

// ---------------------------------------------------------------------------
//  Fleurons dans l'ornement (A5a)
// ---------------------------------------------------------------------------

namespace
{

constexpr std::size_t kFirstFleuron = 56;

struct OrnamentCase
{
	OrnamentDetail detail;
	double scale;
	detail::CapitalEnvelope env;
	CapitalOrnament ornament;
};

const std::vector<OrnamentCase> &OrnamentCases ()
{
	static const std::vector<OrnamentCase> cases = [] {
		std::vector<OrnamentCase> out;
		const OrderData &order = ClassicalOrders ().Get (Order::Corinthian);
		const ColumnProfile &profile = DefaultColumn ().profile;
		for (const OrnamentDetail d : {OrnamentDetail::Light, OrnamentDetail::Heavy})
			for (const double scale : {1.0, 2.0})
			{
				AcanthusOptions o;
				o.detail = d;
				o.scale = scale;
				out.push_back ({d, scale, detail::ReadCapitalEnvelope (order, profile, scale), BuildCorinthianOrnament (order, profile, o)});
			}
		return out;
	}();
	return cases;
}

Point MeshParts (const Mesh &m, const detail::CapitalEnvelope &env, unsigned v)
{
	float q[3] {};
	m.GetVertex (v, q);
	return Parts ({q[0], q[1], q[2]}, env);
}

} // namespace

// K45, K46 : 60 pieces, les 4 dernieres des G (theta = k pi/2 croissant), contigues aux helices, aux comptes de la
// rosace ; centre avant de chaque G sur l'axe de sa face, a haut du chapiteau - c - tipRadius.
TEST (TEST_cgarchi_classical_acanthus_fleuron, the_ornament_ends_with_four_fleurons)
{
	for (const OrnamentCase &c : OrnamentCases ())
	{
		SCOPED_TRACE (std::string (LevelName (c.detail)) + ", echelle " + std::to_string (c.scale));
		const std::vector<CapitalPiece> &P = c.ornament.pieces;
		ASSERT_EQ (P.size (), kFirstFleuron + detail::kFleurons);
		EXPECT_EQ (P[kFirstFleuron - 1].kind, CapitalPieceKind::Helix);
		EXPECT_EQ (P[kFirstFleuron].vertexBegin, P[kFirstFleuron - 1].vertexEnd);
		EXPECT_EQ (P[kFirstFleuron].faceBegin, P[kFirstFleuron - 1].faceEnd);
		const Rosette r = MakeRosette (c.detail);
		for (std::size_t k = 0; k < static_cast<std::size_t> (detail::kFleurons); ++k)
		{
			const CapitalPiece &p = P[kFirstFleuron + k];
			EXPECT_EQ (p.kind, CapitalPieceKind::Fleuron) << k;
			EXPECT_EQ (p.faceEnd - p.faceBegin, r.piece.triangles.size ()) << k;
			EXPECT_EQ (p.vertexEnd - p.vertexBegin, r.piece.positions.size ()) << k;
			const Point centre = MeshParts (*c.ornament.mesh, c.env, p.vertexBegin);
			EXPECT_NEAR (std::remainder (std::atan2 (centre[1], centre[0]) - 0.5 * kPi * static_cast<double> (k), 2.0 * kPi), 0.0, 1e-6) << k;
			EXPECT_NEAR (centre[2], DefaultColumn ().tour.top - kC - r.spec.tipRadius, 1e-4) << k;
		}
		EXPECT_EQ (P.back ().vertexEnd, static_cast<unsigned> (c.ornament.mesh->GetNVertices ()));
		EXPECT_EQ (P.back ().faceEnd, static_cast<unsigned> (c.ornament.mesh->GetNFaces ()));
	}
}

// K47 : les G 1 a 3 sont la representante tournee de k pi/2, sommet a sommet, a 4 ulp float pres a la cote du tour ;
// memes triangles, indices locaux.
TEST (TEST_cgarchi_classical_acanthus_fleuron, the_fleurons_are_the_representative_turned)
{
	for (const OrnamentCase &c : OrnamentCases ())
	{
		SCOPED_TRACE (std::string (LevelName (c.detail)) + ", echelle " + std::to_string (c.scale));
		ASSERT_EQ (c.ornament.pieces.size (), kFirstFleuron + detail::kFleurons);
		const Mesh &m = *c.ornament.mesh;
		const CapitalPiece &g0 = c.ornament.pieces[kFirstFleuron];
		const double tol = 4.0 * FLT_EPSILON * (c.env.foot + 42.0) * c.env.scale;
		double worst = 0.0;
		for (std::size_t k = 1; k < static_cast<std::size_t> (detail::kFleurons); ++k)
		{
			const CapitalPiece &gk = c.ornament.pieces[kFirstFleuron + k];
			ASSERT_EQ (gk.vertexEnd - gk.vertexBegin, g0.vertexEnd - g0.vertexBegin);
			for (unsigned v = 0; v < g0.vertexEnd - g0.vertexBegin; ++v)
			{
				float a[3] {}, b[3] {};
				m.GetVertex (g0.vertexBegin + v, a);
				m.GetVertex (gk.vertexBegin + v, b);
				const double x = a[0], y = a[1];
				const std::array<double, 2> turned = k == 1 ? std::array<double, 2> {-y, x} : k == 2 ? std::array<double, 2> {-x, -y} : std::array<double, 2> {y, -x};
				worst = std::max ({worst, std::fabs (b[0] - turned[0]), std::fabs (b[1] - turned[1]), static_cast<double> (std::fabs (b[2] - a[2]))});
			}
			for (unsigned f = 0; f < g0.faceEnd - g0.faceBegin; ++f)
				for (unsigned e = 0; e < 3; ++e)
					ASSERT_EQ (m.GetFaceVertex (gk.faceBegin + f, e) - static_cast<int> (gk.vertexBegin), m.GetFaceVertex (g0.faceBegin + f, e) - static_cast<int> (g0.vertexBegin));
		}
		EXPECT_LE (worst, tol);
	}
}

// K48 : symetrie D4 de l'ornement entier (sans indices) : la reflexion y -> -y et la rotation de pi/2 envoient
// l'ensemble des sommets sur lui-meme (plus proche voisin, grille de 1e-3 p) ; controle negatif : la rotation de pi/4
// envoie un sommet des G a plus de 1 p de tout sommet de l'ornement (plus proche voisin exact ; les G ne sont que sur
// les faces).
TEST (TEST_cgarchi_classical_acanthus_fleuron, the_ornament_has_the_symmetry_of_the_square)
{
	const auto key = [] (long long i, long long j, long long k) { return static_cast<std::uint64_t> (i * 73856093LL ^ j * 19349663LL ^ k * 83492791LL); };
	const double cell = 1e-3;
	for (const OrnamentCase &c : OrnamentCases ())
	{
		if (c.scale != 1.0)
			continue;
		SCOPED_TRACE (LevelName (c.detail));
		const Mesh &m = *c.ornament.mesh;
		const unsigned n = static_cast<unsigned> (m.GetNVertices ());
		std::vector<Point> points (n);
		std::unordered_multimap<std::uint64_t, unsigned> grid;
		grid.reserve (n);
		const auto cellOf = [&] (double x) { return static_cast<long long> (std::floor (x / cell)); };
		for (unsigned v = 0; v < n; ++v)
		{
			points[v] = MeshParts (m, c.env, v);
			grid.emplace (key (cellOf (points[v][0]), cellOf (points[v][1]), cellOf (points[v][2])), v);
		}
		const auto nearest = [&] (const Point &q) {
			double best = 1.0;
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
		ASSERT_EQ (c.ornament.pieces.size (), kFirstFleuron + detail::kFleurons);
		const double coarse = 0.5;
		std::unordered_multimap<std::uint64_t, unsigned> far;
		far.reserve (n);
		const auto coarseOf = [&] (double x) { return static_cast<long long> (std::floor (x / coarse)); };
		for (unsigned v = 0; v < n; ++v)
			far.emplace (key (coarseOf (points[v][0]), coarseOf (points[v][1]), coarseOf (points[v][2])), v);
		const auto exact = [&] (const Point &q) {
			double best = std::numeric_limits<double>::infinity ();
			const long long i = coarseOf (q[0]), j = coarseOf (q[1]), k = coarseOf (q[2]);
			for (long long ring = 0; ring < 200 && !(best <= coarse * static_cast<double> (ring)); ++ring)
				for (long long di = -ring; di <= ring; ++di)
					for (long long dj = -ring; dj <= ring; ++dj)
						for (long long dk = -ring; dk <= ring; ++dk)
						{
							if (std::max ({std::llabs (di), std::llabs (dj), std::llabs (dk)}) != ring)
								continue;
							const auto range = far.equal_range (key (i + di, j + dj, k + dk));
							for (auto it = range.first; it != range.second; ++it)
							{
								const Point &p = points[it->second];
								best = std::min (best, std::hypot (p[0] - q[0], p[1] - q[1], p[2] - q[2]));
							}
						}
			return best;
		};
		double reflection = 0.0, rotation = 0.0, eighth = 0.0;
		const double s = std::sqrt (0.5);
		for (unsigned v = 0; v < n; ++v)
		{
			const Point &p = points[v];
			reflection = std::max (reflection, nearest ({p[0], -p[1], p[2]}));
			rotation = std::max (rotation, nearest ({-p[1], p[0], p[2]}));
			if (v >= c.ornament.pieces[kFirstFleuron].vertexBegin)
				eighth = std::max (eighth, exact ({s * (p[0] - p[1]), s * (p[0] + p[1]), p[2]}));
		}
		EXPECT_LE (reflection, 1e-5);
		EXPECT_LE (rotation, 1e-5);
		EXPECT_GE (eighth, 1.0);
		std::printf ("%s : K48 reflexion %.2e p, rotation pi/2 %.2e p, rotation pi/4 des G %.2f p (%u sommets)\n", LevelName (c.detail), reflection,
		             rotation, eighth, n);
	}
}

// K49 : jour >= c de chaque G contre les 59 autres pieces (PieceGap, sommets contre triangles, deux sens), mesure
// jusqu'a 3 p ; une piece au moins est vue a moins de 3 p (l'instrument mesure).
TEST (TEST_cgarchi_classical_acanthus_fleuron, each_fleuron_keeps_c_from_the_other_pieces)
{
	for (const OrnamentCase &c : OrnamentCases ())
	{
		if (c.scale != 1.0)
			continue;
		const std::vector<CapitalPiece> &P = c.ornament.pieces;
		ASSERT_EQ (P.size (), kFirstFleuron + detail::kFleurons);
		double least = 3.0;
		std::size_t which = 0, from = 0;
		std::array<double, 3> at {};
		for (std::size_t g = kFirstFleuron; g < P.size (); ++g)
			for (std::size_t i = 0; i < P.size (); ++i)
			{
				if (i == g)
					continue;
				std::array<double, 3> w {};
				const double d = acanthus_test::PieceGap (*c.ornament.mesh, c.env, P[g], P[i], 3.0, &w);
				if (d < least)
				{
					least = d;
					which = i;
					from = g;
					at = w;
				}
			}
		EXPECT_GE (least, kC - kFloat) << LevelName (c.detail);
		EXPECT_LT (least, 3.0) << LevelName (c.detail);
		std::printf ("%s : K49 jour des G aux autres pieces %.4f p (G%zu contre la piece %zu, sommet %.3f, %.3f, h %.3f)\n", LevelName (c.detail), least,
		             from, which, at[0], at[1], at[2]);
	}
}

// K49' : dans le maillage de la colonne, jour >= c entre chaque G et le tour, a 8, 48, 256 secteurs et cannele,
// Light et Heavy (le dos de la rosace lu au reglage de la colonne).
TEST (TEST_cgarchi_classical_acanthus_fleuron, the_column_keeps_c_between_fleurons_and_tour)
{
	const OrderData &order = ClassicalOrders ().Get (Order::Corinthian);
	for (const OrnamentDetail level : {OrnamentDetail::Light, OrnamentDetail::Heavy})
		for (const Setting &setting : Settings ())
		{
			SCOPED_TRACE (std::string (LevelName (level)) + ", " + setting.name);
			cgarchi::ClassicalColumnSettings s;
			s.order = static_cast<int> (Order::Corinthian);
			s.capitalOrnaments = static_cast<int> (level);
			s.radialSegments = setting.radialSegments;
			s.flutes = setting.flutes;
			ColumnProfile profile;
			const std::unique_ptr<Mesh> column = cgarchi::BuildClassicalColumnMesh (s, &profile);
			s.capitalOrnaments = static_cast<int> (OrnamentDetail::None);
			const std::unique_ptr<Mesh> lathe = cgarchi::BuildClassicalColumnMesh (s);
			AcanthusOptions o;
			o.detail = level;
			o.scale = s.module / order.partsPerModule;
			o.radialSegments = s.radialSegments;
			const CapitalOrnament orn = BuildCorinthianOrnament (order, profile, o);
			const detail::CapitalEnvelope env = detail::ReadCapitalEnvelope (order, profile, o.scale);
			const unsigned lv = static_cast<unsigned> (lathe->GetNVertices ()), lf = static_cast<unsigned> (lathe->GetNFaces ());
			ASSERT_EQ (static_cast<unsigned> (column->GetNVertices ()), lv + static_cast<unsigned> (orn.mesh->GetNVertices ()));
			ASSERT_EQ (orn.pieces.size (), kFirstFleuron + detail::kFleurons);
			CapitalPiece tour;
			tour.vertexEnd = lv;
			tour.faceEnd = lf;
			double least = 1.0;
			std::array<double, 3> at {};
			for (std::size_t k = 0; k < static_cast<std::size_t> (detail::kFleurons); ++k)
			{
				CapitalPiece g = orn.pieces[kFirstFleuron + k];
				g.vertexBegin += lv;
				g.vertexEnd += lv;
				g.faceBegin += lf;
				g.faceEnd += lf;
				std::array<double, 3> w {};
				const double d = acanthus_test::PieceGap (*column, env, g, tour, 1.0, &w);
				if (d < least)
				{
					least = d;
					at = w;
				}
			}
			EXPECT_GE (least, kC - kFloat);
			std::printf ("%s, %s : K49' jour des G au tour de la colonne %.5f p (sommet %.3f, %.3f, h %.3f)\n", LevelName (level), setting.name, least,
			             at[0], at[1], at[2]);
		}
}

// K56 (registre AC-46) : le prefixe F + E + C + D + V + H (70 576 / 299 888 premieres faces, 35 432 / 150 088 premiers
// sommets) ne change pas avec les fleurons : topologie a l'egalite, somme ponderee a 1e-8 (relatif), boite a 4 ulp
// float ; octets (FNV-1a) avec MSVC seulement.
TEST (TEST_cgarchi_classical_acanthus_fleuron, the_prefix_before_the_fleurons_is_unchanged)
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
	const Expected expected[] = {
	    {OrnamentDetail::Light, 1.0, 35432, 70576, 0x4c298c6d7fce8f8aULL, 0x24df88e2678f4d78ULL, 73620983702890.125, {-26.9386864, -26.9386864, 318.049988}, {26.9386864, 26.9386864, 353.730011}},
	    {OrnamentDetail::Light, 2.0, 35432, 70576, 0x4c298c6d7fce8f8aULL, 0x555de31c4ec27d79ULL, 294483934811560.5, {-53.8773727, -53.8773727, 636.099976}, {53.8773727, 53.8773727, 707.460022}},
	    {OrnamentDetail::Heavy, 1.0, 150088, 299888, 0xfe902385b15916d9ULL, 0x52d612ad7ec5ef7cULL, 1309066321024595.2, {-26.99617, -26.99617, 318.049988}, {26.99617, 26.99617, 353.730011}},
	    {OrnamentDetail::Heavy, 2.0, 150088, 299888, 0xfe902385b15916d9ULL, 0x5637499c633081b1ULL, 5236265284098381, {-53.9923401, -53.9923401, 636.099976}, {53.9923401, 53.9923401, 707.460022}}};
	for (const Expected &e : expected)
	{
		const OrnamentCase *c = nullptr;
		for (const OrnamentCase &k : OrnamentCases ())
			if (k.detail == e.detail && k.scale == e.scale)
				c = &k;
		ASSERT_NE (c, nullptr);
		SCOPED_TRACE (std::string (LevelName (e.detail)) + ", echelle " + std::to_string (e.scale));
		ASSERT_EQ (c->ornament.pieces.size (), kFirstFleuron + detail::kFleurons);
		ASSERT_EQ (c->ornament.pieces[kFirstFleuron].vertexBegin, e.vertices);
		ASSERT_EQ (c->ornament.pieces[kFirstFleuron].faceBegin, e.faces);
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
		std::printf ("K56 %s %.1f topo=0x%016llxULL bits=0x%016llxULL weighted=%.17g lo=%.9g,%.9g,%.9g hi=%.9g,%.9g,%.9g\n", LevelName (e.detail), e.scale,
		             static_cast<unsigned long long> (p.topology), static_cast<unsigned long long> (p.bits), p.weighted, p.lo[0], p.lo[1], p.lo[2],
		             p.hi[0], p.hi[1], p.hi[2]);
	}
}

// K54 : vue de face (rayon le long de x contre les triangles des pieces F a H), la tige est cachee par la pointe de E
// sur l'axe de face jusqu'a h 24,25 (anneau le plus bas compris) et entierement visible a partir de h 24,55 ; les D
// interieurs ne la masquent pas.
TEST (TEST_cgarchi_classical_acanthus_fleuron, the_stem_foot_hides_behind_the_middle_leaf)
{
	for (const OrnamentCase &c : OrnamentCases ())
	{
		if (c.scale != 1.0)
			continue;
		SCOPED_TRACE (LevelName (c.detail));
		const Mesh &m = *c.ornament.mesh;
		const std::vector<CapitalPiece> &P = c.ornament.pieces;
		ASSERT_EQ (P.size (), kFirstFleuron + detail::kFleurons);
		struct Tri
		{
			Point a, b, cc;
			double yLo, yHi, hLo, hHi, xHi;
		};
		std::vector<std::pair<Tri, CapitalPieceKind>> tris;
		for (std::size_t i = 0; i < kFirstFleuron; ++i)
			for (unsigned f = P[i].faceBegin; f < P[i].faceEnd; ++f)
			{
				Tri t {MeshParts (m, c.env, static_cast<unsigned> (m.GetFaceVertex (f, 0))), MeshParts (m, c.env, static_cast<unsigned> (m.GetFaceVertex (f, 1))),
				       MeshParts (m, c.env, static_cast<unsigned> (m.GetFaceVertex (f, 2))), 0, 0, 0, 0, 0};
				t.yLo = std::min ({t.a[1], t.b[1], t.cc[1]});
				t.yHi = std::max ({t.a[1], t.b[1], t.cc[1]});
				t.hLo = std::min ({t.a[2], t.b[2], t.cc[2]});
				t.hHi = std::max ({t.a[2], t.b[2], t.cc[2]});
				t.xHi = std::max ({t.a[0], t.b[0], t.cc[0]});
				if (t.xHi > 10.0 && t.yLo < 1.0 && t.yHi > -1.0 && t.hLo < 34.0 && t.hHi > 23.0)
					tris.push_back ({t, P[i].kind});
			}
		const auto hiddenBy = [&] (const Point &o) {
			for (const auto &[t, kind] : tris)
			{
				if (o[1] < t.yLo || o[1] > t.yHi || o[2] < t.hLo || o[2] > t.hHi || t.xHi <= o[0])
					continue;
				const double det = (t.b[1] - t.a[1]) * (t.cc[2] - t.a[2]) - (t.cc[1] - t.a[1]) * (t.b[2] - t.a[2]);
				if (std::fabs (det) < 1e-14)
					continue;
				const double w1 = ((o[1] - t.a[1]) * (t.cc[2] - t.a[2]) - (t.cc[1] - t.a[1]) * (o[2] - t.a[2])) / det;
				const double w2 = ((t.b[1] - t.a[1]) * (o[2] - t.a[2]) - (o[1] - t.a[1]) * (t.b[2] - t.a[2])) / det;
				if (w1 < 0.0 || w2 < 0.0 || w1 + w2 > 1.0)
					continue;
				if ((1.0 - w1 - w2) * t.a[0] + w1 * t.b[0] + w2 * t.cc[0] > o[0])
					return static_cast<int> (kind);
			}
			return -1;
		};
		const detail::FleuronSpec spec = detail::DefaultFleuronSpec (c.detail);
		const unsigned stemBegin = P[kFirstFleuron].vertexBegin + MakeRosette (c.detail).piece.rosetteVertices;
		const unsigned q = static_cast<unsigned> (spec.stemSection);
		int low = 0, lowHidden = 0, high = 0, highHidden = 0, byOthers = 0;
		bool lowestHidden = true;
		for (unsigned v = stemBegin; v < P[kFirstFleuron].vertexEnd; ++v)
		{
			const unsigned j = (v - stemBegin) % q;
			if (j == 0 || j >= q - 2)
				continue;
			const Point o = MeshParts (m, c.env, v);
			const int by = hiddenBy (o);
			if (o[2] <= 24.25)
			{
				++low;
				lowHidden += by >= 0 ? 1 : 0;
				byOthers += by >= 0 && by != static_cast<int> (CapitalPieceKind::MiddleLeaf) ? 1 : 0;
				if (v < stemBegin + q)
					lowestHidden = lowestHidden && by >= 0;
			}
			else if (o[2] >= 24.55)
			{
				++high;
				highHidden += by >= 0 ? 1 : 0;
			}
		}
		EXPECT_GT (low, 0);
		EXPECT_TRUE (lowestHidden);
		EXPECT_EQ (lowHidden, low);
		EXPECT_EQ (byOthers, 0);
		EXPECT_GT (high, 0);
		EXPECT_EQ (highHidden, 0);
		std::printf ("%s : K54 tige cachee sous h 24,25 : %d / %d sommets avant (par E) ; au-dessus de h 24,55 : %d / %d caches\n",
		             LevelName (c.detail), lowHidden, low, highHidden, high);
	}
}
