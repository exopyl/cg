#include <cmath>
#include <gtest/gtest.h>

#include "../src/cgmesh/cgmesh.h"

static Mesh_half_edge* make_square_half_edge_faces()
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
    he->m_pMesh->ComputeNormals();
    he->m_pMesh->InitVertexColors(0.5f, 0.5f, 0.5f);

    delete mesh;
    return he;
}

static int count_selected_faces(const int* selected, int size)
{
    int count = 0;
    for (int i = 0; i < size; ++i)
        count += (selected[i] == 1) ? 1 : 0;
    return count;
}

TEST(TEST_regions_faces, constructor_getters_and_init)
{
    Mesh_half_edge* he = make_square_half_edge_faces();
    Cregions_faces regions(he);
    float data[2] = {10.f, 20.f};

    EXPECT_EQ(regions.get_size(), 2);
    EXPECT_EQ(regions.get_mesh_half_edge(), he);

    regions.init(data, 2);

    delete he;
}

TEST(TEST_regions_faces, select_face_and_deselect_face)
{
    Mesh_half_edge* he = make_square_half_edge_faces();
    Cregions_faces regions(he);

    regions.deselect();
    regions.select_face(1);
    EXPECT_EQ(regions.get_selected_region()[0], 0);
    EXPECT_EQ(regions.get_selected_region()[1], 1);

    regions.deselect_face(1);
    EXPECT_EQ(regions.get_selected_region()[1], 0);

    delete he;
}

TEST(TEST_regions_faces, select_faces_by_region_id)
{
    Mesh_half_edge* he = make_square_half_edge_faces();
    Cregions_faces regions(he);
    int* ids = regions.get_regions();

    ids[0] = 7;
    ids[1] = 3;
    regions.deselect();
    regions.select_faces_by_id_region(7);

    EXPECT_EQ(regions.get_selected_region()[0], 1);
    EXPECT_EQ(regions.get_selected_region()[1], 0);

    delete he;
}

TEST(TEST_regions_faces, select_and_deselect_connected_region)
{
    Mesh_half_edge* he = make_square_half_edge_faces();
    Cregions_faces regions(he);
    int* ids = regions.get_regions();

    ids[0] = 4;
    ids[1] = 4;

    regions.deselect();
    regions.select_faces(0);
    EXPECT_EQ(count_selected_faces(regions.get_selected_region(), regions.get_size()), 2);

    regions.deselect_faces(1);
    EXPECT_EQ(count_selected_faces(regions.get_selected_region(), regions.get_size()), 0);

    delete he;
}

TEST(TEST_regions_faces, plane_fitting_on_coplanar_faces)
{
    Mesh_half_edge* he = make_square_half_edge_faces();
    Cregions_faces regions(he);

    regions.deselect();
    regions.select_face(0);
    regions.select_face(1);

    Plane* plane = regions.plane_fitting();
    ASSERT_NE(plane, nullptr);

    Vector3f normal;
    plane->get_normale(normal);

    EXPECT_NEAR(plane->distance_point(Vector3f(0.f, 0.f, 0.f)), 0.f, 1e-5f);
    EXPECT_NEAR(plane->distance_point(Vector3f(1.f, 1.f, 0.f)), 0.f, 1e-5f);
    EXPECT_NEAR(std::fabs(normal.z), 1.f, 1e-5f);

    delete plane;
    delete he;
}

TEST(TEST_regions_faces, refresh_colors_marks_selected_face_vertices)
{
    Mesh_half_edge* he = make_square_half_edge_faces();
    Cregions_faces regions(he);
    int* ids = regions.get_regions();

    ids[0] = 0;
    ids[1] = 1;
    regions.deselect();
    regions.select_face(0);
    regions.refresh_colors();

    float* colors = he->m_pMesh->m_pVertexColors.data();
    ASSERT_NE(colors, nullptr);

    EXPECT_FLOAT_EQ(colors[0], 1.f);
    EXPECT_FLOAT_EQ(colors[1], 1.f);
    EXPECT_FLOAT_EQ(colors[2], 0.f);

    EXPECT_FLOAT_EQ(colors[3], 1.f);
    EXPECT_FLOAT_EQ(colors[4], 1.f);
    EXPECT_FLOAT_EQ(colors[5], 0.f);

    EXPECT_FLOAT_EQ(colors[6], 1.f);
    EXPECT_FLOAT_EQ(colors[7], 1.f);
    EXPECT_FLOAT_EQ(colors[8], 0.f);

    delete he;
}
