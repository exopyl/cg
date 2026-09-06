// ============================================================================
//  K4 -- chronometrage et regle de bascule (svg_couleurs_faisabilite.md, sec. 12)
// ============================================================================
//
// Jalon DECISIONNEL : il ne construit rien de fonctionnel, il mesure. Trois
// postes separes, parce qu'un total ne dit pas QUOI alleger :
//
//   (a) parse nanosvg + aplatissement + epaississement des traits ;
//   (b) marqueterie M2a (Clipper2) ;
//   (c) tessellation + extrusion.
//
// Un quatrieme poste est publie, (d) BuildPolygonRenderData : il est rejoue a
// chaque rafraichissement de la vue (mesh.cpp, appele par BuildMeshPayload) et
// s'ajoute donc aux trois autres dans le cout percu par l'utilisateur. Il n'est
// pas compte dans le total des trois postes, qui doit rester comparable a celui
// de K3b.
//
// ---------------------------------------------------------------------------
//  Ce que K4 mesure de plus que K3b
// ---------------------------------------------------------------------------
// K3b a chronometre UNE execution. Une execution ne dit pas si le chiffre est
// stable : ici chaque poste est repete et rend min / mediane / moyenne / ecart
// type, de sorte que la comparaison au seuil de 300 ms porte sur une grandeur
// dont la dispersion est connue.
//
// Et surtout R6 : sous G2 (noeud monolithique `svg.extrude.colored`), `depth`
// devient un parametre du noeud qui fait l'import ET les booleens, donc toucher
// `depth` invalide sa signature de cache et rejoue TOUT (sec. 11.3). Le
// chronometre compare les deux couts -- rejeu complet contre rejeu du seul
// poste (c), qui est ce que le graphe etage rend gratuit aujourd'hui.
//
// ---------------------------------------------------------------------------
//  Ce que K4 ne fait pas
// ---------------------------------------------------------------------------
// Il ne juge pas la geometrie : un chronometre ne dit rien de la qualite du
// resultat. Il n'assere aucune duree -- une machine plus lente ferait echouer
// la suite sans rien apprendre. Les durees sont PUBLIEES ; la decision revient
// a l'utilisateur, conformement a la regle de bascule de D6.
//
// ⚠ nanosvg est instancie une seule fois dans le depot, par import_svg.cpp qui
// definit NANOSVG_IMPLEMENTATION.
//
// ============================================================================

#include <gtest/gtest.h>

#include "../src/cgmesh/extrude_contours.h"
#include "../src/cgmesh/import_svg.h"
#include "../src/cgmesh/mesh.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

const char* kTiger     = "./test/data/svg/Ghostscript_Tiger.svg";
const char* kRose      = "./test/data/svg/rose.svg";
const char* kSpiderman = "./test/data/svg/spiderman.svg";

// Repetitions par poste. Sept, et non trois : la mediane d'un echantillon impair
// est une valeur observee, et sept laisse assez de points pour qu'un ecart type
// ait un sens sans allonger la suite de plusieurs secondes.
const int kRuns = 7;

// Options du fichier de reference, identiques a celles de K3b : les deux jalons
// doivent chronometrer la meme charge.
SvgExtrudeOptions tigerOptions()
{
    SvgExtrudeOptions opt;
    opt.height       = 0.1f;
    opt.flattenTol   = 0.005f;
    opt.centerAndFit = true;
    opt.invertY      = true;
    return opt;
}

struct Stat
{
    double min    = 0.0;
    double median = 0.0;
    double mean   = 0.0;
    double sdev   = 0.0;   // ecart type d'echantillon (n-1)
    double spread = 0.0;   // (max - min) / mediane, en pourcentage
};

Stat summarize(std::vector<double> v)
{
    Stat s;
    if (v.empty()) return s;
    std::sort(v.begin(), v.end());
    s.min    = v.front();
    s.median = v[v.size() / 2];
    double sum = 0.0;
    for (double x : v) sum += x;
    s.mean = sum / (double)v.size();
    if (v.size() > 1)
    {
        double acc = 0.0;
        for (double x : v) acc += (x - s.mean) * (x - s.mean);
        s.sdev = std::sqrt(acc / (double)(v.size() - 1));
    }
    s.spread = (s.median > 0.0) ? 100.0 * (v.back() - v.front()) / s.median : 0.0;
    return s;
}

void printStat(const char* label, const Stat& s)
{
    std::cout << "    " << std::left << std::setw(26) << label << std::right
              << " min " << std::fixed << std::setprecision(2) << std::setw(8) << s.min
              << "  med " << std::setw(8) << s.median
              << "  moy " << std::setw(8) << s.mean
              << "  ec.t " << std::setw(7) << s.sdev
              << "  etendue " << std::setprecision(1) << std::setw(5) << s.spread << " %"
              << std::setprecision(2) << std::endl;
}

double msSince(const std::chrono::steady_clock::time_point& t0)
{
    return std::chrono::duration<double, std::milli>(
               std::chrono::steady_clock::now() - t0).count();
}

// Poste (c) isole : tessellation et extrusion sur des groupes DEJA resolus. Un
// Append par groupe, ce que fait le chemin `perShapeMaterials`.
double tessellateMs(const std::vector<SvgShapeGroup>& groups, float height,
                    unsigned int* verticesOut = nullptr,
                    unsigned int* facesOut    = nullptr)
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
    const double ms = msSince(t0);

    if (m)
    {
        if (verticesOut) *verticesOut = m->GetNVertices();
        if (facesOut)    *facesOut    = m->GetNFaces();
    }
    return ms;
}

struct PostStats
{
    Stat   a, b, c, total;
    size_t groups   = 0;
    size_t ptsAfter = 0;
};

// Une execution = les trois postes enchaines sur des donnees neuves. Les
// groupes sont reconstruits a chaque tour : reutiliser ceux du tour precedent
// mesurerait une marqueterie appliquee a une entree deja soustraite, donc vide.
PostStats measureThreePosts(const char* file, const SvgExtrudeOptions& opt, int runs)
{
    std::vector<double> va, vb, vc, vt;
    PostStats out;

    for (int i = 0; i < runs; ++i)
    {
        const auto t0 = std::chrono::steady_clock::now();
        std::vector<SvgShapeGroup> groups;
        if (!svg_to_shape_groups(file, opt, groups)) return out;
        const double ma = msSince(t0);

        SvgOverlapStats stats;
        svg_subtract_overlaps(groups, &stats);
        const double mb = stats.sweepMs;

        const double mc = tessellateMs(groups, opt.height);

        va.push_back(ma);
        vb.push_back(mb);
        vc.push_back(mc);
        vt.push_back(ma + mb + mc);

        out.groups = groups.size();
        out.ptsAfter = 0;
        for (const SvgShapeGroup& g : groups)
            for (const ExtrudeContour& c : g.contours)
                out.ptsAfter += c.pts.size();
    }

    out.a     = summarize(va);
    out.b     = summarize(vb);
    out.c     = summarize(vc);
    out.total = summarize(vt);
    return out;
}

} // namespace

// ============================================================================
//  1 -- Chronometrage a trois postes consolide, avec variance
// ============================================================================
//
// Deux colonnes, comme en K3b : sans anneaux (etat K3) et avec (etat D7, celui
// qui sera livre). Les deux viennent de la meme execution, donc de la meme
// machine dans le meme etat : elles sont comparables sans biais.

TEST(TEST_cgmesh_svg_k4, tiger_three_post_timing_consolidated)
{
    struct Run { const char* label; bool rings; };
    const Run runs[2] = { { "sans anneaux (K3)", false }, { "avec anneaux (D7)", true } };

    PostStats ps[2];

    for (int i = 0; i < 2; ++i)
    {
        SvgExtrudeOptions opt = tigerOptions();
        opt.strokeOnFilledShapes = runs[i].rings;
        ps[i] = measureThreePosts(kTiger, opt, kRuns);
        ASSERT_GT(ps[i].groups, 0u) << runs[i].label;
    }

    std::cout << "  Tigre, " << kRuns << " executions par poste, ms :\n";
    for (int i = 0; i < 2; ++i)
    {
        std::cout << "  " << runs[i].label
                  << "  (" << ps[i].groups << " groupes, "
                  << ps[i].ptsAfter << " points apres soustraction)\n";
        printStat("(a) parse+aplatissement", ps[i].a);
        printStat("(b) marqueterie M2a",     ps[i].b);
        printStat("(c) tessell.+extrusion",  ps[i].c);
        printStat("total (a+b+c)",           ps[i].total);
        std::cout << "    part du poste (b)          : "
                  << (100.0 * ps[i].b.median / ps[i].total.median) << " %" << std::endl;
    }

    RecordProperty("msTotalMedianD7", (int)ps[1].total.median);
    RecordProperty("msSweepMedianD7", (int)ps[1].b.median);
    RecordProperty("msParseMedianD7", (int)ps[1].a.median);
    RecordProperty("msTessMedianD7",  (int)ps[1].c.median);
    RecordProperty("sweepShareD7pct", (int)(100.0 * ps[1].b.median / ps[1].total.median));

    // Aucune duree n'est asseree. Seule relation asseree : les trois postes
    // couvrent bien un travail non nul, faute de quoi le chiffre publie serait
    // celui d'un pipeline qui n'a rien fait.
    EXPECT_GT(ps[1].a.median, 0.0);
    EXPECT_GT(ps[1].b.median, 0.0);
    EXPECT_GT(ps[1].c.median, 0.0);
}

// ============================================================================
//  2 -- R6 : ce que coute un changement de `depth`
// ============================================================================
//
// Sous le graphe ACTUEL (svg.contours -> shape.extrude), `depth` est un
// parametre du noeud AVAL : la signature de `svg.contours` ne change pas, son
// entree de cache ressert, et seul le poste (c) est rejoue (sec. 11.2).
//
// Sous G2, `depth` est un parametre du noeud MONOLITHIQUE : sa signature change,
// son entree de cache est invalidee, et les trois postes sont rejoues.
//
// R6 est confirmee si le second cout egale celui d'un recalcul complet -- c'est
// a dire s'il n'existe aucune economie residuelle qu'une implementation aurait
// pu conserver.

TEST(TEST_cgmesh_svg_k4, r6_depth_change_costs_a_full_recompute_under_g2)
{
    SvgExtrudeOptions opt = tigerOptions();
    opt.strokeOnFilledShapes = true;
    opt.perShapeMaterials    = true;
    opt.overlapPolicy        = SvgExtrudeOptions::OverlapPolicy::Subtract;

    // Cout d'un changement de `depth` sous le graphe ETAGE d'aujourd'hui :
    // les groupes resolus sont en cache, seule l'extrusion est refaite.
    std::vector<SvgShapeGroup> resolved;
    ASSERT_TRUE(svg_to_shape_groups(kTiger, opt, resolved));
    svg_subtract_overlaps(resolved, nullptr);

    std::vector<double> staged;
    for (int i = 0; i < kRuns; ++i)
    {
        SvgExtrudeOptions o = opt;
        o.height = 0.05f + 0.01f * (float)i;   // la profondeur change a chaque tour
        staged.push_back(tessellateMs(resolved, o.height));
    }

    // Cout d'un changement de `depth` sous G2 : le pipeline entier, palette
    // comprise, exactement ce que `svg.extrude.colored` recalculerait.
    std::vector<double> monolithic;
    for (int i = 0; i < kRuns; ++i)
    {
        SvgExtrudeOptions o = opt;
        o.height = 0.05f + 0.01f * (float)i;
        const auto t0 = std::chrono::steady_clock::now();
        std::unique_ptr<Mesh> m(import_svg_extruded(kTiger, o));
        const double ms = msSince(t0);
        ASSERT_NE(m, nullptr);
        monolithic.push_back(ms);
    }

    // Recalcul complet de reference : un changement de `flattenTol`, qu'aucun
    // etagement ne protege (sec. 11.7). C'est la borne a laquelle comparer.
    std::vector<double> full;
    for (int i = 0; i < kRuns; ++i)
    {
        SvgExtrudeOptions o = opt;
        o.flattenTol = 0.005f + 0.0001f * (float)i;
        const auto t0 = std::chrono::steady_clock::now();
        std::unique_ptr<Mesh> m(import_svg_extruded(kTiger, o));
        const double ms = msSince(t0);
        ASSERT_NE(m, nullptr);
        full.push_back(ms);
    }

    const Stat sStaged = summarize(staged);
    const Stat sMono   = summarize(monolithic);
    const Stat sFull   = summarize(full);

    std::cout << "  Cout d'un mouvement de curseur sur le Tigre (D7, M2a), ms :\n";
    printStat("depth, graphe etage",    sStaged);
    printStat("depth, G2 monolithique", sMono);
    printStat("flattenTol (reference)", sFull);
    std::cout << "    rapport G2 / recalcul complet : "
              << (sMono.median / sFull.median) << "\n"
              << "    rapport G2 / etage            : "
              << (sMono.median / sStaged.median) << "\n"
              << "    surcout absolu de la regression R6 : "
              << (sMono.median - sStaged.median) << " ms" << std::endl;

    RecordProperty("msDepthStaged", (int)sStaged.median);
    RecordProperty("msDepthG2",     (int)sMono.median);
    RecordProperty("msFullRecompute", (int)sFull.median);

    // Ce que le test etablit, et rien de plus : sous G2 un changement de `depth`
    // coute le meme ordre qu'un recalcul complet, alors que l'etagement le
    // ramenait a une fraction. Les bornes sont larges a dessein -- elles
    // qualifient un rapport, pas une machine.
    EXPECT_LT(sMono.median, 1.5 * sFull.median);
    EXPECT_GT(sMono.median, 0.5 * sFull.median);
    EXPECT_LT(sStaged.median, 0.5 * sMono.median);
}

// ============================================================================
//  3 -- Poste (d) : ce que coute le rafraichissement de la vue
// ============================================================================
//
// BuildPolygonRenderData est rejoue a chaque rafraichissement, independamment de
// la reconstruction du maillage. Il s'ajoute donc aux trois postes dans le delai
// que l'utilisateur percoit, et il est ignore par tout etagement du cache : le
// maillage change, la charge utile est refaite.

TEST(TEST_cgmesh_svg_k4, tiger_render_data_rebuild_cost)
{
    SvgExtrudeOptions opt = tigerOptions();
    opt.strokeOnFilledShapes = true;
    opt.perShapeMaterials    = true;
    opt.overlapPolicy        = SvgExtrudeOptions::OverlapPolicy::Subtract;

    std::unique_ptr<Mesh> m(import_svg_extruded(kTiger, opt));
    ASSERT_NE(m, nullptr);

    std::vector<double> v;
    size_t indices = 0, ranges = 0;
    for (int i = 0; i < kRuns; ++i)
    {
        const auto t0 = std::chrono::steady_clock::now();
        const Mesh::PolygonRenderData rd = m->BuildPolygonRenderData();
        v.push_back(msSince(t0));
        indices = rd.indices.size();
        ranges  = rd.materialRanges.size();
    }

    const Stat s = summarize(v);
    std::cout << "  Tigre (D7, M2a) : " << m->GetNVertices() << " sommets, "
              << m->GetNFaces() << " faces, " << m->GetNMaterials() << " materiaux\n";
    printStat("(d) BuildPolygonRenderData", s);
    std::cout << "    indices " << indices << ", plages de materiau " << ranges << std::endl;

    RecordProperty("msRenderDataMedian", (int)s.median);

    EXPECT_GT(indices, 0u);
    EXPECT_EQ(ranges, (size_t)m->GetNMaterials());
}

// ============================================================================
//  4 -- Les deux autres fichiers de reference (sec. 18)
// ============================================================================
//
// Le Tigre est le pire cas du depot ; le seuil de 300 ms ne doit pas etre lu sur
// lui seul. rose.svg et spiderman.svg disent ce que coute un fichier ordinaire.

TEST(TEST_cgmesh_svg_k4, reference_files_three_post_timing)
{
    struct File { const char* label; const char* path; };
    const File files[2] = { { "rose.svg", kRose }, { "spiderman.svg", kSpiderman } };

    for (const File& f : files)
    {
        SvgExtrudeOptions opt = tigerOptions();
        opt.strokeOnFilledShapes = true;
        const PostStats ps = measureThreePosts(f.path, opt, kRuns);
        ASSERT_GT(ps.groups, 0u) << f.label;

        std::cout << "  " << f.label << " (" << ps.groups << " groupes, "
                  << ps.ptsAfter << " points) :\n";
        printStat("(a) parse+aplatissement", ps.a);
        printStat("(b) marqueterie M2a",     ps.b);
        printStat("(c) tessell.+extrusion",  ps.c);
        printStat("total (a+b+c)",           ps.total);
    }
}
