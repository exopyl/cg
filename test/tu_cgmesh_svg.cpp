#include <gtest/gtest.h>

#include "../src/cgmesh/import_svg.h"
#include "../src/cgmesh/mesh.h"

#include <cmath>
#include <memory>

namespace {

std::unique_ptr<Mesh> importExtruded(const char* path, float height)
{
    SvgExtrudeOptions opt;
    opt.height       = height;
    // En unites du maillage produit, l'objet etant normalise a 1.0 : 1/200e de sa
    // plus grande dimension. Les fixtures carre et triangle sont rectilignes, la
    // valeur ne change donc aucun de leurs comptes -- elle compte pour les
    // fixtures courbes (rose, nazca, batman, spiderman).
    opt.flattenTol   = 0.005f;
    opt.centerAndFit = true;
    opt.invertY      = true;
    return std::unique_ptr<Mesh>(import_svg_extruded(path, opt));
}

// Meme chose, mais en pilotant la tolerance plutot que la hauteur.
std::unique_ptr<Mesh> importExtrudedTol(const char* path, float flattenTol)
{
    SvgExtrudeOptions opt;
    opt.height       = 0.1f;
    opt.flattenTol   = flattenTol;
    opt.centerAndFit = true;
    opt.invertY      = true;
    return std::unique_ptr<Mesh>(import_svg_extruded(path, opt));
}

} // namespace

TEST(TEST_cgmesh_svg, square_produces_extruded_solid)
{
    auto m = importExtruded("./test/data/svg/square.svg", 0.5f);
    ASSERT_NE(m, nullptr);

    // A rect path becomes 4 corner samples (rectangles are degenerate cubics
    // but nanosvg still emits four bezier endpoints). We expect:
    //   - bottom + top caps: 2 * 2 = 4 triangles (rectangle = 2 tri each)
    //   - 4 side quads = 8 triangles
    //   - total: 12 triangles
    //   - vertices: 4 bottom + 4 top = 8
    EXPECT_EQ(m->GetNVertices(), 16u);
    EXPECT_EQ(m->GetNFaces(),    12u);

    EXPECT_TRUE(m->IsTriangleMesh());
}

TEST(TEST_cgmesh_svg, square_bbox_height_matches_param)
{
    auto m = importExtruded("./test/data/svg/square.svg", 0.5f);
    ASSERT_NE(m, nullptr);

    m->computebbox();
    float vmin[3], vmax[3];
    m->bbox().GetMinMax(vmin, vmax);
    EXPECT_NEAR(vmax[2] - vmin[2], 0.5f, 1e-4f) << "extrusion height in Z";
}

TEST(TEST_cgmesh_svg, square_bbox_height_scales_with_param)
{
    auto m1 = importExtruded("./test/data/svg/square.svg", 0.1f);
    auto m2 = importExtruded("./test/data/svg/square.svg", 2.0f);
    ASSERT_NE(m1, nullptr);
    ASSERT_NE(m2, nullptr);

    m1->computebbox();
    m2->computebbox();
    float a[3], b[3], c[3], d[3];
    m1->bbox().GetMinMax(a, b);
    m2->bbox().GetMinMax(c, d);
    EXPECT_NEAR(b[2] - a[2], 0.1f, 1e-4f);
    EXPECT_NEAR(d[2] - c[2], 2.0f, 1e-4f);
}

TEST(TEST_cgmesh_svg, triangle_produces_extruded_solid)
{
    auto m = importExtruded("./test/data/svg/triangle.svg", 1.0f);
    ASSERT_NE(m, nullptr);

    // Triangle: 3 corners (sometimes 3 after redundancy trim).
    //   - caps: 2 * 1 = 2 triangles
    //   - 3 side quads = 6 triangles
    //   - total: 8 triangles
    //   - vertices: 3 + 3 = 6
    EXPECT_EQ(m->GetNVertices(), 12u);
    EXPECT_EQ(m->GetNFaces(),    8u);
}

TEST(TEST_cgmesh_svg, nonexistent_file_returns_null)
{
    auto m = importExtruded("./test/data/svg/does_not_exist.svg", 0.5f);
    EXPECT_EQ(m, nullptr);
}

// ============================================================================
//  Unites de flattenTol
// ============================================================================
//
// circle_small.svg et circle_large.svg decrivent le MEME cercle, l'un sur un
// canevas de 100, l'autre de 2000. Le maillage etant normalise a 1.0 dans les
// deux cas (centerAndFit), la finesse des courbes doit etre la meme : c'est ce
// que garantit l'expression de la tolerance en unites de SORTIE.
//
// Avant le correctif du 2026-08-14, la tolerance etait consommee en unites du
// DOCUMENT : le grand fichier subdivisait deux niveaux de plus, soit environ
// quatre fois plus de sommets pour un resultat visuellement identique.

TEST(TEST_cgmesh_svg, flatten_tol_is_independent_of_document_scale)
{
    auto small = importExtruded("./test/data/svg/circle_small.svg", 0.1f);
    auto large = importExtruded("./test/data/svg/circle_large.svg", 0.1f);
    ASSERT_NE(small, nullptr);
    ASSERT_NE(large, nullptr);

    const unsigned int ns = small->GetNVertices();
    const unsigned int nl = large->GetNVertices();
    ASSERT_GT(ns, 0u);

    // Bande de 5 % et non egalite strice : le facteur 20 n'est pas une puissance
    // de deux, les coordonnees du grand fichier ne sont donc pas le produit exact
    // de celles du petit une fois passees en float. Le critere de platitude peut
    // basculer d'un cote ou de l'autre sur un arc. Ce qu'on verrouille est
    // l'ordre de grandeur, la panne d'origine valant un facteur 4.
    const float ratio = (float)nl / (float)ns;
    EXPECT_NEAR(ratio, 1.f, 0.05f)
        << "meme dessin, echelles de document differentes : " << ns
        << " sommets contre " << nl;
}

TEST(TEST_cgmesh_svg, flatten_tol_still_coarsens_the_curve)
{
    // Le curseur doit rester utile sur toute sa plage : la valeur haute produit
    // franchement moins de sommets que la valeur basse. C'est ce qui manquait sur
    // un grand document, ou la course entiere restait dans le regime fin.
    auto fine   = importExtrudedTol("./test/data/svg/circle_large.svg", 0.001f);
    auto coarse = importExtrudedTol("./test/data/svg/circle_large.svg", 0.05f);
    ASSERT_NE(fine, nullptr);
    ASSERT_NE(coarse, nullptr);

    EXPECT_LT(coarse->GetNVertices(), fine->GetNVertices());
}

TEST(TEST_cgmesh_svg, without_center_and_fit_tolerance_stays_in_document_units)
{
    // L'exception documentee : sans recentrage-ajustement, la sortie est dans les
    // unites du document, la tolerance y est donc deja exprimee et n'est pas
    // convertie. Les deux echelles divergent alors, et c'est le comportement
    // attendu -- une tolerance absolue sur un dessin vingt fois plus grand
    // subdivise davantage.
    SvgExtrudeOptions opt;
    opt.height       = 0.1f;
    opt.flattenTol   = 0.5f;
    opt.centerAndFit = false;
    opt.invertY      = true;

    std::unique_ptr<Mesh> small(import_svg_extruded("./test/data/svg/circle_small.svg", opt));
    std::unique_ptr<Mesh> large(import_svg_extruded("./test/data/svg/circle_large.svg", opt));
    ASSERT_NE(small, nullptr);
    ASSERT_NE(large, nullptr);

    EXPECT_GT(large->GetNVertices(), small->GetNVertices());
}
