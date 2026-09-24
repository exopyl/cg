#include <gtest/gtest.h>

#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/mesh_solidify.h"
#include "../src/cgmesh/parameterized.h"
#include "../src/cgmesh/parameterized_shapes.h"
#include "../src/cgmesh/parametric_catalog.h"

// ===========================================================================
//  mesh_solidify.cpp — epaississement centre (option « Extrude » de maker)
// ===========================================================================

namespace
{
    // Carre unite dans le plan z=0, normale +z, sommets partages.
    Mesh makeSquare()
    {
        const float v[] = {0,0,0,  1,0,0,  1,1,0,  0,1,0};
        unsigned int f[] = {0,1,2,  0,2,3};
        Mesh m;
        m.SetVertices(4, v);
        m.SetFaces(2, 3, f);
        return m;
    }

    void bbox(const Mesh& m, float lo[3], float hi[3])
    {
        for (int k = 0; k < 3; k++) { lo[k] = 1e30f; hi[k] = -1e30f; }
        for (unsigned int i = 0; i < m.GetNVertices(); i++)
        {
            float p[3];
            m.GetVertex(i, p);
            for (int k = 0; k < 3; k++) { lo[k] = std::min(lo[k], p[k]); hi[k] = std::max(hi[k], p[k]); }
        }
    }

    // Bords libres apres soudure geometrique : vide <=> solide ferme.
    size_t bordersAfterWeld(const Mesh& m)
    {
        Mesh w = m;
        w.MergeVertices(1e-5f);
        std::vector<unsigned int> nonManifold, borders;
        w.GetTopologicIssues(nonManifold, borders);
        return borders.size();
    }

    bool allFinite(const Mesh& m)
    {
        for (unsigned int i = 0; i < m.GetNVertices(); i++)
        {
            float p[3];
            m.GetVertex(i, p);
            for (float c : p) if (!std::isfinite(c)) return false;
        }
        return true;
    }

    Parameter* findParam(std::vector<Parameter>& ps, const std::string& name)
    {
        for (Parameter& p : ps) if (p.GetName() == name) return &p;
        return nullptr;
    }
}

TEST(TU_cgmesh_solidify, nonPositiveThicknessCopiesInput)
{
    Mesh in = makeSquare(), out;
    EXPECT_FALSE(SolidifyMesh(in, 0.f, out));
    EXPECT_EQ(out.GetNVertices(), 4u);
    EXPECT_EQ(out.GetNFaces(), 2u);
}

TEST(TU_cgmesh_solidify, openSquareBecomesClosedCenteredSlab)
{
    Mesh in = makeSquare(), out;
    ASSERT_TRUE(SolidifyMesh(in, 0.2f, out));

    // 2 faces + 2 faces retournees + 4 bords x 2 triangles.
    EXPECT_EQ(out.GetNVertices(), 8u);
    EXPECT_EQ(out.GetNFaces(), 12u);

    float lo[3], hi[3];
    bbox(out, lo, hi);
    EXPECT_NEAR(lo[2], -0.1f, 1e-6f);   // epaisseur centree
    EXPECT_NEAR(hi[2],  0.1f, 1e-6f);
    EXPECT_NEAR(hi[0] - lo[0], 1.f, 1e-6f);

    EXPECT_EQ(bordersAfterWeld(out), 0u);
}

TEST(TU_cgmesh_solidify, cubeGetsUniformThicknessAtCorners)
{
    auto shape = MakeParametricShape("Cube");
    Mesh* cube = dynamic_cast<ParameterizedMesh*>(shape.get())->GetMesh();
    ASSERT_NE(cube, nullptr);
    float lo0[3], hi0[3];
    bbox(*cube, lo0, hi0);

    Mesh out;
    ASSERT_TRUE(SolidifyMesh(*cube, 0.1f, out));
    float lo[3], hi[3];
    bbox(out, lo, hi);
    // Sans le facteur 1/cos, le coin ne sortirait que de 0.05/sqrt(3) par axe.
    for (int k = 0; k < 3; k++)
    {
        EXPECT_NEAR(hi[k] - lo[k], (hi0[k] - lo0[k]) + 0.1f, 1e-4f) << "axe " << k;
    }
    EXPECT_EQ(bordersAfterWeld(out), 0u);
}

TEST(TU_cgmesh_solidify, sphereSeamGetsNoWallAndStaysClosed)
{
    auto shape = MakeParametricShape("Sphere");
    Mesh* sphere = dynamic_cast<ParameterizedMesh*>(shape.get())->GetMesh();
    ASSERT_NE(sphere, nullptr);

    Mesh out;
    ASSERT_TRUE(SolidifyMesh(*sphere, 0.1f, out));
    ASSERT_TRUE(allFinite(out));
    // Deux coques fermees (exterieure + interieure), chacune sans bord : les
    // copies de la couture ont recu exactement le meme decalage.
    EXPECT_EQ(bordersAfterWeld(out), 0u);
}

TEST(TU_cgmesh_solidify, mobiusStripClosesAcrossItsSeam)
{
    auto shape = MakeParametricShape("Mobius Strip");
    Mesh* strip = dynamic_cast<ParameterizedMesh*>(shape.get())->GetMesh();
    ASSERT_NE(strip, nullptr);

    Mesh out;
    ASSERT_TRUE(SolidifyMesh(*strip, 0.05f, out));
    EXPECT_TRUE(allFinite(out));
    EXPECT_GT(out.GetNFaces(), 2u * strip->GetNFaces());   // parois du bord libre
    // Pas de cloison a la couture : les faces + et - s'y raccordent, le ruban
    // epaissi est un solide ferme une fois soude.
    EXPECT_EQ(bordersAfterWeld(out), 0u);
}

TEST(TU_cgmesh_solidify, decoratorAddsParamsAndKeepsName)
{
    ParameterizedExtruded ext(MakeParametricShape("Sphere"));
    EXPECT_EQ(ext.GetName(), "Sphere");

    std::vector<Parameter> ps = ext.GetParameters();
    ASSERT_NE(findParam(ps, "nu"), nullptr);            // parametres du generateur
    ASSERT_NE(findParam(ps, "Extrude"), nullptr);
    ASSERT_NE(findParam(ps, "Extrude distance"), nullptr);

    const unsigned int nvPlain = ext.GetMesh()->GetNVertices();
    findParam(ps, "Extrude")->SetBool(true);
    ext.Regenerate();
    EXPECT_EQ(ext.GetMesh()->GetNVertices(), 2u * nvPlain);

    // Un parametre du GENERATEUR le rejoue.
    findParam(ps, "nu")->SetInt(findParam(ps, "nu")->GetInt() + 4);
    ext.Regenerate();
    EXPECT_GT(ext.GetMesh()->GetNVertices(), 2u * nvPlain);
}

// Tout le catalogue, aux valeurs par defaut : l'epaississement produit une
// geometrie finie et non vide. Couvre d'office une forme ajoutee plus tard.
TEST(TU_cgmesh_solidify, everyCatalogShapeSolidifiesToFiniteGeometry)
{
    for (const ParametricShapeEntry& e : ParametricShapes())
    {
        auto shape = e.make();
        ParameterizedMesh* pm = dynamic_cast<ParameterizedMesh*>(shape.get());
        if (!pm || !pm->GetMesh() || pm->GetMesh()->GetNFaces() == 0) continue;

        Mesh out;
        EXPECT_TRUE(SolidifyMesh(*pm->GetMesh(), 0.05f, out)) << e.name;
        EXPECT_GE(out.GetNFaces(), pm->GetMesh()->GetNFaces()) << e.name;
        EXPECT_TRUE(allFinite(out)) << e.name;
    }
}
