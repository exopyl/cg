#include <cstdlib>
#include <gtest/gtest.h>

#include "../src/cgmesh/cgmesh.h"

TEST(TEST_slicer, constructor_and_basic_getters_along_oz)
{
    Mesh* cube = CreateCube(true);
    cube->ComputeNormals();
    Mesh_half_edge he(cube);
    he.create_half_edge();

    Cmodel3d_half_edge_sliced slicer(&he, ALONGOZ, 1.f);

    EXPECT_EQ(slicer.get_model(), &he);
    EXPECT_EQ(slicer.get_n_slices(), 2);
    EXPECT_FLOAT_EQ(slicer.get_step_slice(), 1.f);
    EXPECT_FLOAT_EQ(slicer.get_zmin(), -1.f);

    delete cube;
}

TEST(TEST_slicer, get_areas_contains_central_cube_section)
{
    Mesh* cube = CreateCube(true);
    cube->ComputeNormals();
    Mesh_half_edge he(cube);
    he.create_half_edge();

    Cmodel3d_half_edge_sliced slicer(&he, ALONGOZ, 1.f);
    float* areas = nullptr;
    int size = 0;

    slicer.get_areas(&areas, &size);

    ASSERT_NE(areas, nullptr);
    EXPECT_EQ(size, slicer.get_n_slices());

    float maxArea = 0.f;
    for (int i = 0; i < size; ++i)
        maxArea = (areas[i] > maxArea) ? areas[i] : maxArea;

    EXPECT_NEAR(maxArea, 4.f, 1e-3f);

    free(areas);
    delete cube;
}

TEST(TEST_slicer, get_slice_invalid_index_returns_empty_slice)
{
    Mesh* cube = CreateCube(true);
    cube->ComputeNormals();
    Mesh_half_edge he(cube);
    he.create_half_edge();

    Cmodel3d_half_edge_sliced slicer(&he, ALONGOZ, 1.f);
    Polygon2** slice = nullptr;
    int nContours = -1;

    slicer.get_slice(-1, &slice, &nContours);
    EXPECT_EQ(slice, nullptr);
    EXPECT_EQ(nContours, 0);

    slicer.get_slice(999, &slice, &nContours);
    EXPECT_EQ(slice, nullptr);
    EXPECT_EQ(nContours, 0);

    delete cube;
}

TEST(TEST_slicer, get_slice_returns_non_empty_contour_for_cube)
{
    Mesh* cube = CreateCube(true);
    cube->ComputeNormals();
    Mesh_half_edge he(cube);
    he.create_half_edge();

    Cmodel3d_half_edge_sliced slicer(&he, ALONGOZ, 1.f);

    bool foundContour = false;
    for (int i = 0; i < slicer.get_n_slices(); ++i)
    {
        Polygon2** slice = nullptr;
        int nContours = 0;
        slicer.get_slice(i, &slice, &nContours);
        if (nContours > 0)
        {
            ASSERT_NE(slice, nullptr);
            ASSERT_NE(slice[0], nullptr);
            EXPECT_GT(slice[0]->get_n_points(0), 0);
            foundContour = true;
            break;
        }
    }

    EXPECT_TRUE(foundContour);

    delete cube;
}

TEST(TEST_slicer, constructor_and_getters_along_ox)
{
    Mesh* cube = CreateCube(true);
    cube->ComputeNormals();
    Mesh_half_edge he(cube);
    he.create_half_edge();

    Cmodel3d_half_edge_sliced slicer(&he, ALONGOX, 1.f);

    EXPECT_EQ(slicer.get_n_slices(), 2);
    EXPECT_FLOAT_EQ(slicer.get_xmin(), -1.f);
    EXPECT_FLOAT_EQ(slicer.get_step_slice(), 1.f);

    delete cube;
}
