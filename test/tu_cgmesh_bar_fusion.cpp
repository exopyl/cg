#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "clipper2/clipper.h"

#include "../src/cgmesh/bar_fusion.h"
#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/mesh_validation.h"
#include "../src/cgmesh/polygon2.h"
#include "../src/cgmesh/profile2d.h"

// ===========================================================================
//  Fusion des barres : niveaux 2D
// ===========================================================================
//
// Les niveaux sont lus par des instruments independants de la construction :
// coupe d'une region par une verticale, appartenance d'un point par
// enroulement, distance a un contour, coupe d'un profil par interpolation sur
// ses aretes, distance exacte entre deux bords. Les controles d'imbrication
// passent par Clipper2 : erosion et difference (valides sur des anneaux ecrits
// a la main), dilatation et difference pour la marge controlee.

namespace
{
	const double kStep = 1.0 / kBarFusionGridScale;

	// Ecart admis du bord d'un niveau a la coupe du profil, sur un cote droit,
	// dans les deux sens. Tolerance EMPIRIQUE, pas une borne : elle compte
	// l'ecart de la simplification (kBarFusionSimplifySteps, Ramer-Douglas-
	// Peucker), l'arrondi du contour (1/2 pas) et celui des sommets d'un
	// decalage ou d'un booleen (sqrt(2)/2 pas), mais pas les deux decalages de
	// l'ouverture ; la borne formelle depasse 4,6 pas. Les mesures qui la
	// justifient sont dans le plan (gothic_implementation.md, O2-b).
	const double kSimplifyTolerance = (kBarFusionSimplifySteps + 0.5 + 0.71) * kStep;

	using Ring = std::vector<Vector2d>;
	using Interval = std::pair<double, double>;

	Ring rect(double x0, double y0, double x1, double y1)
	{
		return {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};   // sens trigonometrique
	}

	Ring reversed(Ring r) { return Ring(r.rbegin(), r.rend()); }

	Ring circleRing(double cx, double cy, double r, int n)
	{
		Ring ring;
		for (int k = 0; k < n; ++k)
		{
			const double t = 2.0 * 3.14159265358979323846 * k / n;
			ring.push_back({cx + r * std::cos(t), cy + r * std::sin(t)});
		}
		return ring;
	}

	// Coupe d'un profil ferme a la cote u, par interpolation sur ses aretes.
	std::vector<Interval> profileSection(const Profile2D &p, double u)
	{
		std::vector<double> vs;
		const size_t n = p.points.size();
		for (size_t i = 0; i < n; ++i)
		{
			const Vector2d &a = p.points[i], &b = p.points[(i + 1) % n];
			if ((a.x <= u && b.x > u) || (b.x <= u && a.x > u))
				vs.push_back(a.y + (u - a.x) / (b.x - a.x) * (b.y - a.y));
		}
		std::sort(vs.begin(), vs.end());
		std::vector<Interval> out;
		for (size_t i = 0; i + 1 < vs.size(); i += 2)
			out.push_back({vs[i], vs[i + 1]});
		return out;
	}

	// Demi-largeur d'un profil symetrique a la cote u (crete : 0).
	double halfWidth(const Profile2D &p, double u)
	{
		const std::vector<Interval> s = profileSection(p, u);
		return s.empty() ? 0.0 : 0.5 * (s.back().second - s.front().first);
	}

	// Coupe d'une region par la verticale x : intervalles de y, croissants.
	std::vector<Interval> sectionAtX(const std::vector<BarGridRing> &region, double x)
	{
		std::vector<double> ys;
		for (const BarGridRing &r : region)
			for (size_t i = 0; i < r.size(); ++i)
			{
				const double ax = r[i].x * kStep, ay = r[i].y * kStep;
				const double bx = r[(i + 1) % r.size()].x * kStep, by = r[(i + 1) % r.size()].y * kStep;
				if ((ax <= x && bx > x) || (bx <= x && ax > x))
					ys.push_back(ay + (x - ax) / (bx - ax) * (by - ay));
			}
		std::sort(ys.begin(), ys.end());
		std::vector<Interval> out;
		for (size_t i = 0; i + 1 < ys.size(); i += 2)
			out.push_back({ys[i], ys[i + 1]});
		return out;
	}

	// Enroulement de la region autour de (x, y), non nul = dedans.
	bool inside(const std::vector<BarGridRing> &region, double x, double y)
	{
		int w = 0;
		for (const BarGridRing &r : region)
			for (size_t i = 0; i < r.size(); ++i)
			{
				const double ax = r[i].x * kStep, ay = r[i].y * kStep;
				const double bx = r[(i + 1) % r.size()].x * kStep, by = r[(i + 1) % r.size()].y * kStep;
				const double side = (bx - ax) * (y - ay) - (by - ay) * (x - ax);
				if (ay <= y && by > y && side > 0) ++w;
				else if (by <= y && ay > y && side < 0) --w;
			}
		return w != 0;
	}

	// Distance de (x, y) au contour ferme `ring` (ses aretes).
	double distanceToRing(const Ring &ring, double x, double y)
	{
		double d = std::numeric_limits<double>::infinity();
		for (size_t i = 0; i < ring.size(); ++i)
		{
			const Vector2d &a = ring[i], &b = ring[(i + 1) % ring.size()];
			const double dx = b.x - a.x, dy = b.y - a.y, l2 = dx * dx + dy * dy;
			const double t = (std::max)(0.0, (std::min)(1.0, ((x - a.x) * dx + (y - a.y) * dy) / l2));
			d = (std::min)(d, std::hypot(a.x + t * dx - x, a.y + t * dy - y));
		}
		return d;
	}

	Clipper2Lib::Paths64 toPaths(const std::vector<BarGridRing> &region)
	{
		Clipper2Lib::Paths64 out;
		for (const BarGridRing &r : region)
		{
			Clipper2Lib::Path64 p;
			for (const BarGridPoint &q : r)
				p.emplace_back(q.x, q.y);
			out.push_back(p);
		}
		return out;
	}

	// Aire (pas de grille au carre) de la partie de `inner` hors de `outer`
	// erode de `steps` pas : 0 quand inner est dans outer a `steps` pas pres.
	double areaOutsideEroded(const std::vector<BarGridRing> &inner, const std::vector<BarGridRing> &outer, double steps)
	{
		Clipper2Lib::ClipperOffset co(2.0, 0.1);
		co.AddPaths(toPaths(outer), Clipper2Lib::JoinType::Round, Clipper2Lib::EndType::Polygon);
		Clipper2Lib::Paths64 eroded;
		co.Execute(-steps, eroded);
		Clipper2Lib::Clipper64 c;
		c.AddSubject(toPaths(inner));
		c.AddClip(eroded);
		Clipper2Lib::Paths64 out;
		c.Execute(Clipper2Lib::ClipType::Difference, Clipper2Lib::FillRule::NonZero, out);
		return std::fabs(Clipper2Lib::Area(out));
	}

	void expectNested(const std::vector<BarFusionLevel> &levels)
	{
		for (size_t k = 0; k + 1 < levels.size(); ++k)
			EXPECT_EQ(areaOutsideEroded(levels[k + 1].region, levels[k].region, 3.0), 0.0) << "niveau " << k + 1;
	}

	BarGridRing gridSquare(std::int64_t x0, std::int64_t y0, std::int64_t x1, std::int64_t y1)
	{
		return {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
	}

	Profile2D circleProfile(double r, double cu)
	{
		Profile2D p;
		for (int k = 0; k < 24; ++k)
		{
			const double t = 2.0 * 3.14159265358979323846 * k / 24;
			p.points.push_back({cu + r * std::sin(t), r * std::cos(t)});
		}
		return p;
	}

	Profile2D asymmetricProfile()
	{
		Profile2D p;
		p.points = {{0, 8}, {4, 6}, {5, 2}, {0, -1}};
		return p;
	}

	// got = [lo, hi], elargi d'au plus `widen` ou retreci d'au plus `shrink` de
	// chaque cote.
	void expectInterval(const Interval &got, double lo, double hi, double widen, double shrink)
	{
		EXPECT_GE(got.first, lo - widen) << "bas " << got.first << " attendu " << lo;
		EXPECT_LE(got.first, lo + shrink) << "bas " << got.first << " attendu " << lo;
		EXPECT_GE(got.second, hi - shrink) << "haut " << got.second << " attendu " << hi;
		EXPECT_LE(got.second, hi + widen) << "haut " << got.second << " attendu " << hi;
	}
}

// Le controle d'imbrication voit un niveau egal au precedent ou a 2 pas de lui,
// et laisse passer un niveau a 4 pas.
TEST(TEST_cgmesh_bar_fusion, nesting_check_detects_a_level_too_close_to_the_previous_one)
{
	const std::vector<BarGridRing> outer = {gridSquare(0, 0, 1000, 1000)};
	EXPECT_GT(areaOutsideEroded(outer, outer, 3.0), 0.0);
	EXPECT_GT(areaOutsideEroded({gridSquare(2, 2, 998, 998)}, outer, 3.0), 0.0);
	EXPECT_EQ(areaOutsideEroded({gridSquare(4, 4, 996, 996)}, outer, 3.0), 0.0);
}

// Hauteurs = cotes des sommets du profil, au moins 4 pas sur la hauteur de
// barre : Roll 12 segments 8 niveaux (0 a 1,29 subdivise), Keel 5 (une seule
// arete, subdivisee en 4), Ogee 6 segments 7 (aucune subdivision). Un profil
// releve ou enfonce de 1 decale les hauteurs de 1.
TEST(TEST_cgmesh_bar_fusion, level_heights_follow_the_profile_vertices_with_at_least_four_steps)
{
	const double pi = 3.14159265358979323846;
	std::vector<double> roll = {0.0, 2.5 * std::sin(pi / 12)};
	for (int k = 1; k <= 6; ++k)
		roll.push_back(5.0 * std::sin(k * pi / 12));
	std::vector<double> ogee;
	for (int k = 0; k <= 6; ++k)
	{
		const double s = k / 6.0;
		ogee.push_back(5.0 * (3 * s * s - 2 * s * s * s));
	}
	struct Case { const char *name; Profile2D profile; std::vector<double> heights; };
	const std::vector<Case> cases = {
		{"Roll", rollBarProfile(5.0), roll},
		{"Keel", keelBarProfile(5.0), {0.0, 1.5, 3.0, 4.5, 6.0}},
		{"Ogee", ogeeBarProfile(5.0), ogee},
	};
	for (const Case &c : cases)
	{
		SCOPED_TRACE(c.name);
		const std::vector<double> h = barFusionLevelHeights(c.profile, {1.0});
		ASSERT_EQ(h.size(), c.heights.size());
		for (size_t k = 0; k < h.size(); ++k)
			EXPECT_NEAR(h[k], c.heights[k], 1e-12) << "niveau " << k;
		for (const double du : {1.0, -1.0})
		{
			const std::vector<double> shifted = barFusionLevelHeights(translatedInU(c.profile, du), {1.0});
			ASSERT_EQ(shifted.size(), h.size());
			for (size_t k = 0; k < h.size(); ++k)
				EXPECT_NEAR(shifted[k], h[k] + du, 1e-12);
		}
	}
	{
		SCOPED_TRACE("Roll, echelles 1 et 0,5");
		const std::vector<double> h = barFusionLevelHeights(rollBarProfile(4.0), {1.0, 0.5, 1.0});
		std::vector<double> expected;
		for (int k = 0; k <= 6; ++k)
		{
			expected.push_back(4.0 * std::sin(k * pi / 12));
			if (k > 0 && k < 6) expected.push_back(2.0 * std::sin(k * pi / 12));
		}
		std::sort(expected.begin(), expected.end());
		expected.erase(std::unique(expected.begin(), expected.end(),
		                           [](double a, double b) { return std::fabs(a - b) < 1e-9; }), expected.end());
		ASSERT_EQ(h.size(), expected.size());
		for (size_t k = 0; k < h.size(); ++k)
			EXPECT_NEAR(h[k], expected[k], 1e-12) << "niveau " << k;
	}
}

// Barre droite seule (rectangle 100 x 40, Roll rayon 5) : a chaque niveau, la
// coupe a mi-longueur traverse le cote bas et le cote haut sur la coupe du
// profil a cette hauteur, centree sur le cote, a kSimplifyTolerance pres ; la
// crete est un ruban de +/- 4 pas. Meme resultat pour le contour parcouru en
// sens horaire (profil symetrique).
TEST(TEST_cgmesh_bar_fusion, straight_bar_levels_have_the_width_of_the_profile_section)
{
	const Profile2D roll = rollBarProfile(5.0);
	for (const bool direct : {true, false})
	{
		SCOPED_TRACE(direct ? "sens trigonometrique" : "sens horaire");
		const Ring outline = direct ? rect(0, 0, 100, 40) : reversed(rect(0, 0, 100, 40));
		const std::vector<BarFusionLevel> levels = buildBarFusionLevels({{outline, 1.0}}, roll);
		const std::vector<double> heights = barFusionLevelHeights(roll, {1.0});
		ASSERT_EQ(levels.size(), 8u);
		for (size_t k = 0; k < levels.size(); ++k)
		{
			SCOPED_TRACE("niveau " + std::to_string(k));
			EXPECT_DOUBLE_EQ(levels[k].height, heights[k]);
			const double w = (k == 0) ? 5.0 : (k + 1 == levels.size()) ? kBarFusionApexHalfWidthSteps * kStep
			                                                         : halfWidth(roll, heights[k]);
			const std::vector<Interval> s = sectionAtX(levels[k].region, 50.0);
			ASSERT_EQ(s.size(), 2u);
			expectInterval(s[0], -w, w, kSimplifyTolerance, kSimplifyTolerance);
			expectInterval(s[1], 40.0 - w, 40.0 + w, kSimplifyTolerance, kSimplifyTolerance);
		}
		expectNested(levels);
	}
}

// Deux barres qui se croisent (rectangles 100 x 20 et 20 x 100, Roll rayon
// 3) : a chaque niveau, un point est dans la region exactement quand il est a
// moins de la demi-largeur du profil de l'un OU de l'autre contour (points a
// plus de 6 pas de cette limite, sur une grille de pas 0,73).
TEST(TEST_cgmesh_bar_fusion, crossing_bars_merge_into_their_union)
{
	const Profile2D roll = rollBarProfile(3.0);
	const Ring a = rect(0, 0, 100, 20), b = rect(40, -40, 60, 60);
	const std::vector<BarFusionLevel> levels = buildBarFusionLevels({{a, 1.0}, {b, 1.0}}, roll);
	ASSERT_EQ(levels.size(), 8u);
	for (size_t k = 0; k < levels.size(); ++k)
	{
		SCOPED_TRACE("niveau " + std::to_string(k));
		const double w = (k == 0) ? 3.0 : (k + 1 == levels.size()) ? kBarFusionApexHalfWidthSteps * kStep
		                                                         : halfWidth(roll, levels[k].height);
		int checked = 0, wrong = 0, inBoth = 0;
		for (double x = -5.0; x <= 105.0; x += 0.73)
			for (double y = -45.0; y <= 65.0; y += 0.73)
			{
				const double da = distanceToRing(a, x, y), db = distanceToRing(b, x, y);
				if (std::fabs(da - w) < 6 * kStep || std::fabs(db - w) < 6 * kStep)
					continue;
				const bool expected = da < w || db < w;
				++checked;
				inBoth += (da < w && db < w);
				wrong += (inside(levels[k].region, x, y) != expected);
			}
		EXPECT_EQ(wrong, 0) << "sur " << checked << " points";
		if (k + 1 < levels.size())
			EXPECT_GT(inBoth, 0);
	}
	expectNested(levels);
}

// Profil a contre-depouille (cercle de rayon 4 pose sur la face) : l'empreinte
// est la coupe la plus large (v dans [-4, 4], a l'equateur), pas la coupe au
// pied (un point) ; les niveaux restent imbriques.
TEST(TEST_cgmesh_bar_fusion, undercut_profile_is_filled_towards_the_face)
{
	const std::vector<BarFusionLevel> levels = buildBarFusionLevels({{rect(0, 0, 100, 40), 1.0}}, circleProfile(4.0, 4.0));
	ASSERT_GE(levels.size(), 2u);
	const std::vector<Interval> s = sectionAtX(levels[0].region, 50.0);
	ASSERT_EQ(s.size(), 2u);
	expectInterval(s[0], -4.0, 4.0, kSimplifyTolerance, kSimplifyTolerance);
	expectInterval(s[1], 36.0, 44.0, kSimplifyTolerance, kSimplifyTolerance);
	expectNested(levels);
}

// Profil asymetrique, v de -1 a 8 au pied, crete en v = 2 : v positif est a
// droite du parcours. Contour direct : le cote bas porte y = -v, soit
// [-8, 1] au pied et [-6, -1,4] a la cote 4 ; contour horaire : y = v,
// [-1, 8] et [1,4, 6].
TEST(TEST_cgmesh_bar_fusion, asymmetric_profile_lies_on_the_right_of_the_outline)
{
	const Profile2D asym = asymmetricProfile();
	for (const bool direct : {true, false})
	{
		SCOPED_TRACE(direct ? "sens trigonometrique" : "sens horaire");
		const Ring outline = direct ? rect(0, 0, 100, 40) : reversed(rect(0, 0, 100, 40));
		const std::vector<BarFusionLevel> levels = buildBarFusionLevels({{outline, 1.0}}, asym);
		ASSERT_EQ(levels.size(), 6u);   // 0, 1, 2, 3, 4 (subdivision de 0-4 en pas de 1,25 au plus), 5
		EXPECT_DOUBLE_EQ(levels[4].height, 4.0);
		const double sgn = direct ? -1.0 : 1.0;
		const std::vector<Interval> foot = sectionAtX(levels[0].region, 50.0);
		const std::vector<Interval> at4 = sectionAtX(levels[4].region, 50.0);
		ASSERT_EQ(foot.size(), 2u);
		ASSERT_EQ(at4.size(), 2u);
		auto expectSide = [&](const Interval &got, double v0, double v1) {
			expectInterval(got, (std::min)(sgn * v0, sgn * v1), (std::max)(sgn * v0, sgn * v1), kSimplifyTolerance, kSimplifyTolerance);
		};
		expectSide(foot[0], -1.0, 8.0);
		expectSide(at4[0], 1.4, 6.0);
		expectNested(levels);
	}
}

// Barres d'echelles 1 et 0,5 (Roll rayon 4) : chacune garde la coupe de son
// profil mis a l'echelle (u et v), a la simplification et a l'imbrication pres
// (kSimplifyTolerance, plus au plus 4 pas en retrait quand deux hauteurs
// voisines viennent des deux echelles) ; au-dessus de sa crete (cote 2), la
// petite barre n'a plus de bande.
TEST(TEST_cgmesh_bar_fusion, bars_of_different_scales_keep_their_own_sections)
{
	const Profile2D roll = rollBarProfile(4.0);
	const std::vector<BarFusionLevel> levels =
		buildBarFusionLevels({{rect(0, 0, 100, 40), 1.0}, {rect(0, 100, 100, 140), 0.5}}, roll);
	ASSERT_EQ(levels.size(), 12u);   // 7 cotes par echelle, la crete 2 de l'une egale a 4 sin 30 de l'autre
	ASSERT_EQ(barFusionLevelHeights(roll, {1.0, 0.5}).size(), 12u);
	for (size_t k = 0; k < levels.size(); ++k)
	{
		const double h = levels[k].height;
		SCOPED_TRACE("niveau " + std::to_string(k) + ", cote " + std::to_string(h));
		const bool top = k + 1 == levels.size();
		const double wa = (k == 0) ? 4.0 : top ? kBarFusionApexHalfWidthSteps * kStep : halfWidth(roll, h);
		const std::vector<Interval> s = sectionAtX(levels[k].region, 50.0);
		if (h > 2.0 + 1e-9)
		{
			ASSERT_EQ(s.size(), 2u);
		}
		else
		{
			ASSERT_EQ(s.size(), 4u);
			const double wb = (k == 0) ? 2.0 : (std::fabs(h - 2.0) < 1e-9) ? kBarFusionApexHalfWidthSteps * kStep
			                                                              : 0.5 * halfWidth(roll, 2.0 * h);
			expectInterval(s[2], 100.0 - wb, 100.0 + wb, kSimplifyTolerance, kSimplifyTolerance + 4 * kStep);
			expectInterval(s[3], 140.0 - wb, 140.0 + wb, kSimplifyTolerance, kSimplifyTolerance + 4 * kStep);
		}
		expectInterval(s[0], -wa, wa, kSimplifyTolerance, kSimplifyTolerance + 4 * kStep);
		expectInterval(s[1], 40.0 - wa, 40.0 + wa, kSimplifyTolerance, kSimplifyTolerance + 4 * kStep);
	}
	expectNested(levels);
}

// Contours courbes et barres serrees (cercles de rayon 20 et 6, voisins a
// moins de deux largeurs ; Ogee et Keel) : niveaux imbriques de 3 pas.
TEST(TEST_cgmesh_bar_fusion, curved_and_close_bars_stay_nested)
{
	const std::vector<FusedBar> bars = {{circleRing(0, 0, 20, 96), 1.0}, {circleRing(27, 0, 6, 48), 0.6},
	                                    {reversed(circleRing(0, 30, 8, 64)), 1.0}};
	for (const Profile2D &p : {ogeeBarProfile(3.0), keelBarProfile(3.0), rollBarProfile(3.0)})
	{
		const std::vector<BarFusionLevel> levels = buildBarFusionLevels(bars, p);
		ASSERT_GE(levels.size(), 5u);
		for (const BarFusionLevel &l : levels)
			EXPECT_FALSE(l.region.empty());
		expectNested(levels);
	}
}

// Profil releve ou enfonce de 1 (u_min = +1 ou -1) : memes regions a un pas
// pres, hauteurs decalees de 1.
TEST(TEST_cgmesh_bar_fusion, raised_or_sunk_profile_shifts_the_heights_only)
{
	const Profile2D roll = rollBarProfile(5.0);
	const std::vector<FusedBar> bars = {{rect(0, 0, 100, 40), 1.0}};
	const std::vector<BarFusionLevel> base = buildBarFusionLevels(bars, roll);
	ASSERT_EQ(base.size(), 8u);
	for (const double du : {1.0, -1.0})
	{
		SCOPED_TRACE("du = " + std::to_string(du));
		const std::vector<BarFusionLevel> moved = buildBarFusionLevels(bars, translatedInU(roll, du));
		ASSERT_EQ(moved.size(), base.size());
		for (size_t k = 0; k < base.size(); ++k)
		{
			EXPECT_NEAR(moved[k].height, base[k].height + du, 1e-12);
			const std::vector<Interval> a = sectionAtX(base[k].region, 50.0), b = sectionAtX(moved[k].region, 50.0);
			ASSERT_EQ(a.size(), b.size());
			for (size_t i = 0; i < a.size(); ++i)
			{
				EXPECT_NEAR(a[i].first, b[i].first, kStep);
				EXPECT_NEAR(a[i].second, b[i].second, kStep);
			}
		}
	}
}

TEST(TEST_cgmesh_bar_fusion, invalid_profiles_and_scales_are_refused)
{
	const std::vector<FusedBar> bars = {{rect(0, 0, 100, 40), 1.0}};
	Profile2D two;
	two.points = {{0, 1}, {1, 0}};
	Profile2D flat;
	flat.points = {{0, 1}, {0, 0}, {0, -1}};
	Profile2D nan = rollBarProfile(5.0);
	nan.points[3].y = std::numeric_limits<double>::quiet_NaN();
	for (const Profile2D &p : {two, flat, nan})
	{
		EXPECT_THROW(buildBarFusionLevels(bars, p), std::invalid_argument);
		EXPECT_THROW(barFusionLevelHeights(p, {1.0}), std::invalid_argument);
	}
	for (const double f : {0.0, -1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()})
	{
		EXPECT_THROW(buildBarFusionLevels({{rect(0, 0, 100, 40), f}}, rollBarProfile(5.0)), std::invalid_argument);
		EXPECT_THROW(barFusionLevelHeights(rollBarProfile(5.0), {f}), std::invalid_argument);
	}
}

TEST(TEST_cgmesh_bar_fusion, no_usable_bar_yields_no_level)
{
	EXPECT_TRUE(buildBarFusionLevels({}, rollBarProfile(5.0)).empty());
	EXPECT_TRUE(buildBarFusionLevels({{Ring{{0, 0}, {1, 0}}, 1.0}, {Ring{{0, 0}, {1e-4, 0}, {0, 1e-4}}, 1.0}},
	                                 rollBarProfile(5.0)).empty());
	EXPECT_TRUE(buildBarFusionLevels({{Ring{{0, 0}, {1, 0}, {2, 0}}, 1.0}}, rollBarProfile(5.0)).empty());
	EXPECT_TRUE(barFusionLevelHeights(rollBarProfile(5.0), {}).empty());
}

// Deux barres d'echelles presque egales (1 et 1 + 1e-7, puis 1 + 5e-7, Roll
// rayon 5) : leurs cretes tombent sur un meme niveau ; au sommet, chacune y
// garde son ruban de crete (4 intervalles a mi-longueur, non 2).
TEST(TEST_cgmesh_bar_fusion, nearly_equal_scales_keep_both_crests)
{
	for (const double df : {1e-7, 5e-7})
	{
		SCOPED_TRACE("echelle 1 + " + std::to_string(df));
		const std::vector<BarFusionLevel> levels =
			buildBarFusionLevels({{rect(0, 0, 100, 40), 1.0}, {rect(0, 100, 100, 140), 1.0 + df}}, rollBarProfile(5.0));
		ASSERT_EQ(levels.size(), 8u);
		const std::vector<Interval> s = sectionAtX(levels.back().region, 50.0);
		ASSERT_EQ(s.size(), 4u);
		const double w = kBarFusionApexHalfWidthSteps * kStep;
		expectInterval(s[2], 100.0 - w, 100.0 + w, kSimplifyTolerance, kSimplifyTolerance);
		expectInterval(s[3], 140.0 - w, 140.0 + w, kSimplifyTolerance, kSimplifyTolerance);
	}
}

// Crete en plateau presque plat (sommets en u = 1 et u = 1 - delta, v de
// -0,5 a 0,5), barres d'echelles 1 et 0,5 : pour la petite barre, le sommet
// 1 - delta est a delta / 2 de sa crete en cote, sous la tolerance de crete ;
// au niveau de cette crete, son ruban couvre tout le plateau (v de -0,25 a
// 0,25, plus 4 pas de chaque cote), pas seulement l'un des deux sommets.
TEST(TEST_cgmesh_bar_fusion, crest_plateau_is_kept_for_a_smaller_bar)
{
	for (const double delta : {1.5e-6, 1.9e-6})
	{
		SCOPED_TRACE("delta " + std::to_string(delta));
		Profile2D plateau;
		plateau.points = {{0, -1}, {1, -0.5}, {1 - delta, 0.5}, {0, 1}};
		const std::vector<BarFusionLevel> levels =
			buildBarFusionLevels({{rect(0, 0, 100, 40), 1.0}, {rect(0, 100, 100, 140), 0.5}}, plateau);
		const auto crest = std::find_if(levels.begin(), levels.end(),
		                                [](const BarFusionLevel &l) { return std::fabs(l.height - 0.5) < 1e-5; });
		ASSERT_NE(crest, levels.end());
		const std::vector<Interval> s = sectionAtX(crest->region, 50.0);
		ASSERT_EQ(s.size(), 4u);
		const double w = 0.25 + kBarFusionApexHalfWidthSteps * kStep;
		expectInterval(s[2], 100.0 - w, 100.0 + w, kSimplifyTolerance, kSimplifyTolerance);
		expectInterval(s[3], 140.0 - w, 140.0 + w, kSimplifyTolerance, kSimplifyTolerance);
	}
}

// Barre droite translatee de fractions de pas (64 translations, Roll 3,
// Ogee 5, profil asymetrique) : chaque niveau garde la coupe du profil, a
// kSimplifyTolerance pres, par rapport au contour arrondi a la grille.
TEST(TEST_cgmesh_bar_fusion, sub_step_translations_keep_the_profile_section)
{
	for (const Profile2D &p : {rollBarProfile(3.0), ogeeBarProfile(5.0), asymmetricProfile()})
		for (int t = 0; t < 64; ++t)
		{
			const double x0 = std::fmod(t * 0.618034, 1.0) * 3.7, y0 = std::fmod(t * 0.414214, 1.0) * 2.3;
			const double yb = std::llround(y0 * kBarFusionGridScale) * kStep;
			const std::vector<BarFusionLevel> levels = buildBarFusionLevels({{rect(x0, y0, x0 + 100, y0 + 40), 1.0}}, p);
			ASSERT_GE(levels.size(), 2u);
			for (size_t k = 0; k + 1 < levels.size(); ++k)
			{
				// Enveloppe : la coupe la plus large au-dessus du niveau k.
				double lo = 1e9, hi = -1e9;
				for (size_t j = k; j + 1 < levels.size(); ++j)
					for (const Interval &iv : profileSection(p, j == 0 ? levels[0].height + 1e-7 : levels[j].height))
					{
						lo = (std::min)(lo, iv.first);
						hi = (std::max)(hi, iv.second);
					}
				const std::vector<Interval> s = sectionAtX(levels[k].region, x0 + 50.0);
				ASSERT_EQ(s.size(), 2u) << "translation " << t << ", niveau " << k;
				SCOPED_TRACE("translation " + std::to_string(t) + ", niveau " + std::to_string(k));
				expectInterval(s[0], yb - hi, yb - lo, kSimplifyTolerance, kSimplifyTolerance);
			}
		}
}

namespace
{
	// Distance minimale (pas de grille) entre les bords de deux regions : de
	// sommet a arete, dans les deux sens, en double.
	double boundaryDistance(const std::vector<BarGridRing> &a, const std::vector<BarGridRing> &b)
	{
		double m = std::numeric_limits<double>::infinity();
		for (int pass = 0; pass < 2; ++pass)
		{
			const std::vector<BarGridRing> &points = pass ? b : a, &edges = pass ? a : b;
			for (const BarGridRing &r : points)
				for (const BarGridPoint &p : r)
					for (const BarGridRing &e : edges)
						for (size_t i = 0; i < e.size(); ++i)
						{
							const double ax = (double)e[i].x, ay = (double)e[i].y;
							const double dx = (double)e[(i + 1) % e.size()].x - ax, dy = (double)e[(i + 1) % e.size()].y - ay;
							const double l2 = dx * dx + dy * dy;
							const double t = l2 > 0 ? (std::max)(0.0, (std::min)(1.0, (((double)p.x - ax) * dx + ((double)p.y - ay) * dy) / l2)) : 0.0;
							m = (std::min)(m, std::hypot(ax + t * dx - (double)p.x, ay + t * dy - (double)p.y));
						}
		}
		return m;
	}

	// Graine SplitMix64 : tirages identiques d'un compilateur a l'autre.
	struct Draw
	{
		std::uint64_t s;
		double next()
		{
			std::uint64_t z = (s += 0x9E3779B97F4A7C15ull);
			z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
			z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
			return (double)((z ^ (z >> 31)) >> 11) / 9007199254740992.0;
		}
	};
}

// Barres aleatoires (rectangles tournes, cercles, sens horaire, echelles 1 et
// 0,6 ; Roll, Keel, Ogee) : chaque niveau, dilate de
// kBarFusionNestedMarginSteps, reste dans le precedent, et la distance exacte
// entre leurs bords vaut au moins cette marge moins un pas. Le jeu comporte des
// niveaux repares, aucun retire.
TEST(TEST_cgmesh_bar_fusion, levels_keep_the_controlled_nesting_margin)
{
	const std::vector<Profile2D> profiles = {rollBarProfile(5.0), rollBarProfile(3.0), keelBarProfile(5.0), ogeeBarProfile(5.0)};
	Draw d{7};
	int pairs = 0, repaired = 0, dropped = 0;
	for (int it = 0; it < 40; ++it)
	{
		std::vector<FusedBar> bars;
		for (int b = 0; b < 1 + it % 3; ++b)
		{
			const double cx = d.next() * 60, cy = d.next() * 60, a = d.next() * 3.14159, w = 10 + d.next() * 40, h = 10 + d.next() * 30;
			Ring r;
			if (d.next() < 0.5)
				for (const auto &c : std::vector<std::pair<double, double>>{{-w, -h}, {w, -h}, {w, h}, {-w, h}})
					r.push_back({cx + c.first * std::cos(a) - c.second * std::sin(a), cy + c.first * std::sin(a) + c.second * std::cos(a)});
			else
				r = circleRing(cx, cy, 6 + d.next() * 20, 48 + (int)(d.next() * 48));
			if (d.next() < 0.3)
				r = reversed(r);
			bars.push_back({r, d.next() < 0.3 ? 0.6 : 1.0});
		}
		BarFusionReport report;
		const std::vector<BarFusionLevel> levels = buildBarFusionLevels(bars, profiles[it % 4], &report);
		repaired += report.repairedLevels;
		dropped += report.droppedLevels;
		for (size_t k = 0; k + 1 < levels.size(); ++k)
		{
			SCOPED_TRACE("tirage " + std::to_string(it) + ", niveau " + std::to_string(k + 1));
			++pairs;
			Clipper2Lib::ClipperOffset co(2.0, 0.1);
			co.AddPaths(toPaths(levels[k + 1].region), Clipper2Lib::JoinType::Round, Clipper2Lib::EndType::Polygon);
			Clipper2Lib::Paths64 dilated;
			co.Execute(kBarFusionNestedMarginSteps, dilated);
			Clipper2Lib::Clipper64 c;
			c.AddSubject(dilated);
			c.AddClip(toPaths(levels[k].region));
			Clipper2Lib::Paths64 outside;
			c.Execute(Clipper2Lib::ClipType::Difference, Clipper2Lib::FillRule::NonZero, outside);
			EXPECT_EQ(Clipper2Lib::Area(outside), 0.0);
			EXPECT_GE(boundaryDistance(levels[k].region, levels[k + 1].region), kBarFusionNestedMarginSteps - 1.0);
		}
	}
	EXPECT_GT(pairs, 100);
	EXPECT_GT(repaired, 0);
	EXPECT_EQ(dropped, 0);
}

// ===========================================================================
//  Assemblage : plaque et barres en une seule coque fermee
// ===========================================================================

namespace
{
	Polygon2 plateOf(const std::vector<Ring> &rings)
	{
		Polygon2 p;
		p.alloc_contours((int)rings.size());
		for (size_t c = 0; c < rings.size(); ++c)
		{
			std::vector<float> f;
			for (const Vector2d &v : rings[c])
			{
				f.push_back((float)v.x);
				f.push_back((float)v.y);
			}
			p.add_contour((unsigned int)c, (unsigned int)rings[c].size(), f.data());
		}
		return p;
	}

	// Une composante, aucun defaut de ValidateMesh (soudure 5e-4), test
	// d'auto-intersection complet, volume positif.
	void expectStrictSingleShell(Mesh &m)
	{
		ASSERT_GT(m.GetNFaces(), 0u);
		MeshValidationOptions o;
		o.weldTolerance = 5e-4;
		o.expectedComponents = 1u;
		const MeshValidationReport r = ValidateMesh(m, o);
		EXPECT_EQ(r.components, 1u);
		for (size_t q = 0; q < kMeshDefectCount; ++q)
			if ((MeshDefect)q != MeshDefect::UnexpectedComponentCount)
				EXPECT_EQ(r.counts[q], 0u) << MeshDefectName((MeshDefect)q);
		EXPECT_FALSE(r.selfIntersectionTruncated);
		EXPECT_GT(r.signedVolume, 0.0);
	}
}

// Plaque 120 x 160 percee d'une ouverture 40 x 80, barre sur l'ouverture
// (Roll, Keel, Ogee de rayon 5), plaque d'epaisseur 20 : une seule coque
// fermee, stricte, plus grosse que la plaque seule (remplie sous la barre).
TEST(TEST_cgmesh_bar_fusion, bars_fused_on_a_plate_are_a_single_closed_shell)
{
	const Polygon2 plate = plateOf({rect(-60, -80, 60, 80), reversed(rect(-20, -40, 20, 40))});
	for (const Profile2D &p : {rollBarProfile(5.0), keelBarProfile(5.0), ogeeBarProfile(5.0)})
	{
		Mesh m;
		const FusedBarsResult r = fuseBarsOnPlate(plate, {{rect(-20, -40, 20, 40), 1.0}}, p, 20.0, m);
		EXPECT_FALSE(r.failed);
		EXPECT_GE(r.levels, 5);
		expectStrictSingleShell(m);
		MeshValidationOptions o;
		o.weldTolerance = 5e-4;
		EXPECT_GT(ValidateMesh(m, o).signedVolume, (120.0 * 160.0 - 40.0 * 80.0) * 20.0);
	}
}

// Ouvertures serrees (deux cercles dont les barres se recouvrent, la petite a
// l'echelle 0,6) et une ouverture a 5 du bord de la plaque, dont la barre
// deborde la plaque (noeud plaque x empreinte) : une seule coque stricte.
TEST(TEST_cgmesh_bar_fusion, close_curved_and_overhanging_bars_fuse_into_one_shell)
{
	const Ring big = circleRing(-30, 0, 20, 96), small = circleRing(8, 0, 10, 64), edge = rect(50, -20, 75, 20);
	const Polygon2 plate = plateOf({rect(-80, -80, 80, 80), reversed(big), reversed(small), reversed(edge)});
	for (const Profile2D &p : {rollBarProfile(5.0), ogeeBarProfile(5.0)})
	{
		Mesh m;
		const FusedBarsResult r = fuseBarsOnPlate(plate, {{big, 1.0}, {small, 0.6}, {edge, 1.0}}, p, 20.0, m);
		EXPECT_FALSE(r.failed);
		EXPECT_GT(r.nodeInsertions, 0);
		expectStrictSingleShell(m);
	}
}

// Profils releve (u_min = +1), enfonce (u_min = -1) et a contre-depouille
// (cercle pose sur la face) : une seule coque stricte.
TEST(TEST_cgmesh_bar_fusion, raised_sunk_and_undercut_profiles_fuse_into_one_shell)
{
	const Polygon2 plate = plateOf({rect(-60, -80, 60, 80), reversed(rect(-20, -40, 20, 40))});
	for (const Profile2D &p : {translatedInU(rollBarProfile(5.0), 1.0), translatedInU(rollBarProfile(5.0), -1.0),
	                           circleProfile(4.0, 4.0)})
	{
		Mesh m;
		const FusedBarsResult r = fuseBarsOnPlate(plate, {{rect(-20, -40, 20, 40), 1.0}}, p, 20.0, m);
		EXPECT_FALSE(r.failed);
		expectStrictSingleShell(m);
	}
}

// Plaque vide (aucun contour, ou contour d'aire nulle) : la fusion refuse de
// poser des barres sans plaque ; echec nomme, maillage vide.
TEST(TEST_cgmesh_bar_fusion, empty_plate_fails_with_an_empty_mesh)
{
	const std::vector<FusedBar> bars = {{rect(-20, -40, 20, 40), 1.0}};
	for (const Polygon2 &plate : {Polygon2(), plateOf({Ring{{0, 0}, {10, 0}, {20, 0}}})})
	{
		Mesh m;
		const FusedBarsResult r = fuseBarsOnPlate(plate, bars, rollBarProfile(5.0), 20.0, m);
		EXPECT_TRUE(r.failed);
		ASSERT_NE(r.failure, nullptr);
		EXPECT_NE(std::string(r.failure).find("plate"), std::string::npos) << r.failure;
		EXPECT_EQ(m.GetNFaces(), 0u);
	}
}

// Cote non finie (zTop infini ou NaN), ou cote d'un niveau sous le fond
// (profil enfonce u_min = -1 sur une plaque de 0,2) : echec, maillage vide.
TEST(TEST_cgmesh_bar_fusion, invalid_heights_fail_with_an_empty_mesh)
{
	const Polygon2 plate = plateOf({rect(-60, -80, 60, 80), reversed(rect(-20, -40, 20, 40))});
	const std::vector<FusedBar> bars = {{rect(-20, -40, 20, 40), 1.0}};
	struct Case { const char *name; Profile2D profile; double zTop; };
	const std::vector<Case> cases = {
		{"zTop infini", rollBarProfile(5.0), std::numeric_limits<double>::infinity()},
		{"zTop NaN", rollBarProfile(5.0), std::numeric_limits<double>::quiet_NaN()},
		{"enfonce sous le fond", translatedInU(rollBarProfile(5.0), -1.0), 0.2},
	};
	for (const Case &c : cases)
	{
		SCOPED_TRACE(c.name);
		Mesh m;
		EXPECT_TRUE(fuseBarsOnPlate(plate, bars, c.profile, c.zTop, m).failed);
		EXPECT_EQ(m.GetNFaces(), 0u);
		EXPECT_EQ(m.GetNVertices(), 0u);
	}
}

namespace
{
	// Le niveau, tessele seul (regle Positive), ne cree aucun sommet a une
	// position absente de ses contours : il ne se croise pas. Deux sommets
	// confondus (point commun de deux aretes) donnent une copie a la meme
	// position, qui n'est pas un croisement.
	bool tessellatesWithoutCreatedVertex(const std::vector<BarGridRing> &region)
	{
		Polygon2 poly;
		poly.alloc_contours((int)region.size());
		std::vector<std::pair<float, float>> inputs;
		for (size_t c = 0; c < region.size(); ++c)
		{
			std::vector<float> f;
			for (const BarGridPoint &q : region[c])
			{
				f.push_back((float)(q.x * kStep));
				f.push_back((float)(q.y * kStep));
				inputs.push_back({f[f.size() - 2], f.back()});
			}
			poly.add_contour((unsigned int)c, (unsigned int)region[c].size(), f.data());
		}
		std::sort(inputs.begin(), inputs.end());
		float *v = nullptr;
		unsigned int nv = 0, *f = nullptr, nf = 0;
		poly.tesselate(&v, &nv, &f, &nf, WindingRule::Positive);
		bool none = true;
		for (unsigned int i = 0; i < nv; ++i)
			none = none && std::binary_search(inputs.begin(), inputs.end(), std::make_pair(v[3 * i], v[3 * i + 1]));
		std::free(v);
		std::free(f);
		return none;
	}
}

// Trois barres serrees (un carre tourne, un rectangle et un 23-gone,
// echelles 0,52 a 0,59), Roll, Keel, Ogee, Roll a 3 segments : aucun niveau
// retire ; chaque niveau, tessele seul, ne cree aucun sommet ; l'assemblage ne
// saute aucun niveau et reste strict.
TEST(TEST_cgmesh_bar_fusion, nested_levels_never_cross_themselves)
{
	const std::vector<FusedBar> bars = {
		{{{-66.074424967930071, -13.019743525346101}, {-75.433284256618933, -8.956477515900275},
		  {-79.496550266064759, -18.31533680458913}, {-70.137690977375897, -22.378602814034959}}, 0.52145689101203385},
		{{{-64.923511262367484, -13.93800359470259}, {-46.339506469430702, -13.93800359470259},
		  {-46.339506469430702, 13.93800359470259}, {-64.923511262367484, 13.93800359470259}}, 0.59260721163553498},
		{{{-17.101843091443193, 22.572034271658218}, {-20.507520718478723, 25.251621374378757},
		  {-24.509850529032867, 26.913001291166665}, {-28.811998030497399, 27.432957073901171},
		  {-33.094892623703942, 26.7729259808163}, {-37.040891609613396, 24.981859498644809},
		  {-40.357338295185052, 22.192592834785412}, {-42.798267001484128, 18.611993137650625},
		  {-44.182645212870213, 14.505617106623262}, {-44.407799930513733, 10.178015866504269},
		  {-43.457032459021342, 5.95014780381491}, {-41.400856872281885, 2.1355745515554769},
		  {-38.391770307186391, -0.98279444266546356}, {-34.65294294861296, -3.1736840161354252},
		  {-30.461666517749908, -4.2746059118430413}, {-26.128788813660506, -4.2039097891662269},
		  {-21.975659554060506, -2.9668388561106802}, {-18.310297336832278, -0.65514100455817115},
		  {-15.404545309711276, 2.5597357111556125}, {-13.473909807618019, 6.439358592108567},
		  {-12.661577233673045, 10.695993757314135}, {-13.027794578711601, 15.013946049379147},
		  {-14.545401177585049, 19.072972700118488}}, 0.51755708830483471},
	};
	ASSERT_FALSE(tessellatesWithoutCreatedVertex({{{0, 0}, {1280, 1280}, {1280, 0}, {0, 1280}}}))
		<< "le detecteur doit voir un noeud papillon";
	std::vector<Ring> holes = {rect(-100, -100, 100, 100)};
	for (const FusedBar &b : bars)
		holes.push_back(reversed(b.outline));
	const Polygon2 plate = plateOf(holes);
	for (const Profile2D &p : {rollBarProfile(5.0), keelBarProfile(5.0), ogeeBarProfile(5.0), rollBarProfile(5.0, 3)})
	{
		BarFusionReport report;
		const std::vector<BarFusionLevel> levels = buildBarFusionLevels(bars, p, &report);
		EXPECT_EQ(report.droppedLevels, 0);
		for (size_t k = 0; k < levels.size(); ++k)
			EXPECT_TRUE(tessellatesWithoutCreatedVertex(levels[k].region)) << "niveau " << k;
		Mesh m;
		const FusedBarsResult r = fuseBarsOnPlate(plate, bars, p, 20.0, m);
		EXPECT_EQ(r.skippedLevels, 0);
		EXPECT_EQ(r.nesting.droppedLevels, 0);
		expectStrictSingleShell(m);
	}
}

namespace
{
	BarGridRing gridRect(std::int64_t x0, std::int64_t y0, std::int64_t x1, std::int64_t y1)
	{
		return {{x0 * 128, y0 * 128}, {x1 * 128, y0 * 128}, {x1 * 128, y1 * 128}, {x0 * 128, y1 * 128}};
	}

	// Noeud papillon (contour qui se croise en son centre).
	BarGridRing gridBowtie(std::int64_t x0, std::int64_t y0, std::int64_t x1, std::int64_t y1)
	{
		return {{x0 * 128, y0 * 128}, {x1 * 128, y1 * 128}, {x1 * 128, y0 * 128}, {x0 * 128, y1 * 128}};
	}
}

// Niveaux fournis : R_0 carre 40, R_1 en noeud papillon (se croise), R_2
// carre 20. L'anneau R_0 \ R_1 creerait un sommet au croisement : la garde
// saute R_1, l'anneau R_0 \ R_2 est pose ; la coque reste stricte.
// R_0 en noeud papillon : le fond et le plan zTop seraient refuses, echec.
TEST(TEST_cgmesh_bar_fusion, ring_guard_skips_a_level_that_crosses_itself)
{
	const Polygon2 plate = plateOf({rect(-60, -60, 60, 60)});
	{
		const std::vector<BarFusionLevel> levels = {
			{0.0, {gridRect(-20, -20, 20, 20)}},
			{1.0, {gridBowtie(-15, -15, 15, 15)}},
			{2.0, {gridRect(-10, -10, 10, 10)}},
		};
		Mesh m;
		const FusedBarsResult r = assembleFusedLevels(plate, levels, 20.0, m);
		EXPECT_FALSE(r.failed);
		EXPECT_EQ(r.skippedLevels, 1);
		EXPECT_EQ(r.levels, 2);
		expectStrictSingleShell(m);
	}
	{
		Mesh m;
		const FusedBarsResult r = assembleFusedLevels(plate, {{0.0, {gridBowtie(-20, -20, 20, 20)}}}, 20.0, m);
		EXPECT_TRUE(r.failed);
		EXPECT_EQ(m.GetNFaces(), 0u);
	}
}

// R_0 en papillon (deux carres de coin commun) hors de la plaque : le coin
// commun est sur le bord exterieur de P U R_0, le solide y est pince (une
// arete verticale a quatre parois) ; l'assemblage echoue, maillage vide.
// Temoin : le meme papillon sous la plaque, coin commun interieur, est pose.
TEST(TEST_cgmesh_bar_fusion, footprint_pinched_on_the_outer_boundary_fails)
{
	const std::vector<BarFusionLevel> levels = {{0.0, {{{0, 0}, {1280, 0}, {1280, 1280}, {0, 1280}},
	                                                   {{1280, 1280}, {2560, 1280}, {2560, 2560}, {1280, 2560}}}}};
	{
		SCOPED_TRACE("papillon hors de la plaque");
		Mesh m;
		const FusedBarsResult r = assembleFusedLevels(plateOf({rect(100, 100, 120, 120)}), levels, 20.0, m);
		EXPECT_TRUE(r.failed);
		EXPECT_EQ(m.GetNFaces(), 0u);
	}
	{
		SCOPED_TRACE("papillon sous la plaque");
		Mesh m;
		const FusedBarsResult r = assembleFusedLevels(plateOf({rect(-30, -30, 50, 50)}), levels, 20.0, m);
		EXPECT_FALSE(r.failed);
		expectStrictSingleShell(m);
	}
}

// Niveaux fournis qui se touchent : un sommet de R_1 (losange) pose sur une
// arete de R_0 (carre). L'anneau R_0 \ R_1 aurait ce sommet sur son bord
// exterieur sans qu'il soit un sommet de R_0 : la garde saute R_1, le capot
// ferme R_0, la coque reste stricte. Temoin : le meme losange decolle de
// R_0 est pose.
TEST(TEST_cgmesh_bar_fusion, ring_guard_skips_a_level_touching_the_one_below)
{
	const Polygon2 plate = plateOf({rect(-20, -20, 30, 30)});
	const BarGridRing below = {{0, 0}, {1280, 0}, {1280, 1280}, {0, 1280}};
	for (const bool touching : {true, false})
	{
		SCOPED_TRACE(touching ? "sommet sur l'arete" : "temoin decolle");
		const BarGridRing diamond = {{640, touching ? 0 : 100}, {1000, 640}, {640, 1000}, {280, 640}};
		Mesh m;
		const FusedBarsResult r = assembleFusedLevels(plate, {{0.0, {below}}, {1.0, {diamond}}}, 20.0, m);
		EXPECT_FALSE(r.failed);
		EXPECT_EQ(r.skippedLevels, touching ? 1 : 0);
		expectStrictSingleShell(m);
	}
}
