#include <cmath>

#include <gtest/gtest.h>

#include "../src/cgmesh/cgmesh.h"

#if 0 // TOFIX

// ---------------------------------------------------------------------------
// Helper: create a subdivided cube as Mesh_half_edge
// Creates a triangulated cube, computes normals, builds half-edge,
// then applies Loop subdivision `nSubdiv` times.
// ---------------------------------------------------------------------------
static Mesh_half_edge* make_subdivided_cube(int nSubdiv)
{
	Mesh *cube = CreateCube(true);
	cube->ComputeNormals();

	Mesh_half_edge *he = new Mesh_half_edge(cube);
	he->create_half_edge();

	MeshAlgoSubdivisionLoop subdiv;
	for (int i = 0; i < nSubdiv; i++)
	{
		subdiv.Apply(he);
		he->m_pMesh->ComputeNormals();
		he->create_half_edge();
	}

	delete cube;
	return he;
}

// ===========================================================================
// Cantzler extraction
// ===========================================================================

// ---------------------------------------------------------------------------
// TEST: Cantzler extract on subdivided cube -- detects sharp edges
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_set_lines, Cantzler_Extract_SubdividedCube)
{
	// context
	Mesh_half_edge *he = make_subdivided_cube(1);
	Cset_lines lines(he);

	// action -- threshold at 30 degrees (pi/6) to catch the cube edges
	float threshold = 3.14159f / 6.f;
	lines.cantzler_extract_edges(threshold);

	// expectations -- a cube has 12 edges, should detect lines
	int nLines = lines.get_n_extracted_lines();
	EXPECT_GT(nLines, 0) << "Cantzler should detect edges on a subdivided cube";

	// each extracted line should have at least 2 vertices
	for (int i = 0; i < nLines; i++)
	{
		Cextracted_line *line = lines.get_extracted_line(i);
		EXPECT_GE(line->get_n_vertices(), 2) << "line " << i;
	}

	delete he;
}

// ---------------------------------------------------------------------------
// TEST: Cantzler extract -- higher threshold gives fewer lines
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_set_lines, Cantzler_Extract_ThresholdEffect)
{
	// context
	Mesh_half_edge *he1 = make_subdivided_cube(1);
	Mesh_half_edge *he2 = make_subdivided_cube(1);
	Cset_lines linesLow(he1);
	Cset_lines linesHigh(he2);

	// action -- low threshold catches more edges, high threshold catches fewer
	linesLow.cantzler_extract_edges(0.1f);   // ~6 degrees
	linesHigh.cantzler_extract_edges(1.2f);   // ~69 degrees

	// expectations
	int nLow = linesLow.get_n_extracted_lines();
	int nHigh = linesHigh.get_n_extracted_lines();
	EXPECT_GE(nLow, nHigh) << "lower threshold should detect at least as many edges";

	delete he1;
	delete he2;
}

// ---------------------------------------------------------------------------
// TEST: Cantzler extract + merge
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_set_lines, Cantzler_ExtractAndMerge)
{
	// context
	Mesh_half_edge *he = make_subdivided_cube(1);
	Cset_lines lines(he);
	lines.cantzler_extract_edges(3.14159f / 6.f);
	int nBefore = lines.get_n_extracted_lines();
	ASSERT_GT(nBefore, 0);

	// action -- merge with tolerance
	int n_candidates = nBefore;
	lines.cantzler_merge_edges(n_candidates, 0.3f);

	// expectations -- merging should reduce or maintain line count
	int nAfter = lines.get_n_extracted_lines();
	EXPECT_LE(nAfter, nBefore);
	// a cube has 12 edges; after merging, we expect close to 12 lines
	EXPECT_GE(nAfter, 1);

	delete he;
}

// ===========================================================================
// Ridges and valleys extraction
// ===========================================================================

// ---------------------------------------------------------------------------
// TEST: Extract ridges and valleys on subdivided cube
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_set_lines, RidgesAndValleys_SubdividedCube)
{
	// context -- need more subdivision for curvature estimation
	Mesh_half_edge *he = make_subdivided_cube(2);
	Cset_lines lines(he);

	// action
	float kappa_epsilon = 0.1f;
	lines.extract_ridges_and_valleys(kappa_epsilon);

	// expectations -- should detect ridges on cube edges
	int nLines = lines.get_n_extracted_lines();
	EXPECT_GT(nLines, 0) << "should detect ridges on a subdivided cube";

	for (int i = 0; i < nLines; i++)
	{
		Cextracted_line *line = lines.get_extracted_line(i);
		EXPECT_GE(line->get_n_vertices(), 1) << "line " << i;
	}

	delete he;
}

// ===========================================================================
// Merge close lines
// ===========================================================================

// ---------------------------------------------------------------------------
// TEST: Cantzler extract + merge_close_lines
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_set_lines, MergeCloseLines)
{
	// context
	Mesh_half_edge *he = make_subdivided_cube(1);
	Cset_lines lines(he);
	lines.cantzler_extract_edges(3.14159f / 6.f);
	int nBefore = lines.get_n_extracted_lines();
	ASSERT_GT(nBefore, 0);

	// action -- merge lines with similar angle (cos > 0.95) and close distance
	lines.merge_close_lines(0.95f, 0.5f);

	// expectations
	int nAfter = lines.get_n_extracted_lines();
	EXPECT_LE(nAfter, nBefore);
	EXPECT_GE(nAfter, 1);

	delete he;
}

// ---------------------------------------------------------------------------
// TEST: Cantzler extract + merge_close_lines_pluecker
// ---------------------------------------------------------------------------
#if 0
TEST(TEST_cgmesh_set_lines, MergeCloseLinesPluecker)
{
	// context
	Mesh_half_edge *he = make_subdivided_cube(1);
	Cset_lines lines(he);
	lines.cantzler_extract_edges(3.14159f / 6.f);
	int nBefore = lines.get_n_extracted_lines();
	ASSERT_GT(nBefore, 0);

	// action
	lines.merge_close_lines_pluecker(0.95f, 0.5f);

	// expectations
	int nAfter = lines.get_n_extracted_lines();
	EXPECT_LE(nAfter, nBefore);
	EXPECT_GE(nAfter, 1);

	delete he;
}
#endif

// ===========================================================================
// Merge oriented vertices
// ===========================================================================

// ---------------------------------------------------------------------------
// TEST: Cantzler extract + merge_oriented_vertices
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_set_lines, MergeOrientedVertices)
{
	// context
	Mesh_half_edge *he = make_subdivided_cube(1);
	Cset_lines lines(he);
	lines.cantzler_extract_edges(3.14159f / 6.f);
	int nBefore = lines.get_n_extracted_lines();
	ASSERT_GT(nBefore, 0);

	// action -- merge vertices with similar orientation
	float tolerance_angle = 0.2f;    // radians
	float tolerance_distance = 0.5f;
	lines.merge_oriented_vertices(tolerance_angle, tolerance_distance);

	// expectations
	int nAfter = lines.get_n_extracted_lines();
	EXPECT_LE(nAfter, nBefore);
	EXPECT_GE(nAfter, 1);

	delete he;
}

// ===========================================================================
// Post-processing: extremities, least square fitting, delete isolated
// ===========================================================================

// ---------------------------------------------------------------------------
// TEST: compute_extremities does not crash and lines remain valid
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_set_lines, ComputeExtremities)
{
	// context
	Mesh_half_edge *he = make_subdivided_cube(1);
	Cset_lines lines(he);
	lines.cantzler_extract_edges(3.14159f / 6.f);
	ASSERT_GT(lines.get_n_extracted_lines(), 0);

	// action
	lines.compute_extremities();

	// expectations -- lines still valid, extremities should be finite
	for (int i = 0; i < lines.get_n_extracted_lines(); i++)
	{
		Cextracted_line *line = lines.get_extracted_line(i);
		Vector3f b, e;
		line->get_begin(b);
		line->get_end(e);
		EXPECT_TRUE(std::isfinite(b.x)) << "line " << i << " begin.x not finite";
		EXPECT_TRUE(std::isfinite(b.y)) << "line " << i << " begin.y not finite";
		EXPECT_TRUE(std::isfinite(b.z)) << "line " << i << " begin.z not finite";
		EXPECT_TRUE(std::isfinite(e.x)) << "line " << i << " end.x not finite";
		EXPECT_TRUE(std::isfinite(e.y)) << "line " << i << " end.y not finite";
		EXPECT_TRUE(std::isfinite(e.z)) << "line " << i << " end.z not finite";
	}

	delete he;
}

// ---------------------------------------------------------------------------
// TEST: apply_least_square_fitting does not crash
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_set_lines, ApplyLeastSquareFitting)
{
	// context
	Mesh_half_edge *he = make_subdivided_cube(1);
	Cset_lines lines(he);
	lines.cantzler_extract_edges(3.14159f / 6.f);
	ASSERT_GT(lines.get_n_extracted_lines(), 0);

	// action
	lines.apply_least_square_fitting();

	// expectations -- still valid
	EXPECT_GT(lines.get_n_extracted_lines(), 0);

	delete he;
}

// ---------------------------------------------------------------------------
// TEST: delete_isolated_lines removes small lines
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_set_lines, DeleteIsolatedLines)
{
	// context
	Mesh_half_edge *he = make_subdivided_cube(1);
	Cset_lines lines(he);
	lines.cantzler_extract_edges(3.14159f / 6.f);
	int nBefore = lines.get_n_extracted_lines();
	ASSERT_GT(nBefore, 0);

	// action -- delete lines with fewer than 3 vertices
	lines.delete_isolated_lines(3);

	// expectations
	int nAfter = lines.get_n_extracted_lines();
	EXPECT_LE(nAfter, nBefore);

	// remaining lines should all have >= 3 vertices
	for (int i = 0; i < nAfter; i++)
	{
		Cextracted_line *line = lines.get_extracted_line(i);
		EXPECT_GE(line->get_n_vertices(), 3) << "line " << i << " should have been deleted";
	}

	delete he;
}

// ===========================================================================
// Reinit
// ===========================================================================

// ---------------------------------------------------------------------------
// TEST: reinit clears all extracted lines
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_set_lines, Reinit)
{
	// context
	Mesh_half_edge *he = make_subdivided_cube(1);
	Cset_lines lines(he);
	lines.cantzler_extract_edges(3.14159f / 6.f);
	ASSERT_GT(lines.get_n_extracted_lines(), 0);

	// action
	lines.reinit();

	// expectations
	EXPECT_EQ(lines.get_n_extracted_lines(), 0);

	delete he;
}

// ===========================================================================
// Full pipeline
// ===========================================================================

// ---------------------------------------------------------------------------
// TEST: Cantzler full pipeline -- extract, merge, fit, filter
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_set_lines, Cantzler_FullPipeline)
{
	// context
	Mesh_half_edge *he = make_subdivided_cube(2);
	Cset_lines lines(he);

	// action -- full pipeline
	lines.cantzler_extract_edges(3.14159f / 6.f);
	int nExtracted = lines.get_n_extracted_lines();
	ASSERT_GT(nExtracted, 0);

	lines.merge_close_lines(0.95f, 0.3f);
	int nMerged = lines.get_n_extracted_lines();

	lines.compute_extremities();
	lines.apply_least_square_fitting();
	lines.delete_isolated_lines(2);
	int nFinal = lines.get_n_extracted_lines();

	// expectations
	EXPECT_LE(nMerged, nExtracted);
	EXPECT_LE(nFinal, nMerged);
	// a cube has 12 edges; after full pipeline, should be in a reasonable range
	EXPECT_GE(nFinal, 1);

	// verify line properties
	for (int i = 0; i < nFinal; i++)
	{
		Cextracted_line *line = lines.get_extracted_line(i);
		EXPECT_GT(line->get_length(), 0.f) << "line " << i << " zero length";
		EXPECT_GE(line->get_n_vertices(), 2) << "line " << i;
	}

	delete he;
}

// ---------------------------------------------------------------------------
// TEST: Visualization -- compute_colors does not crash
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_set_lines, ComputeColors)
{
	// context
	Mesh_half_edge *he = make_subdivided_cube(1);
	Cset_lines lines(he);
	lines.cantzler_extract_edges(3.14159f / 6.f);
	ASSERT_GT(lines.get_n_extracted_lines(), 0);

	// action
	lines.compute_colors();
	float *colors = lines.get_colors();

	// expectations
	EXPECT_NE(colors, nullptr);

	delete he;
}

// ---------------------------------------------------------------------------
// TEST: Extracted line properties -- length and density are positive
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_set_lines, ExtractedLine_Properties)
{
	// context
	Mesh_half_edge *he = make_subdivided_cube(1);
	Cset_lines lines(he);
	lines.cantzler_extract_edges(3.14159f / 6.f);
	lines.merge_close_lines(0.95f, 0.5f);
	ASSERT_GT(lines.get_n_extracted_lines(), 0);

	// action & expectations
	for (int i = 0; i < lines.get_n_extracted_lines(); i++)
	{
		Cextracted_line *line = lines.get_extracted_line(i);
		EXPECT_GT(line->get_length(), 0.f) << "line " << i;
		EXPECT_GT(line->get_density(), 0.f) << "line " << i;
		EXPECT_GE(line->get_mean_deviation(), 0.f) << "line " << i;
		EXPECT_GE(line->get_weight(), 0.f) << "line " << i;
	}

	delete he;
}

#endif
