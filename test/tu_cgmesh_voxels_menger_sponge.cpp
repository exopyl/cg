#include <cmath>
#include <cstdio>

#include <gtest/gtest.h>

#include "../src/cgmesh/voxels_menger_sponge.h"

// ---------------------------------------------------------------------------
// Helper: count activated voxels in a Voxels grid
// ---------------------------------------------------------------------------
static unsigned int count_activated(const Voxels &v)
{
	unsigned int count = 0;
	for (unsigned int i = 0; i < v.get_nx(); i++)
		for (unsigned int j = 0; j < v.get_ny(); j++)
			for (unsigned int k = 0; k < v.get_nz(); k++)
				if (v.is_activated(i, j, k))
					count++;
	return count;
}

// ---------------------------------------------------------------------------
// Helper: theoretical number of activated voxels for a Menger sponge
// At each level, each activated voxel is subdivided into 3x3x3 = 27,
// but 7 are removed (the center of each face + the center), keeping 20.
// So at level L: activated = 20^L
// ---------------------------------------------------------------------------
static unsigned int expected_activated(unsigned int level)
{
	return (unsigned int)pow(20., (int)level);
}

// ===========================================================================
// MengerSponge construction tests
// ===========================================================================

// ---------------------------------------------------------------------------
// TEST: Level 1 -- grid size and activated count
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_voxels_menger_sponge, Level1_GridSize)
{
	// context

	// action
	MengerSponge sponge(1);

	// expectations
	EXPECT_EQ(sponge.get_nx(), 3u);
	EXPECT_EQ(sponge.get_ny(), 3u);
	EXPECT_EQ(sponge.get_nz(), 3u);
}

TEST(TEST_cgmesh_voxels_menger_sponge, Level1_ActivatedCount)
{
	// context

	// action
	MengerSponge sponge(1);
	unsigned int activated = count_activated(sponge);

	// expectations -- 20 voxels activated out of 27
	EXPECT_EQ(activated, expected_activated(1));
}

// ---------------------------------------------------------------------------
// TEST: Level 1 -- center voxel is NOT activated
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_voxels_menger_sponge, Level1_CenterNotActivated)
{
	// context

	// action
	MengerSponge sponge(1);

	// expectations -- the 7 removed voxels: center of each face + absolute center
	EXPECT_FALSE(sponge.is_activated(1, 1, 0));
	EXPECT_FALSE(sponge.is_activated(1, 1, 2));
	EXPECT_FALSE(sponge.is_activated(1, 0, 1));
	EXPECT_FALSE(sponge.is_activated(1, 2, 1));
	EXPECT_FALSE(sponge.is_activated(0, 1, 1));
	EXPECT_FALSE(sponge.is_activated(2, 1, 1));
	EXPECT_FALSE(sponge.is_activated(1, 1, 1));
}

// ---------------------------------------------------------------------------
// TEST: Level 1 -- corner voxels ARE activated
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_voxels_menger_sponge, Level1_CornersActivated)
{
	// context

	// action
	MengerSponge sponge(1);

	// expectations -- all 8 corners should be activated
	EXPECT_TRUE(sponge.is_activated(0, 0, 0));
	EXPECT_TRUE(sponge.is_activated(2, 0, 0));
	EXPECT_TRUE(sponge.is_activated(0, 2, 0));
	EXPECT_TRUE(sponge.is_activated(2, 2, 0));
	EXPECT_TRUE(sponge.is_activated(0, 0, 2));
	EXPECT_TRUE(sponge.is_activated(2, 0, 2));
	EXPECT_TRUE(sponge.is_activated(0, 2, 2));
	EXPECT_TRUE(sponge.is_activated(2, 2, 2));
}

// ---------------------------------------------------------------------------
// TEST: Level 2 -- grid size and activated count
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_voxels_menger_sponge, Level2_GridSize)
{
	// context

	// action
	MengerSponge sponge(2);

	// expectations
	EXPECT_EQ(sponge.get_nx(), 9u);
	EXPECT_EQ(sponge.get_ny(), 9u);
	EXPECT_EQ(sponge.get_nz(), 9u);
}

TEST(TEST_cgmesh_voxels_menger_sponge, Level2_ActivatedCount)
{
	// context

	// action
	MengerSponge sponge(2);
	unsigned int activated = count_activated(sponge);

	// expectations -- 20^2 = 400
	EXPECT_EQ(activated, expected_activated(2));
}

// ---------------------------------------------------------------------------
// TEST: Level 3 -- grid size and activated count
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_voxels_menger_sponge, Level3_GridSize)
{
	// context

	// action
	MengerSponge sponge(3);

	// expectations
	EXPECT_EQ(sponge.get_nx(), 27u);
	EXPECT_EQ(sponge.get_ny(), 27u);
	EXPECT_EQ(sponge.get_nz(), 27u);
}

TEST(TEST_cgmesh_voxels_menger_sponge, Level3_ActivatedCount)
{
	// context

	// action
	MengerSponge sponge(3);
	unsigned int activated = count_activated(sponge);

	// expectations -- 20^3 = 8000
	EXPECT_EQ(activated, expected_activated(3));
}

// ---------------------------------------------------------------------------
// TEST: Level 2 -- self-similarity: sub-block at (0,0,0) matches level 1
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_voxels_menger_sponge, Level2_SelfSimilarity)
{
	// context
	MengerSponge level1(1);
	MengerSponge level2(2);

	// action & expectations
	// The 3x3x3 sub-block at (0,0,0) in level 2 should match level 1
	for (unsigned int i = 0; i < 3; i++)
		for (unsigned int j = 0; j < 3; j++)
			for (unsigned int k = 0; k < 3; k++)
				EXPECT_EQ(level2.is_activated(i, j, k), level1.is_activated(i, j, k))
					<< "mismatch at (" << i << "," << j << "," << k << ")";
}

// ===========================================================================
// Voxels::triangulate tests
// ===========================================================================

// ---------------------------------------------------------------------------
// TEST: triangulate level 1 -- produces a valid OBJ file
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_voxels_menger_sponge, Triangulate_Level1_ProducesFile)
{
	// context
	MengerSponge sponge(1);
	char *filename = (char *)"menger_sponge_level1.obj";

	// action
	int ret = sponge.triangulate(filename);

	// expectations
	EXPECT_EQ(ret, 0);
	FILE *f = fopen(filename, "r");
	EXPECT_NE(f, nullptr);
	if (f)
		fclose(f);
}

// ---------------------------------------------------------------------------
// TEST: triangulate level 1 -- OBJ contains vertices and faces
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_voxels_menger_sponge, Triangulate_Level1_HasVerticesAndFaces)
{
	// context
	MengerSponge sponge(1);
	char *filename = (char *)"menger_sponge_level1_check.obj";
	sponge.triangulate(filename);

	// action -- parse OBJ to count vertices and faces
	FILE *f = fopen(filename, "r");
	ASSERT_NE(f, nullptr);
	unsigned int nVertices = 0;
	unsigned int nFaces = 0;
	char line[256];
	while (fgets(line, sizeof(line), f))
	{
		if (line[0] == 'v' && line[1] == ' ')
			nVertices++;
		else if (line[0] == 'f' && line[1] == ' ')
			nFaces++;
	}
	fclose(f);

	// expectations
	EXPECT_GT(nVertices, 0u);
	EXPECT_GT(nFaces, 0u);
}

// ---------------------------------------------------------------------------
// TEST: triangulate level 2 -- more faces than level 1
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_voxels_menger_sponge, Triangulate_Level2_MoreFacesThanLevel1)
{
	// context
	char *file1 = (char *)"menger_tri_l1.obj";
	char *file2 = (char *)"menger_tri_l2.obj";
	MengerSponge sponge1(1);
	MengerSponge sponge2(2);
	sponge1.triangulate(file1);
	sponge2.triangulate(file2);

	// action -- count faces in both files
	auto count_faces = [](const char *filename) -> unsigned int {
		unsigned int count = 0;
		FILE *f = fopen(filename, "r");
		if (!f) return 0;
		char line[256];
		while (fgets(line, sizeof(line), f))
			if (line[0] == 'f' && line[1] == ' ')
				count++;
		fclose(f);
		return count;
	};

	unsigned int faces1 = count_faces(file1);
	unsigned int faces2 = count_faces(file2);

	// expectations -- level 2 has more detail
	EXPECT_GT(faces2, faces1);
}

// ---------------------------------------------------------------------------
// TEST: triangulate -- OBJ face indices reference valid vertices
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_voxels_menger_sponge, Triangulate_Level1_ValidFaceIndices)
{
	// context
	MengerSponge sponge(1);
	char *filename = (char *)"menger_sponge_indices.obj";
	sponge.triangulate(filename);

	// action -- parse and validate
	FILE *f = fopen(filename, "r");
	ASSERT_NE(f, nullptr);
	unsigned int nVertices = 0;
	unsigned int maxIndex = 0;
	char line[512];
	while (fgets(line, sizeof(line), f))
	{
		if (line[0] == 'v' && line[1] == ' ')
		{
			nVertices++;
		}
		else if (line[0] == 'f' && line[1] == ' ')
		{
			// parse quad face indices: "f i1 i2 i3 i4" or "f i1/t1 i2/t2 ..."
			unsigned int idx[4];
			int parsed = sscanf(line, "f %u %u %u %u", &idx[0], &idx[1], &idx[2], &idx[3]);
			if (parsed < 4)
				parsed = sscanf(line, "f %u/%*d %u/%*d %u/%*d %u/%*d", &idx[0], &idx[1], &idx[2], &idx[3]);
			if (parsed >= 4)
			{
				for (int i = 0; i < 4; i++)
					if (idx[i] > maxIndex)
						maxIndex = idx[i];
			}
		}
	}
	fclose(f);

	// expectations -- OBJ indices are 1-based, so max index <= nVertices
	EXPECT_GT(nVertices, 0u);
	EXPECT_LE(maxIndex, nVertices);
}

// ---------------------------------------------------------------------------
// TEST: triangulate -- all faces are quads (4 vertices per face)
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_voxels_menger_sponge, Triangulate_Level1_AllFacesAreQuads)
{
	// context
	MengerSponge sponge(1);
	char *filename = (char *)"menger_sponge_quads.obj";
	sponge.triangulate(filename);

	// action
	FILE *f = fopen(filename, "r");
	ASSERT_NE(f, nullptr);
	char line[512];
	unsigned int nFaces = 0;
	bool allQuads = true;
	while (fgets(line, sizeof(line), f))
	{
		if (line[0] == 'f' && line[1] == ' ')
		{
			nFaces++;
			// count tokens after 'f'
			unsigned int tokenCount = 0;
			char *tok = strtok(line + 2, " \t\n");
			while (tok)
			{
				tokenCount++;
				tok = strtok(nullptr, " \t\n");
			}
			if (tokenCount != 4)
				allQuads = false;
		}
	}
	fclose(f);

	// expectations
	EXPECT_GT(nFaces, 0u);
	EXPECT_TRUE(allQuads);
}
