#include <gtest/gtest.h>

#include "../src/cgimg/cgimg.h"
#include "../src/cgmesh/image_relief.h"
#include "../src/cgmesh/image_vectorization.h"
#include "../src/cgmesh/mesh.h"

#include <cmath>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace {

// PPM (P6): lossless and the encoder ships with cgimg (there is no PNG writer).
//
// The scratch file name is declared INSIDE each test, after the test's own name.
// A file-scope constant had the eight tests of this suite writing the same PPM.
const int   W = 32, H = 32;

void fillRect(Img& img, int x0, int y0, int x1, int y1,
              unsigned char r, unsigned char g, unsigned char b)
{
	for (int y = y0; y < y1; ++y)
		for (int x = x0; x < x1; ++x)
			img.set_pixel((unsigned)x, (unsigned)y, r, g, b, 255);
}

// White background + a centred red square: two colour regions, the background
// one carrying a hole. Same controlled input as
// TEST_cgimg_img.vectorization_detects_regions.
void makeTwoColorImage(Img& img)
{
	img = Img(W, H, false);
	fillRect(img, 0, 0, W, H, 255, 255, 255);
	fillRect(img, 10, 10, 22, 22, 255, 0, 0);
}

// Writes the same image to disk so the file-based entry points can read it.
// Returns false when the format round-trip is unavailable.
bool writeTwoColorImage(const char* path)
{
	Img img;
	makeTwoColorImage(img);
	return img.save(path) == 0;
}

ImageReliefOptions defaultOptions()
{
	ImageReliefOptions opt;
	opt.maxColors     = 8;
	opt.fitSize       = 1.0f;
	opt.blockHeight   = 0.50f;
	opt.baseThickness = 0.20f;
	opt.margin        = 0.05f;
	opt.wallThickness = 0.03f;
	opt.wallHeight    = 0.30f;
	return opt;
}

void getBBox(Mesh& m, float vmin[3], float vmax[3])
{
	m.computebbox();
	m.bbox().GetMinMax(vmin, vmax);
}

} // namespace

// ---------------------------------------------------------------------------
//  Step 1: the structured accessor on the vectorizer
// ---------------------------------------------------------------------------

TEST(TEST_cgmesh_image_relief, get_layers_splits_background_and_square)
{
	Img img;
	makeTwoColorImage(img);

	CLitRasterToVector rtv;
	ASSERT_TRUE(rtv.Vectorize(&img, Color(255, 255, 255), false, nullptr));

	const std::vector<VectorLayer> layers = rtv.GetLayers();
	ASSERT_EQ(layers.size(), 2u) << "white + red square = two colour layers";

	// One layer is the background: outer boundary of the image + the hole left by
	// the square. The other is the square: a single outer boundary, no hole.
	int nWithHole = 0, nSolid = 0;
	for (const VectorLayer& layer : layers)
	{
		int outer = 0, holes = 0;
		for (const VectorContour& c : layer.contours)
		{
			EXPECT_GE(c.pts.size(), 3u);
			if (c.isHole) holes++; else outer++;
		}
		EXPECT_EQ(outer, 1) << "one outer boundary per connected region";
		if (holes == 1)      nWithHole++;
		else if (holes == 0) nSolid++;
	}
	EXPECT_EQ(nWithHole, 1) << "the background encloses the square as a hole";
	EXPECT_EQ(nSolid,    1) << "the square itself has no hole";
}

// ---------------------------------------------------------------------------
//  Step 3: multi-material relief
// ---------------------------------------------------------------------------

TEST(TEST_cgmesh_image_relief, produces_multi_material_mesh)
{
	const char* kInputFile = "./produces_multi_material_mesh.ppm";
	ASSERT_TRUE(writeTwoColorImage(kInputFile));

	const ImageReliefOptions opt = defaultOptions();
	std::unique_ptr<Mesh> m(image_to_relief(kInputFile, opt));
	ASSERT_NE(m, nullptr);

	EXPECT_GT(m->GetNFaces(), 0u);
	EXPECT_TRUE(m->IsTriangleMesh());

	// One material per colour, plus base and wall.
	EXPECT_EQ(m->GetNMaterials(), 2u + 2u)
		<< "two colours + base + wall";

	// Every face carries a material id inside that table.
	for (unsigned int f = 0; f < m->GetNFaces(); ++f)
	{
		const int mat = m->GetFaceMaterialId(f);
		EXPECT_GE(mat, 0);
		EXPECT_LT((unsigned int)mat, m->GetNMaterials());
	}

	// Dumped for visual inspection in the viewer (colours, uniform block height,
	// perimeter wall) -- same convention as the other geometry test suites.
	EXPECT_EQ(m->save("./image_relief.obj"), 0);
}

TEST(TEST_cgmesh_image_relief, bbox_frames_content_with_margin_and_wall)
{
	const char* kInputFile = "./bbox_frames_content_with_margin_and_wall.ppm";
	ASSERT_TRUE(writeTwoColorImage(kInputFile));

	const ImageReliefOptions opt = defaultOptions();
	std::unique_ptr<Mesh> m(image_to_relief(kInputFile, opt));
	ASSERT_NE(m, nullptr);

	float vmin[3], vmax[3];
	getBBox(*m, vmin, vmax);

	// Square image -> content is fitSize on both axes; the base plate adds
	// margin + wallThickness on each side.
	const float expectedXY = opt.fitSize + 2.f * (opt.margin + opt.wallThickness);
	EXPECT_NEAR(vmax[0] - vmin[0], expectedXY, 1e-4f);
	EXPECT_NEAR(vmax[1] - vmin[1], expectedXY, 1e-4f);
	// Content centred on the origin.
	EXPECT_NEAR(0.5f * (vmin[0] + vmax[0]), 0.f, 1e-4f);
	EXPECT_NEAR(0.5f * (vmin[1] + vmax[1]), 0.f, 1e-4f);

	// Base sits at z=0, the tallest of blocks / wall sets the top.
	EXPECT_NEAR(vmin[2], 0.f, 1e-5f);
	EXPECT_NEAR(vmax[2], opt.baseThickness + std::max(opt.blockHeight, opt.wallHeight), 1e-5f);
}

// The whole point of "relief": blocks start at the TOP of the base plate, not at
// z=0. So the only Z values in the mesh are the four plane heights.
TEST(TEST_cgmesh_image_relief, blocks_rise_from_the_top_of_the_base)
{
	const char* kInputFile = "./blocks_rise_from_the_top_of_the_base.ppm";
	ASSERT_TRUE(writeTwoColorImage(kInputFile));

	ImageReliefOptions opt = defaultOptions();
	opt.blockHeight = 0.50f;
	opt.wallHeight  = 0.30f;   // deliberately different from blockHeight

	std::unique_ptr<Mesh> m(image_to_relief(kInputFile, opt));
	ASSERT_NE(m, nullptr);

	const float expected[4] = { 0.f,
	                            opt.baseThickness,
	                            opt.baseThickness + opt.blockHeight,
	                            opt.baseThickness + opt.wallHeight };

	std::set<int> seen;   // millimetre-ish buckets, enough to separate the planes
	for (unsigned int v = 0; v < m->GetNVertices(); ++v)
	{
		float p[3];
		ASSERT_EQ(m->GetVertex(v, p), 0);

		bool matched = false;
		for (int k = 0; k < 4 && !matched; ++k)
			if (std::fabs(p[2] - expected[k]) < 1e-5f)
			{
				matched = true;
				seen.insert(k);
			}
		ASSERT_TRUE(matched) << "unexpected Z plane " << p[2] << " at vertex " << v;
	}

	// All four planes are actually present (base bottom, base top / block bottom,
	// block top, wall top).
	EXPECT_EQ(seen.size(), 4u);
}

// ---------------------------------------------------------------------------
//  Step 4: one mesh per colour
// ---------------------------------------------------------------------------

TEST(TEST_cgmesh_image_relief, per_color_returns_colors_then_base_then_wall)
{
	const char* kInputFile = "./per_color_returns_colors_then_base_then_wall.ppm";
	ASSERT_TRUE(writeTwoColorImage(kInputFile));

	const ImageReliefOptions opt = defaultOptions();
	std::vector<Mesh*> meshes = image_to_relief_per_color(kInputFile, opt);

	// Documented order: [colour_0, colour_1, base, wall].
	ASSERT_EQ(meshes.size(), 2u + 2u);
	for (Mesh* m : meshes)
	{
		ASSERT_NE(m, nullptr);
		EXPECT_GT(m->GetNFaces(), 0u);
		EXPECT_EQ(m->GetNMaterials(), 1u) << "one colour per mesh";
	}
	EXPECT_EQ(meshes[2]->GetMaterial(0)->GetName(), std::string("base"));
	EXPECT_EQ(meshes[3]->GetMaterial(0)->GetName(), std::string("wall"));

	// The base is the widest piece and the only one reaching z=0.
	float bmin[3], bmax[3];
	getBBox(*meshes[2], bmin, bmax);
	EXPECT_NEAR(bmin[2], 0.f, 1e-5f);
	EXPECT_NEAR(bmax[2], opt.baseThickness, 1e-5f);

	float cmin[3], cmax[3];
	getBBox(*meshes[0], cmin, cmax);
	EXPECT_NEAR(cmin[2], opt.baseThickness, 1e-5f);
	EXPECT_NEAR(cmax[2], opt.baseThickness + opt.blockHeight, 1e-5f);

	for (Mesh* m : meshes) delete m;
}

// ---------------------------------------------------------------------------
//  Coincident internal walls
// ---------------------------------------------------------------------------

TEST(TEST_cgmesh_image_relief, dropping_internal_walls_shrinks_mesh_not_shape)
{
	const char* kInputFile = "./dropping_internal_walls_shrinks_mesh_not_shape.ppm";
	ASSERT_TRUE(writeTwoColorImage(kInputFile));

	ImageReliefOptions opt = defaultOptions();
	std::unique_ptr<Mesh> closed(image_to_relief(kInputFile, opt));
	ASSERT_NE(closed, nullptr);

	opt.emitInternalWalls = false;
	std::unique_ptr<Mesh> trimmed(image_to_relief(kInputFile, opt));
	ASSERT_NE(trimmed, nullptr);

	// The square's boundary is shared by both blocks, so both copies of that wall
	// go away; the outer silhouette wall stays. The wall corners left orphaned are
	// compacted away by the builder.
	EXPECT_LT(trimmed->GetNFaces(),    closed->GetNFaces());
	EXPECT_LT(trimmed->GetNVertices(), closed->GetNVertices());
	EXPECT_EQ(trimmed->GetNMaterials(), closed->GetNMaterials());

	// No orphan vertex survives: every vertex is used by at least one face.
	std::set<unsigned int> used;
	for (unsigned int f = 0; f < trimmed->GetNFaces(); ++f)
		for (unsigned int k = 0; k < 3; ++k)
			used.insert((unsigned int)trimmed->GetFaceVertex(f, k));
	EXPECT_EQ(used.size(), trimmed->GetNVertices());

	float amin[3], amax[3], bmin[3], bmax[3];
	getBBox(*closed, amin, amax);
	getBBox(*trimmed, bmin, bmax);
	for (int k = 0; k < 3; ++k)
	{
		EXPECT_NEAR(amin[k], bmin[k], 1e-5f) << "axis " << k;
		EXPECT_NEAR(amax[k], bmax[k], 1e-5f) << "axis " << k;
	}
}

// ---------------------------------------------------------------------------
//  Despeckling (minRegionArea)
// ---------------------------------------------------------------------------

namespace {

// Same two-colour image, plus the two speck shapes a lossy source produces:
//   - compact blobs INSIDE the background;
//   - 1-px hatching ON the white/red boundary, i.e. specks that STRADDLE two
//     colours. That second family is the important one: it is 68% of the small
//     regions on a real JPEG, it has no matching hole contour in any neighbour,
//     and it is what an area filter on vectorized contours cannot remove without
//     punching voids in the surface.
bool writeSpeckledImage(const char* path)
{
	Img img;
	makeTwoColorImage(img);
	for (int k = 0; k < 4; ++k)
		fillRect(img, 2 + k * 7, 3, 5 + k * 7, 6, 255, 0, 0);      // blobs 3x3
	// hachures 1 px a cheval sur le bord du carre rouge (x = 10)
	for (int k = 0; k < 5; ++k)
	{
		fillRect(img, 7, 12 + k * 2, 13, 13 + k * 2, 255, 0, 0);    // rouge dans le blanc
		fillRect(img, 10, 13 + k * 2, 16, 14 + k * 2, 255, 255, 255); // blanc dans le rouge
	}
	return img.save(path) == 0;
}

// Total area of the block top caps. The blocks must TILE the content, so this is
// invariant under despeckling: removing a speck also removes the matching hole
// in its neighbour, which then covers the gap.
double topCapArea(Mesh& m, float zTop)
{
	double area = 0.;
	for (unsigned int f = 0; f < m.GetNFaces(); ++f)
	{
		float p[3][3];
		bool onTop = true;
		for (int k = 0; k < 3; ++k)
		{
			m.GetVertex((unsigned int)m.GetFaceVertex(f, (unsigned int)k), p[k]);
			if (std::fabs(p[k][2] - zTop) > 1e-5f) { onTop = false; break; }
		}
		if (!onTop) continue;
		area += std::fabs(0.5 * ((p[1][0]-p[0][0]) * (p[2][1]-p[0][1])
		                       - (p[1][1]-p[0][1]) * (p[2][0]-p[0][0])));
	}
	return area;
}

} // namespace

TEST(TEST_cgmesh_image_relief, despeckling_removes_specks_without_opening_holes)
{
	const char* speckled = "./tu_image_relief_speckled.ppm";
	ASSERT_TRUE(writeSpeckledImage(speckled));

	ImageReliefOptions opt = defaultOptions();
	const float zTop = opt.baseThickness + opt.blockHeight;

	opt.despecklePasses = 0;                       // tout garder
	opt.minRegionArea   = 0;
	std::unique_ptr<Mesh> raw(image_to_relief(speckled, opt));
	ASSERT_NE(raw, nullptr);

	opt.despecklePasses = 1;                       // defauts
	opt.minRegionArea   = 12;
	std::unique_ptr<Mesh> clean(image_to_relief(speckled, opt));
	ASSERT_NE(clean, nullptr);

	// Les mouchetis (blobs ET hachures a cheval) ont disparu.
	EXPECT_LT(clean->GetNFaces(), raw->GetNFaces()) << "les mouchetis doivent partir";

	// LE point critique : le filtrage se fait sur les ETIQUETTES, qui restent un
	// pavage complet, donc la surface ne s'ouvre pas. Le plan superieur couvre
	// toujours exactement sa bbox -- ce que la version precedente (filtrage des
	// contours par aire) violait des qu'un mouchetis etait a cheval.
	float cmin[3], cmax[3];
	getBBox(*clean, cmin, cmax);
	const double aClean = topCapArea(*clean, zTop);
	// bbox des blocs = contenu = fitSize sur le grand cote, moins la bordure
	// base+mur ajoutee de chaque cote.
	const double side = (double)opt.fitSize;
	const double border = 2.0 * (opt.margin + opt.wallThickness);
	const double content = (cmax[0] - cmin[0] - border) * (cmax[1] - cmin[1] - border);
	ASSERT_GT(content, 0.);
	EXPECT_NEAR(aClean / content, 1.0, 2e-3)
		<< "les regions doivent PAVER le contenu : aucun trou, aucun recouvrement";
	(void)side;

	// Et l'empreinte est inchangee.
	float rmin[3], rmax[3];
	getBBox(*raw, rmin, rmax);
	for (int k = 0; k < 3; ++k)
	{
		EXPECT_NEAR(rmin[k], cmin[k], 1e-5f) << "axe " << k;
		EXPECT_NEAR(rmax[k], cmax[k], 1e-5f) << "axe " << k;
	}
}

// ---------------------------------------------------------------------------
//  Retrait des régions (offset négatif)
// ---------------------------------------------------------------------------

TEST(TEST_cgmesh_image_relief, shrink_erodes_regions_without_moving_the_frame)
{
	const char* kInputFile = "./shrink_erodes_regions_without_moving_the_frame.ppm";
	ASSERT_TRUE(writeTwoColorImage(kInputFile));

	ImageReliefOptions opt = defaultOptions();
	const float zTop = opt.baseThickness + opt.blockHeight;

	opt.shrink = 0.f;
	std::unique_ptr<Mesh> full(image_to_relief(kInputFile, opt));
	ASSERT_NE(full, nullptr);

	opt.shrink = 1.5f;                       // px de la source
	std::unique_ptr<Mesh> thin(image_to_relief(kInputFile, opt));
	ASSERT_NE(thin, nullptr);

	// De la matière a été retirée : l'aire pavée par le dessus des blocs diminue.
	const double aFull = topCapArea(*full, zTop);
	const double aThin = topCapArea(*thin, zTop);
	ASSERT_GT(aFull, 0.);
	EXPECT_LT(aThin, aFull) << "un offset négatif doit retirer de la matière";

	// ... mais l'empreinte ne bouge pas : la bbox est mesurée AVANT le retrait,
	// donc ni l'échelle ni le cadre (base + mur) ne dépendent de `shrink`.
	float fmin[3], fmax[3], tmin[3], tmax[3];
	getBBox(*full, fmin, fmax);
	getBBox(*thin, tmin, tmax);
	for (int k = 0; k < 3; ++k)
	{
		EXPECT_NEAR(fmin[k], tmin[k], 1e-5f) << "axe " << k;
		EXPECT_NEAR(fmax[k], tmax[k], 1e-5f) << "axe " << k;
	}

	// Le nombre de matériaux ne change pas (aucune région entièrement résorbée
	// ici) et le mesh reste un maillage de triangles exploitable.
	EXPECT_EQ(thin->GetNMaterials(), full->GetNMaterials());
	EXPECT_TRUE(thin->IsTriangleMesh());
	EXPECT_GT(thin->GetNFaces(), 0u);
}

// Le sillon doit s'élargir de façon monotone avec le paramètre.
TEST(TEST_cgmesh_image_relief, shrink_is_monotonic)
{
	const char* kInputFile = "./shrink_is_monotonic.ppm";
	ASSERT_TRUE(writeTwoColorImage(kInputFile));

	ImageReliefOptions opt = defaultOptions();
	const float zTop = opt.baseThickness + opt.blockHeight;

	double previous = -1.;
	for (float s : { 0.f, 0.5f, 1.f, 2.f })
	{
		opt.shrink = s;
		std::unique_ptr<Mesh> m(image_to_relief(kInputFile, opt));
		ASSERT_NE(m, nullptr) << "shrink=" << s;
		const double area = topCapArea(*m, zTop);
		if (previous >= 0.)
			EXPECT_LT(area, previous) << "shrink=" << s << " doit retirer plus que le précédent";
		previous = area;
	}
}

// ---------------------------------------------------------------------------
//  Failure paths
// ---------------------------------------------------------------------------

TEST(TEST_cgmesh_image_relief, nonexistent_file_fails_cleanly)
{
	const ImageReliefOptions opt = defaultOptions();
	EXPECT_EQ(image_to_relief("./tu_image_relief_does_not_exist.ppm", opt), nullptr);
	EXPECT_TRUE(image_to_relief_per_color("./tu_image_relief_does_not_exist.ppm", opt).empty());
}

// Le cadre (plaque + mur) est desormais optionnel : un export destine a
// l'impression ne doit pouvoir contenir que les regions extrudees. Avant
// correction, appendBase/appendWall etaient appeles inconditionnellement.
TEST(TEST_cgmesh_image_relief, frame_is_optional)
{
	const char* kInputFile = "./frame_is_optional.ppm";
	ASSERT_TRUE(writeTwoColorImage(kInputFile));

	ImageReliefOptions opt = defaultOptions();
	opt.emitBase = false;
	opt.emitWall = false;

	std::vector<Mesh*> bare = image_to_relief_per_color(kInputFile, opt);
	ASSERT_FALSE(bare.empty());
	for (Mesh* m : bare)
	{
		Material* mat = m->GetMaterial(0);
		ASSERT_NE(mat, nullptr);
		EXPECT_NE(mat->GetName(), "base");
		EXPECT_NE(mat->GetName(), "wall");
	}

	Mesh* single = image_to_relief(kInputFile, opt);
	ASSERT_NE(single, nullptr);
	// Un materiau par couche de couleur, et RIEN de plus.
	EXPECT_EQ(single->GetNMaterials(), (unsigned int)bare.size());

	// L'emprise se reduit au contenu : plus de marge ni de mur.
	float vmin[3], vmax[3];
	getBBox(*single, vmin, vmax);
	EXPECT_NEAR(vmax[0] - vmin[0], opt.fitSize, 1e-3f);
	EXPECT_NEAR(vmin[2], opt.baseThickness, 1e-4f);

	for (Mesh* m : bare) delete m;
	delete single;
}
