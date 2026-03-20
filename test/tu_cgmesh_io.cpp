#include <gtest/gtest.h>

#include "../src/cgmesh/cgmesh.h"


TEST(TEST_cgmesh_io, obj)
{
    // context
    Mesh_half_edge* mesh = new Mesh_half_edge();

    // action
    mesh->load("./test/data/cube.obj");

    // expectations
    EXPECT_EQ(mesh->GetNVertices(), 8);
    EXPECT_EQ(mesh->GetNFaces(), 12);
}

TEST(TEST_cgmesh_io, off)
{
    // context
    Mesh_half_edge* mesh = new Mesh_half_edge();

    // action
    mesh->load("./test/data/cube.off");

    // expectations
    EXPECT_EQ(mesh->GetNVertices(), 8);
    EXPECT_EQ(mesh->GetNFaces(), 12);
}

TEST(TEST_cgmesh_io, stl)
{
    // context
    Mesh_half_edge* mesh = new Mesh_half_edge();

    // action
    mesh->load("./test/data/BunnyLowPoly.stl");

    // expectations
    EXPECT_EQ(mesh->GetNVertices(), 1986);
    EXPECT_EQ(mesh->GetNFaces(), 662);

    // export
    return;

    mesh->export_statistics("stats.html");
}

TEST(TEST_cgmesh_io, 3ds_sink)
{
    // context
    Object3D* object = new Object3D();

    // action
    object->import_file("./test/data/sink.3ds");

    // expectations
    auto& list = object->GetMeshes();
    ASSERT_EQ(list.size(), 4);
}

TEST(TEST_cgmesh_io, 3ds_display)
{
    // context
    Object3D* object = new Object3D();

    // action
    object->import_file("./test/data/display.3ds");

    // expectations
    auto& list = object->GetMeshes();
    ASSERT_EQ(list.size(), 3);
}

TEST(TEST_cgmesh_io, 3ds_floppy)
{
    // context
    Object3D* object = new Object3D();

    // action
    object->import_file("./test/data/floppy.3ds");

    // expectations
    auto& list = object->GetMeshes();
    ASSERT_EQ(list.size(), 1);
}

TEST(TEST_cgmesh_io, ply)
{
    // context
    Object3D* object = new Object3D();

    // action
    object->import_file("./test/data/sofa.ply");

    // expectations
    auto& list = object->GetMeshes();
    ASSERT_EQ(list.size(), 1);
}

TEST(TEST_cgmesh_io, ply_ascii)
{
    // context
    Object3D* object = new Object3D();

    // action
    object->import_file("./test/data/sofa_ascii.ply");

    // expectations
    auto& list = object->GetMeshes();
    ASSERT_EQ(list.size(), 1);
}
