#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <random>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include <cgarchi/gothic_microarchitecture.h>
#include <cgarchi/gothic_microarchitecture_json.h>

using namespace cgarchi::microarch;   // fichier de test du module seul
using nlohmann::json;

// ===========================================================================
//  Microarchitecture gothique : specification JSON v1 (plan d'execution §5, J1-b)
// ===========================================================================

namespace
{
    constexpr double kPi = 3.14159265358979323846;
    const char *const kD4Segment = "./test/data/gothic_micro_d4_segment.json";
    const char *const kC4Pinwheel = "./test/data/gothic_micro_c4_pinwheel.json";

    std::string readFile(const std::string &path)
    {
        std::ifstream in(path, std::ios::binary);
        std::ostringstream os;
        os << in.rdbuf();
        return os.str();
    }

    // Document de base : la reference D4 segment.
    json baseDoc()
    {
        return json::parse(R"({
          "schema": "gothic-microarchitecture/v1",
          "symmetry": { "center": [0, 0], "rotation_order": 4, "reflection_axis_deg": 90 },
          "points": [
            { "id": "A", "xy": [4, 1], "ground": true },
            { "id": "B", "xy": [1, 5], "height": 10 } ],
          "curves": [ { "id": "l1", "type": "segment", "start": "A", "end": "B" } ] })");
    }

    // Arc de reference : centre (5,0), r = 5, de (1,3) a (8,4), sens horaire.
    json arcDoc()
    {
        return json::parse(R"({
          "schema": "gothic-microarchitecture/v1",
          "symmetry": { "center": [0, 0], "rotation_order": 4, "reflection_axis_deg": 90 },
          "points": [
            { "id": "C", "xy": [1, 3], "ground": true },
            { "id": "E", "xy": [8, 4], "height": 15.707963267948966 } ],
          "curves": [ { "id": "l2", "type": "arc", "start": "C", "end": "E", "center": [5, 0], "ccw": false } ] })");
    }

    struct Loaded
    {
        bool            ok = false;
        MicroarchSpec   spec;
        MicroarchReport report;
    };

    Loaded load(const std::string &text)
    {
        Loaded l;
        l.ok = LoadSpecFromJson(text, l.spec, l.report);
        return l;
    }

    // Exactement un diagnostic, de ce code et a ce chemin.
    ::testing::AssertionResult onlyError(const MicroarchReport &r, MicroarchError code, const std::string &path)
    {
        if (r.ErrorCount() + r.WarningCount() != 1 || r.entries.size() != 1)
            return ::testing::AssertionFailure() << "attendu 1 diagnostic : " << ReportToJson(r);
        const MicroarchDiagnostic &d = r.entries.front();
        if (!d.error || d.code != static_cast<int>(code) || d.path != path)
            return ::testing::AssertionFailure() << "attendu " << MicroarchErrorName(code) << " @ " << path
                                                 << " : " << ReportToJson(r);
        return ::testing::AssertionSuccess();
    }

    ::testing::AssertionResult onlyWarning(const MicroarchReport &r, MicroarchWarning code, const std::string &path)
    {
        if (r.ErrorCount() + r.WarningCount() != 1 || r.entries.size() != 1)
            return ::testing::AssertionFailure() << "attendu 1 diagnostic : " << ReportToJson(r);
        const MicroarchDiagnostic &d = r.entries.front();
        if (d.error || d.code != static_cast<int>(code) || d.path != path)
            return ::testing::AssertionFailure() << "attendu " << MicroarchWarningName(code) << " @ " << path
                                                 << " : " << ReportToJson(r);
        return ::testing::AssertionSuccess();
    }

    bool hasEntry(const MicroarchReport &r, bool error, int code, const std::string &path)
    {
        return std::any_of(r.entries.begin(), r.entries.end(), [&](const MicroarchDiagnostic &d) {
            return d.error == error && d.code == code && d.path == path;
        });
    }

    // Egalite EXACTE, champ par champ (pas d'operator== sur Vector2d : voir l'en-tete du module).
    bool sameVec(const Vector2d &a, const Vector2d &b) { return a.x == b.x && a.y == b.y; }

    ::testing::AssertionResult sameSpec(const MicroarchSpec &a, const MicroarchSpec &b)
    {
        auto fail = [](const std::string &what) { return ::testing::AssertionFailure() << what; };
        if (a.levels.size() != b.levels.size()) return fail("levels.size");
        for (std::size_t i = 0; i < a.levels.size(); ++i)
            if (a.levels[i].id != b.levels[i].id || a.levels[i].base != b.levels[i].base) return fail("levels");
        if (!sameVec(a.symmetry.center, b.symmetry.center)) return fail("symmetry.center");
        if (a.symmetry.rotation_order != b.symmetry.rotation_order) return fail("rotation_order");
        if (a.symmetry.reflection_axis_deg != b.symmetry.reflection_axis_deg) return fail("reflection_axis_deg");
        if (a.points.size() != b.points.size()) return fail("points.size");
        for (std::size_t i = 0; i < a.points.size(); ++i)
        {
            const PointSpec &p = a.points[i], &q = b.points[i];
            if (p.id != q.id || !sameVec(p.xy, q.xy) || p.ground != q.ground || p.height != q.height)
                return fail("points[" + std::to_string(i) + "]");
        }
        if (a.curves.size() != b.curves.size()) return fail("curves.size");
        for (std::size_t i = 0; i < a.curves.size(); ++i)
        {
            const BasicCurveSpec &c = a.curves[i], &d = b.curves[i];
            if (c.id != d.id || c.start_point != d.start_point || c.end_point != d.end_point
                || c.level != d.level || c.prc.u != d.prc.u || c.plan.index() != d.plan.index())
                return fail("curves[" + std::to_string(i) + "]");
            if (const auto *s = std::get_if<PlanSegment>(&c.plan))
            {
                const PlanSegment &t = std::get<PlanSegment>(d.plan);
                if (!sameVec(s->start, t.start) || !sameVec(s->end, t.end)) return fail("segment");
            }
            else
            {
                const PlanArc &x = std::get<PlanArc>(c.plan), &y = std::get<PlanArc>(d.plan);
                if (!sameVec(x.center, y.center) || x.radius != y.radius || x.start_angle != y.start_angle
                    || x.sweep != y.sweep)
                    return fail("arc");
            }
        }
        const Tolerances &t = a.tolerances, &u = b.tolerances;
        if (t.point != u.point || t.parameter != u.parameter || t.height != u.height || t.prc_snap != u.prc_snap)
            return fail("tolerances");
        return ::testing::AssertionSuccess();
    }
}

// ---------------------------------------------------------------------------
//  Donnees de reference
// ---------------------------------------------------------------------------

TEST(TEST_cgarchi_gothic_micro_json, reference_files_load_without_diagnostics)
{
    for (const char *path : {kD4Segment, kC4Pinwheel})
    {
        MicroarchSpec spec;
        MicroarchReport report;
        EXPECT_TRUE(LoadSpecFromFile(path, spec, report)) << path;
        EXPECT_EQ(report.ErrorCount(), 0u) << path << " : " << ReportToJson(report);
        EXPECT_EQ(report.WarningCount(), 0u) << path << " : " << ReportToJson(report);
    }

    MicroarchSpec d4, c4;
    MicroarchReport report;
    ASSERT_TRUE(LoadSpecFromFile(kD4Segment, d4, report));
    ASSERT_TRUE(LoadSpecFromFile(kC4Pinwheel, c4, report));
    // D = diagonale de [0,4] x [0,5] = sqrt(41) ; de [-1,3] x [-1,2] = 5.
    EXPECT_NEAR(Scale(d4), std::sqrt(41.0), 1e-12 * std::sqrt(41.0));
    EXPECT_NEAR(Scale(c4), 5.0, 1e-12 * 5.0);

    EXPECT_EQ(d4.symmetry.rotation_order, 4);
    ASSERT_TRUE(d4.symmetry.reflection_axis_deg.has_value());
    EXPECT_EQ(*d4.symmetry.reflection_axis_deg, 90.0);
    EXPECT_FALSE(c4.symmetry.reflection_axis_deg.has_value());
    ASSERT_EQ(d4.points.size(), 2u);
    EXPECT_TRUE(d4.points[0].ground);
    EXPECT_EQ(d4.points[1].Z(), 10.0);
    ASSERT_EQ(d4.curves.size(), 1u);
    EXPECT_EQ(d4.curves[0].start_point, 0);
    EXPECT_EQ(d4.curves[0].end_point, 1);
    const PlanSegment &seg = std::get<PlanSegment>(d4.curves[0].plan);
    EXPECT_TRUE(sameVec(seg.start, Vector2d(4, 1)) && sameVec(seg.end, Vector2d(1, 5)));
    ASSERT_EQ(d4.levels.size(), 1u);
    EXPECT_EQ(c4.levels.size(), 1u);   // defaut
}

TEST(TEST_cgarchi_gothic_micro_json, missing_file_is_a_parse_error)
{
    MicroarchSpec spec;
    MicroarchReport report;
    EXPECT_FALSE(LoadSpecFromFile("./test/data/gothic_micro_absent.json", spec, report));
    EXPECT_TRUE(onlyError(report, MicroarchError::JsonParse, ""));
}

// ---------------------------------------------------------------------------
//  Aller-retour exact
// ---------------------------------------------------------------------------

TEST(TEST_cgarchi_gothic_micro_json, round_trip_is_exact)
{
    json withAxis = arcDoc();
    withAxis["symmetry"]["reflection_axis_deg"] = 150;
    json withPrc = baseDoc();
    withPrc["curves"][0]["prc"] = {{"u", 0.428571}};
    withPrc["tolerances"] = {{"point", 3e-10}, {"height", 0.001}};
    withPrc["levels"] = json::array({{{"id", 7}, {"base", 1.25}}});
    withPrc["curves"][0]["level"] = 7;

    std::vector<std::string> docs = {readFile(kD4Segment), readFile(kC4Pinwheel), arcDoc().dump(),
                                     withAxis.dump(), withPrc.dump()};
    // Angle de l'axe : valeurs limites de [0, 180), puis tirages.
    std::vector<double> axes = {0.0, 5e-324, 1e-300, 150.0, std::nextafter(180.0, 0.0), 179.999999999999};
    std::mt19937 rng(20261022);
    std::uniform_real_distribution<double> uniform(0.0, 180.0);
    for (int i = 0; i < 200; ++i)
        axes.push_back(uniform(rng));
    for (double deg : axes)
    {
        json d = baseDoc();
        d["symmetry"]["reflection_axis_deg"] = deg;
        docs.push_back(d.dump());
    }
    for (const std::string &text : docs)
    {
        const Loaded first = load(text);
        ASSERT_TRUE(first.ok) << ReportToJson(first.report) << "\n" << text;
        const std::string written = SpecToJson(first.spec);
        const Loaded second = load(written);
        ASSERT_TRUE(second.ok) << ReportToJson(second.report) << "\n" << written;
        EXPECT_TRUE(sameSpec(first.spec, second.spec)) << written;
        EXPECT_EQ(second.report.WarningCount(), 0u) << ReportToJson(second.report);
    }
}

// ---------------------------------------------------------------------------
//  Regles de §5.2 : exactement un diagnostic, code et chemin
// ---------------------------------------------------------------------------

TEST(TEST_cgarchi_gothic_micro_json, document_level_rules)
{
    EXPECT_TRUE(onlyError(load("{").report, MicroarchError::JsonParse, ""));
    EXPECT_TRUE(onlyError(load("[1, 2]").report, MicroarchError::JsonType, ""));

    json d = baseDoc();
    d.erase("schema");
    EXPECT_TRUE(onlyError(load(d.dump()).report, MicroarchError::JsonMissing, "schema"));
    d = baseDoc();
    d["schema"] = "gothic-microarchitecture/v2";
    EXPECT_TRUE(onlyError(load(d.dump()).report, MicroarchError::JsonSchema, "schema"));
    d = baseDoc();
    d["schema"] = 1;
    EXPECT_TRUE(onlyError(load(d.dump()).report, MicroarchError::JsonType, "schema"));

    d = baseDoc();
    d["meta"] = {{"anything", {1, 2}}};
    EXPECT_TRUE(load(d.dump()).ok);   // meta : contenu libre
    d["meta"] = 3;
    EXPECT_TRUE(onlyError(load(d.dump()).report, MicroarchError::JsonType, "meta"));
}

TEST(TEST_cgarchi_gothic_micro_json, unknown_fields_at_every_level)
{
    const std::vector<std::pair<std::string, std::string>> cases = {
        {"/foo", "foo"},
        {"/symmetry/foo", "symmetry.foo"},
        {"/points/0/foo", "points[0].foo"},
        {"/curves/0/foo", "curves[0].foo"},
        {"/curves/0/prc", ""},   // remplace ci-dessous
        {"/tolerances/foo", "tolerances.foo"},
        {"/levels/0/foo", "levels[0].foo"},
    };
    for (const auto &[pointer, path] : cases)
    {
        json d = baseDoc();
        if (pointer == "/curves/0/prc")
        {
            d["curves"][0]["prc"] = {{"u", 0.5}, {"foo", 1}};
            EXPECT_TRUE(onlyError(load(d.dump()).report, MicroarchError::JsonSchema, "curves[0].prc.foo"));
            continue;
        }
        if (pointer == "/levels/0/foo")
            d["levels"] = json::array({{{"id", 0}}});
        d[json::json_pointer(pointer)] = 1;
        EXPECT_TRUE(onlyError(load(d.dump()).report, MicroarchError::JsonSchema, path)) << pointer;
    }

    // center / ccw sur un segment.
    json d = baseDoc();
    d["curves"][0]["ccw"] = true;
    EXPECT_TRUE(onlyError(load(d.dump()).report, MicroarchError::JsonSchema, "curves[0].ccw"));
}

TEST(TEST_cgarchi_gothic_micro_json, symmetry_rules)
{
    json d = baseDoc();
    d.erase("symmetry");
    EXPECT_TRUE(onlyError(load(d.dump()).report, MicroarchError::JsonMissing, "symmetry"));

    d = baseDoc();
    d["symmetry"]["rotation_order"] = 2.5;
    EXPECT_TRUE(onlyError(load(d.dump()).report, MicroarchError::JsonType, "symmetry.rotation_order"));
    d["symmetry"]["rotation_order"] = 0;
    EXPECT_TRUE(onlyError(load(d.dump()).report, MicroarchError::JsonValue, "symmetry.rotation_order"));
    d["symmetry"]["rotation_order"] = 65;
    EXPECT_TRUE(onlyError(load(d.dump()).report, MicroarchError::JsonValue, "symmetry.rotation_order"));
    d["symmetry"]["rotation_order"] = 4.0;   // entier au sens de JSON Schema
    EXPECT_TRUE(load(d.dump()).ok);

    d = baseDoc();
    d["symmetry"]["reflection_axis_deg"] = 180;
    EXPECT_TRUE(onlyError(load(d.dump()).report, MicroarchError::JsonValue, "symmetry.reflection_axis_deg"));
    d["symmetry"]["reflection_axis_deg"] = -1;
    EXPECT_TRUE(onlyError(load(d.dump()).report, MicroarchError::JsonValue, "symmetry.reflection_axis_deg"));

    d = baseDoc();
    d["symmetry"]["center"] = "origin";
    EXPECT_TRUE(onlyError(load(d.dump()).report, MicroarchError::JsonType, "symmetry.center"));
    d["symmetry"]["center"] = {1, 2, 3};
    EXPECT_TRUE(onlyError(load(d.dump()).report, MicroarchError::JsonType, "symmetry.center"));
    d = baseDoc();
    d["symmetry"].erase("center");
    EXPECT_TRUE(onlyError(load(d.dump()).report, MicroarchError::JsonMissing, "symmetry.center"));
}

TEST(TEST_cgarchi_gothic_micro_json, point_rules)
{
    json d = baseDoc();
    d["points"][0]["xy"] = {4};
    EXPECT_TRUE(onlyError(load(d.dump()).report, MicroarchError::JsonType, "points[0].xy"));
    d["points"][0]["xy"] = {"4", 1};
    EXPECT_TRUE(onlyError(load(d.dump()).report, MicroarchError::JsonType, "points[0].xy"));
    // 1e999 depasse le double : le parseur le refuse (out_of_range.406), c'est
    // une erreur de syntaxe. Un nombre non fini n'atteint donc jamais le lecteur.
    {
        std::string text = baseDoc().dump();
        const std::string from = "[4,1]";
        ASSERT_NE(text.find(from), std::string::npos);
        text.replace(text.find(from), from.size(), "[1e999,1]");
        EXPECT_TRUE(onlyError(load(text).report, MicroarchError::JsonParse, ""));
    }

    d = baseDoc();
    d["points"] = json::array();
    EXPECT_TRUE(onlyError(load(d.dump()).report, MicroarchError::JsonValue, "points"));
    d.erase("points");
    EXPECT_TRUE(onlyError(load(d.dump()).report, MicroarchError::JsonMissing, "points"));

    d = baseDoc();
    d["points"][1]["id"] = "A";
    d["curves"][0]["end"] = "A";   // garde les deux points references par leur id
    {
        const Loaded l = load(d.dump());
        // "A" -> "A" : la courbe resout ses deux extremites sur le premier "A".
        EXPECT_TRUE(hasEntry(l.report, true, static_cast<int>(MicroarchError::DuplicateId), "points[1].id"))
            << ReportToJson(l.report);
    }
    d = baseDoc();
    d["points"].push_back({{"id", "A"}, {"xy", {2, 2}}, {"height", 3}});
    EXPECT_TRUE(onlyError(load(d.dump()).report, MicroarchError::DuplicateId, "points[2].id"));

    d = baseDoc();
    d["points"][0]["height"] = 2;
    EXPECT_TRUE(onlyError(load(d.dump()).report, MicroarchError::JsonValue, "points[0].height"));
    d = baseDoc();
    d["points"][1].erase("height");
    EXPECT_TRUE(onlyError(load(d.dump()).report, MicroarchError::JsonMissing, "points[1].height"));
    d = baseDoc();
    d["points"][1]["ground"] = false;
    EXPECT_TRUE(load(d.dump()).ok);   // ground false et une hauteur

    d = baseDoc();
    d["points"][0]["ground"] = "yes";
    EXPECT_TRUE(onlyError(load(d.dump()).report, MicroarchError::JsonType, "points[0].ground"));
    d = baseDoc();
    d["points"][1]["height"] = "high";
    EXPECT_TRUE(onlyError(load(d.dump()).report, MicroarchError::JsonType, "points[1].height"));
    d = baseDoc();
    d["points"][1]["id"] = "";
    d["curves"][0]["end"] = "A";
    {
        const Loaded l = load(d.dump());
        EXPECT_TRUE(hasEntry(l.report, true, static_cast<int>(MicroarchError::JsonValue), "points[1].id"))
            << ReportToJson(l.report);
    }

    // Point declare mais non reference : avertissement, document accepte.
    d = baseDoc();
    d["points"].push_back({{"id", "C"}, {"xy", {2, 2}}, {"ground", true}});
    const Loaded l = load(d.dump());
    EXPECT_TRUE(l.ok);
    EXPECT_TRUE(onlyWarning(l.report, MicroarchWarning::FieldIgnored, "points[2]"));
}

TEST(TEST_cgarchi_gothic_micro_json, curve_rules)
{
    // Reference inconnue : le document ne garde que le point reference.
    json d = baseDoc();
    d["points"].erase(0);
    d["curves"][0]["start"] = "Z";
    EXPECT_TRUE(onlyError(load(d.dump()).report, MicroarchError::UnknownReference, "curves[0].start"));
    d = baseDoc();
    d["points"].erase(1);
    d["curves"][0]["end"] = "Z";
    EXPECT_TRUE(onlyError(load(d.dump()).report, MicroarchError::UnknownReference, "curves[0].end"));

    // start == end, puis deux points confondus a tol.point pres.
    d = baseDoc();
    d["points"].erase(1);
    d["curves"][0]["end"] = "A";
    EXPECT_TRUE(onlyError(load(d.dump()).report, MicroarchError::DegenerateCurve, "curves[0].end"));
    d = baseDoc();
    d["points"][1]["xy"] = {4, 1 + 1e-12};
    EXPECT_TRUE(onlyError(load(d.dump()).report, MicroarchError::DegenerateCurve, "curves[0].end"));

    d = baseDoc();
    d["curves"][0]["type"] = "spline";
    EXPECT_TRUE(onlyError(load(d.dump()).report, MicroarchError::JsonValue, "curves[0].type"));
    d = baseDoc();
    d["curves"][0].erase("type");
    EXPECT_TRUE(onlyError(load(d.dump()).report, MicroarchError::JsonMissing, "curves[0].type"));

    d = baseDoc();
    d["curves"].push_back({{"id", "l1"}, {"type", "segment"}, {"start", "A"}, {"end", "C"}});
    d["points"].push_back({{"id", "C"}, {"xy", {2, 2}}, {"height", 3}});
    EXPECT_TRUE(onlyError(load(d.dump()).report, MicroarchError::DuplicateId, "curves[1].id"));

    d = baseDoc();
    d["curves"][0]["level"] = 3;
    EXPECT_TRUE(onlyError(load(d.dump()).report, MicroarchError::UnknownReference, "curves[0].level"));

    d = baseDoc();
    d["levels"] = json::array({{{"id", 0}}, {{"id", 1}, {"base", 20}}});
    EXPECT_TRUE(onlyError(load(d.dump()).report, MicroarchError::UnsupportedMultiLevel, "levels"));
    d["levels"] = json::array();
    EXPECT_TRUE(onlyError(load(d.dump()).report, MicroarchError::JsonValue, "levels"));

    d = baseDoc();
    d["curves"][0]["prc"] = {{"u", 1.0}};
    EXPECT_TRUE(onlyError(load(d.dump()).report, MicroarchError::JsonValue, "curves[0].prc.u"));
    d["curves"][0]["prc"] = {{"u", 0}};
    EXPECT_TRUE(onlyError(load(d.dump()).report, MicroarchError::JsonValue, "curves[0].prc.u"));
    d["curves"][0]["prc"] = json::object();
    EXPECT_TRUE(onlyError(load(d.dump()).report, MicroarchError::JsonMissing, "curves[0].prc.u"));

    d = baseDoc();
    d["curves"] = json::array();
    EXPECT_TRUE(onlyError(load(d.dump()).report, MicroarchError::JsonValue, "curves"));
    d.erase("curves");
    EXPECT_TRUE(onlyError(load(d.dump()).report, MicroarchError::JsonMissing, "curves"));

    d = baseDoc();
    d["tolerances"] = {{"point", 0}};
    EXPECT_TRUE(onlyError(load(d.dump()).report, MicroarchError::JsonValue, "tolerances.point"));

    // D = 0 : le seul point est au centre. La courbe degeneree n'est pas signalee
    // en plus : sans echelle, les controles geometriques n'ont pas de tolerance.
    d = baseDoc();
    d["points"] = json::array({{{"id", "O"}, {"xy", {0, 0}}, {"ground", true}}});
    d["curves"][0]["start"] = "O";
    d["curves"][0]["end"] = "O";
    EXPECT_TRUE(onlyError(load(d.dump()).report, MicroarchError::DegenerateScale, "points"));
}

TEST(TEST_cgarchi_gothic_micro_json, arc_rules)
{
    // Extremites pythagoriciennes : |(1,3)-(5,0)| = |(8,4)-(5,0)| = 5.
    const Loaded ok = load(arcDoc().dump());
    ASSERT_TRUE(ok.ok) << ReportToJson(ok.report);
    EXPECT_EQ(ok.report.ErrorCount() + ok.report.WarningCount(), 0u);
    const PlanArc &arc = std::get<PlanArc>(ok.spec.curves[0].plan);
    EXPECT_NEAR(arc.radius, 5.0, 1e-12 * std::sqrt(65.0));
    EXPECT_NEAR(arc.sweep, -kPi / 2, 1e-12);

    json d = arcDoc();
    d["points"][1]["xy"] = {8, 4.001};
    EXPECT_TRUE(onlyError(load(d.dump()).report, MicroarchError::ArcEndpointMismatch, "curves[0].center"));

    d = arcDoc();
    d["curves"][0]["center"] = {1, 3};   // rayon nul
    EXPECT_TRUE(onlyError(load(d.dump()).report, MicroarchError::DegenerateCurve, "curves[0].center"));

    d = arcDoc();
    d["curves"][0].erase("center");
    EXPECT_TRUE(onlyError(load(d.dump()).report, MicroarchError::JsonMissing, "curves[0].center"));
    d = arcDoc();
    d["curves"][0].erase("ccw");
    EXPECT_TRUE(onlyError(load(d.dump()).report, MicroarchError::JsonMissing, "curves[0].ccw"));
    d = arcDoc();
    d["curves"][0]["ccw"] = 1;
    EXPECT_TRUE(onlyError(load(d.dump()).report, MicroarchError::JsonType, "curves[0].ccw"));
}

// Decision M12 : un arc de rayon > max_arc_radius_ratio * D est refuse, par le
// lecteur comme par la validation C++, avec un seul diagnostic.
TEST(TEST_cgarchi_gothic_micro_json, nearly_flat_arc_is_refused)
{
    // Corde (1,0) -> (-1,0), centre (0, -h) : D = 2, rayon sqrt(h^2 + 1) ~ h.
    // Seuil fixe par la decision M12 (1e6), ecrit en clair : le test ne suit pas la constante.
    const double D = 2.0, limit = 1e6 * D;
    const auto doc = [](double h) {
        json d = json::parse(R"({
          "schema": "gothic-microarchitecture/v1",
          "symmetry": { "center": [0, 0], "rotation_order": 1 },
          "points": [
            { "id": "A", "xy": [1, 0], "ground": true },
            { "id": "B", "xy": [-1, 0], "height": 5 } ],
          "curves": [ { "id": "l", "type": "arc", "start": "A", "end": "B", "ccw": true } ] })");
        d["curves"][0]["center"] = {0.0, -h};
        return d.dump();
    };

    const Loaded below = load(doc(limit * (1 - 1e-9)));
    ASSERT_TRUE(below.ok) << ReportToJson(below.report);
    EXPECT_EQ(below.report.ErrorCount() + below.report.WarningCount(), 0u);
    EXPECT_LE(std::get<PlanArc>(below.spec.curves[0].plan).radius, limit);

    const Loaded above = load(doc(limit * (1 + 1e-9)));
    EXPECT_FALSE(above.ok);
    EXPECT_TRUE(onlyError(above.report, MicroarchError::NearlyFlatArc, "curves[0]"));

    // r / D = 1e8 : les extremites evaluees manquent les points de ~9 tol.point ;
    // le seul diagnostic est l'arc presque plat, pas arc_endpoint_mismatch.
    EXPECT_TRUE(onlyError(load(doc(1e8 * D)).report, MicroarchError::NearlyFlatArc, "curves[0]"));

    // Meme regle pour une specification construite en C++.
    MicroarchSpec spec = below.spec;
    spec.curves[0].plan = MakeArc(Vector2d(1, 0), Vector2d(-1, 0), Vector2d(0, -limit * (1 + 1e-9)), true);
    MicroarchReport r;
    EXPECT_FALSE(ValidateStructure(spec, r));
    EXPECT_TRUE(onlyError(r, MicroarchError::NearlyFlatArc, "curves[0]"));
    spec.curves[0].plan = MakeArc(Vector2d(1, 0), Vector2d(-1, 0), Vector2d(0, -limit * (1 - 1e-9)), true);
    MicroarchReport r2;
    EXPECT_TRUE(ValidateStructure(spec, r2)) << ReportToJson(r2);
}

// Decision M16 : au plus SpecLimits::max_replicas = 16 384 repliques (courbes de
// base x elements du groupe). Sous D64 avec reflexion (128 elements), 128 courbes
// sont acceptees, 129 refusees, par le lecteur comme par la validation C++.
TEST(TEST_cgarchi_gothic_micro_json, replica_limit)
{
    const auto doc = [](int curves) {
        json d = json::parse(R"({
          "schema": "gothic-microarchitecture/v1",
          "symmetry": { "center": [0, 0], "rotation_order": 64, "reflection_axis_deg": 0 },
          "points": [], "curves": [] })");
        for (int i = 0; i < curves; ++i)
        {
            d["points"].push_back(json{{"id", "P" + std::to_string(i)}, {"xy", {i + 1, 0.5}}, {"ground", true}});
            d["points"].push_back(json{{"id", "Q" + std::to_string(i)}, {"xy", {i + 1, 1.5}}, {"height", 2}});
            d["curves"].push_back(json{{"id", "l" + std::to_string(i)}, {"type", "segment"},
                                       {"start", "P" + std::to_string(i)}, {"end", "Q" + std::to_string(i)}});
        }
        return d.dump();
    };
    const Loaded ok = load(doc(128));
    EXPECT_TRUE(ok.ok) << ReportToJson(ok.report);
    EXPECT_EQ(ok.report.Count(MicroarchError::TooManyReplicas), 0u);
    const Loaded refused = load(doc(129));
    EXPECT_FALSE(refused.ok);
    EXPECT_TRUE(onlyError(refused.report, MicroarchError::TooManyReplicas, "curves"));

    // Meme regle en C++ ; sans reflexion (64 elements), 129 courbes passent.
    MicroarchSpec spec = ok.spec;
    spec.curves.push_back(spec.curves.back());
    spec.curves.back().id = "extra";
    MicroarchReport r;
    EXPECT_FALSE(ValidateStructure(spec, r));
    EXPECT_TRUE(onlyError(r, MicroarchError::TooManyReplicas, "curves"));
    spec.symmetry.reflection_axis_deg.reset();
    MicroarchReport r2;
    EXPECT_TRUE(ValidateStructure(spec, r2)) << ReportToJson(r2);
}

// Toutes les erreurs en une passe : pas d'arret a la premiere.
TEST(TEST_cgarchi_gothic_micro_json, independent_errors_are_all_reported)
{
    json d = baseDoc();
    d["symmetry"]["rotation_order"] = 0;
    d["foo"] = true;
    d["points"][1].erase("height");
    const Loaded l = load(d.dump());
    EXPECT_FALSE(l.ok);
    EXPECT_EQ(l.report.ErrorCount(), 3u) << ReportToJson(l.report);
    EXPECT_TRUE(hasEntry(l.report, true, static_cast<int>(MicroarchError::JsonValue), "symmetry.rotation_order"));
    EXPECT_TRUE(hasEntry(l.report, true, static_cast<int>(MicroarchError::JsonSchema), "foo"));
    EXPECT_TRUE(hasEntry(l.report, true, static_cast<int>(MicroarchError::JsonMissing), "points[1].height"));
}

// Un document refuse laisse `out` intact.
TEST(TEST_cgarchi_gothic_micro_json, rejected_document_leaves_the_output_unchanged)
{
    MicroarchSpec out;
    MicroarchReport report;
    ASSERT_TRUE(LoadSpecFromFile(kC4Pinwheel, out, report));
    const MicroarchSpec before = out;

    json d = baseDoc();
    d["points"][1].erase("height");
    EXPECT_FALSE(LoadSpecFromJson(d.dump(), out, report));
    EXPECT_TRUE(sameSpec(out, before));
    EXPECT_FALSE(LoadSpecFromJson("{", out, report));
    EXPECT_TRUE(sameSpec(out, before));
}

TEST(TEST_cgarchi_gothic_micro_json, report_json_format)
{
    json d = baseDoc();
    d["foo"] = 1;
    const json r = json::parse(ReportToJson(load(d.dump()).report));
    ASSERT_TRUE(r.contains("errors") && r.contains("warnings") && r.contains("entries"));
    EXPECT_EQ(r["errors"].size(), kMicroarchErrorCount);
    EXPECT_EQ(r["warnings"].size(), kMicroarchWarningCount);
    EXPECT_EQ(r["errors"]["json_schema"], 1);
    EXPECT_EQ(r["errors"]["json_parse"], 0);
    ASSERT_EQ(r["entries"].size(), 1u);
    EXPECT_EQ(r["entries"][0]["severity"], "error");
    EXPECT_EQ(r["entries"][0]["code"], "json_schema");
    EXPECT_EQ(r["entries"][0]["parameter"], "foo");
}

// ---------------------------------------------------------------------------
//  Bornes : celles du schema, lues dans le fichier
// ---------------------------------------------------------------------------

// Chaque champ numerique borne du schema : accepte a une borne incluse, refuse
// juste au-dela ; refuse a une borne exclue, accepte juste en deca. Les champs
// et leurs bornes sont lus dans le schema, pas recopies.
TEST(TEST_cgarchi_gothic_micro_json, reader_bounds_are_the_schema_bounds)
{
    const std::filesystem::path schemaPath =
        std::filesystem::path("src") / "cgarchi" / "schemas" / "gothic-microarchitecture-v1.schema";
    const std::string text = readFile(schemaPath.string());
    ASSERT_FALSE(text.empty()) << schemaPath.string() << " introuvable";
    const json schema = json::parse(text);

    // Champs numeriques bornes, par pointeur JSON ; un tableau est parcouru par son
    // element 0.
    std::vector<std::pair<std::string, json>> fields;
    std::function<void(const json &, const std::string &)> walk = [&](const json &node, const std::string &path) {
        const json *n = &node;
        if (n->contains("$ref"))
        {
            const std::string ref = (*n)["$ref"].get<std::string>();
            n = &schema["$defs"][ref.substr(ref.rfind('/') + 1)];
        }
        const std::string type = (n->contains("type") && (*n)["type"].is_string()) ? (*n)["type"].get<std::string>() : "";
        const bool bounded = n->contains("minimum") || n->contains("maximum")
                          || n->contains("exclusiveMinimum") || n->contains("exclusiveMaximum");
        if ((type == "number" || type == "integer") && bounded)
            fields.emplace_back(path, *n);
        if (n->contains("properties"))
            for (auto it = (*n)["properties"].begin(); it != (*n)["properties"].end(); ++it)
                walk(it.value(), path + "/" + it.key());
        if (type == "array" && n->contains("items"))
            walk((*n)["items"], path + "/0");
    };
    walk(schema, "");
    // rotation_order, reflection_axis_deg, levels[].id, curves[].level, curves[].prc.u,
    // et les 4 tolerances.
    EXPECT_EQ(fields.size(), 9u) << "champs bornes du schema v1";

    // Tableaux plafonnes (maxItems) de la racine : acceptes au plafond, refuses
    // au-dela, avec un diagnostic sur le tableau. Remplissage : copies de
    // l'element 0 sous un id neuf (les points ajoutes ne sont que non references).
    std::vector<std::pair<std::string, std::size_t>> capped;
    for (auto it = schema["properties"].begin(); it != schema["properties"].end(); ++it)
        if (it.value().contains("maxItems"))
            capped.emplace_back(it.key(), it.value()["maxItems"].get<std::size_t>());
    EXPECT_EQ(capped.size(), 2u) << "tableaux plafonnes du schema v1";
    for (const auto &[name, cap] : capped)
    {
        for (const std::size_t n : {cap, cap + 1})
        {
            json doc = baseDoc();
            json &array = doc[name];
            const json first = array[0];
            for (std::size_t i = array.size(); i < n; ++i)
            {
                json copy = first;
                copy["id"] = "pad" + std::to_string(i);
                array.push_back(std::move(copy));
            }
            const Loaded l = load(doc.dump());
            EXPECT_EQ(l.ok, n == cap) << name << " : " << n << " elements";
            EXPECT_EQ(l.report.ErrorCount(), n == cap ? 0u : 1u) << name << " : " << n;
            if (n != cap)
                EXPECT_TRUE(hasEntry(l.report, true, static_cast<int>(MicroarchError::JsonValue), name)) << name;
        }
    }

    json base = baseDoc();
    base["levels"] = json::array({{{"id", 0}, {"base", 0}}});
    auto check = [&](const std::string &pointer, const json &value, bool accepted) {
        json doc = base;
        doc[json::json_pointer(pointer)] = value;
        // L'id du niveau et le niveau de la courbe se suivent : un niveau non
        // declare serait une autre erreur.
        if (pointer == "/levels/0/id")
            doc["curves"][0]["level"] = value;
        if (pointer == "/curves/0/level")
            doc["levels"][0]["id"] = value;
        const Loaded l = load(doc.dump());
        std::string dotted = pointer.substr(1);
        std::replace(dotted.begin(), dotted.end(), '/', '.');
        for (const char *array : {"levels", "curves"})
        {
            const std::string from = std::string(array) + ".0";
            if (dotted.compare(0, from.size(), from) == 0)
                dotted.replace(0, from.size(), std::string(array) + "[0]");
        }
        EXPECT_EQ(l.ok, accepted) << dotted << " = " << value.dump() << " : " << ReportToJson(l.report);
        if (!accepted)
        {
            EXPECT_TRUE(hasEntry(l.report, true, static_cast<int>(MicroarchError::JsonValue), dotted))
                << dotted << " : " << ReportToJson(l.report);
        }
    };
    for (const auto &[pointer, spec] : fields)
    {
        const bool integer = spec["type"] == "integer";
        auto step = [&](double b) { return integer ? 1.0 : 1e-3 * std::max(std::abs(b), 1e-3); };
        auto value = [&](double v) { return integer ? json(static_cast<long long>(v)) : json(v); };
        if (spec.contains("minimum"))
        {
            const double b = spec["minimum"].get<double>();
            check(pointer, value(b), true);
            check(pointer, value(b - step(b)), false);
        }
        if (spec.contains("maximum"))
        {
            const double b = spec["maximum"].get<double>();
            check(pointer, value(b), true);
            check(pointer, value(b + step(b)), false);
        }
        if (spec.contains("exclusiveMinimum"))
        {
            const double b = spec["exclusiveMinimum"].get<double>();
            check(pointer, value(b), false);
            check(pointer, value(b + step(b)), true);
        }
        if (spec.contains("exclusiveMaximum"))
        {
            const double b = spec["exclusiveMaximum"].get<double>();
            check(pointer, value(b), false);
            check(pointer, value(b - step(b)), true);
        }
    }
}

// ---------------------------------------------------------------------------
//  Domaine de l'axe, niveaux mal formes, types d'elements, regle ground ou height
// ---------------------------------------------------------------------------

// Axe hors de [0, 180) : refuse par le lecteur et par la validation C++, au meme chemin.
TEST(TEST_cgarchi_gothic_micro_json, axis_outside_its_domain_is_refused_everywhere)
{
    for (double deg : {-1e-17, 180.0, 359.999999999})
    {
        json d = baseDoc();
        d["symmetry"]["reflection_axis_deg"] = deg;
        EXPECT_TRUE(onlyError(load(d.dump()).report, MicroarchError::JsonValue, "symmetry.reflection_axis_deg")) << deg;

        Loaded ok = load(baseDoc().dump());
        ASSERT_TRUE(ok.ok);
        ok.spec.symmetry.reflection_axis_deg = deg;
        MicroarchReport r;
        EXPECT_FALSE(ValidateStructure(ok.spec, r)) << deg;
        EXPECT_TRUE(onlyError(r, MicroarchError::JsonValue, "symmetry.reflection_axis_deg")) << deg;
    }
}

// Un niveau mal forme ne cree pas de fausse reference inconnue et ne masque pas
// unsupported_multi_level.
TEST(TEST_cgarchi_gothic_micro_json, malformed_levels_do_not_cascade)
{
    json d = baseDoc();
    d["levels"] = json::array({{{"id", 5}, {"base", "x"}}});
    d["curves"][0]["level"] = 5;
    EXPECT_TRUE(onlyError(load(d.dump()).report, MicroarchError::JsonType, "levels[0].base"));

    d = baseDoc();
    d["levels"] = json::array({{{"id", "a"}}});
    EXPECT_TRUE(onlyError(load(d.dump()).report, MicroarchError::JsonType, "levels[0].id"));

    // Niveau de courbe illisible : pas de reference de niveau controlee.
    d = baseDoc();
    d["levels"] = json::array({{{"id", 5}}});
    d["curves"][0]["level"] = "x";
    EXPECT_TRUE(onlyError(load(d.dump()).report, MicroarchError::JsonType, "curves[0].level"));

    d = baseDoc();
    d["levels"] = json::array({{{"id", 0}}, {{"id", 1}, {"base", "x"}}});
    const Loaded l = load(d.dump());
    EXPECT_EQ(l.report.ErrorCount(), 2u) << ReportToJson(l.report);
    EXPECT_TRUE(hasEntry(l.report, true, static_cast<int>(MicroarchError::JsonType), "levels[1].base"));
    EXPECT_TRUE(hasEntry(l.report, true, static_cast<int>(MicroarchError::UnsupportedMultiLevel), "levels"));
}

// Elements de tableau qui ne sont pas des objets : un diagnostic, sans cascade.
TEST(TEST_cgarchi_gothic_micro_json, array_items_of_the_wrong_type)
{
    json d = baseDoc();
    d["points"].insert(d["points"].begin(), 5);
    EXPECT_TRUE(onlyError(load(d.dump()).report, MicroarchError::JsonType, "points[0]"));

    d = baseDoc();
    d["curves"].insert(d["curves"].begin(), "l0");
    EXPECT_TRUE(onlyError(load(d.dump()).report, MicroarchError::JsonType, "curves[0]"));

    d = baseDoc();
    d["levels"] = json::array({true});
    EXPECT_TRUE(onlyError(load(d.dump()).report, MicroarchError::JsonType, "levels[0]"));

    d = baseDoc();
    d["points"] = {{"id", "A"}};
    EXPECT_TRUE(onlyError(load(d.dump()).report, MicroarchError::JsonType, "points"));
}

// La regle "ground ou height" du schema (oneOf) et celle du lecteur acceptent et
// refusent les memes points. Evaluation du oneOf lu dans le schema : chaque
// branche est un `required` plus des `const` facultatifs.
TEST(TEST_cgarchi_gothic_micro_json, ground_or_height_rule_matches_the_schema)
{
    const std::string text = readFile((std::filesystem::path("src") / "cgarchi" / "schemas"
                                       / "gothic-microarchitecture-v1.schema").string());
    ASSERT_FALSE(text.empty());
    const json schema = json::parse(text);
    const json &oneOf = schema["properties"]["points"]["items"]["oneOf"];
    ASSERT_TRUE(oneOf.is_array());
    ASSERT_EQ(oneOf.size(), 2u);

    auto branchMatches = [](const json &branch, const json &point) {
        for (const json &r : branch.value("required", json::array()))
            if (!point.contains(r.get<std::string>()))
                return false;
        if (branch.contains("properties"))
            for (auto it = branch["properties"].begin(); it != branch["properties"].end(); ++it)
                if (point.contains(it.key()) && it.value().contains("const") && point[it.key()] != it.value()["const"])
                    return false;
        return true;
    };

    struct Case { const char *name; json ground; json height; };
    const Case cases[] = {
        {"ground", true, nullptr},
        {"ground et height", true, 3},
        {"height", nullptr, 3},
        {"ground false et height", false, 3},
        {"ground false", false, nullptr},
        {"rien", nullptr, nullptr},
    };
    std::size_t accepted = 0;
    for (const Case &c : cases)
    {
        json d = baseDoc();
        json &b = d["points"][1];
        b.erase("height");
        if (!c.ground.is_null()) b["ground"] = c.ground;
        if (!c.height.is_null()) b["height"] = c.height;
        std::size_t matching = 0;
        for (const json &branch : oneOf)
            matching += branchMatches(branch, b) ? 1 : 0;
        const bool bySchema = matching == 1;
        const bool byReader = load(d.dump()).ok;
        EXPECT_EQ(byReader, bySchema) << c.name;
        accepted += byReader ? 1 : 0;
    }
    EXPECT_EQ(accepted, 3u);   // ground ; height ; ground false et height
}
