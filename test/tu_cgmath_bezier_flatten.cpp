#include <gtest/gtest.h>

#include "../src/cgmath/bezier_flatten.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

// ===========================================================================
//  Aplatissement adaptatif de Beziers 2D
// ===========================================================================
// L'oracle central n'est ni un compte de points ni une liste de coordonnees --
// les deux dependent de la strategie de subdivision -- mais la DISTANCE de la
// polyligne produite a la courbe exacte : c'est ce que la tolerance promet.
// On l'evalue en echantillonnant la courbe analytiquement (Bernstein) et en
// mesurant, pour chaque echantillon, sa distance au segment le plus proche.

namespace {

Vector2f evalQuadratic (const Vector2f& p0, const Vector2f& c,
                        const Vector2f& p1, float t)
{
	const float u = 1.f - t;
	return Vector2f (u*u*p0.x + 2.f*u*t*c.x + t*t*p1.x,
	                 u*u*p0.y + 2.f*u*t*c.y + t*t*p1.y);
}

Vector2f evalCubic (const Vector2f& p0, const Vector2f& c0,
                    const Vector2f& c1, const Vector2f& p1, float t)
{
	const float u = 1.f - t;
	return Vector2f (u*u*u*p0.x + 3.f*u*u*t*c0.x + 3.f*u*t*t*c1.x + t*t*t*p1.x,
	                 u*u*u*p0.y + 3.f*u*u*t*c0.y + 3.f*u*t*t*c1.y + t*t*t*p1.y);
}

float distToSegment (const Vector2f& p, const Vector2f& a, const Vector2f& b)
{
	const float vx = b.x - a.x, vy = b.y - a.y;
	const float len2 = vx*vx + vy*vy;
	float t = 0.f;
	if (len2 > 0.f)
	{
		t = ((p.x - a.x) * vx + (p.y - a.y) * vy) / len2;
		t = (t < 0.f) ? 0.f : (t > 1.f ? 1.f : t);
	}
	const float dx = p.x - (a.x + t*vx), dy = p.y - (a.y + t*vy);
	return std::sqrt (dx*dx + dy*dy);
}

// Ecart maximal entre la courbe exacte et la polyligne `poly` (qui contient le
// point de depart en tete).
float maxDeviation (const std::vector<Vector2f>& poly,
                    const std::function<Vector2f(float)>& eval, int nSamples)
{
	float worst = 0.f;
	for (int i = 0; i <= nSamples; i++)
	{
		const Vector2f q = eval ((float)i / (float)nSamples);
		float best = 1e30f;
		for (size_t s = 0; s + 1 < poly.size(); s++)
			best = std::min (best, distToSegment (q, poly[s], poly[s+1]));
		worst = std::max (worst, best);
	}
	return worst;
}

}  // namespace

// ---------------------------------------------------------------------------
// Le critere de platitude
// ---------------------------------------------------------------------------

// Temoin : un segment dont les points de controle sont sur la corde EST la
// corde, et doit etre declare plat pour toute tolerance positive, si petite
// soit-elle. Sans ce cas, un critere systematiquement faux passerait inapercu.
TEST(TEST_cgmath_bezier_flatten, a_degenerate_curve_is_already_flat)
{
	const Vector2f p0 (0.f, 0.f), p1 (3.f, 0.f);
	EXPECT_TRUE (flatEnoughQuadratic (p0, Vector2f (1.5f, 0.f), p1, 1e-6f));
	EXPECT_TRUE (flatEnoughCubic (p0, Vector2f (1.f, 0.f), Vector2f (2.f, 0.f),
	                              p1, 1e-6f));
}

TEST(TEST_cgmath_bezier_flatten, a_bulging_curve_is_not_flat)
{
	const Vector2f p0 (0.f, 0.f), p1 (3.f, 0.f);
	EXPECT_FALSE (flatEnoughQuadratic (p0, Vector2f (1.5f, 1.f), p1, 0.01f));
	EXPECT_FALSE (flatEnoughCubic (p0, Vector2f (1.f, 1.f), Vector2f (2.f, 1.f),
	                              p1, 0.01f));
}

// ---------------------------------------------------------------------------
// La promesse de la tolerance
// ---------------------------------------------------------------------------

TEST(TEST_cgmath_bezier_flatten, quadratic_stays_within_tolerance)
{
	const Vector2f p0 (0.f, 0.f), c (1.f, 2.f), p1 (2.f, 0.f);
	for (float tol : { 0.5f, 0.1f, 0.01f })
	{
		std::vector<Vector2f> poly { p0 };
		flattenQuadratic (poly, p0, c, p1, tol);

		ASSERT_GE (poly.size(), 2u);
		EXPECT_FLOAT_EQ (poly.back().x, p1.x);
		EXPECT_FLOAT_EQ (poly.back().y, p1.y);

		const float dev = maxDeviation (poly,
			[&](float t) { return evalQuadratic (p0, c, p1, t); }, 400);
		EXPECT_LE (dev, tol) << "tol = " << tol << ", ecart = " << dev;
	}
}

TEST(TEST_cgmath_bezier_flatten, cubic_stays_within_tolerance)
{
	const Vector2f p0 (0.f, 0.f), c0 (0.f, 3.f), c1 (3.f, 3.f), p1 (3.f, 0.f);
	for (float tol : { 0.5f, 0.1f, 0.01f })
	{
		std::vector<Vector2f> poly { p0 };
		flattenCubic (poly, p0, c0, c1, p1, tol);

		ASSERT_GE (poly.size(), 2u);
		EXPECT_FLOAT_EQ (poly.back().x, p1.x);
		EXPECT_FLOAT_EQ (poly.back().y, p1.y);

		const float dev = maxDeviation (poly,
			[&](float t) { return evalCubic (p0, c0, c1, p1, t); }, 400);
		EXPECT_LE (dev, tol) << "tol = " << tol << ", ecart = " << dev;
	}
}

// Resserrer la tolerance doit RAFFINER, jamais degrossir. C'est ce qui
// distingue un aplatissement adaptatif d'un echantillonnage uniforme.
TEST(TEST_cgmath_bezier_flatten, a_tighter_tolerance_never_emits_fewer_points)
{
	const Vector2f p0 (0.f, 0.f), c0 (0.f, 3.f), c1 (3.f, 3.f), p1 (3.f, 0.f);
	size_t previous = 0;
	for (float tol : { 1.f, 0.5f, 0.1f, 0.01f, 0.001f })
	{
		std::vector<Vector2f> poly { p0 };
		flattenCubic (poly, p0, c0, c1, p1, tol);
		EXPECT_GE (poly.size(), previous) << "tol = " << tol;
		previous = poly.size();
	}
}

// ---------------------------------------------------------------------------
// La convention d'accumulation
// ---------------------------------------------------------------------------

// Le contrat qui permet d'enchainer les segments d'un contour dans UN seul
// vecteur : le point de depart n'est jamais emis, donc aucune jointure n'est
// dupliquee. C'est ce qui interdirait a glutess d'ecarter un sommet et
// d'emporter trois parois avec lui (cf. tu_cgmesh_extrude_contours.cpp).
TEST(TEST_cgmath_bezier_flatten, chaining_two_segments_leaves_no_duplicate_joint)
{
	const Vector2f a (0.f, 0.f), ca (1.f, 1.f), b (2.f, 0.f);
	const Vector2f cb (3.f, -1.f), c (4.f, 0.f);

	std::vector<Vector2f> contour { a };
	flattenQuadratic (contour, a, ca, b, 0.05f);
	const size_t afterFirst = contour.size();
	flattenQuadratic (contour, b, cb, c, 0.05f);

	// Le point de jonction b apparait UNE fois, en fin de premier segment.
	EXPECT_FLOAT_EQ (contour[afterFirst - 1].x, b.x);
	EXPECT_FLOAT_EQ (contour[afterFirst - 1].y, b.y);
	EXPECT_FALSE (contour[afterFirst].x == b.x && contour[afterFirst].y == b.y)
		<< "le point de depart du second segment a ete re-emis";
}

// L'appelant fournit le vecteur : ce qui s'y trouvait deja doit survivre.
TEST(TEST_cgmath_bezier_flatten, output_is_appended_not_replaced)
{
	std::vector<Vector2f> out { Vector2f (-1.f, -1.f), Vector2f (0.f, 0.f) };
	flattenCubic (out, Vector2f (0.f, 0.f), Vector2f (1.f, 1.f),
	              Vector2f (2.f, 1.f), Vector2f (3.f, 0.f), 0.1f);
	ASSERT_GT (out.size(), 2u);
	EXPECT_FLOAT_EQ (out[0].x, -1.f);
	EXPECT_FLOAT_EQ (out[1].x,  0.f);
}

// ---------------------------------------------------------------------------
// Garde-fou
// ---------------------------------------------------------------------------

// Une tolerance nulle ou negative ne peut jamais etre satisfaite : seule la
// borne de profondeur arrete la recursion. Elle doit donc terminer, et sur un
// nombre de points borne -- pas exploser en pile ni en memoire.
TEST(TEST_cgmath_bezier_flatten, an_impossible_tolerance_still_terminates)
{
	const Vector2f p0 (0.f, 0.f), c0 (0.f, 3.f), c1 (3.f, 3.f), p1 (3.f, 0.f);
	std::vector<Vector2f> poly { p0 };
	flattenCubic (poly, p0, c0, c1, p1, 0.f);
	// 2^13 segments au pire, garde-fou de profondeur a 12 inclus.
	EXPECT_LE (poly.size(), 8193u);
	EXPECT_FLOAT_EQ (poly.back().x, p1.x);
	EXPECT_FLOAT_EQ (poly.back().y, p1.y);
}
