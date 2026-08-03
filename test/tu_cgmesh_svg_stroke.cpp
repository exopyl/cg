#include <gtest/gtest.h>

#include "../src/cgmesh/import_svg.h"
#include "../src/cgmesh/mesh.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <memory>
#include <string>

// ===========================================================================
//  Formes au TRAIT : polyline epaissie, polygon rempli
// ===========================================================================
// Une forme sans remplissage etait purement ignoree, ce qui rendait inexploitable
// tout SVG de dessin au trait. Pire : omettre `fill` en SVG veut dire NOIR, et non
// « aucun remplissage », si bien qu'une polyligne ouverte etait refermee d'office
// pour etre remplie. Sur un trace qui se replie sur lui-meme -- une courbe du
// dragon -- ce remplissage degenere en damier, faute de pouvoir designer un
// interieur ; et un cadre de page cense n'etre qu'un trait ressortait en plaque.
//
// La decision se prend maintenant PAR CHEMIN, sur NSVGpath::closed, que nanosvg
// renseigne fidelement -- `<polygon>` donne closed=1, `<polyline>` closed=0
// (nanosvg.h:2826-2833 : meme parseur, closeFlag 0 ou 1) :
//
//   ferme + fill    -> tessellation du remplissage (chemin preexistant)
//   sinon + stroke  -> trace EPAISSI de son stroke-width, via Clipper2
//
// Un `<polygon>` en `fill:none` releve du second cas : c'est un trait ferme,
// epaissi en ANNEAU et non rempli -- la semantique meme de `fill:none`.
//
// ---------------------------------------------------------------------------
//  L'oracle : le volume signe
// ---------------------------------------------------------------------------
// Pour un prisme FERME, le volume signe vaut exactement aire_du_capot x hauteur.
// Tout ecart signale un capot ou une paroi manquante.
//
// Ni un compte de faces ni un compte d'aretes de bord ne le detecte :
// ExtrudedMeshBuilder emet volontairement les capots et les parois en blocs de
// sommets DISJOINTS (cf. son en-tete), de sorte que des aretes de bord existent
// par construction et ne disent rien de l'etancheite.

namespace {

std::unique_ptr<Mesh> importStroke(const char* path, float height, float strokeScale = 1.0f)
{
    SvgExtrudeOptions opt;
    opt.height      = height;
    opt.strokeScale = strokeScale;
    return std::unique_ptr<Mesh>(import_svg_extruded(path, opt));
}

void writeSvg(const char* path, const char* body)
{
    std::ofstream f(path);
    f << "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\">\n" << body << "</svg>\n";
}

// Volume signe (theoreme de la divergence), aire du capot superieur, nombre de
// triangles de paroi.
void measure(Mesh& m, float height, double& volume, double& topArea, unsigned int& nWalls)
{
    volume = 0.0; topArea = 0.0; nWalls = 0;
    for (unsigned int f = 0; f < m.GetNFaces(); f++)
    {
        if (m.GetFaceNVertices(f) != 3) continue;
        float p[3][3];
        for (int k = 0; k < 3; k++) m.GetVertex((unsigned int)m.GetFaceVertex(f, k), p[k]);
        volume += ((double)p[0][0]*((double)p[1][1]*p[2][2]-(double)p[2][1]*p[1][2])
                 - (double)p[0][1]*((double)p[1][0]*p[2][2]-(double)p[2][0]*p[1][2])
                 + (double)p[0][2]*((double)p[1][0]*p[2][1]-(double)p[2][0]*p[1][1])) / 6.0;
        const bool atTop = std::fabs(p[0][2]-height) < 1e-4f
                        && std::fabs(p[1][2]-height) < 1e-4f
                        && std::fabs(p[2][2]-height) < 1e-4f;
        const bool atBot = std::fabs(p[0][2]) < 1e-4f && std::fabs(p[1][2]) < 1e-4f
                        && std::fabs(p[2][2]) < 1e-4f;
        if (atTop)
            topArea += (((double)p[1][0]-p[0][0])*((double)p[2][1]-p[0][1])
                      - ((double)p[1][1]-p[0][1])*((double)p[2][0]-p[0][0])) / 2.0;
        else if (!atBot)
            nWalls++;
    }
}

void expectWatertight(const char* svgBody, const char* stem)
{
    const std::string path = std::string("./") + stem + ".svg";
    writeSvg(path.c_str(), svgBody);

    const float h = 0.2f;
    std::unique_ptr<Mesh> m = importStroke(path.c_str(), h);
    ASSERT_NE(m, nullptr) << "aucun maillage produit";

    double volume = 0, topArea = 0;
    unsigned int nWalls = 0;
    measure(*m, h, volume, topArea, nWalls);

    EXPECT_GT(topArea, 0.0) << "capot superieur vide";
    EXPECT_GT(nWalls, 0u)   << "aucune paroi laterale";
    EXPECT_NEAR(volume, topArea * h, 1e-4 * topArea * h)
        << "volume " << volume << " au lieu de " << (topArea * h)
        << " : le solide n'est pas ferme";

    std::remove(path.c_str());
}

const char* kSquareFilled =
    "<polygon points=\"0,0 100,0 100,100 0,100\" fill=\"#a0a0a0\"/>\n";
const char* kSquareStroked =
    "<g style=\"stroke:rgb(0,0,0);stroke-width:10;fill:none;\">\n"
    "<polygon points=\"0,0 100,0 100,100 0,100\"/>\n</g>\n";

}  // namespace

// Garde-fou du chemin PREEXISTANT : un polygone qui declare un remplissage est
// tessele, et le solide est ferme.
TEST(TEST_cgmesh_svg_stroke, filled_polygon_extrudes_to_a_closed_solid)
{
    expectWatertight(kSquareFilled, "tu_svgs_polyfill");
}

// `fill:none` sur un polygone : trait FERME, donc anneau epaissi.
TEST(TEST_cgmesh_svg_stroke, unfilled_polygon_becomes_a_thickened_ring)
{
    expectWatertight(kSquareStroked, "tu_svgs_polyring");
}

// Un anneau n'est pas un disque : son capot doit etre STRICTEMENT plus petit que
// celui du meme contour rempli. Sans cette comparaison, un anneau qui degenererait
// en surface pleine passerait quand meme le test d'etancheite.
TEST(TEST_cgmesh_svg_stroke, a_ring_covers_less_than_the_filled_square)
{
    writeSvg("./tu_svgs_cmp_fill.svg", kSquareFilled);
    writeSvg("./tu_svgs_cmp_ring.svg", kSquareStroked);

    std::unique_ptr<Mesh> full = importStroke("./tu_svgs_cmp_fill.svg", 0.2f);
    std::unique_ptr<Mesh> ring = importStroke("./tu_svgs_cmp_ring.svg", 0.2f);
    ASSERT_NE(full, nullptr);
    ASSERT_NE(ring, nullptr);

    double vF = 0, aF = 0, vR = 0, aR = 0;
    unsigned int wF = 0, wR = 0;
    measure(*full, 0.2f, vF, aF, wF);
    measure(*ring, 0.2f, vR, aR, wR);
    EXPECT_LT(aR, aF) << "l'anneau couvre autant que le carre plein : il a ete rempli";

    std::remove("./tu_svgs_cmp_fill.svg");
    std::remove("./tu_svgs_cmp_ring.svg");
}

// Une polyligne OUVERTE devient un ruban de la largeur du trait.
TEST(TEST_cgmesh_svg_stroke, open_polyline_becomes_a_thickened_ribbon)
{
    expectWatertight("<g style=\"stroke:rgb(0,0,0);stroke-width:10;fill:none;\">\n"
                     "<polyline points=\"0,0 100,0 100,100\"/>\n</g>\n",
                     "tu_svgs_ribbon");
}

// Le cas minimal : UN segment. Deux points suffisent pour un trait, la ou un
// contour a remplir en exige trois -- c'est pourquoi le seuil est distinct selon
// le chemin emprunte.
TEST(TEST_cgmesh_svg_stroke, a_single_segment_polyline_extrudes)
{
    expectWatertight("<g style=\"stroke:rgb(0,0,0);stroke-width:10;fill:none;\">\n"
                     "<polyline points=\"0,0 100,0\"/>\n</g>\n",
                     "tu_svgs_seg");
}

// strokeScale elargit le ruban sans changer l'emprise du trace.
TEST(TEST_cgmesh_svg_stroke, stroke_scale_widens_the_ribbon)
{
    writeSvg("./tu_svgs_scale.svg",
             "<g style=\"stroke:rgb(0,0,0);stroke-width:4;fill:none;\">\n"
             "<polyline points=\"0,0 100,0 100,100\"/>\n</g>\n");

    std::unique_ptr<Mesh> thin  = importStroke("./tu_svgs_scale.svg", 0.2f, 1.0f);
    std::unique_ptr<Mesh> thick = importStroke("./tu_svgs_scale.svg", 0.2f, 3.0f);
    ASSERT_NE(thin,  nullptr);
    ASSERT_NE(thick, nullptr);

    double vT = 0, aT = 0, vK = 0, aK = 0;
    unsigned int wT = 0, wK = 0;
    measure(*thin,  0.2f, vT, aT, wT);
    measure(*thick, 0.2f, vK, aK, wK);
    EXPECT_GT(aK, aT) << "strokeScale n'elargit pas le ruban";

    std::remove("./tu_svgs_scale.svg");
}

// Ni remplissage ni trait : rien a extruder, echec propre.
TEST(TEST_cgmesh_svg_stroke, a_shape_with_neither_fill_nor_stroke_yields_nothing)
{
    writeSvg("./tu_svgs_nothing.svg",
             "<g style=\"fill:none;stroke:none;\">\n"
             "<polyline points=\"0,0 100,0\"/>\n</g>\n");
    EXPECT_EQ(importStroke("./tu_svgs_nothing.svg", 0.2f), nullptr);
    std::remove("./tu_svgs_nothing.svg");
}
