#include <gtest/gtest.h>

#include "../src/cgmesh/extrude_contours.h"
#include "../src/cgmesh/mesh.h"

#include <cmath>
#include <memory>
#include <vector>

// ===========================================================================
//  Points de contour redondants
// ===========================================================================
// glutess ECARTE un sommet consecutif confondu avec son voisin : il n'apparait
// alors dans aucun triangle. Or les parois se construisent en retrouvant chaque
// arete de contour parmi les triangles de capot (ExtrudedMeshBuilder::Append,
// « tessellation gap; skip wall »). Un sommet ecarte fait donc perdre TROIS
// aretes -- celle qui entre, celle de longueur nulle, celle qui sort -- et donc
// trois parois, sans le moindre signalement.
//
// Ce n'etait pas une hypothese : sur un trace dense issu d'un L-systeme, 190
// aretes sur 1633 sortaient sans paroi, et parmi ces 190, le nombre dont les DEUX
// extremites etaient utilisees par un triangle valait exactement zero. La
// totalite des pertes venait de la.
//
// Effet du nettoyage, sur les memes fichiers -- ecart entre le volume signe et
// aire x hauteur :
//
//     Board  x1   -70,43 %  ->  -0,03 %
//     Board  x3   -25,81 %  ->  -0,005 %
//     Dragon x3    -8,70 %  ->  +0,000 %
//
// L'oracle est le volume signe : pour un prisme FERME il vaut exactement
// aire_du_capot x hauteur. Aucun compte de faces ni d'aretes de bord ne le
// remplace -- Append emet volontairement capots et parois en blocs de sommets
// DISJOINTS, donc des aretes de bord existent par construction.

namespace {

// Volume signe (theoreme de la divergence) et aire du capot superieur.
void measure(Mesh& m, float height, double& volume, double& topArea, unsigned int& nWalls)
{
	volume = 0.0; topArea = 0.0; nWalls = 0;
	for (unsigned int f = 0; f < m.GetNFaces(); f++)
	{
		if (m.GetFaceNVertices(f) != 3) continue;
		float p[3][3];
		for (int k = 0; k < 3; k++) m.GetVertex((unsigned int)m.GetFaceVertex(f, k), p[k]);
		volume += ((double)p[0][0]*((double)p[1][1]*p[2][2]-(double)p[2][1]*p[1][2])
		         - (double)p[0][1]*((double)p[1][0]*p[2][2]-(double)p[2][0]*p[1][2])
		         + (double)p[0][2]*((double)p[1][0]*p[2][1]-(double)p[2][0]*p[1][1])) / 6.0;
		const bool atTop = std::fabs(p[0][2]-height) < 1e-4f
		                && std::fabs(p[1][2]-height) < 1e-4f
		                && std::fabs(p[2][2]-height) < 1e-4f;
		const bool atBot = std::fabs(p[0][2]) < 1e-4f && std::fabs(p[1][2]) < 1e-4f
		                && std::fabs(p[2][2]) < 1e-4f;
		if (atTop)
			topArea += (((double)p[1][0]-p[0][0])*((double)p[2][1]-p[0][1])
			          - ((double)p[1][1]-p[0][1])*((double)p[2][0]-p[0][0])) / 2.0;
		else if (!atBot)
			nWalls++;
	}
}

std::unique_ptr<Mesh> extrude(const std::vector<Vector2f>& pts, float height)
{
	ExtrudeContour c;
	c.pts = pts;
	ExtrudeAppendOptions ao;
	ao.zBottom = 0.f;
	ao.zTop    = height;
	ao.normalizeOrientation = false;
	ExtrudedMeshBuilder b;
	if (!b.Append({ c }, ao) || b.Empty())
		return nullptr;
	return std::unique_ptr<Mesh>(b.Build());
}

void expectWatertight(const std::vector<Vector2f>& pts, double expectedArea, const char* what)
{
	SCOPED_TRACE(what);
	const float h = 0.25f;
	std::unique_ptr<Mesh> m = extrude(pts, h);
	ASSERT_NE(m, nullptr) << "aucun maillage";

	double volume = 0, topArea = 0;
	unsigned int nWalls = 0;
	measure(*m, h, volume, topArea, nWalls);

	EXPECT_GT(nWalls, 0u) << "aucune paroi laterale";
	EXPECT_NEAR(std::fabs(topArea), expectedArea, 1e-4)
		<< "le capot ne couvre pas la surface attendue";
	EXPECT_NEAR(volume, topArea * h, 1e-6)
		<< "volume " << volume << " au lieu de " << (topArea * h)
		<< " : des parois manquent";
}

// Carre unite, 4 sommets, aire 1.
const std::vector<Vector2f> kSquare = {
	{0.f, 0.f}, {1.f, 0.f}, {1.f, 1.f}, {0.f, 1.f}
};

}  // namespace

// Temoin : un contour propre est etanche. Sans lui, un test qui passerait pour de
// mauvaises raisons ne se distinguerait pas.
TEST(TEST_cgmesh_extrude_contours, a_clean_contour_is_watertight)
{
	expectWatertight(kSquare, 1.0, "carre unite");
}

// UN point strictement duplique au milieu du contour.
TEST(TEST_cgmesh_extrude_contours, an_exactly_duplicated_point_keeps_it_watertight)
{
	std::vector<Vector2f> pts = {
		{0.f, 0.f}, {1.f, 0.f}, {1.f, 0.f}, {1.f, 1.f}, {0.f, 1.f}
	};
	expectWatertight(pts, 1.0, "doublon exact");
}

// PLUSIEURS doublons consecutifs, sur des sommets differents : c'est la
// configuration reelle observee en sortie de Clipper2.
TEST(TEST_cgmesh_extrude_contours, several_duplicate_runs_keep_it_watertight)
{
	std::vector<Vector2f> pts = {
		{0.f, 0.f}, {0.f, 0.f}, {0.f, 0.f},
		{1.f, 0.f}, {1.f, 0.f},
		{1.f, 1.f},
		{0.f, 1.f}, {0.f, 1.f}
	};
	expectWatertight(pts, 1.0, "trois series de doublons");
}

// Doublon a un ULP de float pres, non exact. C'est le cas MESURE : les points
// sortent identiques de Clipper2 et la mise a l'echelle de recenterAndFit les
// separe d'un bit. Une comparaison exacte les raterait, d'ou la tolerance
// RELATIVE.
TEST(TEST_cgmesh_extrude_contours, a_one_ulp_duplicate_keeps_it_watertight)
{
	const float x = 1.f;
	const float xEps = std::nextafter(x, 2.f);   // x + 1 ULP
	ASSERT_NE(x, xEps) << "les deux valeurs doivent bien differer";
	std::vector<Vector2f> pts = {
		{0.f, 0.f}, {x, 0.f}, {xEps, 0.f}, {1.f, 1.f}, {0.f, 1.f}
	};
	expectWatertight(pts, 1.0, "doublon a 1 ULP");
}

// Le dernier point confondu avec le PREMIER : l'arete de fermeture est implicite,
// donc ce doublon degenere en fin de boucle et doit etre retire aussi.
TEST(TEST_cgmesh_extrude_contours, a_repeated_first_point_at_the_end_keeps_it_watertight)
{
	std::vector<Vector2f> pts = {
		{0.f, 0.f}, {1.f, 0.f}, {1.f, 1.f}, {0.f, 1.f}, {0.f, 0.f}
	};
	expectWatertight(pts, 1.0, "premier point repete a la fin");
}

// Contre-epreuve : le nettoyage ne doit pas fusionner des sommets DISTINCTS. Une
// tolerance trop large aplatirait les details fins d'un trace.
TEST(TEST_cgmesh_extrude_contours, close_but_distinct_points_are_preserved)
{
	// Un cran de 1e-3 sur un contour de taille 1 : mille fois au-dessus de la
	// tolerance, il doit survivre et contribuer a l'aire.
	std::vector<Vector2f> withNotch = {
		{0.f, 0.f}, {1.f, 0.f}, {1.f, 1.f},
		{0.5f, 1.f}, {0.5f, 1.f - 1e-3f}, {0.f, 1.f - 1e-3f}
	};
	const float h = 0.25f;
	std::unique_ptr<Mesh> m = extrude(withNotch, h);
	ASSERT_NE(m, nullptr);

	double volume = 0, topArea = 0;
	unsigned int nWalls = 0;
	measure(*m, h, volume, topArea, nWalls);

	// Six sommets distincts => six parois, donc douze triangles lateraux.
	EXPECT_EQ(nWalls, 12u) << "des sommets distincts ont ete fusionnes";
	EXPECT_NEAR(volume, topArea * h, 1e-6) << "le contour a cran n'est pas ferme";
	// L'aire vaut 1 moins la bande retiree par le cran (0.5 x 1e-3).
	EXPECT_NEAR(std::fabs(topArea), 1.0 - 0.5e-3, 1e-5);
}

// Un contour qui se reduit a moins de trois points distincts n'a pas de surface :
// il doit etre ecarte sans produire de maillage ni planter.
TEST(TEST_cgmesh_extrude_contours, a_contour_collapsing_below_three_points_is_dropped)
{
	std::vector<Vector2f> pts = {
		{2.f, 3.f}, {2.f, 3.f}, {2.f, 3.f}, {2.f, 3.f}
	};
	EXPECT_EQ(extrude(pts, 0.25f), nullptr) << "un point repete n'a pas de surface";
}
