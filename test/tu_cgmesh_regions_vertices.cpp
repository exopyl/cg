#include <gtest/gtest.h>

#include "../src/cgmesh/cgmesh.h"

static Mesh_half_edge* make_square_half_edge_vertices()
{
    Mesh* mesh = new Mesh(4, 2);
    mesh->SetVertex(0, 0.f, 0.f, 0.f);
    mesh->SetVertex(1, 1.f, 0.f, 0.f);
    mesh->SetVertex(2, 1.f, 1.f, 0.f);
    mesh->SetVertex(3, 0.f, 1.f, 0.f);
    mesh->SetFace(0, 0, 1, 2);
    mesh->SetFace(1, 0, 2, 3);
    mesh->ComputeNormals();

    Mesh_half_edge* he = new Mesh_half_edge(mesh);
    he->create_half_edge();
    he->m_pMesh->InitVertexColors(0.5f, 0.5f, 0.5f);

    delete mesh;
    return he;
}

static int count_selected_vertices(const int* selected, int size)
{
    int count = 0;
    for (int i = 0; i < size; ++i)
        count += (selected[i] == 1) ? 1 : 0;
    return count;
}

TEST(TEST_regions_vertices, constructor_and_size)
{
    Mesh_half_edge* he = make_square_half_edge_vertices();
    Cregions_vertices regions(he);

    EXPECT_EQ(regions.get_size(), 4);

    delete he;
}

TEST(TEST_regions_vertices, set_datas_and_get_extremas)
{
    Mesh_half_edge* he = make_square_half_edge_vertices();
    Cregions_vertices regions(he);
    float values[4] = {1.f, 4.f, -2.f, 3.f};
    float minValue = 0.f;
    float maxValue = 0.f;

    regions.set_datas(values, 4);
    regions.get_extremas(&minValue, &maxValue);

    EXPECT_FLOAT_EQ(regions.get_datas()[0], 1.f);
    EXPECT_FLOAT_EQ(regions.get_datas()[1], 4.f);
    EXPECT_FLOAT_EQ(regions.get_datas()[2], -2.f);
    EXPECT_FLOAT_EQ(regions.get_datas()[3], 3.f);
    EXPECT_FLOAT_EQ(minValue, -2.f);
    EXPECT_FLOAT_EQ(maxValue, 4.f);

    delete he;
}

TEST(TEST_regions_vertices, select_threshold_up_then_select_all_regions)
{
    Mesh_half_edge* he = make_square_half_edge_vertices();
    Cregions_vertices regions(he);
    float values[4] = {1.f, 4.f, -2.f, 3.f};

    regions.set_datas(values, 4);
    regions.select_threshold_up(2.5f);
    regions.select_all_regions();

    int* selected = regions.get_selected_region();
    ASSERT_NE(selected, nullptr);
    EXPECT_EQ(count_selected_vertices(selected, regions.get_size()), 2);
    EXPECT_EQ(selected[0], 0);
    EXPECT_EQ(selected[1], 1);
    EXPECT_EQ(selected[2], 0);
    EXPECT_EQ(selected[3], 1);

    delete he;
}

TEST(TEST_regions_vertices, select_n_down_marks_lowest_values)
{
    Mesh_half_edge* he = make_square_half_edge_vertices();
    Cregions_vertices regions(he);
    float values[4] = {1.f, 4.f, -2.f, 3.f};

    regions.set_datas(values, 4);
    regions.select_n_down(50.f);
    regions.select_all_regions();

    int* selected = regions.get_selected_region();
    ASSERT_NE(selected, nullptr);
    EXPECT_EQ(count_selected_vertices(selected, regions.get_size()), 2);
    EXPECT_EQ(selected[0], 1);
    EXPECT_EQ(selected[1], 0);
    EXPECT_EQ(selected[2], 1);
    EXPECT_EQ(selected[3], 0);

    delete he;
}

TEST(TEST_regions_vertices, select_and_deselect_vertex)
{
    Mesh_half_edge* he = make_square_half_edge_vertices();
    Cregions_vertices regions(he);

    regions.deselect();
    regions.select_vertex(2);
    EXPECT_EQ(regions.get_selected_region()[2], 1);

    regions.deselect_vertex(2);
    EXPECT_EQ(regions.get_selected_region()[2], 0);

    delete he;
}

TEST(TEST_regions_vertices, refresh_colors_uses_selected_region_color)
{
    Mesh_half_edge* he = make_square_half_edge_vertices();
    Cregions_vertices regions(he);
    float values[4] = {1.f, 4.f, -2.f, 3.f};

    regions.set_datas(values, 4);
    regions.select_threshold_up(2.5f);
    regions.select_all_regions();
    regions.refresh_colors();

    float* colors = he->m_pMesh->m_pVertexColors.data();
    ASSERT_NE(colors, nullptr);

    EXPECT_FLOAT_EQ(colors[0], 0.5f);
    EXPECT_FLOAT_EQ(colors[1], 0.5f);
    EXPECT_FLOAT_EQ(colors[2], 0.5f);

    EXPECT_FLOAT_EQ(colors[3], 1.f);
    EXPECT_FLOAT_EQ(colors[4], 1.f);
    EXPECT_FLOAT_EQ(colors[5], 1.f);

    EXPECT_FLOAT_EQ(colors[9], 1.f);
    EXPECT_FLOAT_EQ(colors[10], 1.f);
    EXPECT_FLOAT_EQ(colors[11], 1.f);

    delete he;
}
