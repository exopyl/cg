#include <gtest/gtest.h>

#include "../src/cgimg/cgimg.h"
#include "../src/cgmesh/image_pixel_blocks.h"
#include "../src/cgmesh/image_region_pipeline.h"
#include "../src/cgmesh/image_vectorization.h"
#include "../src/cgmesh/material.h"
#include "../src/cgmesh/mesh.h"

#include <cmath>
#include <set>
#include <string>
#include <vector>

namespace {

// PPM (P6) : sans perte, et l'encodeur est fourni par cgimg (pas d'ecrivain PNG).
const char* kFile = "./tu_pixel_blocks_input.ppm";

void fillRect(Img& img, int x0, int y0, int x1, int y1,
              unsigned char r, unsigned char g, unsigned char b)
{
	for (int y = y0; y < y1; ++y)
		for (int x = x0; x < x1; ++x)
			img.set_pixel((unsigned)x, (unsigned)y, r, g, b, 255);
}

// Options « neutres » : aucun pretraitement, pour que les tests mesurent la
// geometrie et non le debruitage. preSmooth/refine a 0 gardent les couleurs
// EXACTEMENT telles qu'on les a posees.
ImagePixelBlocksOptions rawOptions(int pixelWidth, int maxColors = 8)
{
	ImagePixelBlocksOptions opt;
	opt.pixelWidth       = pixelWidth;
	opt.maxColors        = maxColors;
	opt.workingMaxDim    = 0;
	opt.preSmoothPasses  = 0;
	opt.refineIterations = 0;
	opt.despecklePasses  = 0;
	opt.minRegionArea    = 0;
	opt.emitBase         = false;
	opt.emitWall         = false;
	opt.fitSize          = 1.0f;
	opt.blockHeight      = 0.50f;
	opt.baseThickness    = 0.20f;
	return opt;
}

void getBBox(Mesh& m, float vmin[3], float vmax[3])
{
	m.computebbox();
	m.bbox().GetMinMax(vmin, vmax);
}

std::string materialName(Mesh& m, unsigned int i)
{
	Material* mat = m.GetMaterial(i);
	return mat ? mat->GetName() : std::string();
}

void destroyAll(std::vector<Mesh*>& v)
{
	for (Mesh* m : v) delete m;
	v.clear();
}

// Toutes les couleurs distinctes d'une image, empaquetees.
std::set<unsigned int> colorsOf(const Img& img)
{
	std::set<unsigned int> out;
	for (unsigned int y = 0; y < img.height(); ++y)
		for (unsigned int x = 0; x < img.width(); ++x)
		{
			unsigned char r = 0, g = 0, b = 0, a = 0;
			img.get_pixel(x, y, &r, &g, &b, &a);
			out.insert(((unsigned)r << 16) | ((unsigned)g << 8) | (unsigned)b);
		}
	return out;
}

unsigned int pixelAt(const Img& img, int x, int y)
{
	unsigned char r = 0, g = 0, b = 0, a = 0;
	img.get_pixel((unsigned)x, (unsigned)y, &r, &g, &b, &a);
	return ((unsigned)r << 16) | ((unsigned)g << 8) | (unsigned)b;
}

const unsigned int RED   = 0xff0000;
const unsigned int BLUE  = 0x0000ff;
const unsigned int WHITE = 0xffffff;

} // namespace

// ---------------------------------------------------------------------------
//  Pixelisation : vote majoritaire
// ---------------------------------------------------------------------------

// Un bloc source 4x4 contenant 10 rouges et 6 bleus doit sortir ROUGE. Une
// moyenne produirait une teinte violacee absente de l'image -- c'est precisement
// ce que le vote evite.
TEST(TEST_cgmesh_image_pixel_blocks, majority_vote_picks_the_dominant_colour)
{
	Img img(4, 4, false);
	fillRect(img, 0, 0, 4, 4, 255, 0, 0);       // 16 rouges
	fillRect(img, 0, 0, 3, 2, 0, 0, 255);       // 6 bleus -> 10 rouges restants

	ASSERT_TRUE(pixelize_majority(img, 1));
	EXPECT_EQ(img.width(), 1u);
	EXPECT_EQ(img.height(), 1u);
	EXPECT_EQ(pixelAt(img, 0, 0), RED);
}

// Le vote ne choisit que parmi des couleurs presentes : l'ensemble des couleurs
// de sortie est inclus dans celui des couleurs d'entree.
TEST(TEST_cgmesh_image_pixel_blocks, majority_vote_never_invents_a_colour)
{
	Img img(64, 64, false);
	for (int y = 0; y < 64; ++y)
		for (int x = 0; x < 64; ++x)
		{
			const bool red = ((x / 3) + (y / 5)) % 2 == 0;
			img.set_pixel((unsigned)x, (unsigned)y,
			              red ? 255 : 0, 0, red ? 0 : 255, 255);
		}
	const std::set<unsigned int> before = colorsOf(img);
	ASSERT_EQ(before.size(), 2u);

	ASSERT_TRUE(pixelize_majority(img, 7));     // 64 n'est pas multiple de 7
	const std::set<unsigned int> after = colorsOf(img);

	for (unsigned int c : after)
		EXPECT_EQ(before.count(c), 1u) << "couleur inventee : " << std::hex << c;
}

// Les bornes de bloc couvrent TOUTE la source, reste compris. Img::resize mode 2
// avance d'un pas constant W/w et laisse tomber les dernieres colonnes : sur
// 100 px en 7 cellules, il n'en couvrirait que 98.
TEST(TEST_cgmesh_image_pixel_blocks, majority_vote_covers_the_trailing_remainder)
{
	// Blanc partout, sauf les 4 dernieres colonnes en rouge. Avec 100/7 = 14 (pas
	// constant), les colonnes 98-99 ne seraient jamais lues.
	Img img(100, 10, false);
	fillRect(img, 0, 0, 100, 10, 255, 255, 255);
	fillRect(img, 96, 0, 100, 10, 255, 0, 0);

	ASSERT_TRUE(pixelize_majority(img, 7));
	ASSERT_EQ(img.width(), 7u);

	// La derniere cellule couvre [85,100) : 15 px dont 4 rouges -> reste blanche,
	// mais elle DOIT avoir lu les colonnes de queue. On le verifie autrement :
	// avec une cellule qui les contient en majorite.
	Img img2(100, 10, false);
	fillRect(img2, 0, 0, 100, 10, 255, 255, 255);
	fillRect(img2, 90, 0, 100, 10, 255, 0, 0);   // 10 derniers px rouges

	ASSERT_TRUE(pixelize_majority(img2, 10));    // cellule 9 = [90,100), 100% rouge
	ASSERT_EQ(img2.width(), 10u);
	EXPECT_EQ(pixelAt(img2, 9, 0), RED)
		<< "la derniere cellule n'a pas lu les colonnes de queue";
}

TEST(TEST_cgmesh_image_pixel_blocks, majority_vote_refuses_to_upscale)
{
	Img img(8, 8, false);
	fillRect(img, 0, 0, 8, 8, 1, 2, 3);
	EXPECT_TRUE(pixelize_majority(img, 32));   // no-op, pas une erreur
	EXPECT_EQ(img.width(), 8u);
}

// ---------------------------------------------------------------------------
//  Contours en escalier
// ---------------------------------------------------------------------------

// Sans lissage, un contour pixelise n'a QUE des aretes axiales. Avec le lissage
// de Taubin (defaut historique), les coins sont arrondis et des aretes obliques
// apparaissent : c'est la difference que la brique existe pour obtenir.
TEST(TEST_cgmesh_image_pixel_blocks, unsmoothed_contours_are_axis_aligned)
{
	Img img(16, 16, false);
	fillRect(img, 0, 0, 16, 16, 255, 255, 255);
	fillRect(img, 2, 2, 10, 6, 255, 0, 0);      // un L : deux rectangles accoles
	fillRect(img, 2, 6, 6, 13, 255, 0, 0);

	auto countDiagonalEdges = [](bool smooth) {
		Img local(16, 16, false);
		fillRect(local, 0, 0, 16, 16, 255, 255, 255);
		fillRect(local, 2, 2, 10, 6, 255, 0, 0);
		fillRect(local, 2, 6, 6, 13, 255, 0, 0);

		CLitRasterToVector rtv;
		Palette* pal = local.get_palette();
		EXPECT_TRUE(rtv.Vectorize(&local, Color(), false, pal,
		                          smooth ? 0.5f : -1.f, smooth));
		delete pal;

		int diagonal = 0;
		for (const VectorLayer& layer : rtv.GetLayers())
			for (const VectorContour& c : layer.contours)
			{
				const size_t n = c.pts.size();
				for (size_t i = 0; i < n; ++i)
				{
					const Vector2f& a = c.pts[i];
					const Vector2f& b = c.pts[(i + 1) % n];
					const float dx = std::fabs(b.x - a.x), dy = std::fabs(b.y - a.y);
					if (dx > 1e-4f && dy > 1e-4f) ++diagonal;
				}
			}
		return diagonal;
	};

	EXPECT_EQ(countDiagonalEdges(/*smooth=*/false), 0)
		<< "escalier attendu : aucune arete ne doit etre oblique";
	EXPECT_GT(countDiagonalEdges(/*smooth=*/true), 0)
		<< "le lissage doit, lui, produire des obliques (sinon le test ne prouve rien)";
}

// ---------------------------------------------------------------------------
//  Segmentation en blocs connexes
// ---------------------------------------------------------------------------

// DEUX carres rouges disjoints + un carre bleu sur fond blanc.
// Un objet par BLOC CONNEXE => 4 maillages, et les deux rouges ont des bbox
// disjointes. Un objet par COULEUR n'en donnerait que 3, les deux rouges reunis.
TEST(TEST_cgmesh_image_pixel_blocks, per_component_splits_disjoint_blocks_of_one_colour)
{
	Img img(32, 32, false);
	fillRect(img, 0, 0, 32, 32, 255, 255, 255);
	fillRect(img,  2,  2, 10, 10, 255, 0, 0);     // rouge A (haut-gauche)
	fillRect(img, 22, 22, 30, 30, 255, 0, 0);     // rouge B (bas-droite)
	fillRect(img,  2, 22, 10, 30, 0, 0, 255);     // bleu
	ASSERT_EQ(img.save(kFile), 0);

	ImagePixelBlocksOptions opt = rawOptions(/*pixelWidth=*/16, /*maxColors=*/4);
	std::vector<Mesh*> meshes = image_to_pixel_blocks_per_component(kFile, opt);

	// fond blanc + 2 rouges + 1 bleu
	ASSERT_EQ(meshes.size(), 4u) << "un maillage par bloc connexe attendu";

	// Les deux blocs rouges portent la MEME couleur de palette mais des indices de
	// bloc differents -- c'est exactement ce que le nom du materiau doit refleter.
	std::vector<size_t> reds;
	for (size_t i = 0; i < meshes.size(); ++i)
	{
		Material* mat = meshes[i]->GetMaterial(0);
		ASSERT_NE(mat, nullptr);
		auto* mc = dynamic_cast<MaterialColor*>(mat);
		ASSERT_NE(mc, nullptr);
		if (mc->GetFloatRed() > .8f && mc->GetFloatGreen() < .25f && mc->GetFloatBlue() < .25f)
			reds.push_back(i);
		EXPECT_EQ(materialName(*meshes[i], 0).rfind("block_", 0), 0u)
			<< "nom de materiau attendu block_NNNN_color_NN, obtenu "
			<< materialName(*meshes[i], 0);
	}
	ASSERT_EQ(reds.size(), 2u) << "les deux blocs rouges doivent rester distincts";

	float aMin[3], aMax[3], bMin[3], bMax[3];
	getBBox(*meshes[reds[0]], aMin, aMax);
	getBBox(*meshes[reds[1]], bMin, bMax);
	const bool disjointX = aMax[0] <= bMin[0] + 1e-4f || bMax[0] <= aMin[0] + 1e-4f;
	const bool disjointY = aMax[1] <= bMin[1] + 1e-4f || bMax[1] <= aMin[1] + 1e-4f;
	EXPECT_TRUE(disjointX || disjointY)
		<< "les bbox des deux blocs rouges doivent etre disjointes";

	destroyAll(meshes);
}

// Un anneau rouge autour d'un coeur bleu : le bloc rouge doit porter un TROU,
// donc encadrer le bleu sans le recouvrir.
TEST(TEST_cgmesh_image_pixel_blocks, a_ring_block_keeps_its_hole)
{
	Img img(32, 32, false);
	fillRect(img, 0, 0, 32, 32, 255, 255, 255);
	fillRect(img,  6,  6, 26, 26, 255, 0, 0);     // anneau rouge...
	fillRect(img, 12, 12, 20, 20, 0, 0, 255);     // ...autour d'un coeur bleu
	ASSERT_EQ(img.save(kFile), 0);

	ImagePixelBlocksOptions opt = rawOptions(/*pixelWidth=*/16, /*maxColors=*/4);
	std::vector<Mesh*> meshes = image_to_pixel_blocks_per_component(kFile, opt);
	ASSERT_EQ(meshes.size(), 3u);   // fond + anneau + coeur

	// Le coeur bleu est strictement contenu dans la bbox de l'anneau rouge.
	int redIdx = -1, blueIdx = -1;
	for (size_t i = 0; i < meshes.size(); ++i)
	{
		auto* mc = dynamic_cast<MaterialColor*>(meshes[i]->GetMaterial(0));
		ASSERT_NE(mc, nullptr);
		if (mc->GetFloatRed() > .8f && mc->GetFloatBlue() < .25f) redIdx = (int)i;
		if (mc->GetFloatBlue() > .8f && mc->GetFloatRed() < .25f) blueIdx = (int)i;
	}
	ASSERT_GE(redIdx, 0);
	ASSERT_GE(blueIdx, 0);

	float rMin[3], rMax[3], bMin[3], bMax[3];
	getBBox(*meshes[redIdx], rMin, rMax);
	getBBox(*meshes[blueIdx], bMin, bMax);
	EXPECT_LT(rMin[0], bMin[0]);
	EXPECT_LT(rMin[1], bMin[1]);
	EXPECT_GT(rMax[0], bMax[0]);
	EXPECT_GT(rMax[1], bMax[1]);

	// Un anneau a DEUX contours (exterieur + trou), donc plus de parois qu'un plein.
	// Le coeur, lui, est un simple carre.
	EXPECT_GT(meshes[redIdx]->GetNFaces(), meshes[blueIdx]->GetNFaces())
		<< "l'anneau troue doit avoir plus de faces que le carre plein";

	destroyAll(meshes);
}

// ---------------------------------------------------------------------------
//  Maillage d'affichage
// ---------------------------------------------------------------------------

TEST(TEST_cgmesh_image_pixel_blocks, display_mesh_has_one_material_per_palette_colour)
{
	Img img(32, 32, false);
	fillRect(img, 0, 0, 32, 32, 255, 255, 255);
	fillRect(img,  2,  2, 10, 10, 255, 0, 0);
	fillRect(img, 22, 22, 30, 30, 255, 0, 0);     // MEME couleur, deux blocs
	fillRect(img,  2, 22, 10, 30, 0, 0, 255);
	ASSERT_EQ(img.save(kFile), 0);

	ImagePixelBlocksOptions opt = rawOptions(/*pixelWidth=*/16, /*maxColors=*/4);
	Mesh* m = image_to_pixel_blocks(kFile, opt);
	ASSERT_NE(m, nullptr);

	// 3 couleurs (blanc, rouge, bleu) -- PAS 4 : les deux blocs rouges partagent
	// leur materiau, c'est l'image qu'on lit a l'ecran, pas la decoupe.
	EXPECT_EQ(m->GetNMaterials(), 3u);
	delete m;
}

TEST(TEST_cgmesh_image_pixel_blocks, base_and_wall_come_last_when_requested)
{
	Img img(32, 32, false);
	fillRect(img, 0, 0, 32, 32, 255, 255, 255);
	fillRect(img, 10, 10, 22, 22, 255, 0, 0);
	ASSERT_EQ(img.save(kFile), 0);

	ImagePixelBlocksOptions opt = rawOptions(/*pixelWidth=*/16, /*maxColors=*/4);
	opt.emitBase = true;
	opt.emitWall = true;

	std::vector<Mesh*> meshes = image_to_pixel_blocks_per_component(kFile, opt);
	ASSERT_GE(meshes.size(), 4u);   // >= fond + carre + base + mur
	EXPECT_EQ(materialName(*meshes[meshes.size() - 2], 0), "base");
	EXPECT_EQ(materialName(*meshes[meshes.size() - 1], 0), "wall");
	destroyAll(meshes);
}

TEST(TEST_cgmesh_image_pixel_blocks, palette_is_bounded_by_max_colors)
{
	// Degrade continu : sans borne, chaque cellule aurait sa teinte.
	Img img(64, 64, false);
	for (int y = 0; y < 64; ++y)
		for (int x = 0; x < 64; ++x)
			img.set_pixel((unsigned)x, (unsigned)y,
			              (unsigned char)(x * 4), (unsigned char)(y * 4), 128, 255);
	ASSERT_EQ(img.save(kFile), 0);

	ImagePixelBlocksOptions opt = rawOptions(/*pixelWidth=*/16, /*maxColors=*/4);
	Mesh* m = image_to_pixel_blocks(kFile, opt);
	ASSERT_NE(m, nullptr);
	EXPECT_LE(m->GetNMaterials(), 4u);
	delete m;
}

// ---------------------------------------------------------------------------
//  Retrait des regions
// ---------------------------------------------------------------------------

// `shrink` creuse un sillon entre blocs voisins : chaque bloc RETRECIT, mais le
// cadre (donc l'emprise globale) ne bouge pas -- la bbox est figee avant.
TEST(TEST_cgmesh_image_pixel_blocks, shrink_erodes_blocks_without_moving_the_frame)
{
	Img img(32, 32, false);
	fillRect(img, 0, 0, 32, 32, 255, 255, 255);
	fillRect(img, 8, 8, 24, 24, 255, 0, 0);
	ASSERT_EQ(img.save(kFile), 0);

	ImagePixelBlocksOptions opt = rawOptions(/*pixelWidth=*/16, /*maxColors=*/4);
	opt.emitBase = true;                       // le cadre sert de reference fixe

	Mesh* plain = image_to_pixel_blocks(kFile, opt);
	ASSERT_NE(plain, nullptr);
	opt.shrink = 0.15f;
	Mesh* eroded = image_to_pixel_blocks(kFile, opt);
	ASSERT_NE(eroded, nullptr);

	float pMin[3], pMax[3], eMin[3], eMax[3];
	getBBox(*plain, pMin, pMax);
	getBBox(*eroded, eMin, eMax);
	for (int k = 0; k < 2; ++k)
	{
		EXPECT_NEAR(pMin[k], eMin[k], 1e-4f) << "le cadre ne doit pas bouger (axe " << k << ")";
		EXPECT_NEAR(pMax[k], eMax[k], 1e-4f) << "le cadre ne doit pas bouger (axe " << k << ")";
	}

	delete plain;
	delete eroded;
}

// ---------------------------------------------------------------------------
//  Robustesse
// ---------------------------------------------------------------------------

TEST(TEST_cgmesh_image_pixel_blocks, nonexistent_file_fails_cleanly)
{
	ImagePixelBlocksOptions opt = rawOptions(16);
	EXPECT_EQ(image_to_pixel_blocks("./tu_pixel_blocks_does_not_exist.ppm", opt), nullptr);
	EXPECT_TRUE(image_to_pixel_blocks_per_component("./tu_pixel_blocks_does_not_exist.ppm", opt).empty());
}

// Une largeur cible plus grande que la source ne doit pas casser : la
// pixelisation devient un no-op et la brique se comporte comme un relief non
// lisse.
TEST(TEST_cgmesh_image_pixel_blocks, pixel_width_larger_than_source_is_harmless)
{
	Img img(16, 16, false);
	fillRect(img, 0, 0, 16, 16, 255, 255, 255);
	fillRect(img, 4, 4, 12, 12, 255, 0, 0);
	ASSERT_EQ(img.save(kFile), 0);

	ImagePixelBlocksOptions opt = rawOptions(/*pixelWidth=*/256, /*maxColors=*/4);
	Mesh* m = image_to_pixel_blocks(kFile, opt);
	ASSERT_NE(m, nullptr);
	EXPECT_GT(m->GetNFaces(), 0u);
	delete m;
}
