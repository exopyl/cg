// ============================================================================
//  K3b -- le trait des formes remplies (svg_couleurs_faisabilite.md, section 23)
// ============================================================================
//
// D7 : le trait d'une forme FERMEE ET REMPLIE doit etre produit. Il l'est en
// ANNEAU -- les deux bords de la boucle --, dans un groupe pousse apres le
// remplissage de la meme forme, donc peint par-dessus lui et, sous `Subtract`,
// le decoupant. C'est la semantique SVG.
//
// Ce que cette suite etablit, dans l'ordre des criteres de la section 23 :
//
//   1. drapeau a `false`, rien ne bouge -- verifie ICI sur une fixture, et par
//      la suite complete K0/K1/K2/K3 qui n'a ete ni touchee ni requalifiee ;
//   2. l'anneau est un ANNEAU : deux contours, aire = perimetre x largeur ;
//   3. les trous sont traces, un anneau par sous-chemin ;
//   4. l'anneau decoupe son propre remplissage -- ce qui distingue « le trait
//      est produit » de « le trait est produit AU BON RANG » ;
//   5. le compte de groupes du Tigre augmente exactement du nombre de formes
//      remplies ET tracees ;
//   6. l'aire de la reunion est preservee par le balayage sous D7 ;
//   7. volumetrie et cout, CONSIGNES quels qu'ils soient.
//
// Plus le plancher de largeur (`minStrokeWorldWidth`) : il elargit un trait trop
// fin, il n'en supprime aucun.
//
// ---------------------------------------------------------------------------
//  Ce que cette suite ne fait pas
// ---------------------------------------------------------------------------
// Elle ne juge RIEN au chronometre. Les durees sont publiees, aucune n'est
// assertee : le cout est l'objet du jalon suivant, et les separer est ce qui
// permet de garder la correction meme si le cout deplait.
//
// Les fixtures a aire exacte tournent avec `centerAndFit` a FAUX. Sous
// `centerAndFit`, l'anneau deborde de la silhouette et fait donc partie de ce
// qui est ajuste a 1.0 : l'aire attendue dependrait alors de la largeur du
// trait, ce qui rendrait l'oracle circulaire.
//
// ⚠ nanosvg est instancie une seule fois dans le depot, par import_svg.cpp qui
// definit NANOSVG_IMPLEMENTATION. L'en-tete est donc inclus ici SANS la macro.
//
// ============================================================================

#include <gtest/gtest.h>

#include "../src/cgmesh/contour_ops.h"
#include "../src/cgmesh/extrude_contours.h"
#include "../src/cgmesh/import_svg.h"
#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/stroke_contours.h"

#include "../extern/nanosvg/nanosvg.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

const char* kTiger = "./test/data/svg/Ghostscript_Tiger.svg";

SvgExtrudeOptions tigerOptions()
{
    SvgExtrudeOptions opt;
    opt.height       = 0.1f;
    opt.flattenTol   = 0.005f;
    opt.centerAndFit = true;
    opt.invertY      = true;
    return opt;
}

// Aires exactes : pas de recentrage-ajustement, donc les unites de sortie sont
// celles du document (cf. l'en-tete de ce fichier).
SvgExtrudeOptions exactOptions()
{
    SvgExtrudeOptions opt;
    opt.height       = 0.1f;
    opt.flattenTol   = 0.005f;
    opt.centerAndFit = false;
    opt.invertY      = true;
    return opt;
}

void writeSvg(const char* path, const char* body)
{
    std::ofstream f(path);
    f << "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\">\n" << body << "</svg>\n";
}

double netArea(const std::vector<ExtrudeContour>& region)
{
    double sum = 0.0;
    for (const ExtrudeContour& c : region) sum += (double)contourSignedArea(c.pts);
    return std::fabs(sum);
}

size_t countContourPoints(const std::vector<SvgShapeGroup>& groups)
{
    size_t n = 0;
    for (const SvgShapeGroup& g : groups)
        for (const ExtrudeContour& c : g.contours) n += c.pts.size();
    return n;
}

std::vector<ExtrudeContour> unionOfGroups(const std::vector<SvgShapeGroup>& groups)
{
    std::vector<ExtrudeContour> u;
    for (const SvgShapeGroup& g : groups)
        if (!g.contours.empty()) u = unionContours(u, g.contours);
    return u;
}

double sumOfGroupAreas(const std::vector<SvgShapeGroup>& groups)
{
    double sum = 0.0;
    for (const SvgShapeGroup& g : groups) sum += netArea(g.contours);
    return sum;
}

// ---------------------------------------------------------------------------
//  Emprise sur les points de controle
// ---------------------------------------------------------------------------
// Reprend controlHullLargestExtent (import_svg.cpp) : meme filtre de formes,
// meme parcours. La mesure de largeur de trait doit porter sur le facteur
// REELLEMENT utilise pour convertir `flattenTol` et `minStrokeWorldWidth`, sinon
// elle decrit une autre grandeur que celle que le code applique.
float controlHullExtent(const NSVGimage* image, const SvgExtrudeOptions& opt)
{
    bool any = false;
    float minX = 0.f, minY = 0.f, maxX = 0.f, maxY = 0.f;

    for (const NSVGshape* shape = image->shapes; shape; shape = shape->next)
    {
        if (!opt.ignoreShapeId.empty() && opt.ignoreShapeId == shape->id) continue;
        if ((shape->flags & NSVG_FLAGS_VISIBLE) == 0) continue;

        const bool hasFill   = (shape->fill.type   != NSVG_PAINT_NONE);
        const bool hasStroke = (shape->stroke.type != NSVG_PAINT_NONE);
        if (!hasFill && !(hasStroke && opt.strokeToVolume)) continue;

        for (const NSVGpath* path = shape->paths; path; path = path->next)
            for (int i = 0; i < path->npts; ++i)
            {
                const float x = path->pts[i*2 + 0];
                const float y = path->pts[i*2 + 1];
                if (!any) { minX = maxX = x; minY = maxY = y; any = true; continue; }
                minX = std::min(minX, x); maxX = std::max(maxX, x);
                minY = std::min(minY, y); maxY = std::max(maxY, y);
            }
    }
    return any ? std::max(maxX - minX, maxY - minY) : 0.f;
}

// Boite englobante d'un groupe, lue sur ses contours et non sur la forme source :
// c'est celle qu'une indexation spatiale de l'accumulateur utiliserait.
struct Box { float lo[2]; float hi[2]; bool valid = false; };

Box groupBox(const SvgShapeGroup& g)
{
    Box b;
    for (const ExtrudeContour& c : g.contours)
        for (const Vector2f& p : c.pts)
        {
            if (!b.valid) { b.lo[0] = b.hi[0] = p.x; b.lo[1] = b.hi[1] = p.y; b.valid = true; continue; }
            b.lo[0] = std::min(b.lo[0], p.x); b.hi[0] = std::max(b.hi[0], p.x);
            b.lo[1] = std::min(b.lo[1], p.y); b.hi[1] = std::max(b.hi[1], p.y);
        }
    return b;
}

// Le contact simple compte comme une intersection : le chiffre est un MAJORANT
// du nombre de paires qui se recouvrent vraiment.
size_t secantBoxPairs(const std::vector<SvgShapeGroup>& groups)
{
    std::vector<Box> boxes;
    boxes.reserve(groups.size());
    for (const SvgShapeGroup& g : groups) boxes.push_back(groupBox(g));

    size_t n = 0;
    for (size_t i = 0; i < boxes.size(); ++i)
        for (size_t j = i + 1; j < boxes.size(); ++j)
        {
            if (!boxes[i].valid || !boxes[j].valid) continue;
            if (boxes[i].hi[0] < boxes[j].lo[0] || boxes[j].hi[0] < boxes[i].lo[0]) continue;
            if (boxes[i].hi[1] < boxes[j].lo[1] || boxes[j].hi[1] < boxes[i].lo[1]) continue;
            ++n;
        }
    return n;
}

// Poste (c) du chronometrage : tessellation et extrusion seules, sur des groupes
// deja resolus. import_svg_extruded refait les postes (a) et (b), il ne peut
// donc pas servir a isoler celui-ci.
struct TessResult { double ms = 0.0; unsigned int vertices = 0u; unsigned int faces = 0u; };

TessResult tessellate(const std::vector<SvgShapeGroup>& groups, float height)
{
    ExtrudeAppendOptions ao;
    ao.zBottom              = 0.0f;
    ao.zTop                 = height;
    ao.winding              = ExtrudeWinding::NonZero;
    ao.normalizeOrientation = false;

    const auto t0 = std::chrono::steady_clock::now();
    ExtrudedMeshBuilder b;
    for (const SvgShapeGroup& g : groups)
        if (!g.contours.empty()) b.Append(g.contours, ao);
    std::unique_ptr<Mesh> m(b.Build());
    const auto t1 = std::chrono::steady_clock::now();

    TessResult r;
    r.ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    if (m) { r.vertices = m->GetNVertices(); r.faces = m->GetNFaces(); }
    return r;
}

// Charge utile transmise au navigateur a chaque rafraichissement : positions et
// normales a 24 octets par sommet, indices a 12 octets par triangle. Megaoctets
// DECIMAUX, comme la section 5.1 -- les deux chiffres sont a comparer.
double payloadMo(unsigned int vertices, unsigned int faces)
{
    return (24.0 * (double)vertices + 12.0 * (double)faces) / 1e6;
}

// Un carre plein de cote 1 avec un trait de 0,1. Joint MITRE explicite : le
// defaut de nanosvg vaut deja mitre, mais l'aire attendue en depend -- un joint
// arrondi rognerait les quatre coins.
const char* kStrokedSquare =
    "  <path fill=\"#888888\" stroke=\"#ff0000\" stroke-width=\"0.1\""
    " stroke-linejoin=\"miter\" d=\"M0,0 H1 V1 H0 Z\"/>\n";

const char* kUnstrokedSquare =
    "  <path fill=\"#888888\" d=\"M0,0 H1 V1 H0 Z\"/>\n";

// Carre de cote 1 perce d'un carre de cote 0,4, en even-odd : DEUX sous-chemins
// fermes dans une seule forme.
const char* kStrokedSquareWithHole =
    "  <path fill=\"#888888\" fill-rule=\"evenodd\" stroke=\"#ff0000\""
    " stroke-width=\"0.05\" stroke-linejoin=\"miter\""
    " d=\"M0,0 H1 V1 H0 Z M0.3,0.3 H0.7 V0.7 H0.3 Z\"/>\n";

} // namespace

// ============================================================================
//  Critere 1 -- le drapeau a `false` ne produit rien
// ============================================================================
//
// La non-regression complete se lit sur la suite K0/K1/K2/K3, inchangee. Ce test
// fige ici le cas positif du drapeau : sans lui, on ne saurait pas si la suite
// reste verte parce que le defaut protege ou parce que l'anneau ne marche pas.

TEST(TEST_cgmesh_svg_k3b, a_filled_and_stroked_shape_yields_its_ring_only_when_asked)
{
    const char* path = "k3b_flag.svg";
    writeSvg(path, kStrokedSquare);

    SvgExtrudeOptions opt = exactOptions();
    ASSERT_FALSE(opt.strokeOnFilledShapes) << "le defaut n'est plus celui que K0-K3 supposent";

    std::vector<SvgShapeGroup> off;
    ASSERT_TRUE(svg_to_shape_groups(path, opt, off));
    EXPECT_EQ(off.size(), 1u);
    EXPECT_TRUE(off[0].paint.hasFill);
    EXPECT_NEAR(netArea(off[0].contours), 1.0, 1e-4);

    opt.strokeOnFilledShapes = true;
    std::vector<SvgShapeGroup> on;
    ASSERT_TRUE(svg_to_shape_groups(path, opt, on));
    ASSERT_EQ(on.size(), 2u);

    // Le remplissage est INCHANGE : l'anneau s'ajoute, il ne se substitue pas.
    EXPECT_TRUE(on[0].paint.hasFill);
    EXPECT_NEAR(netArea(on[0].contours), 1.0, 1e-4);

    // Le groupe d'anneau vient APRES, porte le meme rang, et n'est pas un
    // remplissage : c'est ce qui lui donne la couleur du trait.
    EXPECT_FALSE(on[1].paint.hasFill);
    EXPECT_EQ(on[1].paint.rank, on[0].paint.rank);

    std::remove(path);
}

// ============================================================================
//  Critere 2 -- l'anneau est un anneau
// ============================================================================
//
// Deux modes de panne que ce test separe :
//   - aire 0,2 au lieu de 0,4 : un seul cote a ete decale ;
//   - un seul contour au lieu de deux : la boucle n'a pas ete refermee.

TEST(TEST_cgmesh_svg_k3b, the_ring_has_two_contours_and_the_area_of_perimeter_times_width)
{
    const char* path = "k3b_ring.svg";
    writeSvg(path, kStrokedSquare);

    SvgExtrudeOptions opt = exactOptions();
    opt.strokeOnFilledShapes = true;

    std::vector<SvgShapeGroup> groups;
    ASSERT_TRUE(svg_to_shape_groups(path, opt, groups));
    ASSERT_EQ(groups.size(), 2u);

    const std::vector<ExtrudeContour>& ring = groups[1].contours;
    const double area = netArea(ring);

    std::cout << "  anneau du carre : contours " << ring.size()
              << "  aire " << area << " (attendu 0.4)" << std::endl;

    EXPECT_EQ(ring.size(), 2u) << "un seul contour : la boucle n'est pas refermee";
    EXPECT_NEAR(area, 0.4, 1e-3) << "0.2 signalerait un decalage d'un seul cote";

    // L'emprise confirme la lecture de l'aire : le bord exterieur est a -0,05 et
    // +1,05, donc les deux cotes du trace ont bien ete decales.
    float lo = 1e30f, hi = -1e30f;
    for (const ExtrudeContour& c : ring)
        for (const Vector2f& p : c.pts) { lo = std::min(lo, p.x); hi = std::max(hi, p.x); }
    EXPECT_NEAR(lo, -0.05f, 1e-3f);
    EXPECT_NEAR(hi,  1.05f, 1e-3f);

    std::remove(path);
}

// La reponse au point laisse ouvert en 22.1 : `EndType::Joined` referme-t-il la
// boucle lui-meme ? Le test l'etablit des deux cotes -- l'arete de fermeture est
// tracee sans qu'on repete le premier point, et la repeter ne change rien.
TEST(TEST_cgmesh_svg_k3b, the_closing_edge_is_implicit_and_a_repeated_first_point_is_harmless)
{
    const std::vector<std::array<float, 2>> square =
        { {0.f,0.f}, {1.f,0.f}, {1.f,1.f}, {0.f,1.f} };
    std::vector<std::array<float, 2>> squareRepeated = square;
    squareRepeated.push_back({0.f, 0.f});

    const auto areaOf = [](const std::vector<std::vector<std::array<float, 2>>>& rs)
    {
        double sum = 0.0;
        for (const auto& r : rs)
        {
            std::vector<Vector2f> pts;
            pts.reserve(r.size());
            for (const auto& p : r) pts.emplace_back(p[0], p[1]);
            sum += (double)contourSignedArea(pts);
        }
        return std::fabs(sum);
    };

    const auto ring   = strokeClosedToContours({ square },         0.1f, StrokeJoin::Miter);
    const auto ringR  = strokeClosedToContours({ squareRepeated }, 0.1f, StrokeJoin::Miter);
    // Cas positif : les MEMES points traites comme un trace OUVERT laissent le
    // quatrieme cote sans trait. C'est la panne que le compte de contours seul
    // ne verrait pas.
    const auto ribbon = strokeToContours({ square }, 0.1f, StrokeJoin::Miter, StrokeCap::Butt);

    std::cout << "  EndType::Joined : contours " << ring.size() << " aire " << areaOf(ring)
              << "  | premier point repete : contours " << ringR.size()
              << " aire " << areaOf(ringR)
              << "  | trace ouvert : contours " << ribbon.size()
              << " aire " << areaOf(ribbon) << std::endl;

    EXPECT_EQ(ring.size(),  2u);
    EXPECT_EQ(ringR.size(), 2u);
    EXPECT_NEAR(areaOf(ring),  0.4, 1e-6);
    EXPECT_NEAR(areaOf(ringR), 0.4, 1e-6);

    // Le trace ouvert perd exactement un cote sur quatre.
    EXPECT_EQ(ribbon.size(), 1u);
    EXPECT_NEAR(areaOf(ribbon), 0.3, 1e-3);
}

// ============================================================================
//  Critere 3 -- les trous sont traces
// ============================================================================

TEST(TEST_cgmesh_svg_k3b, every_subpath_gets_its_own_ring_including_holes)
{
    const char* path = "k3b_hole.svg";
    writeSvg(path, kStrokedSquareWithHole);

    SvgExtrudeOptions opt = exactOptions();
    opt.strokeOnFilledShapes = true;

    std::vector<SvgShapeGroup> groups;
    ASSERT_TRUE(svg_to_shape_groups(path, opt, groups));
    ASSERT_EQ(groups.size(), 2u);

    // Remplissage : carre perce, deux contours, aire 1 - 0,16.
    EXPECT_EQ(groups[0].contours.size(), 2u);
    EXPECT_NEAR(netArea(groups[0].contours), 1.0 - 0.16, 1e-4);

    // Anneau : deux contours par sous-chemin.
    //   exterieur  4 x 1,0 x 0,05 = 0,20
    //   trou       4 x 0,4 x 0,05 = 0,08
    const double area = netArea(groups[1].contours);
    std::cout << "  anneaux du carre perce : contours " << groups[1].contours.size()
              << "  aire " << area << " (attendu 0.28)" << std::endl;

    EXPECT_EQ(groups[1].contours.size(), 4u) << "le trou n'est pas trace";
    EXPECT_NEAR(area, 0.28, 1e-3);

    std::remove(path);
}

// ============================================================================
//  Critere 4 -- l'anneau decoupe son PROPRE remplissage
// ============================================================================
//
// C'est le critere de RANG, et non de production : un anneau pousse avant son
// remplissage passerait les criteres 2 et 3 et echouerait ici.

TEST(TEST_cgmesh_svg_k3b, under_subtraction_the_ring_cuts_the_fill_of_its_own_shape)
{
    const char* path = "k3b_cut.svg";
    writeSvg(path, kStrokedSquare);

    SvgExtrudeOptions opt = exactOptions();
    opt.strokeOnFilledShapes = true;

    std::vector<SvgShapeGroup> groups;
    ASSERT_TRUE(svg_to_shape_groups(path, opt, groups));
    ASSERT_EQ(groups.size(), 2u);

    const double fillBefore = netArea(groups[0].contours);
    const double ringArea   = netArea(groups[1].contours);

    // Oracle INDEPENDANT du chemin teste : la reunion du remplissage et de son
    // anneau vaut la forme dilatee d'une demi-largeur, calculee par Clipper2.
    const std::vector<ExtrudeContour> dilated =
        offsetContours(groups[0].contours, 0.05f, StrokeJoin::Miter);
    const double dilatedArea = netArea(dilated);

    SvgOverlapStats stats;
    svg_subtract_overlaps(groups, &stats);

    const double fillAfter = netArea(groups[0].contours);

    std::cout << "  remplissage " << fillBefore << " -> " << fillAfter
              << "  anneau " << ringArea
              << "  reunion " << (fillAfter + ringArea)
              << "  (forme dilatee " << dilatedArea << ")" << std::endl;

    // Le trait mord sur son remplissage, de sa moitie interieure.
    EXPECT_LT(fillAfter, fillBefore - 1e-4) << "le trait ne decoupe pas son remplissage";
    EXPECT_NEAR(fillAfter, 0.81, 1e-3);

    // L'anneau, lui, est intact : il est AU-DESSUS.
    EXPECT_NEAR(netArea(groups[1].contours), ringArea, 1e-6);

    // Les deux regions sont disjointes et leur reunion est la forme dilatee.
    EXPECT_NEAR(fillAfter + ringArea, dilatedArea, 1e-3);
    EXPECT_NEAR(dilatedArea, 1.21, 1e-3);

    std::remove(path);
}

// ============================================================================
//  Le plancher de largeur
// ============================================================================

TEST(TEST_cgmesh_svg_k3b, the_width_floor_widens_thin_strokes_and_leaves_the_others_alone)
{
    // Deux carres de cote 1 : l'un trace a 0,02, l'autre a 0,20. Le plancher est
    // pose a 0,10 -- entre les deux.
    const char* path = "k3b_floor.svg";
    writeSvg(path,
             "  <path fill=\"#888888\" stroke=\"#ff0000\" stroke-width=\"0.02\""
             " stroke-linejoin=\"miter\" d=\"M0,0 H1 V1 H0 Z\"/>\n"
             "  <path fill=\"#888888\" stroke=\"#00ff00\" stroke-width=\"0.20\""
             " stroke-linejoin=\"miter\" d=\"M3,0 H4 V1 H3 Z\"/>\n");

    SvgExtrudeOptions opt = exactOptions();
    opt.strokeOnFilledShapes = true;
    ASSERT_FLOAT_EQ(opt.minStrokeWorldWidth, 0.0f) << "le plancher n'est plus desactive par defaut";

    std::vector<SvgShapeGroup> off;
    ASSERT_TRUE(svg_to_shape_groups(path, opt, off));
    ASSERT_EQ(off.size(), 4u);   // deux formes, remplissage puis anneau

    opt.minStrokeWorldWidth = 0.10f;
    std::vector<SvgShapeGroup> on;
    ASSERT_TRUE(svg_to_shape_groups(path, opt, on));
    ASSERT_EQ(on.size(), 4u) << "le plancher a supprime ou ajoute un groupe";

    const double thinOff  = netArea(off[1].contours);
    const double thinOn   = netArea(on[1].contours);
    const double thickOff = netArea(off[3].contours);
    const double thickOn  = netArea(on[3].contours);

    std::cout << "  plancher 0.10 : trait 0.02 -> aire " << thinOff << " puis " << thinOn
              << "  |  trait 0.20 -> aire " << thickOff << " puis " << thickOn << std::endl;

    // Sous le plancher : l'anneau ressort a la LARGEUR DU PLANCHER, 4 x 1 x 0,10.
    EXPECT_NEAR(thinOff, 4.0 * 0.02, 1e-3);
    EXPECT_NEAR(thinOn,  4.0 * 0.10, 1e-3);

    // Au-dessus : rien ne bouge, au bit pres du chemin float -> double -> float.
    EXPECT_NEAR(thickOff, 4.0 * 0.20, 1e-3);
    EXPECT_NEAR(thickOn,  thickOff,   1e-6);

    // Le plancher elargit, il ne supprime jamais.
    EXPECT_GT(thinOn, thinOff);

    std::remove(path);
}

// ============================================================================
//  Critere 5 -- ce que D7 recupere sur le Tigre
// ============================================================================

TEST(TEST_cgmesh_svg_k3b, tiger_gains_one_group_per_filled_and_stroked_shape)
{
    SvgExtrudeOptions opt = tigerOptions();

    std::vector<SvgShapeGroup> without;
    ASSERT_TRUE(svg_to_shape_groups(kTiger, opt, without));

    opt.strokeOnFilledShapes = true;
    std::vector<SvgShapeGroup> with;
    ASSERT_TRUE(svg_to_shape_groups(kTiger, opt, with));

    // Compte independant, lu sur le document : formes visibles remplies ET
    // tracees. C'est cette mesure qui fait l'oracle, pas un nombre fige.
    NSVGimage* image = nsvgParseFromFile(kTiger, "px", 96.0f);
    ASSERT_NE(image, nullptr);
    int filledAndStroked = 0;
    for (const NSVGshape* s = image->shapes; s; s = s->next)
    {
        if ((s->flags & NSVG_FLAGS_VISIBLE) == 0) continue;
        if (s->fill.type == NSVG_PAINT_NONE || s->stroke.type == NSVG_PAINT_NONE) continue;
        bool anyClosed = false;
        for (const NSVGpath* p = s->paths; p; p = p->next) if (p->closed) anyClosed = true;
        if (anyClosed) ++filledAndStroked;
    }
    nsvgDelete(image);

    std::cout << "  Tigre : groupes " << without.size() << " -> " << with.size()
              << "  (+" << (long)with.size() - (long)without.size()
              << ", formes remplies ET tracees : " << filledAndStroked << ")" << std::endl;
    RecordProperty("groupsWithoutRings", (int)without.size());
    RecordProperty("groupsWithRings",    (int)with.size());
    RecordProperty("filledAndStroked",   filledAndStroked);

    EXPECT_EQ(with.size(), without.size() + (size_t)filledAndStroked);
}

// ============================================================================
//  Critere 6 -- l'aire de la reunion survit au balayage sous D7
// ============================================================================

TEST(TEST_cgmesh_svg_k3b, tiger_union_area_is_preserved_under_subtraction_with_rings)
{
    SvgExtrudeOptions opt = tigerOptions();
    opt.strokeOnFilledShapes = true;

    std::vector<SvgShapeGroup> groups;
    ASSERT_TRUE(svg_to_shape_groups(kTiger, opt, groups));

    const double unionBefore = netArea(unionOfGroups(groups));

    SvgOverlapStats stats;
    svg_subtract_overlaps(groups, &stats);

    const double areaAfter  = sumOfGroupAreas(groups);
    const double unionAfter = netArea(unionOfGroups(groups));
    const double drift      = std::fabs(unionAfter - unionBefore) / unionBefore;

    std::cout << "  Tigre sous D7 : reunion " << unionBefore << " -> " << unionAfter
              << "  (ecart relatif " << drift << ")\n"
              << "    aire des regions gardees : " << areaAfter << "\n"
              << "    groupes disparus         : " << stats.fullyCoveredGroups << std::endl;
    RecordProperty("unionAreaD7", (int)(unionBefore * 1e6));
    RecordProperty("unionDriftPpb", (int)(drift * 1e9));

    // Rien n'est perdu hors recouvrement, et les regions rendues sont disjointes.
    EXPECT_NEAR(unionAfter, unionBefore, 1e-4 * unionBefore);
    EXPECT_NEAR(areaAfter,  unionAfter,  1e-4 * unionAfter);

    // L'anneau AGRANDIT la reunion : il deborde de sa forme d'une demi-largeur.
    // Sans cela, D7 ne recupererait rien de visible.
    SvgExtrudeOptions plain = tigerOptions();
    std::vector<SvgShapeGroup> without;
    ASSERT_TRUE(svg_to_shape_groups(kTiger, plain, without));
    EXPECT_GT(unionBefore, netArea(unionOfGroups(without)));
}

// ============================================================================
//  Section 22.7 -- largeur des anneaux rapportee a l'emprise du document
// ============================================================================
//
// Le tableau de la section 22.7 est une ESTIMATION batie sur une emprise brute
// minoree. Cette mesure la remplace : elle lit `strokeWidth` apres parse et le
// divise par l'emprise que le code utilise lui-meme.

TEST(TEST_cgmesh_svg_k3b, tiger_stroke_widths_relative_to_the_document_extent)
{
    const SvgExtrudeOptions opt = tigerOptions();

    NSVGimage* image = nsvgParseFromFile(kTiger, "px", 96.0f);
    ASSERT_NE(image, nullptr);

    const float extent = controlHullExtent(image, opt);
    ASSERT_GT(extent, 1e-6f);

    std::vector<double> ratios;
    for (const NSVGshape* s = image->shapes; s; s = s->next)
    {
        if ((s->flags & NSVG_FLAGS_VISIBLE) == 0) continue;
        if (s->stroke.type == NSVG_PAINT_NONE) continue;
        ratios.push_back((double)s->strokeWidth / (double)extent);
    }
    nsvgDelete(image);

    ASSERT_FALSE(ratios.empty());
    std::sort(ratios.begin(), ratios.end());
    const double lo  = ratios.front();
    const double hi  = ratios.back();
    const double med = ratios[ratios.size() / 2];

    // Largeurs distinctes declarees : la section 22.7 en annonce quatre.
    std::vector<double> distinct;
    for (double r : ratios)
        if (distinct.empty() || std::fabs(r - distinct.back()) > 1e-9) distinct.push_back(r);

    std::cout << "  Tigre, largeur de trait / emprise (" << ratios.size() << " formes tracees,"
              << " emprise " << extent << ") :\n"
              << "    min     " << lo  << "   soit " << (lo  / opt.flattenTol) << " x flattenTol\n"
              << "    mediane " << med << "   soit " << (med / opt.flattenTol) << " x flattenTol\n"
              << "    max     " << hi  << "   soit " << (hi  / opt.flattenTol) << " x flattenTol\n"
              << "    largeurs distinctes : " << distinct.size();
    for (double d : distinct) std::cout << "  " << d;
    std::cout << "\n    a fitSize = 100 mm : de " << (lo * 100.0) << " mm a "
              << (hi * 100.0) << " mm" << std::endl;

    RecordProperty("strokedShapes",     (int)ratios.size());
    RecordProperty("widthRatioMinE9",   (int)(lo  * 1e9));
    RecordProperty("widthRatioMedE9",   (int)(med * 1e9));
    RecordProperty("widthRatioMaxE9",   (int)(hi  * 1e9));
    RecordProperty("distinctWidths",    (int)distinct.size());

    // La seule relation assertee : les ratios sont ordonnes et non degeneres.
    // Le reste est mesure, et la mesure a raison contre le tableau estime.
    EXPECT_GT(lo, 0.0);
    EXPECT_LE(lo, med);
    EXPECT_LE(med, hi);
}

// ============================================================================
//  Critere 7 -- volumetrie et cout sous D7, consignes quels qu'ils soient
// ============================================================================
//
// Aucune duree n'est assertee. Ce test est l'ENTREE de K4, pas son arbitrage :
// la section 22.4 rend un seuil de 300 ms probablement hors d'atteinte, et
// ecrire ici un critere que la construction rend impossible n'apprendrait rien.

TEST(TEST_cgmesh_svg_k3b, tiger_volumetry_and_three_post_timing_under_d7)
{
    struct Run { const char* label; bool rings; };
    const Run runs[2] = { { "sans anneaux (K3)", false }, { "avec anneaux (D7)", true } };

    size_t   groupsN[2]   = { 0, 0 };
    size_t   ptsBefore[2] = { 0, 0 };
    size_t   ptsAfter[2]  = { 0, 0 };
    double   msParse[2]   = { 0.0, 0.0 };
    double   msSweep[2]   = { 0.0, 0.0 };
    double   msTess[2]    = { 0.0, 0.0 };
    unsigned nv[2]        = { 0u, 0u };
    unsigned nf[2]        = { 0u, 0u };
    unsigned nMat[2]      = { 0u, 0u };
    size_t   pairs[2]     = { 0, 0 };

    for (int i = 0; i < 2; ++i)
    {
        SvgExtrudeOptions opt = tigerOptions();
        opt.strokeOnFilledShapes = runs[i].rings;

        // Poste (a) : parse nanosvg, aplatissement, epaississement des traits,
        // resolution des regles de remplissage.
        const auto t0 = std::chrono::steady_clock::now();
        std::vector<SvgShapeGroup> groups;
        ASSERT_TRUE(svg_to_shape_groups(kTiger, opt, groups)) << runs[i].label;
        const auto t1 = std::chrono::steady_clock::now();
        msParse[i]   = std::chrono::duration<double, std::milli>(t1 - t0).count();
        groupsN[i]   = groups.size();
        ptsBefore[i] = countContourPoints(groups);
        pairs[i]     = secantBoxPairs(groups);

        // Poste (b) : marqueterie.
        SvgOverlapStats stats;
        svg_subtract_overlaps(groups, &stats);
        msSweep[i]  = stats.sweepMs;
        ptsAfter[i] = countContourPoints(groups);

        // Poste (c) : tessellation et extrusion.
        const TessResult t = tessellate(groups, opt.height);
        msTess[i] = t.ms;
        nv[i]     = t.vertices;
        nf[i]     = t.faces;

        // Palette : le pipeline complet, pour le compte de materiaux seul.
        opt.perShapeMaterials = true;
        opt.overlapPolicy     = SvgExtrudeOptions::OverlapPolicy::Subtract;
        std::unique_ptr<Mesh> m(import_svg_extruded(kTiger, opt));
        ASSERT_NE(m, nullptr) << runs[i].label;
        nMat[i] = m->GetNMaterials();
    }

    for (int i = 0; i < 2; ++i)
    {
        const size_t possible = groupsN[i] * (groupsN[i] - 1) / 2;
        std::cout << "  Tigre, " << runs[i].label << " :\n"
                  << "    groupes                  : " << groupsN[i] << "\n"
                  << "    points de contour        : " << ptsBefore[i]
                  << " -> " << ptsAfter[i] << "\n"
                  << "    sommets / faces          : " << nv[i] << " / " << nf[i] << "\n"
                  << "    materiaux                : " << nMat[i] << "\n"
                  << "    charge utile             : " << payloadMo(nv[i], nf[i]) << " Mo\n"
                  << "    ms (a) parse+aplatis.    : " << msParse[i] << "\n"
                  << "    ms (b) marqueterie       : " << msSweep[i] << "\n"
                  << "    ms (c) tessell.+extrusion: " << msTess[i] << "\n"
                  << "    ms total                 : " << (msParse[i] + msSweep[i] + msTess[i]) << "\n"
                  << "    paires de boites secantes: " << pairs[i] << " / " << possible
                  << "  (" << (100.0 * (double)pairs[i] / (double)possible) << " %)\n"
                  << "    partenaires par groupe   : "
                  << (2.0 * (double)pairs[i] / (double)groupsN[i]) << std::endl;
    }

    std::cout << "  facteurs D7 / K3 : points x"
              << ((double)ptsAfter[1] / (double)ptsAfter[0])
              << "  sommets x" << ((double)nv[1] / (double)nv[0])
              << "  balayage x" << (msSweep[1] / msSweep[0]) << std::endl;

    RecordProperty("groupsD7",        (int)groupsN[1]);
    RecordProperty("pointsAfterD7",   (int)ptsAfter[1]);
    RecordProperty("verticesD7",      (int)nv[1]);
    RecordProperty("facesD7",         (int)nf[1]);
    RecordProperty("materialsD7",     (int)nMat[1]);
    RecordProperty("payloadKoD7",     (int)(payloadMo(nv[1], nf[1]) * 1024.0));
    RecordProperty("msParseD7",       (int)msParse[1]);
    RecordProperty("msSweepD7",       (int)msSweep[1]);
    RecordProperty("msTessellateD7",  (int)msTess[1]);
    RecordProperty("secantPairsD7",   (int)pairs[1]);
    RecordProperty("secantPairsK3",   (int)pairs[0]);

    // Seules relations assertees : le maillage n'est pas vide, et D7 en ajoute.
    for (int i = 0; i < 2; ++i)
    {
        EXPECT_GT(nv[i], 0u);
        EXPECT_EQ(nv[i] % 4u, 0u);
    }
    EXPECT_GT(groupsN[1], groupsN[0]);
    EXPECT_GT(nv[1],      nv[0]);
}
