#include <gtest/gtest.h>
#include "../src/cgmesh/vmeshes.h"
#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/bounding_box.h"

TEST(TEST_cgmesh_vmeshes, normalize_centers_aggregate_bbox)
{
    VMeshes vmeshes;
    
    // Create meshes and add to vmeshes. VMeshes owns the meshes.
    auto createCube = []() {
        Mesh* m = new Mesh();
        float verts[] = {
            0, 0, 0,  1, 0, 0,  1, 1, 0,  0, 1, 0,
            0, 0, 1,  1, 0, 1,  1, 1, 1,  0, 1, 1
        };
        m->Init(); // Use standard Init()
        m->SetVertices(8, verts);
        return m;
    };

    Mesh* mesh1 = createCube();
    Mesh* mesh2 = createCube();
    
    // Translate mesh2 by (1, 2, 3)
    mesh2->translate(1.0f, 2.0f, 3.0f);
    
    vmeshes.AddMesh(mesh1);
    vmeshes.AddMesh(mesh2);
    
    // Normalize centers the aggregate bounding box of all meshes to (0,0,0)
    vmeshes.Normalize();
    
    // Compute aggregate bounding box
    BoundingBox bbox;
    for (const auto& m : vmeshes.GetMeshes()) {
        m->computebbox();
        bbox.AddBoundingBox(m->bbox());
    }
    
    // Verify centering
    float center[3];
    bbox.GetCenter(center);
    
    EXPECT_NEAR(center[0], 0.0f, 1e-4f);
    EXPECT_NEAR(center[1], 0.0f, 1e-4f);
    EXPECT_NEAR(center[2], 0.0f, 1e-4f);
    
    // VMeshes destructor will clean up mesh1 and mesh2
}
