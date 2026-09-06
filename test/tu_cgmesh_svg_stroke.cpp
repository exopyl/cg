#include <gtest/gtest.h>

#include "../src/cgmesh/contour_ops.h"
#include "../src/cgmesh/import_svg.h"
#include "../src/cgmesh/mesh.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

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
//   ferme + stroke  -> ANNEAU : les deux bords de la boucle, arete de fermeture
//                      comprise (strokeClosedToContours)
//   ouvert + stroke -> RUBAN a extremites, de la largeur du trait
//                      (strokeToContours)
//
// Un `<polygon>` ou un `<rect>` en `fill:none` releve du deuxieme cas : c'est un
// trait ferme, epaissi en ANNEAU et non rempli -- la semantique meme de
// `fill:none`.
//
// ---------------------------------------------------------------------------
//  Les oracles
// ---------------------------------------------------------------------------
// 1. Le VOLUME SIGNE. Pour un prisme FERME, il vaut exactement
//    aire_du_capot x hauteur. Tout ecart signale un capot ou une paroi
//    manquante.
//
//    Ni un compte de faces ni un compte d'aretes de bord ne le detecte :
//    ExtrudedMeshBuilder emet volontairement les capots et les parois en blocs
//    de sommets DISJOINTS (cf. son en-tete), de sorte que des aretes de bord
//    existent par construction et ne disent rien de l'etancheite.
//
// 2. L'AIRE ANALYTIQUE de l'anneau, perimetre x largeur pour un contour
//    rectiligne a joints mitre. C'est le seul oracle qui distingue l'anneau du
//    ruban : les deux sont etanches, et les deux couvrent moins que la forme
//    pleine. Un anneau de rectangle mesure a trois cotes sur quatre est un
//    ruban.
//
//    Il exige `centerAndFit` a FAUX, sans quoi la sortie n'est plus dans les
//    unites du document et l'aire n'a plus de valeur analytique.

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

// Aire du capot dans les unites du DOCUMENT : c'est la grandeur que l'aire
// analytique de l'anneau permet de verifier.
double exactTopArea(const char* svgBody, const char* stem)
{
    const std::string path = std::string("./") + stem + ".svg";
    writeSvg(path.c_str(), svgBody);

    const float h = 0.2f;
    SvgExtrudeOptions opt;
    opt.height       = h;
    opt.centerAndFit = false;
    std::unique_ptr<Mesh> m(import_svg_extruded(path.c_str(), opt));
    std::remove(path.c_str());
    if (!m) return 0.0;

    double volume = 0, topArea = 0;
    unsigned int nWalls = 0;
    measure(*m, h, volume, topArea, nWalls);
    return topArea;
}

double netArea(const std::vector<ExtrudeContour>& region)
{
    double sum = 0.0;
    for (const ExtrudeContour& c : region) sum += (double)contourSignedArea(c.pts);
    return std::fabs(sum);
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
//
// L'etancheite ne suffit pas a l'etablir : un ruban a trois cotes est etanche
// lui aussi. L'aire tranche -- 4 x 100 x 10 pour l'anneau des quatre cotes,
// 3 x 100 x 10 si l'arete de fermeture n'est pas tracee.
TEST(TEST_cgmesh_svg_stroke, unfilled_polygon_becomes_a_thickened_ring)
{
    expectWatertight(kSquareStroked, "tu_svgs_polyring");

    const double area = exactTopArea(kSquareStroked, "tu_svgs_polyring_area");
    std::cout << "  anneau du polygone : aire " << area << " (attendu 4000)" << std::endl;
    EXPECT_NEAR(area, 4000.0, 1.0)
        << "3000 signalerait un ruban ouvert : le quatrieme cote n'est pas trace";
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

// ===========================================================================
//  L'anneau d'une forme SANS remplissage, sur son aire analytique
// ===========================================================================
//
// Deux oracles, et il en faut deux :
//   - DEUX contours : un bord exterieur et un bord interieur. Un seul contour
//     est un ruban, quelle que soit son aire ;
//   - l'aire vaut perimetre x largeur, soit 4 x 100 x 4 = 1600 pour ce
//     rectangle a joints mitre. 1200 signalerait qu'un cote sur quatre --
//     l'arete de fermeture -- n'est pas trace.
//
// Le trait d'une forme sans remplissage ne depend PAS de
// `strokeOnFilledShapes`, qui ne gouverne que celui des formes remplies : la
// fixture est lue avec les options par defaut, drapeau compris.
TEST(TEST_cgmesh_svg_stroke, closed_stroke_only_yields_a_ring_of_perimeter_times_width)
{
    SvgExtrudeOptions opt;
    opt.centerAndFit = false;
    ASSERT_FALSE(opt.strokeOnFilledShapes) << "le defaut n'est plus celui que ce test suppose";

    std::vector<SvgShapeGroup> groups;
    ASSERT_TRUE(svg_to_shape_groups("./test/data/svg/closed_stroke_only.svg", opt, groups));
    ASSERT_EQ(groups.size(), 1u) << "une forme au trait seul rend un seul groupe";

    const std::vector<ExtrudeContour>& ring = groups[0].contours;
    const double area = netArea(ring);

    std::cout << "  anneau du rect : contours " << ring.size()
              << "  aire " << area << " (attendu 1600)" << std::endl;

    EXPECT_FALSE(groups[0].paint.hasFill) << "le groupe porte la couleur du trait";
    EXPECT_EQ(ring.size(), 2u) << "un seul contour : c'est un ruban, pas un anneau";
    EXPECT_NEAR(area, 1600.0, 1.0)
        << "1200 signalerait un ruban ouvert : l'arete de fermeture n'est pas tracee";

    // L'emprise confirme la lecture de l'aire : le bord exterieur est decale de
    // la demi-largeur de part et d'autre du trace, soit 8 sur 10 et 110.
    float lo = 1e30f, hi = -1e30f;
    for (const ExtrudeContour& c : ring)
        for (const Vector2f& p : c.pts) { lo = std::min(lo, p.x); hi = std::max(hi, p.x); }
    EXPECT_NEAR(lo,   8.0f, 1e-2f);
    EXPECT_NEAR(hi, 112.0f, 1e-2f);
}

// Le meme anneau sur un contour COURBE. L'aire analytique vaut
// pi x (52^2 - 48^2) = 400 pi ~ 1256,6 ; l'ecart residuel est celui de
// l'aplatissement, et le figer ici interdit qu'une modification de
// `flattenTol` le degrade en silence.
TEST(TEST_cgmesh_svg_stroke, unfilled_circle_yields_a_ring_of_the_analytic_area)
{
    const char* body =
        "<circle cx=\"60\" cy=\"60\" r=\"50\" fill=\"none\""
        " stroke=\"#ff0000\" stroke-width=\"4\"/>\n";

    const double area = exactTopArea(body, "tu_svgs_circring");
    std::cout << "  anneau du cercle : aire " << area << " (attendu 400 pi = 1256.6)" << std::endl;
    EXPECT_NEAR(area, 400.0 * 3.14159265358979, 5.0);
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
