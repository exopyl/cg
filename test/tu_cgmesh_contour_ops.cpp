#include <gtest/gtest.h>

#include "../src/cgmesh/contour_ops.h"

#include <cmath>
#include <vector>

// ===========================================================================
//  Operations sur contours fermes
// ===========================================================================
//
// Le decalage est la brique de la PLAQUE SILHOUETTE et, plus tard, des bandes
// d'arete profilee. Ce qui se teste ici n'est pas « Clipper2 sait dilater » --
// c'est son affaire -- mais les trois proprietes dont le reste du chantier
// depend :
//
//   1. le sens du decalage (dilater grandit, retrecir rapetisse, jusqu'a rien) ;
//   2. le COMPTE DES MORCEAUX, seul garde-fou contre une piece qui sort en
//      lettres libres ;
//   3. le sort des CONTRE-FORMES, qui se referment en dilatant.
//
// ===========================================================================

namespace {

ExtrudeContour square (float cx, float cy, float half, bool clockwise = false)
{
	ExtrudeContour c;
	c.pts.push_back (Vector2f (cx - half, cy - half));
	c.pts.push_back (Vector2f (cx + half, cy - half));
	c.pts.push_back (Vector2f (cx + half, cy + half));
	c.pts.push_back (Vector2f (cx - half, cy + half));
	if (clockwise) std::reverse (c.pts.begin (), c.pts.end ());
	return c;
}

// Emprise du jeu, pour lire l'effet d'un decalage sans dependre du nombre de
// points que l'arrondi des coins produit.
void extent (const std::vector<ExtrudeContour>& in, float& w, float& h)
{
	float x0 = 0.f, y0 = 0.f, x1 = 0.f, y1 = 0.f;
	ASSERT_TRUE (contoursBBox (in, x0, y0, x1, y1));
	w = x1 - x0;
	h = y1 - y0;
}

float totalAbsArea (const std::vector<ExtrudeContour>& in)
{
	float a = 0.f;
	for (const ExtrudeContour& c : in) a += std::fabs (contourSignedArea (c.pts));
	return a;
}

}  // namespace

// --- mesures elementaires ---------------------------------------------------

TEST(TEST_cgmesh_contour_ops, signed_area_reports_the_winding)
{
	const ExtrudeContour ccw = square (0.f, 0.f, 5.f);
	const ExtrudeContour cw  = square (0.f, 0.f, 5.f, true);
	EXPECT_NEAR (contourSignedArea (ccw.pts), 100.f, 1e-4f);
	EXPECT_NEAR (contourSignedArea (cw.pts), -100.f, 1e-4f);
}

TEST(TEST_cgmesh_contour_ops, an_empty_set_has_no_bbox)
{
	float x0 = 42.f, y0 = 42.f, x1 = 42.f, y1 = 42.f;
	EXPECT_FALSE (contoursBBox ({}, x0, y0, x1, y1));
	// Et l'echec ne touche a rien : une emprise vide n'a pas de valeur honnete.
	EXPECT_FLOAT_EQ (x0, 42.f);
	EXPECT_FLOAT_EQ (y1, 42.f);
}

TEST(TEST_cgmesh_contour_ops, a_rounded_rect_cuts_its_corners_and_keeps_its_bbox)
{
	const std::vector<Vector2f> sharp = roundedRectContour (0.f, 0.f, 10.f, 6.f, 0.f);
	EXPECT_EQ (sharp.size (), 4u);
	EXPECT_GT (contourSignedArea (sharp), 0.f) << "le trace doit etre trigonometrique";

	const std::vector<Vector2f> round = roundedRectContour (0.f, 0.f, 10.f, 6.f, 2.f);
	EXPECT_GT (round.size (), 4u);
	// Les coins mangent de la matiere sans deborder de l'emprise.
	EXPECT_LT (contourSignedArea (round), contourSignedArea (sharp));
	for (const Vector2f& p : round)
	{
		EXPECT_GE (p.x, -1e-4f); EXPECT_LE (p.x, 10.f + 1e-4f);
		EXPECT_GE (p.y, -1e-4f); EXPECT_LE (p.y, 6.f + 1e-4f);
	}

	// Rayon borne a la moitie du plus PETIT cote : au-dela, la forme serait
	// repliee. A 6 de hauteur, un rayon de 50 vaut donc 3 -- un stade.
	const std::vector<Vector2f> clamped = roundedRectContour (0.f, 0.f, 10.f, 6.f, 50.f);
	float ymin = clamped[0].y, ymax = clamped[0].y;
	for (const Vector2f& p : clamped) { ymin = std::min (ymin, p.y); ymax = std::max (ymax, p.y); }
	EXPECT_NEAR (ymax - ymin, 6.f, 1e-3f);
}

// --- decalage ---------------------------------------------------------------

TEST(TEST_cgmesh_contour_ops, a_positive_delta_dilates_and_a_negative_one_erodes)
{
	const std::vector<ExtrudeContour> in { square (0.f, 0.f, 5.f) };

	float w = 0.f, h = 0.f;
	extent (offsetContours (in, 2.f, StrokeJoin::Miter), w, h);
	EXPECT_NEAR (w, 14.f, 0.05f);   // 10 + 2 x 2
	EXPECT_NEAR (h, 14.f, 0.05f);

	extent (offsetContours (in, -2.f, StrokeJoin::Miter), w, h);
	EXPECT_NEAR (w, 6.f, 0.05f);
	EXPECT_NEAR (h, 6.f, 0.05f);
}

TEST(TEST_cgmesh_contour_ops, eroding_past_the_matter_returns_nothing)
{
	const std::vector<ExtrudeContour> in { square (0.f, 0.f, 5.f) };
	// La demi-epaisseur vaut 5 : au-dela, il ne reste rien. Ce n'est pas une
	// panne, c'est le resultat -- et l'appelant doit pouvoir le distinguer.
	const std::vector<ExtrudeContour> gone = offsetContours (in, -6.f, StrokeJoin::Round);
	EXPECT_TRUE (gone.empty ());
}

TEST(TEST_cgmesh_contour_ops, a_round_join_bevels_less_than_a_miter)
{
	const std::vector<ExtrudeContour> in { square (0.f, 0.f, 5.f) };
	const float miter = totalAbsArea (offsetContours (in, 3.f, StrokeJoin::Miter));
	const float round = totalAbsArea (offsetContours (in, 3.f, StrokeJoin::Round));
	const float bevel = totalAbsArea (offsetContours (in, 3.f, StrokeJoin::Bevel));

	// L'onglet remplit le coin, l'arrondi le contourne, le chanfrein le coupe
	// droit : trois aires strictement ordonnees.
	EXPECT_GT (miter, round);
	EXPECT_GT (round, bevel);
}

// --- connexite : LE garde-fou de la plaque silhouette -----------------------

TEST(TEST_cgmesh_contour_ops, the_piece_count_straddles_the_connectivity_threshold)
{
	// Deux carres de 10, separes de 6 : il faut donc dilater de plus de 3 pour
	// que les halos se touchent. C'est exactement la regle « moitie de l'espace
	// qui les separe ».
	const std::vector<ExtrudeContour> in {
		square (0.f, 0.f, 5.f),
		square (16.f, 0.f, 5.f),
	};

	int pieces = -1;
	std::vector<ExtrudeContour> apart = offsetContours (in, 2.5f, StrokeJoin::Miter,
	                                                   2.f, &pieces);
	EXPECT_FALSE (apart.empty ());
	EXPECT_EQ (pieces, 2) << "sous le seuil, la piece sort en morceaux -- et c'est "
	                         "ce que le compte doit dire AVANT l'impression";

	pieces = -1;
	std::vector<ExtrudeContour> joined = offsetContours (in, 3.5f, StrokeJoin::Miter,
	                                                    2.f, &pieces);
	EXPECT_FALSE (joined.empty ());
	EXPECT_EQ (pieces, 1) << "au-dessus du seuil, les halos ont fusionne";
}

TEST(TEST_cgmesh_contour_ops, the_piece_count_is_optional_and_zero_on_failure)
{
	int pieces = 7;
	EXPECT_TRUE (offsetContours ({}, 1.f, StrokeJoin::Round, 2.f, &pieces).empty ());
	EXPECT_EQ (pieces, 0);

	// Et l'omettre reste licite : le compte demande un PolyTree de plus.
	const std::vector<ExtrudeContour> in { square (0.f, 0.f, 5.f) };
	EXPECT_FALSE (offsetContours (in, 1.f, StrokeJoin::Round).empty ());
}

// --- contre-formes ----------------------------------------------------------

TEST(TEST_cgmesh_contour_ops, dilating_closes_a_counter)
{
	// Un « o » : enveloppe de 20 et contre-forme de 4, tracees en sens OPPOSES
	// comme le veut la convention (cf. l'en-tete de contour_ops.h).
	const std::vector<ExtrudeContour> ring {
		square (0.f, 0.f, 10.f),
		square (0.f, 0.f, 2.f, true),
	};

	// Dilater de 1 : le trou perd 1 de chaque cote, il reste un trou.
	const std::vector<ExtrudeContour> still = offsetContours (ring, 1.f, StrokeJoin::Miter);
	EXPECT_EQ (still.size (), 2u) << "la contre-forme devrait survivre a ce decalage";

	// Dilater de 3 : le trou faisait 4 de cote, il est BOUCHE. C'est voulu pour
	// une plaque de fond -- sinon on verrait a travers -- et il faut le savoir.
	const std::vector<ExtrudeContour> closed = offsetContours (ring, 3.f, StrokeJoin::Miter);
	EXPECT_EQ (closed.size (), 1u) << "la contre-forme aurait du se refermer";
}

// --- booleens 2D ------------------------------------------------------------
//
// Ce qui se garde ici n'est pas « Clipper2 sait intersecter » mais le CONTRAT
// que le reste du depot lit : l'ordre de la difference, le sort d'un resultat
// vide, et l'accord des trois operations entre elles.

TEST(TEST_cgmesh_contour_ops, the_union_merges_what_overlaps)
{
	const std::vector<ExtrudeContour> a { square (0.f, 0.f, 5.f) };
	const std::vector<ExtrudeContour> b { square (6.f, 0.f, 5.f) };   // recouvrement de 4

	const std::vector<ExtrudeContour> merged = unionContours (a, b);
	// UN seul contour : les deux carres se recouvrent, ils ne font plus qu'une
	// piece. C'est ce que « fondre » veut dire.
	EXPECT_EQ (merged.size (), 1u);
	// Et l'aire est celle des deux moins le recouvrement, pas leur somme.
	EXPECT_LT (totalAbsArea (merged), 200.f);
	EXPECT_GT (totalAbsArea (merged), 100.f);

	// Deux formes DISJOINTES restent deux morceaux -- l'union ne rapproche rien.
	const std::vector<ExtrudeContour> far { square (40.f, 0.f, 5.f) };
	EXPECT_EQ (unionContours (a, far).size (), 2u);
}

TEST(TEST_cgmesh_contour_ops, the_difference_is_not_symmetric)
{
	const std::vector<ExtrudeContour> big { square (0.f, 0.f, 10.f) };    // aire 400
	const std::vector<ExtrudeContour> small { square (0.f, 0.f, 3.f) };   // aire 36

	// A moins B : la matiere PERCEE de l'outil -- deux contours, l'enveloppe et
	// le trou.
	const std::vector<ExtrudeContour> holed = differenceContours (big, small);
	EXPECT_EQ (holed.size (), 2u);
	EXPECT_NEAR (totalAbsArea (holed), 400.f + 36.f, 1.f)
		<< "les aires sont prises en valeur absolue : enveloppe PLUS trou";

	// B moins A : rien du tout, le petit etant entierement dans le grand. C'est
	// la dissymetrie, et c'est pourquoi les ports sont nommes.
	EXPECT_TRUE (differenceContours (small, big).empty ());

	// Retirer RIEN rend la matiere telle quelle -- normalisee, pas copiee.
	EXPECT_EQ (differenceContours (big, {}).size (), 1u);
}

TEST(TEST_cgmesh_contour_ops, the_intersection_of_disjoint_regions_is_empty)
{
	const std::vector<ExtrudeContour> a { square (0.f, 0.f, 5.f) };
	const std::vector<ExtrudeContour> far { square (40.f, 0.f, 5.f) };
	EXPECT_TRUE (intersectionContours (a, far).empty ());

	// Intersecter avec RIEN rend RIEN, et non « tout » : le raccourci symetrique
	// a celui de la difference serait faux ici.
	EXPECT_TRUE (intersectionContours (a, {}).empty ());
	EXPECT_TRUE (intersectionContours ({}, a).empty ());

	// Et deux formes qui se touchent rendent leur part commune.
	const std::vector<ExtrudeContour> b { square (6.f, 0.f, 5.f) };
	const std::vector<ExtrudeContour> common = intersectionContours (a, b);
	ASSERT_EQ (common.size (), 1u);
	EXPECT_NEAR (totalAbsArea (common), 4.f * 10.f, 0.1f);   // 4 de large, 10 de haut
}

TEST(TEST_cgmesh_contour_ops, the_three_operations_agree_on_areas)
{
	// |A ∪ B| = |A| + |B| - |A ∩ B|. L'invariant qui lie les trois : s'il cesse
	// de tenir, l'une d'elles a change de sens sans que rien ne le dise.
	const std::vector<ExtrudeContour> a { square (0.f, 0.f, 5.f) };
	const std::vector<ExtrudeContour> b { square (6.f, 2.f, 5.f) };

	const float unionArea = totalAbsArea (unionContours (a, b));
	const float interArea = totalAbsArea (intersectionContours (a, b));
	EXPECT_NEAR (unionArea, 100.f + 100.f - interArea, 0.1f);

	// Et la difference complete le tableau : |A - B| = |A| - |A ∩ B|.
	EXPECT_NEAR (totalAbsArea (differenceContours (a, b)), 100.f - interArea, 0.1f);
}
