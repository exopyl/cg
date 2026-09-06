// ============================================================================
//  K4b -- indexation spatiale de la marqueterie (svg_couleurs_faisabilite.md)
// ============================================================================
//
// K4 a mesure que le balayage M2a pesait 93 % du pipeline au navigateur, parce
// que l'accumulateur naif soustrayait le DESSIN ENTIER a chaque forme. K4b ne
// soustrait plus que les formes sus-jacentes dont la boite englobante coupe
// celle de la forme courante.
//
// C'est un changement de PERFORMANCE a geometrie strictement constante. Rien de
// ce que K0 a K4 asserent ne doit bouger, et cette suite ajoute la seule chose
// que les autres ne pouvaient pas dire : que l'algorithme indexe rend, forme
// par forme, exactement ce que rendait l'algorithme naif.
//
// ---------------------------------------------------------------------------
//  L'oracle, et pourquoi il est de cette forme
// ---------------------------------------------------------------------------
// L'algorithme naif est reproduit ICI, dans le test, et non conserve en
// production sous un drapeau : une seconde politique publique serait un chemin
// mort a documenter et a maintenir, alors que le role de ce code est d'etre une
// REFERENCE FIGEE contre laquelle comparer. Il est copie de l'etat K3 de
// `svg_subtract_overlaps` (import_svg.cpp), a l'identique.
//
// La comparaison porte sur deux niveaux, parce qu'ils ne disent pas la meme
// chose :
//
//   1. l'egalite GEOMETRIQUE -- l'aire de la difference symetrique des deux
//      regions. Elle ne depend ni de l'ordre des contours, ni du sommet de
//      depart, ni du nombre de points ;
//   2. l'egalite POINT PAR POINT -- meme nombre de contours, meme nombre de
//      points par contour, memes coordonnees. Un contour ferme n'ayant pas de
//      premier sommet, la comparaison est faite apres REALIGNEMENT CYCLIQUE :
//      sans cela elle mesurerait le sommet d'ouverture choisi par Clipper2, qui
//      n'est pas de la geometrie.
//
// Les deux sont mesures et publies. Ce qui est asserte comme EXACT est la
// structure -- meme nombre de contours, meme nombre de points ; les coordonnees
// et les aires le sont a la grille de Clipper2 pres, et le compte de groupes
// identiques au bit est publie sans etre asserte. La raison est mesuree, pas
// supposee : les deux algorithmes n'enchainent pas le meme nombre de passes
// Clipper2, et une passe arrondit a 1e-6.
//
// ---------------------------------------------------------------------------
//  Le mode de panne que cette suite existe pour attraper
// ---------------------------------------------------------------------------
// Rater un partenaire est SILENCIEUX : la region ressort simplement trop grande,
// elle recouvre sa voisine, et aucune assertion de K3 ne s'en apercoit sur une
// forme isolee. Le detecteur a donc ete vu echouer avant d'etre cru -- en
// retrecissant artificiellement les boites de l'index, cf. le journal du
// document. Un detecteur qui n'a jamais rien trouve ne prouve rien.
//
// ============================================================================

#include <gtest/gtest.h>

#include "../src/cgmesh/contour_ops.h"
#include "../src/cgmesh/import_svg.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {

const char* kTiger     = "./test/data/svg/Ghostscript_Tiger.svg";
const char* kRose      = "./test/data/svg/rose.svg";
const char* kSpiderman = "./test/data/svg/spiderman.svg";
const char* kBatman    = "./test/data/svg/batman.svg";
const char* kNazca     = "./test/data/svg/nazca.svg";

const int kRuns = 7;

// ---------------------------------------------------------------------------
//  Tolerances -- d'ou elles viennent
// ---------------------------------------------------------------------------
// Les deux algorithmes n'enchainent pas les memes passes Clipper2 : le naif
// soustrait un accumulateur deja arrondi 300 fois, l'indexe soustrait des
// formes d'origine. Clipper2 travaille a `precision = 6`, soit une grille de
// 1e-6 unite monde ; les frontieres ne peuvent donc coincider mieux que cela,
// et une frontiere de longueur 1 peut differer d'un ruban d'aire 1e-6.
//
// Ces seuils separent CET ecart-la de celui qu'on cherche a exclure. Un
// partenaire manque rend une region trop grande de l'aire d'un recouvrement,
// soit 1e-3 a 1e-1 sur le Tigre : quatre ordres de grandeur au-dessus. C'est
// verifie, pas suppose -- la perturbation de l'index est consignee au document.
//
// Le seuil de position vaut le cinquantieme de `flattenTol` (0,005), c'est-a-
// dire de la tolerance a laquelle la geometrie est ECHANTILLONNEE : un ecart
// qui lui est inferieur de deux ordres de grandeur ne peut pas etre une
// difference de forme.
const double kGridTol      = 1e-4;   // flattenTol / 50
const double kGroupAreaTol = 1e-6;   // ecart d'aire admis sur UNE region
const double kTotalAreaTol = 3e-6;   // cumule -- la meme tolerance qu'en K3

SvgExtrudeOptions defaultOptions()
{
    SvgExtrudeOptions opt;
    opt.height       = 0.1f;
    opt.flattenTol   = 0.005f;
    opt.centerAndFit = true;
    opt.invertY      = true;
    return opt;
}

void writeSvg(const char* path, const std::string& body)
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

double msSince(const std::chrono::steady_clock::time_point& t0)
{
    return std::chrono::duration<double, std::milli>(
               std::chrono::steady_clock::now() - t0).count();
}

// ---------------------------------------------------------------------------
//  L'algorithme naif -- etat K3 de svg_subtract_overlaps, copie mot pour mot
// ---------------------------------------------------------------------------
// `couvert` accumule la reunion de TOUT ce qui est au-dessus, et croit jusqu'a
// la complexite du dessin entier. C'est ce que K4b remplace, et c'est ce contre
// quoi K4b se compare.
void subtractOverlapsNaive(std::vector<SvgShapeGroup>& groups,
                           size_t* clipPointsFed = nullptr)
{
    std::vector<ExtrudeContour> covered;
    size_t fed = 0;

    for (size_t k = groups.size(); k-- > 0; )
    {
        const std::vector<ExtrudeContour> shape = std::move(groups[k].contours);
        for (const ExtrudeContour& c : covered) fed += c.pts.size();
        groups[k].contours = differenceContours(shape, covered);
        covered = unionContours(covered, shape);
    }

    if (clipPointsFed) *clipPointsFed = fed;
}

// Ce que l'indexation donne a manger a Clipper2, mesure de la meme facon : la
// somme, sur toutes les formes, des points du soustracteur. C'est la grandeur
// qui explique le facteur obtenu -- le nombre de PARTENAIRES ne l'explique pas,
// parce que l'accumulateur naif, etant une reunion, est plus simple que la
// somme des formes qu'il contient.
size_t indexedClipPointsFed(const std::vector<SvgShapeGroup>& groups)
{
    const size_t n = groups.size();
    std::vector<float> x0(n), y0(n), x1(n), y1(n);
    std::vector<bool>  ok(n, false);
    for (size_t k = 0; k < n; ++k)
        ok[k] = contoursBBox(groups[k].contours, x0[k], y0[k], x1[k], y1[k]);

    size_t fed = 0;
    for (size_t k = 0; k < n; ++k)
    {
        if (!ok[k]) continue;
        for (size_t j = k + 1; j < n; ++j)
        {
            if (!ok[j]) continue;
            if (x0[k] > x1[j] || x0[j] > x1[k] || y0[k] > y1[j] || y0[j] > y1[k]) continue;
            for (const ExtrudeContour& c : groups[j].contours) fed += c.pts.size();
        }
    }
    return fed;
}

// ---------------------------------------------------------------------------
//  Comparateur
// ---------------------------------------------------------------------------

struct RegionDiff
{
    size_t groups            = 0;
    size_t emptinessMismatch = 0;   // l'un rend une region, l'autre rien
    size_t contourMismatch   = 0;   // nombres de contours differents
    size_t pointCountMismatch = 0;  // memes contours, nombres de points differents
    size_t rotatedContours   = 0;   // memes points, ouverts a un autre sommet
    double maxSymDiffArea    = 0.0; // pire aire de difference symetrique
    double sumSymDiffArea    = 0.0;
    double maxPointDist      = 0.0; // pire ecart APRES realignement cyclique
    size_t identicalGroups   = 0;   // egaux point par point, au bit pres
};

// Ecart maximal entre deux contours de meme longueur, apres realignement
// CYCLIQUE. Un contour ferme n'a pas de premier sommet : Clipper2 peut l'ouvrir
// ailleurs sans que la geometrie change d'un iota, et comparer les tableaux
// dans l'ordre brut mesurerait alors ce decalage et non un ecart de forme.
// `rotated` dit si un decalage a ete necessaire.
double maxDistanceCyclic(const std::vector<Vector2f>& a, const std::vector<Vector2f>& b,
                         bool& rotated)
{
    const size_t n = a.size();
    rotated = false;
    if (n == 0) return 0.0;

    size_t best = 0;
    double bestDist = 1e300;
    for (size_t s = 0; s < n; ++s)
    {
        const double dx = (double)a[0].x - (double)b[s].x;
        const double dy = (double)a[0].y - (double)b[s].y;
        const double d  = dx*dx + dy*dy;
        if (d < bestDist) { bestDist = d; best = s; }
    }
    rotated = (best != 0);

    double worst = 0.0;
    for (size_t p = 0; p < n; ++p)
    {
        const Vector2f& q = b[(p + best) % n];
        const double dx = (double)a[p].x - (double)q.x;
        const double dy = (double)a[p].y - (double)q.y;
        worst = std::max(worst, std::sqrt(dx*dx + dy*dy));
    }
    return worst;
}

RegionDiff compareRegions(const std::vector<SvgShapeGroup>& a,
                          const std::vector<SvgShapeGroup>& b)
{
    RegionDiff d;
    d.groups = std::min(a.size(), b.size());

    for (size_t g = 0; g < d.groups; ++g)
    {
        const std::vector<ExtrudeContour>& ra = a[g].contours;
        const std::vector<ExtrudeContour>& rb = b[g].contours;

        if (ra.empty() != rb.empty()) ++d.emptinessMismatch;

        // Niveau 1 : egalite geometrique. Deux differences plutot qu'un XOR --
        // contour_ops.h n'expose pas le XOR, et la somme des deux differences
        // est la meme mesure.
        const double sym = netArea(differenceContours(ra, rb))
                         + netArea(differenceContours(rb, ra));
        d.sumSymDiffArea += sym;
        d.maxSymDiffArea = std::max(d.maxSymDiffArea, sym);

        // Niveau 2 : egalite point par point.
        if (ra.size() != rb.size()) { ++d.contourMismatch; continue; }

        bool identical = true;
        for (size_t c = 0; c < ra.size(); ++c)
        {
            if (ra[c].pts.size() != rb[c].pts.size())
            {
                ++d.pointCountMismatch;
                identical = false;
                break;
            }
            for (size_t p = 0; p < ra[c].pts.size(); ++p)
                if (ra[c].pts[p].x != rb[c].pts[p].x || ra[c].pts[p].y != rb[c].pts[p].y)
                {
                    identical = false;
                    break;
                }

            bool rotated = false;
            d.maxPointDist = std::max(d.maxPointDist,
                                      maxDistanceCyclic(ra[c].pts, rb[c].pts, rotated));
            if (rotated) ++d.rotatedContours;
        }
        if (identical) ++d.identicalGroups;
    }
    return d;
}

void printDiff(const char* label, const RegionDiff& d)
{
    std::cout << "  " << label << " : " << d.groups << " groupes\n"
              << "    egalite geometrique   : difference symetrique max "
              << std::scientific << std::setprecision(3) << d.maxSymDiffArea
              << ", cumulee " << d.sumSymDiffArea << std::defaultfloat << "\n"
              << "    egalite point a point : " << d.identicalGroups
              << " groupes identiques au bit"
              << ", " << d.contourMismatch << " ecarts de nombre de contours"
              << ", " << d.pointCountMismatch << " ecarts de nombre de points"
              << ", " << d.rotatedContours << " contours ouverts a un autre sommet"
              << ", ecart max apres realignement " << std::scientific << d.maxPointDist
              << std::defaultfloat << "\n"
              << "    emptinessMismatch     : " << d.emptinessMismatch << std::endl;
}

// Charge un fichier, produit les deux resolutions et les compare.
RegionDiff compareOnFile(const char* path, const SvgExtrudeOptions& opt,
                         SvgOverlapStats* statsOut = nullptr)
{
    std::vector<SvgShapeGroup> naive;
    if (!svg_to_shape_groups(path, opt, naive)) { ADD_FAILURE() << path; return {}; }
    std::vector<SvgShapeGroup> indexed = naive;

    subtractOverlapsNaive(naive);

    SvgOverlapStats stats;
    svg_subtract_overlaps(indexed, &stats);
    if (statsOut) *statsOut = stats;

    return compareRegions(naive, indexed);
}

} // namespace

// ============================================================================
//  1 -- Egalite ancien / nouveau sur les fixtures du depot
// ============================================================================
//
// Le Tigre est le cas qui a motive l'indexation : 304 groupes sous D7, 10,5 %
// de paires secantes. Les quatre autres fichiers sont la pour que l'egalite ne
// soit pas etablie sur une seule topologie.

TEST(TEST_cgmesh_svg_k4b, indexed_subtraction_matches_the_naive_sweep_on_the_tiger)
{
    SvgExtrudeOptions opt = defaultOptions();
    opt.strokeOnFilledShapes = true;

    SvgOverlapStats stats;
    const RegionDiff d = compareOnFile(kTiger, opt, &stats);

    printDiff("Tigre (D7)", d);
    std::cout << "    assiette de l'index   : " << stats.overlapPairs << " paires secantes sur "
              << stats.candidatePairs << " ("
              << (100.0 * (double)stats.overlapPairs / (double)stats.candidatePairs)
              << " %), soit " << (2.0 * (double)stats.overlapPairs / (double)d.groups)
              << " partenaires par groupe" << std::endl;

    RecordProperty("groups",       (int)d.groups);
    RecordProperty("overlapPairs", (int)stats.overlapPairs);
    RecordProperty("symDiffE12",       (int)(d.sumSymDiffArea * 1e12));
    RecordProperty("maxPointDistE9",   (int)(d.maxPointDist * 1e9));
    RecordProperty("bitIdenticalGroups", (int)d.identicalGroups);

    ASSERT_GT(d.groups, 300u);
    EXPECT_EQ(d.emptinessMismatch,  0u);
    EXPECT_EQ(d.contourMismatch,    0u);
    EXPECT_EQ(d.pointCountMismatch, 0u);
    EXPECT_LT(d.maxPointDist,    kGridTol);
    EXPECT_LT(d.maxSymDiffArea,  kGroupAreaTol);
    EXPECT_LT(d.sumSymDiffArea,  kTotalAreaTol);
}

TEST(TEST_cgmesh_svg_k4b, indexed_subtraction_matches_the_naive_sweep_on_other_fixtures)
{
    struct File { const char* label; const char* path; };
    const File files[4] = { { "rose.svg", kRose }, { "spiderman.svg", kSpiderman },
                            { "batman.svg", kBatman }, { "nazca.svg", kNazca } };

    for (const File& f : files)
    {
        SvgExtrudeOptions opt = defaultOptions();
        opt.strokeOnFilledShapes = true;

        SvgOverlapStats stats;
        const RegionDiff d = compareOnFile(f.path, opt, &stats);
        printDiff(f.label, d);
        std::cout << "    assiette de l'index   : " << stats.overlapPairs << " / "
                  << stats.candidatePairs << " paires" << std::endl;

        EXPECT_GT(d.groups, 0u)             << f.label;
        EXPECT_EQ(d.emptinessMismatch,  0u) << f.label;
        EXPECT_EQ(d.contourMismatch,    0u) << f.label;
        EXPECT_EQ(d.pointCountMismatch, 0u) << f.label;
        EXPECT_LT(d.maxPointDist,   kGridTol)      << f.label;
        EXPECT_LT(d.maxSymDiffArea, kGroupAreaTol) << f.label;
    }
}

// ============================================================================
//  2 -- Le piege : un trou du dessus ne doit pas repercer la region du dessous
// ============================================================================
//
// Trois formes, du dessous vers le dessus :
//
//   C -- un grand carre plein, celui dont on regarde la region ;
//   A -- un anneau (carre perce) pose sur C : son TROU laisse voir C ;
//   B -- un carre plein qui recouvre exactement le trou de A.
//
// La region gardee de C doit etre C prive de A ET de B : le trou de A est
// bouche par B, il ne doit rien laisser reapparaitre. Passer A et B comme clips
// separes SANS les canoniser ferait s'annuler le -1 du trou de A et le +1 de
// l'exterieur de B sous NonZero : C ressortirait avec un ilot au milieu.
//
// L'aire attendue se calcule a la main : C = 100x100, A couvre 60x60 moins un
// trou de 20x20, B couvre ce trou. A union B = 60x60 plein. Region de C =
// 10000 - 3600 = 6400, soit 64 % de C.

TEST(TEST_cgmesh_svg_k4b, a_hole_above_does_not_pierce_the_region_below)
{
    const char* path = "k4b_hole_above.svg";
    writeSvg(path,
        // C, dessous
        "  <rect x=\"0\" y=\"0\" width=\"100\" height=\"100\" fill=\"#0000ff\"/>\n"
        // A, anneau : enveloppe 20..80, trou 40..60 (sous-chemins de sens opposes)
        "  <path d=\"M20,20 L80,20 L80,80 L20,80 Z M40,40 L40,60 L60,60 L60,40 Z\""
        " fill=\"#ff0000\"/>\n"
        // B, bouchon exactement sur le trou de A
        "  <rect x=\"40\" y=\"40\" width=\"20\" height=\"20\" fill=\"#00ff00\"/>\n");

    SvgExtrudeOptions opt = defaultOptions();

    std::vector<SvgShapeGroup> groups;
    ASSERT_TRUE(svg_to_shape_groups(path, opt, groups));
    ASSERT_EQ(groups.size(), 3u);

    const double areaC = netArea(groups[0].contours);
    const double areaA = netArea(groups[1].contours);

    // La fixture est bien celle qu'on croit : A est un anneau, pas un carre
    // plein -- 60x60 - 20x20 = 3200, soit 32 % de C.
    EXPECT_NEAR(areaA / areaC, 0.32, 1e-3) << "la fixture n'a pas de trou";

    std::vector<SvgShapeGroup> naive = groups;
    subtractOverlapsNaive(naive);
    svg_subtract_overlaps(groups, nullptr);

    const double keptNaive   = netArea(naive[0].contours);
    const double keptIndexed = netArea(groups[0].contours);

    std::cout << "  region de C : naif " << (keptNaive / areaC)
              << "  indexe " << (keptIndexed / areaC)
              << "  (attendu 0.64 ; 0.68 signalerait un trou reperce)"
              << "\n    contours de C : naif " << naive[0].contours.size()
              << ", indexe " << groups[0].contours.size() << std::endl;

    // 0,64 et non 0,68 : le trou de A est bouche par B.
    EXPECT_NEAR(keptIndexed / areaC, 0.64, 1e-3);
    EXPECT_NEAR(keptNaive / areaC,   0.64, 1e-3);

    // C prive d'un carre interieur est un anneau : une enveloppe et un trou. Un
    // trou reperce y ajouterait un ilot, donc un TROISIEME contour.
    EXPECT_EQ(groups[0].contours.size(), 2u);
    EXPECT_EQ(naive[0].contours.size(),  2u);

    const RegionDiff d = compareRegions(naive, groups);
    EXPECT_EQ(d.identicalGroups, d.groups);
    EXPECT_LT(d.maxSymDiffArea, kGroupAreaTol);

    std::remove(path);
}

// ============================================================================
//  3 -- Ce que l'indexation coute et ce qu'elle rapporte, sur le Tigre
// ============================================================================
//
// Aucune duree n'est asseree : une machine plus lente ferait echouer la suite
// sans rien apprendre. Les deux algorithmes sont chronometres dans la MEME
// execution, donc sur la meme machine dans le meme etat -- c'est la seule facon
// de rendre le rapport comparable.

TEST(TEST_cgmesh_svg_k4b, tiger_sweep_cost_naive_versus_indexed)
{
    SvgExtrudeOptions opt = defaultOptions();
    opt.strokeOnFilledShapes = true;

    std::vector<SvgShapeGroup> reference;
    ASSERT_TRUE(svg_to_shape_groups(kTiger, opt, reference));

    std::vector<double> vNaive, vIndexed, vNormalize;
    size_t fedNaive = 0;
    SvgOverlapStats stats;

    for (int i = 0; i < kRuns; ++i)
    {
        std::vector<SvgShapeGroup> g = reference;
        const auto t0 = std::chrono::steady_clock::now();
        subtractOverlapsNaive(g, &fedNaive);
        vNaive.push_back(msSince(t0));
    }

    for (int i = 0; i < kRuns; ++i)
    {
        std::vector<SvgShapeGroup> g = reference;
        svg_subtract_overlaps(g, &stats);
        vIndexed.push_back(stats.sweepMs);
    }

    // Poste de normalisation, isole : une union par forme, sur la forme seule.
    // C'est le prix de l'hypothese qui rend la concatenation des clips exacte.
    for (int i = 0; i < kRuns; ++i)
    {
        const auto t0 = std::chrono::steady_clock::now();
        for (const SvgShapeGroup& g : reference)
        {
            const std::vector<ExtrudeContour> u = unionContours(g.contours, {});
            (void)u;
        }
        vNormalize.push_back(msSince(t0));
    }

    const size_t fedIndexed = indexedClipPointsFed(reference);

    std::sort(vNaive.begin(), vNaive.end());
    std::sort(vIndexed.begin(), vIndexed.end());
    std::sort(vNormalize.begin(), vNormalize.end());
    const double medNaive     = vNaive[vNaive.size() / 2];
    const double medIndexed   = vIndexed[vIndexed.size() / 2];
    const double medNormalize = vNormalize[vNormalize.size() / 2];

    std::cout << "  Tigre (D7), balayage seul, mediane de " << kRuns << " executions :\n"
              << "    naif    : " << medNaive   << " ms\n"
              << "    indexe  : " << medIndexed << " ms   (dont normalisation "
              << medNormalize << " ms)\n"
              << "    facteur : " << (medNaive / medIndexed) << "\n"
              << "  points de soustracteur donnes a Clipper2 :\n"
              << "    naif    : " << fedNaive   << "\n"
              << "    indexe  : " << fedIndexed << "   (facteur "
              << ((double)fedNaive / (double)fedIndexed) << ")\n"
              << "  paires secantes : " << stats.overlapPairs << " / "
              << stats.candidatePairs << " = "
              << (100.0 * (double)stats.overlapPairs / (double)stats.candidatePairs)
              << " %  -> facteur de reduction ESPERE "
              << ((double)reference.size() * (double)(reference.size() - 1) * 0.5
                  / (double)stats.overlapPairs)
              << std::endl;

    RecordProperty("msNaive",   (int)medNaive);
    RecordProperty("msIndexed", (int)medIndexed);
    RecordProperty("speedupPct", (int)(100.0 * medNaive / medIndexed));

    // Seule relation asseree : les deux algorithmes ont travaille.
    EXPECT_GT(medNaive,   0.0);
    EXPECT_GT(medIndexed, 0.0);
}

// ============================================================================
//  4 -- Le cas degenere : densite de recouvrement de 100 %
// ============================================================================
//
// Cent carres identiques decales d'un pixel : toutes les paires de boites se
// coupent, aucune forme n'en recouvre entierement une autre. L'index ne peut
// alors ecarter personne, et l'indexation devient un COUT NET : elle paie les
// tests de boites, et elle perd le benefice de l'accumulateur, qui fondait les
// formes deja vues en une region plus simple que leur somme.
//
// Ce test ne juge pas : il PUBLIE. C'est la borne haute du compromis, et elle
// doit etre connue de qui lira le chiffre du Tigre.

TEST(TEST_cgmesh_svg_k4b, fully_overlapping_shapes_are_the_worst_case_of_the_index)
{
    const char* path = "k4b_all_overlapping.svg";
    const int   n    = 100;

    std::string body;
    for (int i = 0; i < n; ++i)
    {
        char buf[256];
        std::snprintf(buf, sizeof(buf),
                      "  <rect x=\"%d\" y=\"%d\" width=\"400\" height=\"400\""
                      " fill=\"#%02x%02x40\"/>\n", i, i, i * 2, 255 - i * 2);
        body += buf;
    }
    writeSvg(path, body);

    SvgExtrudeOptions opt = defaultOptions();

    std::vector<SvgShapeGroup> reference;
    ASSERT_TRUE(svg_to_shape_groups(path, opt, reference));
    ASSERT_EQ(reference.size(), (size_t)n);

    std::vector<double> vNaive, vIndexed;
    SvgOverlapStats stats;
    for (int i = 0; i < kRuns; ++i)
    {
        std::vector<SvgShapeGroup> g = reference;
        const auto t0 = std::chrono::steady_clock::now();
        subtractOverlapsNaive(g);
        vNaive.push_back(msSince(t0));
    }
    for (int i = 0; i < kRuns; ++i)
    {
        std::vector<SvgShapeGroup> g = reference;
        svg_subtract_overlaps(g, &stats);
        vIndexed.push_back(stats.sweepMs);
    }

    std::sort(vNaive.begin(), vNaive.end());
    std::sort(vIndexed.begin(), vIndexed.end());
    const double medNaive   = vNaive[vNaive.size() / 2];
    const double medIndexed = vIndexed[vIndexed.size() / 2];

    std::cout << "  " << n << " carres tous secants (densite "
              << (100.0 * (double)stats.overlapPairs / (double)stats.candidatePairs)
              << " %) :\n"
              << "    naif   : " << medNaive   << " ms\n"
              << "    indexe : " << medIndexed << " ms\n"
              << "    facteur : " << (medNaive / medIndexed) << std::endl;

    RecordProperty("worstCaseMsNaive",   (int)(medNaive * 1000));
    RecordProperty("worstCaseMsIndexed", (int)(medIndexed * 1000));

    // La densite est bien de 100 % : sans cela le test ne mesurerait pas le pire
    // cas qu'il pretend mesurer.
    EXPECT_EQ(stats.overlapPairs, stats.candidatePairs);

    // Et la geometrie reste la meme, y compris ici.
    std::vector<SvgShapeGroup> naive = reference, indexed = reference;
    subtractOverlapsNaive(naive);
    svg_subtract_overlaps(indexed, nullptr);
    const RegionDiff d = compareRegions(naive, indexed);
    printDiff("100 carres secants", d);
    EXPECT_EQ(d.emptinessMismatch, 0u);
    EXPECT_LT(d.maxSymDiffArea, kGroupAreaTol);

    std::remove(path);
}
