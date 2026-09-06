// ============================================================================
//  INSTRUMENT DE MESURE -- jalon K4 (svg_couleurs_faisabilite.md, section 12)
// ============================================================================
//
// Ce fichier N'EST PAS la fonctionnalite. C'est un harnais jetable, hors de la
// cible `maker`, dont l'unique role est de rendre en WebAssembly les memes trois
// postes que le test natif `test/tu_cgmesh_svg_k4_timing.cpp` :
//
//   (a) parse nanosvg + aplatissement + epaississement des traits ;
//   (b) marqueterie M2a (Clipper2) ;
//   (c) tessellation + extrusion ;
//   (d) BuildPolygonRenderData, rejoue a chaque rafraichissement de la vue.
//
// ---------------------------------------------------------------------------
//  Pourquoi un harnais et non la page `svg.html`
// ---------------------------------------------------------------------------
// Le noeud `svg.extrude.colored` (G2) n'existe qu'au jalon K5. Le gabarit
// `svg.json` passe aujourd'hui par `svg.contours -> shape.extrude`, qui n'exerce
// ni la marqueterie ni le trait des formes remplies : le chronometrer donnerait
// un chiffre exact decrivant une configuration qui ne sera pas livree.
//
// Ce harnais appelle donc directement les memes fonctions de `cgmesh` que le
// noeud appellera, avec les memes options. Il mesure le CALCUL, dans le meme
// moteur WebAssembly que la page ; il ne mesure ni le transport de la charge
// utile vers three.js, ni le rendu. Le dire est la condition pour que le chiffre
// serve.
//
// ---------------------------------------------------------------------------
//  Duree de vie
// ---------------------------------------------------------------------------
// Reemploye par K4b, qui lui a ajoute le balayage NAIF de reference : les deux
// algorithmes sont ainsi chronometres dans le meme moteur et la meme execution.
// A supprimer une fois K4b clos et ses chiffres verses au document. Il n'est lie
// a aucune cible du CMakeLists : il se construit par `build.ps1`, a cote.
//
// ============================================================================

#include "../../../src/cgmesh/contour_ops.h"
#include "../../../src/cgmesh/extrude_contours.h"
#include "../../../src/cgmesh/import_svg.h"
#include "../../../src/cgmesh/mesh.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace {

// Chemin dans le systeme de fichiers virtuel : le SVG est embarque par
// --embed-file, de sorte que la page n'ait rien a telecharger et que la mesure
// ne comprenne aucune latence reseau.
const char* kTiger = "/tiger.svg";

const int kRuns = 7;

SvgExtrudeOptions tigerOptions()
{
    SvgExtrudeOptions opt;
    opt.height       = 0.1f;
    opt.flattenTol   = 0.005f;
    opt.centerAndFit = true;
    opt.invertY      = true;
    return opt;
}

struct Stat { double min = 0.0, median = 0.0, mean = 0.0, sdev = 0.0, spread = 0.0; };

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
    std::printf("    %-26s min %8.2f  med %8.2f  moy %8.2f  ec.t %7.2f  etendue %5.1f %%\n",
                label, s.min, s.median, s.mean, s.sdev, s.spread);
}

double msSince(const std::chrono::steady_clock::time_point& t0)
{
    return std::chrono::duration<double, std::milli>(
               std::chrono::steady_clock::now() - t0).count();
}

// Balayage NAIF -- etat K3 de svg_subtract_overlaps, copie mot pour mot, comme
// dans test/tu_cgmesh_svg_k4b_indexed_subtract.cpp. Il sert de reference : le
// harnais chronometre les deux algorithmes dans le MEME moteur, la meme page et
// la meme execution, de sorte que le rapport ne depende d'aucune comparaison
// entre deux campagnes.
double subtractNaiveMs(std::vector<SvgShapeGroup>& groups)
{
    const auto t0 = std::chrono::steady_clock::now();
    std::vector<ExtrudeContour> covered;
    for (size_t k = groups.size(); k-- > 0; )
    {
        const std::vector<ExtrudeContour> shape = std::move(groups[k].contours);
        groups[k].contours = differenceContours(shape, covered);
        covered = unionContours(covered, shape);
    }
    return msSince(t0);
}

double tessellateMs(const std::vector<SvgShapeGroup>& groups, float height)
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
    return msSince(t0);
}

void threePosts(const char* label, bool rings)
{
    SvgExtrudeOptions opt = tigerOptions();
    opt.strokeOnFilledShapes = rings;

    std::vector<double> va, vb, vc, vt;
    size_t groups = 0, points = 0;

    for (int i = 0; i < kRuns; ++i)
    {
        const auto t0 = std::chrono::steady_clock::now();
        std::vector<SvgShapeGroup> g;
        if (!svg_to_shape_groups(kTiger, opt, g)) { std::printf("  ECHEC de lecture\n"); return; }
        const double ma = msSince(t0);

        SvgOverlapStats stats;
        svg_subtract_overlaps(g, &stats);
        const double mb = stats.sweepMs;

        const double mc = tessellateMs(g, opt.height);

        va.push_back(ma); vb.push_back(mb); vc.push_back(mc);
        vt.push_back(ma + mb + mc);

        groups = g.size();
        points = 0;
        for (const SvgShapeGroup& gg : g)
            for (const ExtrudeContour& c : gg.contours) points += c.pts.size();
    }

    const Stat sa = summarize(va), sb = summarize(vb), sc = summarize(vc), st = summarize(vt);
    std::printf("  %s  (%u groupes, %u points apres soustraction)\n",
                label, (unsigned)groups, (unsigned)points);
    printStat("(a) parse+aplatissement", sa);
    printStat("(b) marqueterie M2a",     sb);
    printStat("(c) tessell.+extrusion",  sc);
    printStat("total (a+b+c)",           st);
    std::printf("    part du poste (b)          : %.2f %%\n",
                (st.median > 0.0) ? 100.0 * sb.median / st.median : 0.0);
}

// Poste (b) sous les deux algorithmes, cote a cote. Les groupes sont
// reconstruits a chaque tour : les reutiliser mesurerait une marqueterie
// appliquee a une entree deja soustraite.
void sweepNaiveVersusIndexed(bool rings)
{
    SvgExtrudeOptions opt = tigerOptions();
    opt.strokeOnFilledShapes = rings;

    std::vector<SvgShapeGroup> reference;
    if (!svg_to_shape_groups(kTiger, opt, reference)) { std::printf("  ECHEC de lecture\n"); return; }

    std::vector<double> vn, vi;
    SvgOverlapStats stats;
    for (int i = 0; i < kRuns; ++i)
    {
        std::vector<SvgShapeGroup> g = reference;
        vn.push_back(subtractNaiveMs(g));
    }
    for (int i = 0; i < kRuns; ++i)
    {
        std::vector<SvgShapeGroup> g = reference;
        svg_subtract_overlaps(g, &stats);
        vi.push_back(stats.sweepMs);
    }

    const Stat sn = summarize(vn), si = summarize(vi);
    std::printf("  poste (b) seul, %u groupes, %u paires secantes sur %u (%.2f %%)\n",
                (unsigned)reference.size(), stats.overlapPairs, stats.candidatePairs,
                100.0 * (double)stats.overlapPairs / (double)stats.candidatePairs);
    printStat("(b) naif   (etat K4)", sn);
    printStat("(b) indexe (K4b)",     si);
    std::printf("    facteur                    : %.2f\n",
                (si.median > 0.0) ? sn.median / si.median : 0.0);
}

// R6 : sous G2, `depth` est un parametre du noeud monolithique, donc son
// changement rejoue tout. Sous le graphe etage d'aujourd'hui, seul le poste (c)
// est rejoue. Les deux couts sont mesures, pas deduits.
void depthCost()
{
    SvgExtrudeOptions opt = tigerOptions();
    opt.strokeOnFilledShapes = true;
    opt.perShapeMaterials    = true;
    opt.overlapPolicy        = SvgExtrudeOptions::OverlapPolicy::Subtract;

    std::vector<SvgShapeGroup> resolved;
    if (!svg_to_shape_groups(kTiger, opt, resolved)) { std::printf("  ECHEC de lecture\n"); return; }
    svg_subtract_overlaps(resolved, nullptr);

    std::vector<double> staged, mono, full;
    for (int i = 0; i < kRuns; ++i)
        staged.push_back(tessellateMs(resolved, 0.05f + 0.01f * (float)i));

    for (int i = 0; i < kRuns; ++i)
    {
        SvgExtrudeOptions o = opt;
        o.height = 0.05f + 0.01f * (float)i;
        const auto t0 = std::chrono::steady_clock::now();
        std::unique_ptr<Mesh> m(import_svg_extruded(kTiger, o));
        mono.push_back(msSince(t0));
    }

    for (int i = 0; i < kRuns; ++i)
    {
        SvgExtrudeOptions o = opt;
        o.flattenTol = 0.005f + 0.0001f * (float)i;
        const auto t0 = std::chrono::steady_clock::now();
        std::unique_ptr<Mesh> m(import_svg_extruded(kTiger, o));
        full.push_back(msSince(t0));
    }

    const Stat ss = summarize(staged), sm = summarize(mono), sf = summarize(full);
    printStat("depth, graphe etage",    ss);
    printStat("depth, G2 monolithique", sm);
    printStat("flattenTol (reference)", sf);
    std::printf("    rapport G2 / recalcul complet : %.2f\n",
                (sf.median > 0.0) ? sm.median / sf.median : 0.0);
    std::printf("    rapport G2 / etage            : %.2f\n",
                (ss.median > 0.0) ? sm.median / ss.median : 0.0);
    std::printf("    surcout absolu de R6          : %.2f ms\n", sm.median - ss.median);
}

void renderData()
{
    SvgExtrudeOptions opt = tigerOptions();
    opt.strokeOnFilledShapes = true;
    opt.perShapeMaterials    = true;
    opt.overlapPolicy        = SvgExtrudeOptions::OverlapPolicy::Subtract;

    std::unique_ptr<Mesh> m(import_svg_extruded(kTiger, opt));
    if (!m) { std::printf("  ECHEC de construction\n"); return; }

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

    std::printf("  Tigre (D7, M2a) : %u sommets, %u faces, %u materiaux\n",
                m->GetNVertices(), m->GetNFaces(), m->GetNMaterials());
    printStat("(d) BuildPolygonRenderData", summarize(v));
    std::printf("    indices %u, plages de materiau %u\n",
                (unsigned)indices, (unsigned)ranges);
}

} // namespace

int main()
{
    std::printf("=== K4 -- chronometrage WebAssembly, Tigre, %d executions par poste ===\n", kRuns);
    threePosts("sans anneaux (K3)", false);
    threePosts("avec anneaux (D7)", true);
    std::printf("  Balayage naif contre balayage indexe (D7) :\n");
    sweepNaiveVersusIndexed(true);
    std::printf("  Cout d'un mouvement de curseur (D7, M2a), ms :\n");
    depthCost();
    std::printf("  Rafraichissement de la vue :\n");
    renderData();
    std::printf("=== fin ===\n");
    return 0;
}
