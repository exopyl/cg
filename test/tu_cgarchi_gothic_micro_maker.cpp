#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <fstream>
#include <locale>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <cgarchi/gothic_microarchitecture.h>
#include <cgarchi/gothic_microarchitecture_export.h>
#include <cgarchi/gothic_microarchitecture_json.h>
#include <cgarchi/gothic_microarchitecture_solid.h>
#include <cgarchi/gothic_microarchitecture_tubes.h>
#include <cgarchi/parameterized_gothic_microarchitecture.h>
#include "mesh.h"
#include "mesh_validation.h"

using namespace cgarchi::microarch;   // fichier de test du module seul
using cgarchi::MicroarchMakerLimits;
using cgarchi::ParameterizedGothicMicroarchitecture;

// ===========================================================================
//  Microarchitecture gothique : adaptateur Maker (J6)
// ===========================================================================
//
// Plan d'execution, §13.4.1. Reference D4 segment, parametres convertis de
// l'adaptateur (5 % de D... ici 5 permil, 6 cotes, 5 deg) : V = 1 104, F = 2 176.

namespace
{
    constexpr double kPi = 3.14159265358979323846;
    const char *const kD4Segment = "./test/data/gothic_micro_d4_segment.json";

    std::string readText(const std::string &path)
    {
        std::ifstream in(path, std::ios::binary);
        std::ostringstream os;
        os << in.rdbuf();
        return os.str();
    }

    Parameter *find(std::vector<Parameter> &params, const std::string &name)
    {
        for (Parameter &p : params)
            if (p.GetName() == name)
                return &p;
        ADD_FAILURE() << "parametre absent : " << name;
        return nullptr;
    }

    // Reseau et JSON calcules hors de l'adaptateur, a partir du meme texte.
    struct Reference
    {
        MicroarchSpec   spec;
        MicroarchReport report;
        Network         net;
    };

    Reference reference(const std::string &text)
    {
        Reference r;
        EXPECT_TRUE(LoadSpecFromJson(text, r.spec, r.report)) << ReportToJson(r.report);
        std::optional<Network> net = BuildNetwork(r.spec, r.report);
        EXPECT_TRUE(net.has_value()) << ReportToJson(r.report);
        if (net)
            r.net = std::move(*net);
        return r;
    }

    std::size_t count(const MicroarchReport &r, MicroarchError e) { return r.Count(e); }

    struct CommaPunct : std::numpunct<char>
    {
        char do_decimal_point() const override { return ','; }
        char do_thousands_sep() const override { return '.'; }
        std::string do_grouping() const override { return "\3"; }
    };

    std::string shifted(const std::string &text, double dx, double dy)
    {
        MicroarchSpec spec;
        MicroarchReport r;
        EXPECT_TRUE(LoadSpecFromJson(text, spec, r));
        spec.symmetry.center = Vector2d(spec.symmetry.center.x + dx, spec.symmetry.center.y + dy);
        for (PointSpec &p : spec.points)
            p.xy = Vector2d(p.xy.x + dx, p.xy.y + dy);
        return SpecToJson(spec);
    }
}

// 1. Lecture, reseau, JSON, puis tubes.
TEST(TEST_cgarchi_gothic_micro_maker, load_builds_the_network_then_the_json_then_the_tubes)
{
    const std::string text = readText(kD4Segment);
    const Reference ref = reference(text);
    const std::string expected = NetworkToJson(ref.net, ref.spec, ref.report);
    ParameterizedGothicMicroarchitecture obj;
    int calls = 0;
    std::string received;
    bool meshDuringSink = true;
    std::size_t tubesDuringSink = 99;
    ASSERT_TRUE(obj.Load(text, [&](const std::string &json) {
        ++calls;
        received = json;
        meshDuringSink = obj.GetMesh() != nullptr;
        tubesDuringSink = obj.TubeBuilds();
    })) << ReportToJson(obj.LoadReport());
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(received, expected);
    EXPECT_FALSE(meshDuringSink);
    EXPECT_EQ(tubesDuringSink, 0u);
    ASSERT_NE(obj.GetMesh(), nullptr);
    EXPECT_EQ(obj.GetMesh()->GetNVertices(), 1104u);
    EXPECT_EQ(obj.GetMesh()->GetNFaces(), 2176u);
    // Texte natif pour le harnais WASM (comparaison par empreinte).
    std::ofstream("tmp_gothic_micro_maker_d4_segment_network.json", std::ios::binary) << received;
}

// 2. Cache : aucune reconstruction sans changement.
TEST(TEST_cgarchi_gothic_micro_maker, regenerate_without_change_rebuilds_nothing)
{
    ParameterizedGothicMicroarchitecture obj;
    ASSERT_TRUE(obj.Load(readText(kD4Segment), nullptr));
    for (int k = 0; k < 3; ++k)
        obj.Regenerate();
    EXPECT_EQ(obj.TubeBuilds(), 1u);
    EXPECT_EQ(obj.NetworkBuilds(), 1u);
}

// 3. Un parametre d'affichage ne reconstruit que les tubes.
TEST(TEST_cgarchi_gothic_micro_maker, a_display_parameter_rebuilds_the_tubes_only)
{
    ParameterizedGothicMicroarchitecture obj;
    ASSERT_TRUE(obj.Load(readText(kD4Segment), nullptr));
    MicroarchReport r0;
    const std::string curves = obj.NetworkObj(r0);
    std::vector<Parameter> params = obj.GetParameters();
    find(params, "Tube sides")->SetInt(3);
    obj.Regenerate();
    ASSERT_NE(obj.GetMesh(), nullptr);
    EXPECT_EQ(obj.GetMesh()->GetNVertices(), 552u);
    EXPECT_EQ(obj.GetMesh()->GetNFaces(), 1072u);
    EXPECT_EQ(obj.NetworkBuilds(), 1u);
    EXPECT_EQ(obj.TubeBuilds(), 2u);
    MicroarchReport r1;
    EXPECT_EQ(obj.NetworkObj(r1), curves);

    // Le pas angulaire seul fait partie de la cle du cache : 2 degres echantillonnent
    // plus finement, donc plus de triangles.
    find(params, "Sampling angle (deg)")->SetFloat(2.f);
    obj.Regenerate();
    EXPECT_EQ(obj.TubeBuilds(), 3u);
    EXPECT_EQ(obj.NetworkBuilds(), 1u);
    ASSERT_NE(obj.GetMesh(), nullptr);
    EXPECT_GT(obj.GetMesh()->GetNFaces(), 1072u);
}

// 4. Les bornes des curseurs sont acceptees, converties exactement.
TEST(TEST_cgarchi_gothic_micro_maker, slider_bounds_are_accepted_exactly)
{
    ParameterizedGothicMicroarchitecture obj;
    ASSERT_TRUE(obj.Load(readText(kD4Segment), nullptr));
    std::vector<Parameter> params = obj.GetParameters();
    find(params, "Tube radius (permil of D)")->SetFloat(100.f);
    find(params, "Sampling angle (deg)")->SetFloat(22.5f);
    EXPECT_EQ(find(params, "Tube radius (permil of D)")->GetMaxFloat(), 100.f);
    EXPECT_EQ(find(params, "Sampling angle (deg)")->GetMaxFloat(), 22.5f);
    obj.Regenerate();
    EXPECT_EQ(obj.CurrentTubeParams().radius_ratio, TubeLimits::max_radius_ratio);
    EXPECT_EQ(obj.CurrentTubeParams().sampling.max_angle, TubeLimits::max_angle);
    EXPECT_EQ(obj.TubeReport().Count(MicroarchError::TubeInvalidParameter), 0u) << ReportToJson(obj.TubeReport());
    EXPECT_NE(obj.GetMesh(), nullptr);
}

// 5. Hors bornes : signale, jamais ramene.
TEST(TEST_cgarchi_gothic_micro_maker, out_of_range_values_are_reported_not_clamped)
{
    for (const char *which : {"radius", "sides"})
    {
        SCOPED_TRACE(which);
        ParameterizedGothicMicroarchitecture obj;
        ASSERT_TRUE(obj.Load(readText(kD4Segment), nullptr));
        std::vector<Parameter> params = obj.GetParameters();
        if (std::string(which) == "radius")
            find(params, "Tube radius (permil of D)")->SetFloat(0.f);
        else
            find(params, "Tube sides")->SetInt(2);
        obj.Regenerate();
        EXPECT_EQ(obj.GetMesh(), nullptr);
        ASSERT_EQ(obj.TubeReport().ErrorCount(), 1u) << ReportToJson(obj.TubeReport());
        EXPECT_EQ(obj.TubeReport().Count(MicroarchError::TubeInvalidParameter), 1u);
        EXPECT_EQ(obj.TubeReport().entries.back().path, std::string(which) == "radius" ? "tubes.radius_ratio" : "tubes.sides");
        EXPECT_TRUE(obj.HasNetwork());

        // Une valeur valide efface le refus : le rapport ne decrit que la derniere tentative.
        if (std::string(which) == "radius")
            find(params, "Tube radius (permil of D)")->SetFloat(5.f);
        else
            find(params, "Tube sides")->SetInt(6);
        obj.Regenerate();
        EXPECT_EQ(obj.TubeReport().ErrorCount(), 0u) << ReportToJson(obj.TubeReport());
        ASSERT_NE(obj.GetMesh(), nullptr);
        EXPECT_EQ(obj.GetMesh()->GetNFaces(), 2176u);
    }
}

// 6. Un refus des tubes garde le reseau.
TEST(TEST_cgarchi_gothic_micro_maker, a_tube_refusal_keeps_the_network)
{
    MicroarchMakerLimits low;
    low.tubes.max_triangles = 2175;
    ParameterizedGothicMicroarchitecture obj(low);
    int calls = 0;
    EXPECT_TRUE(obj.Load(readText(kD4Segment), [&](const std::string &) { ++calls; }));
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(obj.GetMesh(), nullptr);
    EXPECT_EQ(count(obj.TubeReport(), MicroarchError::TubeLimitExceeded), 1u);
    MicroarchReport r;
    EXPECT_FALSE(obj.NetworkObj(r).empty());

    MicroarchMakerLimits exact;
    exact.tubes.max_triangles = 2176;
    ParameterizedGothicMicroarchitecture ok(exact);
    EXPECT_TRUE(ok.Load(readText(kD4Segment), nullptr));
    EXPECT_NE(ok.GetMesh(), nullptr);
}

// 7. Une specification refusee laisse un objet vide.
TEST(TEST_cgarchi_gothic_micro_maker, a_refused_spec_leaves_an_empty_object)
{
    // Arc de centre (0,0), r = 3, de 0 a 120 deg sous C4 : recouvrement (suite network).
    MicroarchSpec overlap;
    overlap.symmetry.center = Vector2d(0, 0);
    overlap.symmetry.rotation_order = 4;
    PointSpec a; a.id = "A"; a.xy = Vector2d(3, 0); a.ground = true;
    PointSpec b; b.id = "B"; b.xy = Vector2d(3 * std::cos(2 * kPi / 3), 3 * std::sin(2 * kPi / 3)); b.height = 5.0;
    overlap.points = {a, b};
    BasicCurveSpec c; c.id = "l"; c.start_point = 0; c.end_point = 1; c.plan = MakeArc(a.xy, b.xy, Vector2d(0, 0), true);
    overlap.curves = {c};
    const std::pair<std::string, MicroarchError> cases[] = {{"{ not json", MicroarchError::JsonParse},
                                                            {SpecToJson(overlap), MicroarchError::PlanOverlap}};
    for (const auto &[text, code] : cases)
    {
        SCOPED_TRACE(MicroarchErrorName(code));
        ParameterizedGothicMicroarchitecture obj;
        bool called = false;
        EXPECT_FALSE(obj.Load(text, [&](const std::string &) { called = true; }));
        EXPECT_FALSE(called);
        EXPECT_FALSE(obj.HasNetwork());
        EXPECT_EQ(obj.GetMesh(), nullptr);
        EXPECT_GE(obj.LoadReport().Count(code), 1u) << ReportToJson(obj.LoadReport());
    }
}

// 8. Taille du texte : bornee avant le parsing.
TEST(TEST_cgarchi_gothic_micro_maker, spec_text_size_is_bounded)
{
    std::string text = readText(kD4Segment);
    ASSERT_LT(text.size(), SpecLimits::max_text_bytes);
    text.append(SpecLimits::max_text_bytes - text.size(), ' ');
    ASSERT_EQ(text.size(), SpecLimits::max_text_bytes);
    ParameterizedGothicMicroarchitecture ok;
    EXPECT_TRUE(ok.Load(text, nullptr)) << ReportToJson(ok.LoadReport());

    text.push_back(' ');
    ParameterizedGothicMicroarchitecture big;
    EXPECT_FALSE(big.Load(text, nullptr));
    ASSERT_EQ(big.LoadReport().ErrorCount(), 1u) << ReportToJson(big.LoadReport());
    EXPECT_EQ(count(big.LoadReport(), MicroarchError::JsonTooLarge), 1u);

    // Texte non JSON de la meme taille : seulement json_too_large, donc avant le parsing.
    ParameterizedGothicMicroarchitecture junk;
    EXPECT_FALSE(junk.Load(std::string(SpecLimits::max_text_bytes + 1, '{'), nullptr));
    ASSERT_EQ(junk.LoadReport().ErrorCount(), 1u) << ReportToJson(junk.LoadReport());
    EXPECT_EQ(count(junk.LoadReport(), MicroarchError::JsonTooLarge), 1u);
}

// 9. L'OBJ des tubes applique l'origine, en double.
TEST(TEST_cgarchi_gothic_micro_maker, tube_obj_applies_the_origin)
{
    const std::string text = readText(kD4Segment);
    ParameterizedGothicMicroarchitecture ref;
    ASSERT_TRUE(ref.Load(text, nullptr));
    const double dx = 1e6, dy = -1e6;
    ParameterizedGothicMicroarchitecture far;
    ASSERT_TRUE(far.Load(shifted(text, dx, dy), nullptr)) << ReportToJson(far.LoadReport());
    ASSERT_NE(far.GetMesh(), nullptr);
    const std::locale previous = std::locale::global(std::locale(std::locale::classic(), new CommaPunct));
    MicroarchReport report;
    const std::string obj = far.TubeObj(report);
    std::locale::global(previous);
    ASSERT_FALSE(obj.empty()) << ReportToJson(report);
    EXPECT_EQ(obj.find(','), std::string::npos);

    const std::vector<float> &rv = ref.GetMesh()->GetVertices();
    const Vector3d o = ref.MeshOrigin();
    const double r = ref.CurrentTubeParams().radius_ratio * 6.4031242374328485;   // D = sqrt(41)
    std::istringstream in(obj);
    in.imbue(std::locale::classic());
    // Faces : indices a partir de 1, dans l'ordre et l'orientation des triangles du maillage.
    const std::vector<unsigned int> tris = far.GetMesh()->GetTriangles();
    std::string line;
    std::size_t v = 0, f = 0, faceMismatch = 0;
    double worst = 0.0;
    while (std::getline(in, line))
    {
        std::istringstream ls(line);
        ls.imbue(std::locale::classic());
        std::string tag;
        ls >> tag;
        if (tag == "v")
        {
            double x = 0, y = 0, z = 0;
            ls >> x >> y >> z;
            ASSERT_LT(3 * v + 2, rv.size());
            worst = std::max({worst, std::fabs(x - (rv[3 * v] + o.x + dx)), std::fabs(y - (rv[3 * v + 1] + o.y + dy)),
                              std::fabs(z - (rv[3 * v + 2] + o.z))});
            ++v;
        }
        else if (tag == "f")
        {
            unsigned long a = 0, b = 0, c = 0;
            ls >> a >> b >> c;
            ASSERT_LT(3 * f + 2, tris.size());
            faceMismatch += a != tris[3 * f] + 1ul || b != tris[3 * f + 1] + 1ul || c != tris[3 * f + 2] + 1ul;
            ++f;
        }
    }
    EXPECT_EQ(faceMismatch, 0u);
    EXPECT_EQ(v, far.GetMesh()->GetNVertices());
    EXPECT_EQ(f, far.GetMesh()->GetNFaces());
    RecordProperty("tube_obj_error_over_r", std::to_string(worst / r));
    EXPECT_LE(worst, 1e-3 * r);
}

// 10. L'OBJ des courbes suit le pas angulaire courant.
TEST(TEST_cgarchi_gothic_micro_maker, curves_obj_uses_the_current_sampling)
{
    const std::string text = readText(kD4Segment);
    const Reference ref = reference(text);
    ParameterizedGothicMicroarchitecture obj;
    ASSERT_TRUE(obj.Load(text, nullptr));
    MicroarchReport r5;
    const std::string at5 = obj.NetworkObj(r5);
    std::vector<Parameter> params = obj.GetParameters();
    find(params, "Sampling angle (deg)")->SetFloat(2.f);
    MicroarchReport r2, e2;
    const std::string at2 = obj.NetworkObj(r2);
    SamplingParams two;
    two.max_angle = 2.0 * (kPi / 180.0);
    EXPECT_EQ(at2, NetworkToObj(ref.net, ref.spec, two, e2));
    EXPECT_NE(at2, at5);
}

// 11. Plafonds des exports.
TEST(TEST_cgarchi_gothic_micro_maker, export_limits_are_enforced)
{
    const std::string text = readText(kD4Segment);
    for (const auto &[points, accepted] : {std::make_pair(std::size_t(184), true), std::make_pair(std::size_t(183), false)})
    {
        MicroarchMakerLimits l;
        l.max_obj_points = points;
        ParameterizedGothicMicroarchitecture obj(l);
        ASSERT_TRUE(obj.Load(text, nullptr));
        MicroarchReport r;
        EXPECT_EQ(obj.NetworkObj(r).empty(), !accepted) << points;
        EXPECT_EQ(r.Count(MicroarchError::SampleLimitExceeded), accepted ? 0u : 1u);
        if (!accepted)
            EXPECT_EQ(r.entries.back().path, "export.curves");
    }
    for (const auto &[triangles, accepted] : {std::make_pair(std::size_t(2176), true), std::make_pair(std::size_t(2175), false)})
    {
        MicroarchMakerLimits l;
        l.max_obj_triangles = triangles;
        ParameterizedGothicMicroarchitecture obj(l);
        ASSERT_TRUE(obj.Load(text, nullptr));
        MicroarchReport r;
        EXPECT_EQ(obj.TubeObj(r).empty(), !accepted) << triangles;
        EXPECT_EQ(r.Count(MicroarchError::TubeLimitExceeded), accepted ? 0u : 1u);
        if (!accepted)
            EXPECT_EQ(r.entries.back().path, "export.tubes");
    }
}

// 12. TakeMesh oublie la cle du cache.
TEST(TEST_cgarchi_gothic_micro_maker, take_mesh_forgets_the_cache)
{
    ParameterizedGothicMicroarchitecture obj;
    ASSERT_TRUE(obj.Load(readText(kD4Segment), nullptr));
    std::unique_ptr<Mesh> taken(obj.TakeMesh());
    ASSERT_NE(taken, nullptr);
    obj.Regenerate();
    EXPECT_NE(obj.GetMesh(), nullptr);
    EXPECT_EQ(obj.TubeBuilds(), 2u);
}

// ===========================================================================
//  Solide imprimable dans Maker (J7-d, decisions M29 point 5 et M33)
// ===========================================================================

namespace
{
    struct Stl
    {
        std::uint32_t      count = 0;
        std::vector<float> normals, vertices;   // 3 par triangle ; 9 par triangle
    };

    Stl readStl(const std::string &bytes)
    {
        Stl s;
        if (bytes.size() < 84)
            return s;
        std::memcpy(&s.count, bytes.data() + 80, 4);
        if (bytes.size() != 84 + 50 * static_cast<std::size_t>(s.count))
            return Stl{};
        for (std::size_t f = 0; f < s.count; ++f)
        {
            float rec[12];
            std::memcpy(rec, bytes.data() + 84 + 50 * f, sizeof rec);
            s.normals.insert(s.normals.end(), rec, rec + 3);
            s.vertices.insert(s.vertices.end(), rec + 3, rec + 12);
        }
        return s;
    }

    // Maillage soude par egalite exacte des sommets float (relecture d'un STL).
    Mesh weldStl(const Stl &s)
    {
        std::map<std::array<float, 3>, unsigned int> index;
        std::vector<float> v;
        std::vector<unsigned int> t;
        for (std::size_t i = 0; i < s.vertices.size(); i += 3)
        {
            const std::array<float, 3> p{s.vertices[i], s.vertices[i + 1], s.vertices[i + 2]};
            const auto it = index.emplace(p, static_cast<unsigned int>(v.size() / 3));
            if (it.second)
                v.insert(v.end(), p.begin(), p.end());
            t.push_back(it.first->second);
        }
        Mesh m;
        m.SetVertices(static_cast<unsigned int>(v.size() / 3), v.data());
        m.SetFaces(static_cast<unsigned int>(t.size() / 3), 3, t.data());
        return m;
    }

    PrintConstraints resinPrinter(double minWallMm)
    {
        PrintConstraints p;
        p.minWallMm = minWallMm;
        return p;
    }

    SolidParams solidAt(double ratio, int q)
    {
        SolidParams p;
        p.radius_ratio = ratio;
        p.cells_per_radius = q;
        return p;
    }

    // Boite du solide de reference, en unites du dessin (sommet + origine).
    void drawingBox(const SolidMesh &s, double lo[3], double hi[3])
    {
        const std::vector<float> &v = s.mesh->GetVertices();
        const double o[3] = {s.origin.x, s.origin.y, s.origin.z};
        for (int k = 0; k < 3; ++k)
        {
            lo[k] = std::numeric_limits<double>::infinity();
            hi[k] = -lo[k];
        }
        for (std::size_t i = 0; i < v.size(); i += 3)
            for (int k = 0; k < 3; ++k)
            {
                lo[k] = std::min(lo[k], double(v[i + k]) + o[k]);
                hi[k] = std::max(hi[k], double(v[i + k]) + o[k]);
            }
    }
}

// 13. STL en mm, centre en x et y, base a z = 0 ; relu et soude : ferme, 2-variete, oriente.
TEST(TEST_cgarchi_gothic_micro_maker, solid_stl_is_in_millimetres_centred_on_the_base)
{
    const std::string text = readText(kD4Segment);
    const Reference ref = reference(text);
    MicroarchReport build;
    const SolidMesh solid = BuildSolidMesh(ref.net, solidAt(0.05, 3), build);
    ASSERT_TRUE(solid.mesh);
    const std::size_t F = solid.mesh->GetNFaces();
    double lo[3], hi[3];
    drawingBox(solid, lo, hi);

    ParameterizedGothicMicroarchitecture obj;
    ASSERT_TRUE(obj.Load(text, nullptr));
    const double mmPerUnit = 2.0;
    const cgarchi::MicroarchSolidExport e = obj.ExportSolidStl(solidAt(0.05, 3), mmPerUnit, resinPrinter(0.4));
    EXPECT_EQ(e.verdict, cgarchi::MicroarchSolidExport::Verdict::Printable) << ReportToJson(e.report);
    EXPECT_STREQ(cgarchi::SolidExportVerdictName(e.verdict), "printable");
    EXPECT_EQ(e.triangles, F);
    ASSERT_EQ(e.stl.size(), 84 + 50 * F);
    const Stl stl = readStl(e.stl);
    ASSERT_EQ(stl.count, F);
    float slo[3] = {1e30f, 1e30f, 1e30f}, shi[3] = {-1e30f, -1e30f, -1e30f};
    for (std::size_t i = 0; i < stl.vertices.size(); i += 3)
        for (int k = 0; k < 3; ++k)
        {
            slo[k] = std::min(slo[k], stl.vertices[i + k]);
            shi[k] = std::max(shi[k], stl.vertices[i + k]);
        }
    const double tol = 1e-5 * (hi[0] - lo[0]) * mmPerUnit;
    EXPECT_NEAR(0.5 * (slo[0] + shi[0]), 0.0, tol);
    EXPECT_NEAR(0.5 * (slo[1] + shi[1]), 0.0, tol);
    EXPECT_NEAR(slo[2], 0.0, tol);
    for (int k = 0; k < 3; ++k)
    {
        EXPECT_NEAR(shi[k] - slo[k], (hi[k] - lo[k]) * mmPerUnit, tol) << "axe " << k;
        EXPECT_NEAR(e.extent_mm[k], (hi[k] - lo[k]) * mmPerUnit, tol) << "axe " << k;
    }
    // Normale du fichier = sens du triangle (regle de la main droite).
    std::size_t against = 0;
    for (std::size_t f = 0; f < stl.count; ++f)
    {
        const float *a = &stl.vertices[9 * f], *b = a + 3, *c = a + 6;
        const double e1[3] = {double(b[0]) - a[0], double(b[1]) - a[1], double(b[2]) - a[2]};
        const double e2[3] = {double(c[0]) - a[0], double(c[1]) - a[1], double(c[2]) - a[2]};
        const double n[3] = {e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0]};
        const double nl = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
        const float *fn = &stl.normals[3 * f];
        const double fl = std::sqrt(double(fn[0]) * fn[0] + double(fn[1]) * fn[1] + double(fn[2]) * fn[2]);
        // Normale du fichier unitaire et alignee sur le triangle (cosinus > 0,999).
        if (!(nl > 0.0) || std::fabs(fl - 1.0) > 1e-5 || (n[0] * fn[0] + n[1] * fn[1] + n[2] * fn[2]) / nl < 0.999)
            ++against;
    }
    EXPECT_EQ(against, 0u);
    // Relu et soude : P1-P3.
    const Mesh reread = weldStl(stl);
    MeshValidationOptions vo;
    vo.weldTolerance = 1e-3 * solid.cell * mmPerUnit;
    const MeshValidationReport r = ValidateMesh(reread, vo);
    EXPECT_EQ(r.Count(MeshDefect::BoundaryEdge), 0u);
    EXPECT_EQ(r.Count(MeshDefect::NonManifoldEdge), 0u);
    EXPECT_EQ(r.Count(MeshDefect::NonManifoldVertex), 0u);
    EXPECT_EQ(r.Count(MeshDefect::InconsistentOrientation), 0u);
    EXPECT_EQ(r.Count(MeshDefect::InvertedShell), 0u);
    EXPECT_GT(r.signedVolume, 0.0);
}

// 14. L'origine est appliquee en double : le meme reseau translate de 1e6 donne le meme STL.
TEST(TEST_cgarchi_gothic_micro_maker, solid_stl_is_independent_of_the_drawing_position)
{
    const std::string text = readText(kD4Segment);
    ParameterizedGothicMicroarchitecture near, far;
    ASSERT_TRUE(near.Load(text, nullptr));
    ASSERT_TRUE(far.Load(shifted(text, 1e6, -1e6), nullptr));
    const cgarchi::MicroarchSolidExport a = near.ExportSolidStl(solidAt(0.05, 3), 1.0, resinPrinter(0.1));
    const cgarchi::MicroarchSolidExport b = far.ExportSolidStl(solidAt(0.05, 3), 1.0, resinPrinter(0.1));
    ASSERT_FALSE(a.stl.empty());
    ASSERT_EQ(a.stl.size(), b.stl.size());
    const Stl sa = readStl(a.stl), sb = readStl(b.stl);
    const double r = 0.05 * reference(text).net.tol.scale;
    double worst = 0.0;
    for (std::size_t i = 0; i < sa.vertices.size(); ++i)
        worst = std::max(worst, std::fabs(double(sa.vertices[i]) - sb.vertices[i]));
    RecordProperty("stl_shift_1e6_worst_over_r", std::to_string(worst / r));
    EXPECT_LT(worst, 1e-4 * r);
}

// 15. Verdicts : imprimable, non imprimable, non verifie au-dela du seuil (STL exporte), refus au
// plafond exact (aucun STL), refus des parametres.
TEST(TEST_cgarchi_gothic_micro_maker, solid_export_reports_each_verdict)
{
    using V = cgarchi::MicroarchSolidExport::Verdict;
    const std::string text = readText(kD4Segment);
    ParameterizedGothicMicroarchitecture probe;
    ASSERT_TRUE(probe.Load(text, nullptr));
    const std::size_t F = probe.ExportSolidStl(solidAt(0.05, 3), 1.0, resinPrinter(0.1)).triangles;
    ASSERT_GT(F, 0u);
    {
        const cgarchi::MicroarchSolidExport e = probe.ExportSolidStl(solidAt(0.05, 3), 1.0, resinPrinter(100.0));
        EXPECT_EQ(e.verdict, V::NotPrintable);
        EXPECT_EQ(e.report.Count(MicroarchWarning::SolidWallTooThin), 1u);
        EXPECT_EQ(e.stl.size(), 84 + 50 * F);
    }
    for (const std::size_t threshold : {F - 1, F})
    {
        MicroarchMakerLimits limits;
        limits.max_verified_triangles = threshold;
        ParameterizedGothicMicroarchitecture obj(limits);
        ASSERT_TRUE(obj.Load(text, nullptr));
        const cgarchi::MicroarchSolidExport e = obj.ExportSolidStl(solidAt(0.05, 3), 1.0, resinPrinter(0.1));
        EXPECT_EQ(e.verdict, threshold < F ? V::Unverified : V::Printable) << ReportToJson(e.report);
        EXPECT_EQ(e.report.Count(MicroarchWarning::SolidUnverified), threshold < F ? 1u : 0u);
        EXPECT_EQ(e.stl.size(), 84 + 50 * F);
    }
    for (const std::size_t cap : {F - 1, F})
    {
        MicroarchMakerLimits limits;
        limits.solid.max_triangles = cap;
        ParameterizedGothicMicroarchitecture obj(limits);
        ASSERT_TRUE(obj.Load(text, nullptr));
        const cgarchi::MicroarchSolidExport e = obj.ExportSolidStl(solidAt(0.05, 3), 1.0, resinPrinter(0.1));
        if (cap < F)
        {
            EXPECT_EQ(e.verdict, V::Refused);
            EXPECT_STREQ(cgarchi::SolidExportVerdictName(e.verdict), "refused");
            EXPECT_TRUE(e.stl.empty());
            EXPECT_EQ(e.triangles, 0u);
            // Refus tardif (M32 b) : seule erreur, sur solid.triangles ; l'estimation qui le precede
            // n'est qu'une information.
            EXPECT_EQ(e.report.Count(MicroarchError::SolidLimitExceeded), 1u) << ReportToJson(e.report);
            std::size_t errors = 0;
            for (const MicroarchDiagnostic &d : e.report.entries)
                if (d.error)
                {
                    ++errors;
                    EXPECT_EQ(d.path, "solid.triangles");
                }
            EXPECT_EQ(errors, 1u);
        }
        else
            EXPECT_EQ(e.verdict, V::Printable) << ReportToJson(e.report);
    }
    for (const double scale : {0.0, -1.0, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()})
    {
        const cgarchi::MicroarchSolidExport e = probe.ExportSolidStl(solidAt(0.05, 3), scale, resinPrinter(0.1));
        EXPECT_EQ(e.verdict, V::Refused);
        EXPECT_TRUE(e.stl.empty());
        ASSERT_EQ(e.report.entries.size(), 1u);
        EXPECT_EQ(e.report.Count(MicroarchError::SolidInvalidParameter), 1u);
        EXPECT_EQ(e.report.entries[0].path, "solid.mm_per_unit");
    }
    {
        const cgarchi::MicroarchSolidExport e = probe.ExportSolidStl(solidAt(0.05, 1), 1.0, resinPrinter(0.1));
        EXPECT_EQ(e.verdict, V::Refused);
        EXPECT_EQ(e.report.Count(MicroarchError::SolidInvalidParameter), 1u);
    }
    ParameterizedGothicMicroarchitecture empty;
    EXPECT_EQ(empty.ExportSolidStl(solidAt(0.05, 3), 1.0, resinPrinter(0.1)).verdict, V::Refused);
}

// 16. Rien n'est garde : les tubes, leur cache et le reseau sont ceux d'avant l'export.
TEST(TEST_cgarchi_gothic_micro_maker, solid_export_keeps_nothing)
{
    ParameterizedGothicMicroarchitecture obj;
    ASSERT_TRUE(obj.Load(readText(kD4Segment), nullptr));
    const Mesh *tubes = obj.GetMesh();
    ASSERT_NE(tubes, nullptr);
    const Vector3d origin = obj.MeshOrigin();
    const cgarchi::MicroarchSolidExport e = obj.ExportSolidStl(solidAt(0.05, 3), 1.0, resinPrinter(0.1));
    ASSERT_FALSE(e.stl.empty());
    EXPECT_EQ(obj.GetMesh(), tubes);
    EXPECT_EQ(obj.GetMesh()->GetNFaces(), 2176u);
    EXPECT_EQ(obj.TubeBuilds(), 1u);
    EXPECT_EQ(obj.NetworkBuilds(), 1u);
    EXPECT_EQ(obj.MeshOrigin().x, origin.x);
    EXPECT_EQ(obj.MeshOrigin().z, origin.z);
    obj.Regenerate();
    EXPECT_EQ(obj.TubeBuilds(), 1u);
}
