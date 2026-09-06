// ============================================================================
//  K3 -- marqueterie par difference accumulee (svg_couleurs_faisabilite.md, s12)
// ============================================================================
//
// `overlapPolicy = Subtract` ampute chaque region de tout ce qui la couvre, du
// dessus vers le dessous. Ce que la suite doit etablir, dans cet ordre :
//
//   1. le VOLUME SIGNE du solide vaut celui de la reunion des formes ;
//   2. deux triangles de capot ne partagent aucun point interieur ;
//   3. rien n'est perdu hors recouvrement : l'aire de la reunion des regions
//      produites egale celle de la reunion des formes ;
//   4. les formes integralement recouvertes sont comptees.
//
// ---------------------------------------------------------------------------
//  L'oracle, et ce qu'il ne suffit PAS de verifier
// ---------------------------------------------------------------------------
// Le motif de tu_cgmesh_svg_stroke.cpp:33-41 -- volume signe egal a
// aire_du_capot x hauteur -- teste l'ETANCHEITE, et rien d'autre. Il reste vrai
// pour deux prismes fermes qui s'interpenetrent : chacun est etanche, les deux
// volumes s'additionnent, les deux capots aussi, et la relation tient alors que
// la matiere est comptee DEUX FOIS dans le recouvrement.
//
// Il ne discrimine donc pas la marqueterie a lui seul. Ce qui la discrimine est
// la comparaison a une mesure INDEPENDANTE du maillage : l'aire de la reunion
// des formes d'entree, calculee par Clipper2. Les deux sont verifiees ici, et le
// test `..._is_seen_to_fail_without_subtraction` fige le cas positif -- un
// oracle qu'on n'a jamais vu echouer ne prouve rien.
//
// ---------------------------------------------------------------------------
//  Ce que cette suite ne fait pas
// ---------------------------------------------------------------------------
// Elle ne juge RIEN au chronometre. La duree du balayage est publiee comme
// statistique ; le cout est l'objet d'un jalon distinct, et separer les deux est
// ce qui permet de garder la correction meme si le cout deplait.
//
// Aucun compte absolu du Tigre n'est fige : les oracles sont des RELATIONS et
// les valeurs absolues sont imprimees, comme en K0 et K2. Un nombre fige lierait
// la suite a la tessellation d'un compilateur.
//
// ============================================================================

#include <gtest/gtest.h>

#include "../src/cgmesh/contour_ops.h"
#include "../src/cgmesh/import_svg.h"
#include "../src/cgmesh/material.h"
#include "../src/cgmesh/mesh.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

const char* kTiger = "./test/data/svg/Ghostscript_Tiger.svg";

// Aire de la reunion des formes du Tigre, mesuree par K0 (section 16). C'est la
// valeur que la soustraction ne doit ni entamer ni depasser.
const double kTigerUnionArea = 0.6271;

SvgExtrudeOptions defaultOptions()
{
    SvgExtrudeOptions opt;
    opt.height       = 0.1f;
    opt.flattenTol   = 0.005f;
    opt.centerAndFit = true;
    opt.invertY      = true;
    return opt;
}

void writeSvg(const char* path, const char* body)
{
    std::ofstream f(path);
    f << "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\">\n" << body << "</svg>\n";
}

// Aire nette d'une region orientee par Clipper2 : enveloppes positives, trous
// negatifs.
double netArea(const std::vector<ExtrudeContour>& region)
{
    double sum = 0.0;
    for (const ExtrudeContour& c : region) sum += (double)contourSignedArea(c.pts);
    return std::fabs(sum);
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

size_t countContourPoints(const std::vector<SvgShapeGroup>& groups)
{
    size_t n = 0;
    for (const SvgShapeGroup& g : groups)
        for (const ExtrudeContour& c : g.contours) n += c.pts.size();
    return n;
}

// Volume signe (theoreme de la divergence) et aire signee du capot superieur --
// le motif de tu_cgmesh_svg_stroke.cpp:33-41.
struct MeshMeasure
{
    double       volume  = 0.0;
    double       topArea = 0.0;
    unsigned int nTop    = 0u;
    unsigned int nWalls  = 0u;
};

MeshMeasure measure(const Mesh& m, float height)
{
    MeshMeasure r;
    for (unsigned int f = 0; f < m.GetNFaces(); ++f)
    {
        if (m.GetFaceNVertices(f) != 3) continue;
        float p[3][3];
        for (int k = 0; k < 3; ++k)
            m.GetVertex((unsigned int)m.GetFaceVertex(f, k), p[k]);
        r.volume += ((double)p[0][0]*((double)p[1][1]*p[2][2]-(double)p[2][1]*p[1][2])
                   - (double)p[0][1]*((double)p[1][0]*p[2][2]-(double)p[2][0]*p[1][2])
                   + (double)p[0][2]*((double)p[1][0]*p[2][1]-(double)p[2][0]*p[1][1])) / 6.0;
        const bool atTop = std::fabs(p[0][2]-height) < 1e-4f
                        && std::fabs(p[1][2]-height) < 1e-4f
                        && std::fabs(p[2][2]-height) < 1e-4f;
        const bool atBot = std::fabs(p[0][2]) < 1e-4f && std::fabs(p[1][2]) < 1e-4f
                        && std::fabs(p[2][2]) < 1e-4f;
        if (atTop)
        {
            r.topArea += (((double)p[1][0]-p[0][0])*((double)p[2][1]-p[0][1])
                        - ((double)p[1][1]-p[0][1])*((double)p[2][0]-p[0][0])) / 2.0;
            ++r.nTop;
        }
        else if (!atBot)
        {
            ++r.nWalls;
        }
    }
    return r;
}

// ---------------------------------------------------------------------------
//  Recouvrement de deux triangles de capot
// ---------------------------------------------------------------------------
// Axes separateurs : pour deux convexes, les normales aux aretes des deux
// polygones suffisent. Un chevauchement de projection inferieur a `eps` compte
// pour NUL, si bien que deux triangles qui se touchent par une arete ou un
// sommet -- ce que la tessellation produit a foison a l'interieur d'une meme
// region -- sont declares disjoints. Seul un point INTERIEUR commun est signale.

struct Tri2D { double x[3]; double y[3]; };

bool separatedAlong(const Tri2D& a, const Tri2D& b, double ax, double ay, double eps)
{
    const double len = std::sqrt(ax*ax + ay*ay);
    if (len < 1e-18) return false;
    ax /= len; ay /= len;

    double aMin = 1e300, aMax = -1e300, bMin = 1e300, bMax = -1e300;
    for (int k = 0; k < 3; ++k)
    {
        const double pa = a.x[k]*ax + a.y[k]*ay;
        const double pb = b.x[k]*ax + b.y[k]*ay;
        aMin = std::min(aMin, pa); aMax = std::max(aMax, pa);
        bMin = std::min(bMin, pb); bMax = std::max(bMax, pb);
    }
    return (std::min(aMax, bMax) - std::max(aMin, bMin)) <= eps;
}

bool interiorsOverlap(const Tri2D& a, const Tri2D& b, double eps)
{
    for (int k = 0; k < 3; ++k)
    {
        const int n = (k + 1) % 3;
        if (separatedAlong(a, b, -(a.y[n]-a.y[k]), a.x[n]-a.x[k], eps)) return false;
        if (separatedAlong(a, b, -(b.y[n]-b.y[k]), b.x[n]-b.x[k], eps)) return false;
    }
    return true;
}

std::vector<Tri2D> topCapTriangles(const Mesh& m, float height)
{
    std::vector<Tri2D> out;
    for (unsigned int f = 0; f < m.GetNFaces(); ++f)
    {
        if (m.GetFaceNVertices(f) != 3) continue;
        float p[3][3];
        for (int k = 0; k < 3; ++k)
            m.GetVertex((unsigned int)m.GetFaceVertex(f, k), p[k]);
        if (std::fabs(p[0][2]-height) > 1e-4f || std::fabs(p[1][2]-height) > 1e-4f
         || std::fabs(p[2][2]-height) > 1e-4f) continue;
        Tri2D t;
        for (int k = 0; k < 3; ++k) { t.x[k] = p[k][0]; t.y[k] = p[k][1]; }
        out.push_back(t);
    }
    return out;
}

unsigned int countOverlappingCapPairs(const Mesh& m, float height, double eps)
{
    const std::vector<Tri2D> tris = topCapTriangles(m, height);
    unsigned int n = 0;
    for (size_t i = 0; i < tris.size(); ++i)
        for (size_t j = i + 1; j < tris.size(); ++j)
            if (interiorsOverlap(tris[i], tris[j], eps)) ++n;
    return n;
}

// Deux carres de 100, decales d'une demi-largeur : l'archetype du recouvrement.
// Aire des formes 2 x 100^2, aire de leur reunion 1,5 x 100^2.
const char* kTwoHalfOverlappingSquares =
    "  <rect x=\"0\"  y=\"0\" width=\"100\" height=\"100\" fill=\"#ff0000\"/>\n"
    "  <rect x=\"50\" y=\"0\" width=\"100\" height=\"100\" fill=\"#00ff00\"/>\n";

// Trois carres en escalier : chacun mord sur le suivant.
const char* kThreeStaggeredSquares =
    "  <rect x=\"0\"  y=\"0\"  width=\"100\" height=\"100\" fill=\"#ff0000\"/>\n"
    "  <rect x=\"50\" y=\"25\" width=\"100\" height=\"100\" fill=\"#00ff00\"/>\n"
    "  <rect x=\"25\" y=\"60\" width=\"100\" height=\"100\" fill=\"#0000ff\"/>\n";

// Un petit carre entierement sous un grand, dessine APRES lui donc au-dessus.
const char* kHiddenUnderCover =
    "  <rect x=\"40\" y=\"40\" width=\"20\"  height=\"20\"  fill=\"#ff0000\"/>\n"
    "  <rect x=\"0\"  y=\"0\"  width=\"100\" height=\"100\" fill=\"#0000ff\"/>\n";

// Aires d'entree d'une fixture, mesurees par Clipper2 et non par le maillage :
// c'est l'independance de cette mesure qui fait d'elle un oracle.
struct InputAreas
{
    double sum   = 0.0;   // somme des aires des formes, recouvrements comptes
    double union_ = 0.0;  // aire de leur reunion
};

InputAreas inputAreas(const char* path, const SvgExtrudeOptions& opt)
{
    InputAreas a;
    std::vector<SvgShapeGroup> groups;
    EXPECT_TRUE(svg_to_shape_groups(path, opt, groups)) << path;
    a.sum   = sumOfGroupAreas(groups);
    a.union_ = netArea(unionOfGroups(groups));
    return a;
}

} // namespace

// ============================================================================
//  Critere 1 -- l'oracle de volume signe
// ============================================================================

// Le cas POSITIF, fige dans la suite : sans marqueterie, chaque forme apporte
// tout son volume et le recouvrement est compte deux fois. L'oracle doit le
// voir, sans quoi son succes sous Subtract ne dirait rien.
TEST(TEST_cgmesh_svg_k3, the_volume_oracle_is_seen_to_fail_without_subtraction)
{
    const char* path = "k3_two_squares_none.svg";
    writeSvg(path, kTwoHalfOverlappingSquares);

    SvgExtrudeOptions opt = defaultOptions();
    opt.perShapeMaterials = true;
    ASSERT_EQ(opt.overlapPolicy, SvgExtrudeOptions::OverlapPolicy::None);

    const InputAreas in = inputAreas(path, opt);
    ASSERT_GT(in.sum, in.union_ * 1.01) << "la fixture ne se recouvre pas";

    std::unique_ptr<Mesh> m(import_svg_extruded(path, opt));
    ASSERT_NE(m, nullptr);
    const MeshMeasure r = measure(*m, opt.height);

    std::cout << "  None      : volume " << r.volume
              << "  capot " << r.topArea
              << "  (reunion attendue " << in.union_
              << ", somme des formes " << in.sum << ")" << std::endl;

    // Chaque prisme est etanche pris a part : la relation d'etancheite tient
    // meme ici. C'est ce qui la rend insuffisante.
    EXPECT_NEAR(r.volume, r.topArea * opt.height, 1e-4 * r.topArea * opt.height);

    // Ce qui NE tient pas : le volume est celui des deux formes cumulees, pas
    // celui de leur reunion.
    EXPECT_NEAR(r.volume, in.sum * opt.height, 1e-3 * in.sum * opt.height);
    EXPECT_GT(r.volume, in.union_ * opt.height * 1.01)
        << "l'oracle ne discrimine pas : le detecteur est inutilisable";

    std::remove(path);
}

TEST(TEST_cgmesh_svg_k3, subtraction_makes_the_volume_match_the_union_of_the_shapes)
{
    const char* path = "k3_two_squares_subtract.svg";
    writeSvg(path, kTwoHalfOverlappingSquares);

    SvgExtrudeOptions opt = defaultOptions();
    const InputAreas in = inputAreas(path, opt);

    // La marqueterie gouverne la geometrie seule : les deux reglages de palette
    // doivent rendre le meme volume.
    for (const bool perShape : { false, true })
    {
        opt.perShapeMaterials = perShape;
        opt.overlapPolicy     = SvgExtrudeOptions::OverlapPolicy::Subtract;

        std::unique_ptr<Mesh> m(import_svg_extruded(path, opt));
        ASSERT_NE(m, nullptr) << "perShapeMaterials=" << perShape;
        const MeshMeasure r = measure(*m, opt.height);

        std::cout << "  Subtract  : volume " << r.volume
                  << "  capot " << r.topArea
                  << "  (reunion attendue " << in.union_
                  << ")  perShapeMaterials=" << perShape << std::endl;

        EXPECT_GT(r.nTop,   0u);
        EXPECT_GT(r.nWalls, 0u);

        // Le solide est ferme...
        EXPECT_NEAR(r.volume, r.topArea * opt.height, 1e-4 * r.topArea * opt.height);
        // ... ET il occupe exactement la reunion des formes, ni plus ni moins.
        EXPECT_NEAR(r.topArea, in.union_, 1e-4 * in.union_);
        EXPECT_NEAR(r.volume,  in.union_ * opt.height, 1e-4 * in.union_ * opt.height);
    }

    std::remove(path);
}

// ============================================================================
//  Critere 2 -- regions XY disjointes
// ============================================================================

TEST(TEST_cgmesh_svg_k3, cap_triangles_share_no_interior_point)
{
    const char* path = "k3_three_squares.svg";
    writeSvg(path, kThreeStaggeredSquares);

    SvgExtrudeOptions opt = defaultOptions();
    opt.perShapeMaterials = true;

    // Cas positif : sans marqueterie, les capots se marchent dessus. Sans cette
    // mesure, un detecteur qui ne trouve jamais rien passerait pour une preuve.
    std::unique_ptr<Mesh> none(import_svg_extruded(path, opt));
    ASSERT_NE(none, nullptr);
    const unsigned int nOverlapNone = countOverlappingCapPairs(*none, opt.height, 1e-6);
    EXPECT_GT(nOverlapNone, 0u) << "le detecteur de recouvrement ne detecte rien";

    opt.overlapPolicy = SvgExtrudeOptions::OverlapPolicy::Subtract;
    std::unique_ptr<Mesh> cut(import_svg_extruded(path, opt));
    ASSERT_NE(cut, nullptr);
    const unsigned int nOverlapCut = countOverlappingCapPairs(*cut, opt.height, 1e-6);

    std::cout << "  paires de triangles de capot qui se recouvrent : "
              << nOverlapNone << " -> " << nOverlapCut << std::endl;
    EXPECT_EQ(nOverlapCut, 0u);

    std::remove(path);
}

// Sur le Tigre, la verification paire a paire serait quadratique en dizaines de
// milliers de triangles. La meme propriete se lit sur les AIRES : des regions
// dont la somme des aires egale l'aire de leur reunion ne se recouvrent pas,
// a un ensemble de mesure nulle pres.
TEST(TEST_cgmesh_svg_k3, tiger_regions_are_disjoint_and_lose_nothing)
{
    const SvgExtrudeOptions opt = defaultOptions();

    std::vector<SvgShapeGroup> groups;
    ASSERT_TRUE(svg_to_shape_groups(kTiger, opt, groups));

    const double areaBefore  = sumOfGroupAreas(groups);
    const double unionBefore = netArea(unionOfGroups(groups));

    SvgOverlapStats stats;
    svg_subtract_overlaps(groups, &stats);

    const double areaAfter  = sumOfGroupAreas(groups);
    const double unionAfter = netArea(unionOfGroups(groups));

    unsigned int emptyGroups = 0u;
    for (const SvgShapeGroup& g : groups) if (g.contours.empty()) ++emptyGroups;

    std::cout << "  Tigre, marqueterie :\n"
              << "    groupes                  : " << groups.size() << "\n"
              << "    aire des formes          : " << areaBefore  << "\n"
              << "    aire de leur reunion     : " << unionBefore << "\n"
              << "    aire des regions gardees : " << areaAfter   << "\n"
              << "    reunion des regions      : " << unionAfter  << "\n"
              << "    aire retiree             : " << stats.removedArea << "\n"
              << "    groupes disparus         : " << stats.fullyCoveredGroups << "\n"
              << "    ms -- balayage seul      : " << stats.sweepMs << std::endl;
    RecordProperty("inputArea",          (int)(areaBefore  * 1e6));
    RecordProperty("unionArea",          (int)(unionBefore * 1e6));
    RecordProperty("keptArea",           (int)(areaAfter   * 1e6));
    RecordProperty("removedArea",        (int)(stats.removedArea * 1e6));
    RecordProperty("fullyCoveredGroups", (int)stats.fullyCoveredGroups);
    RecordProperty("sweepMs",            (int)stats.sweepMs);

    // Les statistiques disent la meme chose que la mesure faite ici.
    EXPECT_NEAR(stats.inputArea, areaBefore, 1e-6);
    EXPECT_NEAR(stats.keptArea,  areaAfter,  1e-6);
    EXPECT_EQ(stats.fullyCoveredGroups, emptyGroups);

    // Critere 3 : rien n'est perdu hors recouvrement.
    EXPECT_NEAR(unionAfter, unionBefore, 1e-4 * unionBefore);
    // Et la reunion est bien celle que K0 avait mesuree.
    EXPECT_NEAR(unionBefore, kTigerUnionArea, 1e-3);

    // Critere 2, version globale : plus aucun recouvrement entre regions.
    EXPECT_NEAR(areaAfter, unionAfter, 1e-4 * unionAfter);

    // Critere 4 : des formes disparaissent, et pas toutes.
    EXPECT_GT(stats.fullyCoveredGroups, 0u);
    EXPECT_LT(stats.fullyCoveredGroups, (unsigned int)groups.size());
}

// ============================================================================
//  Critere 4 -- une forme integralement recouverte disparait, et est comptee
// ============================================================================

TEST(TEST_cgmesh_svg_k3, a_fully_covered_shape_emits_no_face_and_is_counted)
{
    const char* path = "k3_hidden.svg";
    writeSvg(path, kHiddenUnderCover);

    SvgExtrudeOptions opt = defaultOptions();
    opt.perShapeMaterials = true;
    opt.overlapPolicy     = SvgExtrudeOptions::OverlapPolicy::Subtract;

    SvgExtrudeMapping mapping;
    std::unique_ptr<Mesh> m(import_svg_extruded(path, opt, &mapping));
    ASSERT_NE(m, nullptr);

    ASSERT_EQ(mapping.facesOfGroup.size(), 2u);
    EXPECT_EQ(mapping.overlap.fullyCoveredGroups, 1u);

    // Le petit carre, dessine en premier donc dessous, n'emet rien.
    EXPECT_EQ(mapping.facesOfGroup[0].second, 0u);
    EXPECT_EQ(mapping.materialOfGroup[0], (unsigned int)MATERIAL_NONE);
    EXPECT_GT(mapping.facesOfGroup[1].second, 0u);

    // Sa couleur n'entre pas dans la palette : un materiau sans face publierait
    // un lot de rendu vide.
    EXPECT_EQ(m->GetNMaterials(), 1u);
    const Material* mat = m->GetMaterial(0);
    ASSERT_NE(mat, nullptr);
    ASSERT_EQ(mat->GetType(), MATERIAL_COLOR);
    EXPECT_FLOAT_EQ(static_cast<const MaterialColor*>(mat)->GetFloatBlue(), 1.0f);

    // Le grand carre est intact : la marqueterie n'entame que ce qui est couvert.
    EXPECT_EQ(m->GetNVertices(), 16u);
    EXPECT_EQ(m->GetNFaces(),    12u);

    std::remove(path);
}

// Une forme qui n'est recouverte par personne ressort inchangee : la
// soustraction ne doit pas normaliser au point de deplacer un sommet.
TEST(TEST_cgmesh_svg_k3, disjoint_shapes_are_left_untouched)
{
    const char* path = "k3_disjoint.svg";
    writeSvg(path,
             "  <rect x=\"0\"  y=\"0\" width=\"40\" height=\"40\" fill=\"#ff0000\"/>\n"
             "  <rect x=\"60\" y=\"0\" width=\"40\" height=\"40\" fill=\"#00ff00\"/>\n");

    const SvgExtrudeOptions opt = defaultOptions();

    std::vector<SvgShapeGroup> before;
    ASSERT_TRUE(svg_to_shape_groups(path, opt, before));
    ASSERT_EQ(before.size(), 2u);

    std::vector<SvgShapeGroup> after = before;
    SvgOverlapStats stats;
    svg_subtract_overlaps(after, &stats);

    // Tolerance RELATIVE : la region fait l'aller-retour float -> double
    // Clipper2 -> float, ce qui la deplace de quelques ULP. Un seuil absolu
    // serre passerait sur cette fixture et echouerait sur une plus grande.
    EXPECT_EQ(stats.fullyCoveredGroups, 0u);
    EXPECT_NEAR(stats.removedArea, 0.0, 1e-6 * stats.inputArea);

    ASSERT_EQ(after.size(), before.size());
    for (size_t g = 0; g < before.size(); ++g)
    {
        const double a = netArea(before[g].contours);
        ASSERT_EQ(after[g].contours.size(), before[g].contours.size()) << "groupe " << g;
        EXPECT_NEAR(netArea(after[g].contours), a, 1e-6 * a) << "groupe " << g;
    }

    std::remove(path);
}

// ============================================================================
//  Predictions K3 (section 19) -- mesure, pas rattrapage
// ============================================================================
//
// La section 20.2 a retire la conclusion « M2a allege le maillage » : elle
// confondait l'AIRE et la COMPLEXITE DE FRONTIERE. Les points de contour sont
// attendus en hausse -- chaque decoupe borne une region par la frontiere de son
// recouvrant --, les triangles de capot en forte baisse, les sommets
// indetermines. Aucune bande n'est assertee : la mesure a raison contre la
// prediction, et l'ecart s'analyse au lieu de se rattraper.

TEST(TEST_cgmesh_svg_k3, tiger_volumetry_under_subtraction)
{
    SvgExtrudeOptions opt = defaultOptions();

    std::vector<SvgShapeGroup> groups;
    ASSERT_TRUE(svg_to_shape_groups(kTiger, opt, groups));
    const size_t pointsBefore = countContourPoints(groups);

    SvgOverlapStats stats;
    svg_subtract_overlaps(groups, &stats);
    const size_t pointsAfter = countContourPoints(groups);

    // Trois maillages produits : le monolithique de K1, celui par forme de K2, et
    // la marqueterie. Subtract n'a pas de variante monolithique -- il impose une
    // tessellation par region (cf. import_svg.cpp).
    struct Run { const char* label; bool perShape; SvgExtrudeOptions::OverlapPolicy pol; };
    const Run runs[3] = {
        { "None     mono     ", false, SvgExtrudeOptions::OverlapPolicy::None     },
        { "None     par forme", true,  SvgExtrudeOptions::OverlapPolicy::None     },
        { "Subtract par forme", true,  SvgExtrudeOptions::OverlapPolicy::Subtract },
    };

    unsigned int nv[3] = {0,0,0}, nf[3] = {0,0,0}, nTop[3] = {0,0,0}, nWall[3] = {0,0,0};

    for (int i = 0; i < 3; ++i)
    {
        opt.perShapeMaterials = runs[i].perShape;
        opt.overlapPolicy     = runs[i].pol;
        std::unique_ptr<Mesh> m(import_svg_extruded(kTiger, opt));
        ASSERT_NE(m, nullptr) << runs[i].label;
        const MeshMeasure r = measure(*m, opt.height);
        nv[i]    = m->GetNVertices();
        nf[i]    = m->GetNFaces();
        nTop[i]  = r.nTop;
        nWall[i] = r.nWalls;
    }

    // ------------------------------------------------------------------------
    //  Les sommets de reprise COMBINE
    // ------------------------------------------------------------------------
    // glutess appelle COMBINE chaque fois qu'il doit fabriquer un sommet qui
    // n'etait pas dans l'entree : croisement de deux contours, ou fusion de deux
    // sommets confondus. Leur nombre est exactement
    //
    //     sommets tessellés  -  points de contour fournis
    //
    // lu sur le builder AVANT Build(), donc avant que les sommets orphelins
    // soient ecartes. Le compte de 740 publie par K2 etait mesure APRES Build :
    // il melangeait les reprises et l'elagage, et sous-estimait donc les
    // premieres.
    //
    // Ce qui est compare ici est le CHEMIN DE PRODUCTION -- un polygone par
    // region sous K2 comme sous K3 -- et, pour information seulement, ce que
    // couterait un polygone unique.
    const auto tessellatedVertices = [&opt](const std::vector<SvgShapeGroup>& gs,
                                            bool oneAppendPerGroup) -> size_t
    {
        ExtrudeAppendOptions ao;
        ao.zBottom              = 0.0f;
        ao.zTop                 = opt.height;
        ao.winding              = ExtrudeWinding::NonZero;
        ao.normalizeOrientation = false;

        ExtrudedMeshBuilder b;
        if (oneAppendPerGroup)
        {
            for (const SvgShapeGroup& g : gs)
                if (!g.contours.empty()) b.Append(g.contours, ao);
        }
        else
        {
            std::vector<ExtrudeContour> all;
            for (const SvgShapeGroup& g : gs)
                all.insert(all.end(), g.contours.begin(), g.contours.end());
            b.Append(all, ao);
        }
        return b.GetNVertices() / 4u;   // quatre blocs de n sommets par Append
    };

    std::vector<SvgShapeGroup> raw;
    ASSERT_TRUE(svg_to_shape_groups(kTiger, opt, raw));

    const size_t tessMonoNone = tessellatedVertices(raw,    false);
    const size_t tessPerNone  = tessellatedVertices(raw,    true);
    const size_t tessMonoSub  = tessellatedVertices(groups, false);
    const size_t tessPerSub   = tessellatedVertices(groups, true);

    const long combineMonoNone = (long)tessMonoNone - (long)pointsBefore;
    const long combinePerNone  = (long)tessPerNone  - (long)pointsBefore;
    const long combineMonoSub  = (long)tessMonoSub  - (long)pointsAfter;
    const long combinePerSub   = (long)tessPerSub   - (long)pointsAfter;

    std::cout << "  Tigre, volumetrie :\n"
              << "    points de contour : " << pointsBefore << " -> " << pointsAfter
              << "  (x" << ((double)pointsAfter / (double)pointsBefore) << ")\n";
    for (int i = 0; i < 3; ++i)
        std::cout << "    " << runs[i].label
                  << " : sommets " << nv[i]
                  << "  faces " << nf[i]
                  << "  capot " << nTop[i]
                  << "  parois " << nWall[i] << "\n";
    std::cout << "    triangles de capot par point de contour : "
              << (double)nTop[0] / (double)pointsBefore << " (None mono), "
              << (double)nTop[1] / (double)pointsBefore << " (None par forme), "
              << (double)nTop[2] / (double)pointsAfter  << " (Subtract)\n"
              << "    reprises COMBINE (sommets tessellés - points fournis) :\n"
              << "      chemin de production, un polygone par region :\n"
              << "        None     : " << tessPerNone << " - " << pointsBefore
              << " = " << combinePerNone << "\n"
              << "        Subtract : " << tessPerSub  << " - " << pointsAfter
              << " = " << combinePerSub  << "\n"
              << "      pour information, polygone unique :\n"
              << "        None     : " << tessMonoNone << " - " << pointsBefore
              << " = " << combineMonoNone << "\n"
              << "        Subtract : " << tessMonoSub  << " - " << pointsAfter
              << " = " << combineMonoSub  << std::endl;

    RecordProperty("contourPointsNone",     (int)pointsBefore);
    RecordProperty("contourPointsSubtract", (int)pointsAfter);
    RecordProperty("verticesSubtract",      (int)nv[2]);
    RecordProperty("facesSubtract",         (int)nf[2]);
    RecordProperty("capTrisSubtract",       (int)nTop[2]);
    RecordProperty("combinePerRegionNone",     (int)combinePerNone);
    RecordProperty("combinePerRegionSubtract", (int)combinePerSub);
    RecordProperty("combineMonolithicNone",    (int)combineMonoNone);

    // Relations structurelles, seules assertees : quatre blocs de n sommets par
    // Append, et un maillage non vide.
    for (int i = 0; i < 3; ++i)
    {
        EXPECT_EQ(nv[i] % 4u, 0u) << runs[i].label;
        EXPECT_GT(nf[i], 0u)      << runs[i].label;
    }

    // Le chemin de production ne demande a glutess de resoudre AUCUNE
    // intersection entre regions : ce qui subsiste est l'auto-intersection d'une
    // region avec elle-meme, marginale sous les deux politiques.
    EXPECT_LT(combinePerNone, (long)pointsBefore / 100);
    EXPECT_LT(combinePerSub,  (long)pointsAfter  / 100);

    // Cas positif de l'instrument : un polygone unique, lui, en fabrique par
    // milliers. Sans cette mesure, « presque zero » ne prouverait rien.
    EXPECT_GT(combineMonoNone, 100 * combinePerNone);
}
