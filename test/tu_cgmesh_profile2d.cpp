#include <gtest/gtest.h>

#include "../src/cgmesh/profile2d.h"

#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/polygon2.h"

#include <cmath>
#include <vector>

// ===========================================================================
//  Profile2D -- le profil devenu valeur, et l'extrusion qui le consomme
// ===========================================================================
// La fonction vient du gothique, ou elle ne connaissait qu'un chanfrein decrit
// par (chamW, chamD). Deux choses doivent etre etablies, et elles ne sont pas
// de meme nature :
//
//   1. NON-REGRESSION : sur un chanfrein, la generalisation rend EXACTEMENT ce
//      que rendait la forme heritee. Les cinq cas du gothique le disent deja de
//      l'exterieur ; ici on le dit sommet par sommet ;
//   2. GENERALISATION REELLE : un profil a plus de deux points produit plus de
//      niveaux. Sans ce cas, la boucle sur les points du profil ne serait jamais
//      parcourue qu'avec deux elements -- le test serait vert et le profil
//      resterait un chanfrein deguise.

namespace {

// Carre de 200 avec un trou carre de 100, matiere a GAUCHE : contour exterieur
// dans le sens trigonometrique, trou dans le sens horaire. Les deux sont assez
// GRANDS pour que le garde-fou ne les ramene pas a une paroi verticale -- le
// rayon caracteristique vaut 112,8 et 56,4 contre le seuil de max (24, 4 x
// largeur).
Polygon2 makeFramePolygon ()
{
	Polygon2 poly;
	poly.alloc_contours (2);
	float outer[8] = { -100.f, -100.f, 100.f, -100.f, 100.f, 100.f, -100.f, 100.f };
	float hole[8]  = { -50.f, -50.f, -50.f, 50.f, 50.f, 50.f, 50.f, -50.f };
	poly.add_contour (0, 4, outer);
	poly.add_contour (1, 4, hole);
	return poly;
}

void meshZRange (Mesh &m, float &zMin, float &zMax)
{
	zMin = 0.f; zMax = 0.f;
	const unsigned int n = m.GetNVertices ();
	for (unsigned int i = 0; i < n; ++i)
	{
		float v[3];
		m.GetVertex (i, v);
		if (i == 0) { zMin = zMax = v[2]; continue; }
		zMin = std::min (zMin, v[2]);
		zMax = std::max (zMax, v[2]);
	}
}

double signedVolume (Mesh &m)
{
	double vol = 0.;
	std::vector<unsigned int> tris = m.GetTriangles ();
	for (size_t t = 0; t + 2 < tris.size (); t += 3)
	{
		float a[3], b[3], c[3];
		m.GetVertex (tris[t], a);
		m.GetVertex (tris[t+1], b);
		m.GetVertex (tris[t+2], c);
		vol += ( (double)a[0]*((double)b[1]*c[2] - (double)b[2]*c[1])
		       - (double)a[1]*((double)b[0]*c[2] - (double)b[2]*c[0])
		       + (double)a[2]*((double)b[0]*c[1] - (double)b[1]*c[0]) ) / 6.0;
	}
	return vol;
}

bool sameGeometry (Mesh &a, Mesh &b)
{
	if (a.GetNVertices () != b.GetNVertices ()) return false;
	if (a.GetNFaces () != b.GetNFaces ()) return false;
	for (unsigned int i = 0; i < a.GetNVertices (); ++i)
	{
		float p[3], q[3];
		a.GetVertex (i, p);
		b.GetVertex (i, q);
		for (int k = 0; k < 3; ++k)
			if (p[k] != q[k]) return false;
	}
	return a.GetTriangles () == b.GetTriangles ();
}

}  // namespace

// ---------------------------------------------------------------------------
//  Les fabriques
// ---------------------------------------------------------------------------

TEST (TEST_cgmesh_profile2d, a_chamfer_is_the_two_point_profile_it_always_was)
{
	const Profile2D p = chamferSplayProfile (2.0, 4.0);
	ASSERT_EQ (p.points.size (), 2u);
	EXPECT_EQ (p.points[0].x, 0.0);
	EXPECT_EQ (p.points[0].y, 0.0);
	EXPECT_EQ (p.points[1].x, 4.0);      // u = enfoncement
	EXPECT_EQ (p.points[1].y, 2.0);      // v = rentrant
}

TEST (TEST_cgmesh_profile2d, a_cavetto_is_monotone_and_reaches_the_same_corner)
{
	const int segments = 6;
	const Profile2D p = cavettoSplayProfile (2.0, 4.0, segments);
	ASSERT_EQ (p.points.size (), (std::size_t)(segments + 1));
	EXPECT_EQ (p.points[0].x, 0.0);
	EXPECT_EQ (p.points[0].y, 0.0);
	// Il aboutit au meme coin que le chanfrein : c'est ce qui permet de
	// comparer les deux a garde-fou egal.
	EXPECT_NEAR (p.points.back ().x, 4.0, 1e-12);
	EXPECT_NEAR (p.points.back ().y, 2.0, 1e-12);
	for (std::size_t i = 1; i < p.points.size (); ++i)
	{
		EXPECT_GE (p.points[i].x, p.points[i-1].x) << "u doit croitre";
		EXPECT_GE (p.points[i].y, p.points[i-1].y) << "v doit croitre";
	}
	// Concave : la corde est en dessous de l'arc, donc le point median est plus
	// rentre que le chanfrein a la meme profondeur.
	const Profile2D straight = chamferSplayProfile (2.0, 4.0);
	const Vector2d mid = p.points[segments / 2];
	EXPECT_GT (mid.y, 2.0 * (mid.x / 4.0)) << "un cavet doit creuser plus qu'une droite";
	EXPECT_EQ (straight.points.size (), 2u);
}

TEST (TEST_cgmesh_profile2d, bar_profiles_are_closed_sections_relative_to_the_front_face)
{
	const Profile2D roll = rollBarProfile (5.0, 12);
	EXPECT_EQ (roll.points.size (), 13u);
	const Profile2D keel = keelBarProfile (5.0);
	ASSERT_EQ (keel.points.size (), 3u);
	// u RELATIF : la section part de la face de reference, u = 0.
	EXPECT_EQ (keel.points[0].x, 0.0);
	EXPECT_EQ (keel.points[2].x, 0.0);
	EXPECT_NEAR (keel.points[1].x, 6.0, 1e-12);
	// Symetrique en v : c'est une barre.
	EXPECT_NEAR (keel.points[0].y, -keel.points[2].y, 1e-12);

	const Profile2D ogee = ogeeBarProfile (5.0, 6);
	EXPECT_EQ (ogee.points.size (), 13u);

	// Nombre d'echantillons degenere : les fabriques le ramenent a leur minimum
	// plutot que de rendre un profil a moins de deux points, que l'extrusion
	// refuserait plus loin sans pouvoir dire d'ou il vient.
	EXPECT_EQ (cavettoSplayProfile (2.0, 4.0, 0).points.size (), 2u);
	EXPECT_EQ (rollBarProfile (5.0, 0).points.size (), 3u);
	EXPECT_EQ (ogeeBarProfile (5.0, 0).points.size (), 3u);   // deux flancs d'un point, plus le sommet

	// Le PLACEMENT appartient au consommateur.
	const Profile2D placed = translatedInU (keel, 20.0);
	ASSERT_EQ (placed.points.size (), 3u);
	EXPECT_EQ (placed.points[0].x, 20.0);
	EXPECT_EQ (placed.points[1].x, 26.0);
	EXPECT_EQ (placed.points[0].y, keel.points[0].y);
}

// ---------------------------------------------------------------------------
//  Non-regression : la forme heritee n'est plus qu'un emballage
// ---------------------------------------------------------------------------

TEST (TEST_cgmesh_profile2d, the_legacy_chamfer_call_and_the_profile_call_agree_vertex_for_vertex)
{
	Polygon2 a = makeFramePolygon ();
	Polygon2 b = makeFramePolygon ();

	Mesh legacy;
	ASSERT_NO_THROW (extrudeProfiledToMesh (a, legacy, 0.0, 10.0, 2.0, 4.0));

	Mesh viaProfile;
	ASSERT_NO_THROW (extrudeProfiledToMesh (b, chamferSplayProfile (2.0, 4.0), viaProfile,
	                                        0.0, 10.0));

	ASSERT_GT (legacy.GetNVertices (), 0u);
	EXPECT_TRUE (sameGeometry (legacy, viaProfile))
		<< "l'ancienne signature doit etre EXACTEMENT le nouvel appel, emballe";
}

// ---------------------------------------------------------------------------
//  Generalisation : un profil a plus de deux points
// ---------------------------------------------------------------------------

TEST (TEST_cgmesh_profile2d, a_multi_point_profile_adds_one_ring_per_point)
{
	Polygon2 a = makeFramePolygon ();
	Polygon2 b = makeFramePolygon ();

	Mesh chamfer;
	ASSERT_NO_THROW (extrudeProfiledToMesh (a, chamferSplayProfile (2.0, 4.0), chamfer,
	                                        0.0, 10.0));
	Mesh cavetto;
	ASSERT_NO_THROW (extrudeProfiledToMesh (b, cavettoSplayProfile (2.0, 4.0, 6), cavetto,
	                                        0.0, 10.0));

	// 8 points de contour, 5 niveaux de plus (7 points de profil contre 2) :
	// la difference est exactement 8 x 5 = 40 sommets.
	EXPECT_EQ (cavetto.GetNVertices (), chamfer.GetNVertices () + 40u);
	EXPECT_GT (cavetto.GetNFaces (), chamfer.GetNFaces ());

	float zMin = 0.f, zMax = 0.f;
	meshZRange (cavetto, zMin, zMax);
	EXPECT_NEAR (zMin, 0.f, 1e-4f);
	EXPECT_NEAR (zMax, 10.f, 1e-4f);

	// Un cavet creuse davantage qu'un chanfrein : a garde-fou egal, il enleve
	// plus de matiere.
	const double vChamfer = signedVolume (chamfer);
	const double vCavetto = signedVolume (cavetto);
	EXPECT_GT (vChamfer, 0.0);
	EXPECT_GT (vCavetto, 0.0);
	EXPECT_LT (vCavetto, vChamfer);
}

TEST (TEST_cgmesh_profile2d, a_profile_deeper_than_the_piece_is_compressed_not_truncated)
{
	Polygon2 poly = makeFramePolygon ();
	Mesh mesh;
	// Profondeur 500 pour une piece de 10.
	ASSERT_NO_THROW (extrudeProfiledToMesh (poly, cavettoSplayProfile (2.0, 500.0, 6), mesh,
	                                        0.0, 10.0));
	float zMin = 0.f, zMax = 0.f;
	meshZRange (mesh, zMin, zMax);
	EXPECT_NEAR (zMin, 0.f, 1e-4f);
	EXPECT_NEAR (zMax, 10.f, 1e-4f);
	// Tous les niveaux tiennent dans la piece : rien sous le fond.
	EXPECT_GE (zMin, -1e-4f);
}

// ---------------------------------------------------------------------------
//  Refus -- un profil mal forme est nomme, pas devine
// ---------------------------------------------------------------------------

TEST (TEST_cgmesh_profile2d, a_malformed_profile_is_refused)
{
	Polygon2 poly = makeFramePolygon ();
	Mesh mesh;

	Profile2D single;
	single.points.push_back (Vector2d (0.0, 0.0));
	EXPECT_THROW (extrudeProfiledToMesh (poly, single, mesh, 0.0, 10.0), std::runtime_error);

	Profile2D offAnchor;
	offAnchor.points.push_back (Vector2d (0.0, 1.0));
	offAnchor.points.push_back (Vector2d (4.0, 2.0));
	EXPECT_THROW (extrudeProfiledToMesh (poly, offAnchor, mesh, 0.0, 10.0), std::runtime_error);

	Profile2D negative;
	negative.points.push_back (Vector2d (0.0, 0.0));
	negative.points.push_back (Vector2d (4.0, -2.0));
	EXPECT_THROW (extrudeProfiledToMesh (poly, negative, mesh, 0.0, 10.0), std::runtime_error);

	Polygon2 empty;
	EXPECT_THROW (extrudeProfiledToMesh (empty, chamferSplayProfile (2.0, 4.0), mesh, 0.0, 10.0),
	              std::runtime_error);
}

// ---------------------------------------------------------------------------
//  Le polygone est pris en CONST -- la contrainte que l'extraction devait lever
// ---------------------------------------------------------------------------

TEST (TEST_cgmesh_profile2d, a_const_polygon_is_enough_to_extrude)
{
	// Ce cas ne compilerait pas avant la correction de la surface de lecture de
	// Polygon2 : c'est le compilateur qui verifie, pas une assertion. Il vaut
	// pour un noeud tenant une valeur de lien immuable, qui n'a jamais de
	// reference modifiable a offrir.
	const Polygon2 poly = makeFramePolygon ();
	Mesh mesh;
	ASSERT_NO_THROW (extrudeProfiledToMesh (poly, chamferSplayProfile (2.0, 4.0), mesh,
	                                        0.0, 10.0));
	EXPECT_GT (mesh.GetNVertices (), 0u);
	EXPECT_EQ (poly.get_n_contours (), 2);
}

// ---------------------------------------------------------------------------
//  Le garde-fou d'auto-intersection -- la raison meme de la generalisation
// ---------------------------------------------------------------------------
//
// La fonction a ete GENERALISEE plutot que reecrite pour ne pas perdre ce
// garde-fou : le decalage « vers la pierre » se recoupe sur un contour concave
// pointu, et sur un contour trop petit la moulure mange la matiere. Deux
// mesures par contour decident, et toutes deux retombent alors sur une paroi
// VERTICALE.
//
// ⚠ Ces deux branches n'etaient atteintes par AUCUNE fixture : la campagne de
// sabotage a montre que retirer le garde-fou laissait tout vert, y compris les
// cinq cas du gothique. Ce cas existe pour cela.

namespace {

// Etoile a cinq branches : rayon exterieur 100, interieur 30. Chaque sommet
// rentrant est un redent, donc la courbure concave cumulee est tres au-dessus du
// seuil, tandis que le rayon caracteristique reste largement au-dessus de la
// borne de taille -- c'est la branche « redents » et elle seule.
Polygon2 makeStarPolygon ()
{
	std::vector<float> pts;
	const double PI = 3.14159265358979323846;
	for (int k = 0; k < 10; ++k)
	{
		const double r = (k % 2 == 0) ? 100.0 : 30.0;
		const double a = PI * (double)k / 5.0;
		pts.push_back ((float)(r * std::cos (a)));
		pts.push_back ((float)(r * std::sin (a)));
	}
	Polygon2 poly;
	poly.alloc_contours (1);
	poly.add_contour (0, 10, pts.data ());
	return poly;
}

// Carre de 20 : rayon caracteristique 11,3, sous la borne de 24. C'est la
// branche « contour trop petit ».
Polygon2 makeSmallSquare ()
{
	Polygon2 poly;
	poly.alloc_contours (1);
	float pts[8] = { -10.f, -10.f, 10.f, -10.f, 10.f, 10.f, -10.f, 10.f };
	poly.add_contour (0, 4, pts);
	return poly;
}

Polygon2 makeBigSquare ()
{
	Polygon2 poly;
	poly.alloc_contours (1);
	float pts[8] = { -100.f, -100.f, 100.f, -100.f, 100.f, 100.f, -100.f, 100.f };
	poly.add_contour (0, 4, pts);
	return poly;
}

}  // namespace

TEST (TEST_cgmesh_profile2d, a_cusped_or_tiny_contour_falls_back_to_a_vertical_wall)
{
	// Un profil de largeur NULLE est, par construction, une paroi verticale :
	// c'est l'oracle, et il ne depend d'aucun chiffre releve sur le code.
	const Profile2D wide = chamferSplayProfile (6.0, 10.0);
	const Profile2D none = chamferSplayProfile (0.0, 10.0);

	{
		Polygon2 a = makeStarPolygon (), b = makeStarPolygon ();
		Mesh profiled, vertical;
		ASSERT_NO_THROW (extrudeProfiledToMesh (a, wide, profiled, 0.0, 20.0));
		ASSERT_NO_THROW (extrudeProfiledToMesh (b, none, vertical, 0.0, 20.0));
		EXPECT_TRUE (sameGeometry (profiled, vertical))
			<< "un contour a redents doit retomber sur une paroi verticale";
	}
	{
		Polygon2 a = makeSmallSquare (), b = makeSmallSquare ();
		Mesh profiled, vertical;
		ASSERT_NO_THROW (extrudeProfiledToMesh (a, wide, profiled, 0.0, 20.0));
		ASSERT_NO_THROW (extrudeProfiledToMesh (b, none, vertical, 0.0, 20.0));
		EXPECT_TRUE (sameGeometry (profiled, vertical))
			<< "un contour trop petit doit retomber sur une paroi verticale";
	}
	{
		// CONTROLE POSITIF : sans lui, les deux cas ci-dessus passeraient tout
		// aussi bien sur une fonction qui n'appliquerait JAMAIS de profil.
		Polygon2 a = makeBigSquare (), b = makeBigSquare ();
		Mesh profiled, vertical;
		ASSERT_NO_THROW (extrudeProfiledToMesh (a, wide, profiled, 0.0, 20.0));
		ASSERT_NO_THROW (extrudeProfiledToMesh (b, none, vertical, 0.0, 20.0));
		EXPECT_FALSE (sameGeometry (profiled, vertical))
			<< "une grande ouverture convexe doit garder sa moulure";
	}
}
