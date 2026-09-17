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

// Regression : les tampons du clipper avaient deux capacites FIXES et non
// verifiees -- 100 contours par plan, et 2048 floats soit 682 POINTS par contour.
// Un cylindre a 1500 cotes rend une section d'environ 1500 points : l'ancien code
// ecrivait largement hors du bloc alloue.
TEST(TEST_slicer, contour_larger_than_the_old_fixed_capacity)
{
    const unsigned int sides = 1500;   // > 682 points par section : l'ancienne borne
    Mesh* cyl = CreateCylinder(2.f, 1.f, sides, true, true);
    ASSERT_NE(cyl, nullptr);
    cyl->ComputeNormals();
    Mesh_half_edge he(cyl);
    he.create_half_edge();

    Cmodel3d_half_edge_sliced slicer(&he, ALONGOZ, 0.5f);

    int maxPoints = 0;
    float bestArea = 0.f;
    for (int i = 0; i < slicer.get_n_slices(); ++i)
    {
        Polygon2** slice = nullptr;
        int nContours = 0;
        slicer.get_slice(i, &slice, &nContours);
        for (int c = 0; c < nContours; ++c)
        {
            const int np = slice[c]->get_n_points(0);
            if (np > maxPoints)
            {
                maxPoints = np;
                bestArea = fabsf(slice[c]->area());
            }
        }
    }

    EXPECT_GT(maxPoints, 682);
    // Et le contour est bien la section du cylindre, pas de la memoire relue.
    EXPECT_NEAR(bestArea, 3.14159265f, 0.05f);

    delete cyl;
}

// Une seconde destruction complete, pour que les detecteurs de fuite (CRT debug
// heap, ASAN) aient de quoi mordre si le destructeur redevenait vide.
TEST(TEST_slicer, destructor_releases_every_slice)
{
    Mesh* cyl = CreateCylinder(2.f, 1.f, 64, true, true);
    ASSERT_NE(cyl, nullptr);
    cyl->ComputeNormals();
    Mesh_half_edge he(cyl);
    he.create_half_edge();

    for (int rep = 0; rep < 8; ++rep)
    {
        Cmodel3d_half_edge_sliced slicer(&he, ALONGOZ, 0.05f);
        EXPECT_GT(slicer.get_n_slices(), 0);
    }

    delete cyl;
}

// ============================================================================
//  PRECONDITION TOPOLOGIQUE -- tests de CARACTERISATION
// ============================================================================
//
// Le slicer ne marche que sur la demi-arete : il suit `m_pair` de face en face.
// Il exige donc une variete FERMEE, et les trois tests suivants fixent ce qui se
// passe quand elle ne l'est pas. Ils decrivent le comportement ACTUEL, defauts
// compris ; qui corrigera l'un d'eux les fera echouer, et c'est voulu.
//
// ============================================================================

// Reference : sur une variete fermee, et hors plan tangent, la section est exacte.
TEST(TEST_slicer, closed_manifold_section_is_exact)
{
    Mesh* cyl = CreateCylinder(2.f, 1.f, 32, true, true);
    cyl->ComputeNormals();
    Mesh_half_edge he(cyl);
    he.create_half_edge();
    for (unsigned int i = 0; i < cyl->GetNVertices(); ++i)
        ASSERT_FALSE(he.is_border(i)) << "le cylindre capuchonne doit etre ferme";

    Cmodel3d_half_edge_sliced slicer(&he, ALONGOZ, 0.5f);
    Polygon2** slice = nullptr;
    int nContours = 0;
    slicer.get_slice(1, &slice, &nContours);   // plan a mi-hauteur, non tangent

    ASSERT_EQ(nContours, 1);
    EXPECT_EQ(slice[0]->get_n_points(0), 64);
    EXPECT_NEAR(fabsf(slice[0]->area()), 3.14159265f, 0.03f);

    delete cyl;
}

// LIMITE 1 -- un BORD tronque la marche. Le tube sans capuchons coupe par un plan
// PARALLELE a son axe devrait rendre deux segments verticaux de longueur 2 ; la
// marche bute sur `m_pair == -1` au premier rebord et abandonne le contour la.
TEST(TEST_slicer, open_boundary_truncates_the_contour)
{
    Mesh* tube = CreateCylinder(2.f, 1.f, 32, false, true);   // SANS capuchons
    tube->ComputeNormals();
    Mesh_half_edge he(tube);
    he.create_half_edge();

    // Perpendiculairement a l'axe, la section ne touche aucun bord : elle est juste.
    {
        Cmodel3d_half_edge_sliced slicer(&he, ALONGOZ, 0.5f);
        Polygon2** slice = nullptr;
        int nContours = 0;
        slicer.get_slice(1, &slice, &nContours);
        ASSERT_EQ(nContours, 1);
        EXPECT_NEAR(fabsf(slice[0]->area()), 3.14159265f, 0.03f);
    }

    // Parallelement a l'axe, elle traverse les deux rebords : contours tronques
    // a DEUX points, au lieu des deux segments attendus.
    {
        Cmodel3d_half_edge_sliced slicer(&he, ALONGOX, 0.5f);
        int nTruncated = 0;
        for (int i = 0; i < slicer.get_n_slices(); ++i)
        {
            Polygon2** slice = nullptr;
            int nContours = 0;
            slicer.get_slice(i, &slice, &nContours);
            for (int c = 0; c < nContours; ++c)
                if (slice[c]->get_n_points(0) <= 2) nTruncated++;
        }
        EXPECT_GT(nTruncated, 0) << "limite connue : un bord tronque le contour";
    }

    delete tube;
}

// LIMITE 2 -- une arete NON VARIETE. L'appariement de half_edge.cpp se fait deux a
// deux sur une cle non orientee : une arete portee par trois faces en laisse une
// sans opposee, et la marche s'y interrompt. `is_manifold()` ne detecte pas ce
// cas -- il teste la topologie autour des SOMMETS.
TEST(TEST_slicer, non_manifold_edge_leaves_half_edges_unpaired)
{
    float v[] = {
        -1,-1, 0,   1,-1, 0,   1, 1, 0,  -1, 1, 0,
        -1,-1, 1,   1,-1, 1,
    };
    unsigned int f[] = {
        0,1,2,  0,2,3,      // quad de base
        0,1,5,  0,5,4,      // aile verticale : 2e face sur l'arete {0,1}
        1,0,4,              // 3e face sur l'arete {0,1}
    };
    Mesh_half_edge he(6, v, 5, f);
    he.create_half_edge();

    Che_mesh* cm = he.GetCheMesh();
    int nUnpaired = 0;
    for (int e = 0; e < cm->m_ne; ++e)
        if (cm->edge(e).m_pair < 0) nUnpaired++;
    EXPECT_GT(nUnpaired, 0);

    // Le slicer ne plante pas, mais ce qu'il rend n'a pas de sens : des contours
    // de deux ou trois points la ou la section est une ligne brisee complete.
    Cmodel3d_half_edge_sliced slicer(&he, ALONGOZ, 0.25f);
    int nDegenerate = 0;
    for (int i = 0; i < slicer.get_n_slices(); ++i)
    {
        Polygon2** slice = nullptr;
        int nContours = 0;
        slicer.get_slice(i, &slice, &nContours);
        for (int c = 0; c < nContours; ++c)
            if (slice[c]->get_n_points(0) <= 3) nDegenerate++;
    }
    EXPECT_GT(nDegenerate, 0) << "limite connue : non variete -> contours brises";
}

// LIMITE 3 -- un plan COPLANAIRE a une face, meme sur un maillage parfaitement
// ferme. La premiere tranche tombe sur z_min, donc sur le capuchon : toutes les
// distances y valent zero, tous les produits sont <= 0, et chaque face du
// capuchon ouvre son propre contour. Trente-deux contours d'aire NaN au lieu
// d'un disque.
TEST(TEST_slicer, plane_coplanar_with_a_face_is_degenerate)
{
    Mesh* cyl = CreateCylinder(2.f, 1.f, 32, true, true);
    cyl->ComputeNormals();
    Mesh_half_edge he(cyl);
    he.create_half_edge();

    Cmodel3d_half_edge_sliced slicer(&he, ALONGOZ, 0.5f);
    Polygon2** slice = nullptr;
    int nContours = 0;
    slicer.get_slice(0, &slice, &nContours);   // plan pose sur le capuchon

    EXPECT_GT(nContours, 1) << "limite connue : plan tangent -> un contour par face";

    delete cyl;
}
