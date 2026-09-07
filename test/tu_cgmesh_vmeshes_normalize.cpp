#include <gtest/gtest.h>
#include "../src/cgmesh/vmeshes.h"
#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/bounding_box.h"
#include <cstdio>

TEST(TEST_cgmesh_vmeshes, normalize_centers_aggregate_bbox)
{
    return; // TODO : to restore
    
    VMeshes vmeshes;
    
    auto createCube = []() {
        Mesh* m = new Mesh();
        float verts[] = {
            0, 0, 0,  1, 0, 0,  1, 1, 0,  0, 1, 0,
            0, 0, 1,  1, 0, 1,  1, 1, 1,  0, 1, 1
        };
        m->Init();
        m->SetVertices(8, verts);
        return m;
    };

    Mesh* mesh1 = createCube();
    Mesh* mesh2 = createCube();
    
    mesh2->translate(1.0f, 2.0f, 3.0f);
    
    vmeshes.AddMesh(mesh1);
    vmeshes.AddMesh(mesh2);
    
    vmeshes.Normalize();
    
    BoundingBox bbox;
    for (const auto& m : vmeshes.GetMeshes()) {
        m->computebbox();
        bbox.AddBoundingBox(m->bbox());
    }
    
    float center[3];
    bbox.GetCenter(center);
    
    EXPECT_NEAR(center[0], 0.0f, 1e-4f);
    EXPECT_NEAR(center[1], 0.0f, 1e-4f);
    EXPECT_NEAR(center[2], 0.0f, 1e-4f);
}

// ---------------------------------------------------------------------------
//  La CIBLE de la normalisation
// ---------------------------------------------------------------------------
// Rien n'ancrait le facteur d'echelle : le test ci-dessus ne verifie que le
// recentrage, et il est desactive. Or la cible est une valeur de CONVENTION --
// les unites monde du depot sont des millimetres, et la cible se lit sur la base
// de coupe, graduee en centimetres : 100 mm, soit dix graduations. Une valeur de
// convention sans test se perd au premier refactoring.
//
// Le maillage d'epreuve a des dimensions DISTINCTES sur les trois axes
// (40 x 10 x 20) : une cible appliquee par erreur a la mauvaise dimension, ou une
// mise a l'echelle non uniforme, se verrait. Un cube ne dirait rien de tout cela.
TEST(TEST_cgmesh_vmeshes, normalize_brings_the_largest_dimension_to_ten_centimetres)
{
    VMeshes vmeshes;

    Mesh* m = new Mesh();
    m->Init(3, 1);
    m->SetVertex(0,  0.f,  0.f,  0.f);
    m->SetVertex(1, 40.f, 10.f,  0.f);
    m->SetVertex(2,  0.f,  0.f, 20.f);
    m->SetFace(0, 0, 1, 2);
    vmeshes.AddMesh(m);

    vmeshes.Normalize();

    BoundingBox bbox;
    for (const auto& mesh : vmeshes.GetMeshes())
    {
        mesh->computebbox();
        bbox.AddBoundingBox(mesh->bbox());
    }
    float mn[3], mx[3];
    bbox.GetMinMax(mn, mx);

    EXPECT_NEAR(bbox.GetLargestLength(), VMeshes::kNormalizedSize, 1e-4f);
    EXPECT_NEAR(VMeshes::kNormalizedSize, 100.f, 1e-6f)
        << "la cible EST cent millimetres : dix graduations de la base de coupe";

    // Facteur 100/40 = 2,5, le MEME sur les trois axes : les proportions du
    // modele sont conservees, seule sa taille change.
    EXPECT_NEAR(mx[0] - mn[0], 100.00f, 1e-4f);
    EXPECT_NEAR(mx[1] - mn[1],  25.00f, 1e-4f);
    EXPECT_NEAR(mx[2] - mn[2],  50.00f, 1e-4f);

    // Et centre sur l'origine, ce que le test desactive ci-dessus voulait dire.
    float center[3];
    bbox.GetCenter(center);
    EXPECT_NEAR(center[0], 0.f, 1e-4f);
    EXPECT_NEAR(center[1], 0.f, 1e-4f);
    EXPECT_NEAR(center[2], 0.f, 1e-4f);
}
