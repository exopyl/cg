#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <locale>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include <cgarchi/gothic_microarchitecture.h>
#include <cgarchi/gothic_microarchitecture_export.h>
#include <cgarchi/gothic_microarchitecture_json.h>

using namespace cgarchi::microarch;   // fichier de test du module seul
using nlohmann::json;

// ===========================================================================
//  Microarchitecture gothique : export du reseau (JSON, OBJ)
// ===========================================================================
//
// Plan d'execution, §7 et §8 J4-b. Sorties dans tmp/ (relatif, cree par le test).

namespace
{
    constexpr double kPi = 3.14159265358979323846;
    const char *const kD4Segment = "./test/data/gothic_micro_d4_segment.json";
    const char *const kC4Pinwheel = "./test/data/gothic_micro_c4_pinwheel.json";

    MicroarchSpec loadFile(const char *path)
    {
        MicroarchSpec spec;
        MicroarchReport report;
        EXPECT_TRUE(LoadSpecFromFile(path, spec, report)) << ReportToJson(report);
        return spec;
    }

    PointSpec point(const std::string &id, double x, double y, std::optional<double> height)
    {
        PointSpec p;
        p.id = id;
        p.xy = Vector2d(x, y);
        p.ground = !height;
        p.height = height;
        return p;
    }

    MicroarchSpec d4Arc()
    {
        MicroarchSpec spec;
        spec.symmetry.center = Vector2d(0, 0);
        spec.symmetry.rotation_order = 4;
        spec.symmetry.reflection_axis_deg = 90.0;
        spec.points = {point("C", 1, 3, std::nullopt), point("E", 8, 4, 5 * kPi)};
        BasicCurveSpec c;
        c.id = "l";
        c.start_point = 0;
        c.end_point = 1;
        c.plan = MakeArc(spec.points[0].xy, spec.points[1].xy, Vector2d(5, 0), false);
        spec.curves = {c};
        return spec;
    }

    struct Built
    {
        MicroarchSpec   spec;
        MicroarchReport report;
        Network         net;
    };

    Built build(MicroarchSpec spec)
    {
        Built b;
        b.spec = std::move(spec);
        std::optional<Network> net = BuildNetwork(b.spec, b.report);
        EXPECT_TRUE(net.has_value()) << ReportToJson(b.report);
        if (net)
            b.net = std::move(*net);
        return b;
    }

    // Egalite exacte d'un tableau JSON de doubles.
    bool sameXY(const json &j, const Vector2d &p) { return j.size() == 2 && j[0].get<double>() == p.x && j[1].get<double>() == p.y; }
    bool sameXYZ(const json &j, const Vector3d &p)
    {
        return j.size() == 3 && j[0].get<double>() == p.x && j[1].get<double>() == p.y && j[2].get<double>() == p.z;
    }

    void expectIncidences(const json &j, const std::vector<Incidence> &v)
    {
        ASSERT_EQ(j.size(), v.size());
        for (std::size_t k = 0; k < v.size(); ++k)
        {
            EXPECT_EQ(j[k]["curve"].get<int>(), v[k].curve);
            EXPECT_EQ(j[k]["s"].get<double>(), v[k].s);
        }
    }

    // Separateur decimal ',' : la locale globale ne doit pas atteindre l'OBJ.
    struct CommaPunct : std::numpunct<char>
    {
        char do_decimal_point() const override { return ','; }
        char do_thousands_sep() const override { return '.'; }
        std::string do_grouping() const override { return "\3"; }
    };

    std::string readText(const std::string &path)
    {
        std::ifstream in(path, std::ios::binary);
        std::ostringstream os;
        os << in.rdbuf();
        return os.str();
    }
}

// ---------------------------------------------------------------------------
//  JSON du reseau
// ---------------------------------------------------------------------------

// Relu, chaque double est egal a la valeur du Network ; nombres d'elements egaux.
TEST(TEST_cgarchi_gothic_micro_export, network_json_round_trip)
{
    for (const MicroarchSpec &spec : {loadFile(kD4Segment), loadFile(kC4Pinwheel), d4Arc()})
    {
        const Built b = build(spec);
        const Network &n = b.net;
        const json j = json::parse(NetworkToJson(n, b.spec, b.report));
        EXPECT_EQ(j["schema"].get<std::string>(), kMicroarchNetworkSchemaV1);
        EXPECT_EQ(j["scale"].get<double>(), n.tol.scale);
        EXPECT_EQ(j["tolerances"]["point"].get<double>(), n.tol.point);
        EXPECT_EQ(j["tolerances"]["parameter"].get<double>(), n.tol.parameter);
        EXPECT_EQ(j["tolerances"]["height"].get<double>(), n.tol.height);
        EXPECT_EQ(j["tolerances"]["prc_snap"].get<double>(), n.tol.prc_snap);
        ASSERT_EQ(j["levels"].size(), n.levels.size());

        ASSERT_EQ(j["basic_curves"].size(), n.basics.size());
        for (std::size_t i = 0; i < n.basics.size(); ++i)
        {
            const json &bc = j["basic_curves"][i];
            const BasicCurveResult &r = n.basics[i];
            EXPECT_EQ(bc["id"].get<std::string>(), b.spec.curves[i].id);
            EXPECT_EQ(bc["prc"]["s"].get<double>(), r.prc.s);
            EXPECT_EQ(bc["prc"]["candidates"].size(), r.prc.candidates.size());
            for (std::size_t k = 0; k < r.prc.candidates.size(); ++k)
            {
                EXPECT_EQ(bc["prc"]["candidates"][k]["s"].get<double>(), r.prc.candidates[k].s);
                EXPECT_TRUE(sameXY(bc["prc"]["candidates"][k]["xy"], r.prc.candidates[k].xy));
            }
            const json &p = bc["profile"];
            EXPECT_EQ(p["length"].get<double>(), r.profile.length);
            EXPECT_EQ(p["z_start"].get<double>(), r.profile.z_start);
            EXPECT_EQ(p["z_end"].get<double>(), r.profile.z_end);
            EXPECT_EQ(p["s_prc"].get<double>(), r.profile.s_prc);
            EXPECT_EQ(p["z_prc"].get<double>(), r.profile.z_prc);
            EXPECT_EQ(p["r_start"].get<double>(), r.profile.r_start);
            EXPECT_EQ(p["r_end"].get<double>(), r.profile.r_end);
            EXPECT_TRUE(sameXY(p["center_start"], StartCenter(r.profile)));
            EXPECT_TRUE(sameXY(p["center_end"], EndCenter(r.profile)));
            EXPECT_EQ(p["monotonic"].get<bool>(), r.profile.monotonic);
        }

        ASSERT_EQ(j["curves"].size(), n.curves.size());
        for (std::size_t i = 0; i < n.curves.size(); ++i)
        {
            const json &c = j["curves"][i];
            const SpaceCurve &s = n.curves[i];
            EXPECT_EQ(c["basic"].get<int>(), s.basic);
            EXPECT_EQ(c["element"]["k"].get<int>(), s.element.k);
            EXPECT_EQ(c["element"]["reflected"].get<bool>(), s.element.reflected);
            if (const auto *seg = std::get_if<PlanSegment>(&s.plan))
            {
                EXPECT_EQ(c["plan"]["type"].get<std::string>(), "segment");
                EXPECT_TRUE(sameXY(c["plan"]["start"], seg->start));
                EXPECT_TRUE(sameXY(c["plan"]["end"], seg->end));
            }
            else
            {
                const PlanArc &a = std::get<PlanArc>(s.plan);
                EXPECT_EQ(c["plan"]["type"].get<std::string>(), "arc");
                EXPECT_TRUE(sameXY(c["plan"]["center"], a.center));
                EXPECT_EQ(c["plan"]["radius"].get<double>(), a.radius);
                EXPECT_EQ(c["plan"]["start_angle_rad"].get<double>(), a.start_angle);
                EXPECT_EQ(c["plan"]["sweep_rad"].get<double>(), a.sweep);
            }
        }

        ASSERT_EQ(j["junctions"].size(), n.junctions.size());
        for (std::size_t i = 0; i < n.junctions.size(); ++i)
        {
            const json &jn = j["junctions"][i];
            EXPECT_TRUE(sameXYZ(jn["position"], n.junctions[i].position));
            EXPECT_EQ(jn["max_gap"].get<double>(), n.junctions[i].max_gap);
            EXPECT_EQ(jn["prc"].get<bool>(), n.junctions[i].prc);
            EXPECT_EQ(jn["ground"].get<bool>(), n.junctions[i].ground);
            expectIncidences(jn["incidences"], n.junctions[i].incidences);
        }
        ASSERT_EQ(j["crossings"].size(), n.crossings.size());
        for (std::size_t i = 0; i < n.crossings.size(); ++i)
        {
            const json &x = j["crossings"][i];
            EXPECT_TRUE(sameXY(x["xy"], n.crossings[i].xy));
            EXPECT_EQ(x["min_dz"].get<double>(), n.crossings[i].min_dz);
            ASSERT_EQ(x["groups"].size(), n.crossings[i].groups.size());
            for (std::size_t g = 0; g < n.crossings[i].groups.size(); ++g)
            {
                EXPECT_EQ(x["groups"][g]["z"].get<double>(), n.crossings[i].groups[g].z);
                expectIncidences(x["groups"][g]["incidences"], n.crossings[i].groups[g].incidences);
            }
        }
        expectIncidences(j["free_ends"], n.free_ends);
        EXPECT_EQ(j["report"]["entries"].size(), b.report.entries.size());
    }
}

// ---------------------------------------------------------------------------
//  OBJ
// ---------------------------------------------------------------------------

// Reference D4 segment ecrite dans tmp/ puis relue : 8 objets, 8 polylignes,
// indices valides, sommets a 1e-15 en relatif des echantillons.
TEST(TEST_cgarchi_gothic_micro_export, obj_of_the_d4_segment_reference)
{
    const Built b = build(loadFile(kD4Segment));
    std::filesystem::create_directories("tmp");
    const std::string path = "tmp/gothic_micro_d4_segment.obj";
    MicroarchReport report;
    ASSERT_TRUE(WriteNetworkObj(b.net, b.spec, SamplingParams{}, path, report)) << ReportToJson(report);

    MicroarchReport sampling;
    const std::vector<std::vector<Vector3d>> lines = SampleNetwork(b.net, SamplingParams{}, sampling);
    std::vector<Vector3d> expected;
    for (const auto &l : lines)
        expected.insert(expected.end(), l.begin(), l.end());

    std::istringstream in(readText(path));
    in.imbue(std::locale::classic());
    std::vector<Vector3d> v;
    std::vector<std::vector<long>> polylines;
    int objects = 0;
    std::string line;
    while (std::getline(in, line))
    {
        std::istringstream ls(line);
        ls.imbue(std::locale::classic());
        std::string tag;
        ls >> tag;
        if (tag == "o")
            ++objects;
        else if (tag == "v")
        {
            double x = 0, y = 0, z = 0;
            ls >> x >> y >> z;
            ASSERT_FALSE(ls.fail()) << line;
            v.emplace_back(x, y, z);
        }
        else if (tag == "l")
        {
            polylines.emplace_back();
            long i = 0;
            while (ls >> i)
                polylines.back().push_back(i);
        }
    }
    EXPECT_EQ(objects, 8);
    ASSERT_EQ(polylines.size(), 8u);
    ASSERT_EQ(v.size(), expected.size());
    std::size_t total = 0;
    for (std::size_t k = 0; k < polylines.size(); ++k)
    {
        EXPECT_EQ(polylines[k].size(), lines[k].size());
        for (long i : polylines[k])
        {
            EXPECT_GE(i, 1);
            EXPECT_LE(i, static_cast<long>(v.size()));
        }
        total += polylines[k].size();
    }
    EXPECT_EQ(total, v.size());
    const auto close = [](double a, double e) { return std::fabs(a - e) <= 1e-15 * std::fabs(e); };
    for (std::size_t k = 0; k < v.size(); ++k)
        EXPECT_TRUE(close(v[k].x, expected[k].x) && close(v[k].y, expected[k].y) && close(v[k].z, expected[k].z)) << "v " << k + 1;
}

// La locale globale (separateur decimal ',') n'atteint pas l'OBJ.
TEST(TEST_cgarchi_gothic_micro_export, obj_ignores_the_global_locale)
{
    const Built b = build(d4Arc());
    const std::locale previous = std::locale::global(std::locale(std::locale::classic(), new CommaPunct));
    MicroarchReport report;
    const std::string text = NetworkToObj(b.net, b.spec, SamplingParams{}, report);
    std::locale::global(previous);
    ASSERT_FALSE(text.empty());
    EXPECT_EQ(text.find(','), std::string::npos);
    EXPECT_NE(text.find('.'), std::string::npos);
}

TEST(TEST_cgarchi_gothic_micro_export, obj_to_an_impossible_path_is_false)
{
    const Built b = build(loadFile(kD4Segment));
    std::filesystem::create_directories("tmp");
    const std::string blocker = "tmp/gothic_micro_export_blocker.txt";
    {
        std::ofstream f(blocker);
        f << "x";
    }
    MicroarchReport report;
    bool written = true;
    EXPECT_NO_THROW(written = WriteNetworkObj(b.net, b.spec, SamplingParams{}, blocker + "/sub/x.obj", report));
    EXPECT_FALSE(written);
    ASSERT_EQ(report.ErrorCount(), 1u);
    EXPECT_EQ(report.Count(MicroarchError::ExportWriteFailed), 1u);
    EXPECT_EQ(report.entries.back().path, "export.path");
}

// Plafond M15 : l'OBJ est refuse comme SampleNetwork.
TEST(TEST_cgarchi_gothic_micro_export, obj_respects_the_sample_limit)
{
    MicroarchSpec spec;
    spec.symmetry.center = Vector2d(0, 0);
    for (int i = 0; i < 77; ++i)
    {
        spec.points.push_back(point("P" + std::to_string(i), i, 0, std::nullopt));
        spec.points.push_back(point("Q" + std::to_string(i), i, 1, 2.0));
        BasicCurveSpec c;
        c.id = "l" + std::to_string(i);
        c.start_point = 2 * i;
        c.end_point = 2 * i + 1;
        c.plan = PlanSegment{spec.points[2 * i].xy, spec.points[2 * i + 1].xy};
        spec.curves.push_back(c);
    }
    const Built b = build(spec);
    MicroarchReport report;
    EXPECT_TRUE(NetworkToObj(b.net, b.spec, SamplingParams{1e-9}, report).empty());
    EXPECT_EQ(report.Count(MicroarchError::SampleLimitExceeded), 1u);
    MicroarchReport r2;
    EXPECT_FALSE(WriteNetworkObj(b.net, b.spec, SamplingParams{1e-9}, "tmp/gothic_micro_export_limit.obj", r2));
}

// Noms d'objet : un seul mot, sans '#', uniques par l'indice de la courbe.
TEST(TEST_cgarchi_gothic_micro_export, obj_object_names_are_unique_single_words)
{
    MicroarchSpec spec = loadFile(kD4Segment);
    spec.curves[0].id = "a b\tc";
    const Built b = build(spec);
    MicroarchReport report;
    const std::string text = NetworkToObj(b.net, b.spec, SamplingParams{}, report);
    EXPECT_NE(text.find("\no a_b_c_k0_0\n"), std::string::npos);
    EXPECT_NE(text.find("\no a_b_c_k3_r_7\n"), std::string::npos);

    // « a b » et « a_b » donneraient le meme nom sans l'indice ; '#' ouvrirait un commentaire.
    MicroarchSpec two;
    two.symmetry.center = Vector2d(0, 0);
    two.points = {point("A", 0, 0, std::nullopt), point("B", 0, 1, 2.0), point("C", 1, 0, std::nullopt), point("E", 1, 1, 2.0)};
    for (const auto &[id, a] : {std::make_pair(std::string("a b"), 0), std::make_pair(std::string("a_b#"), 2)})
    {
        BasicCurveSpec c;
        c.id = id;
        c.start_point = a;
        c.end_point = a + 1;
        c.plan = PlanSegment{two.points[a].xy, two.points[a + 1].xy};
        two.curves.push_back(c);
    }
    const Built t = build(two);
    MicroarchReport r2;
    std::istringstream in(NetworkToObj(t.net, t.spec, SamplingParams{}, r2));
    std::vector<std::string> names;
    std::string line;
    while (std::getline(in, line))
        if (line.rfind("o ", 0) == 0)
            names.push_back(line.substr(2));
    ASSERT_EQ(names.size(), 2u);
    EXPECT_NE(names[0], names[1]);
    for (const std::string &n : names)
        EXPECT_EQ(n.find('#'), std::string::npos) << n;
}

// ---------------------------------------------------------------------------
//  Schema de sortie
// ---------------------------------------------------------------------------

namespace
{
    // Validation par le schema lu sur disque (meme outillage que les tests du schema
    // d'entree : nlohmann, mots-cles lus a la main). Mots-cles pris en charge : type,
    // const, enum, required, properties, additionalProperties (false ou schema),
    // propertyNames, items, minItems, maxItems, minimum, oneOf, $ref local
    // (#/$defs/...), et les annotations $schema, $id, $defs, title, description.
    // Tout autre mot-cle fait echouer la validation. Rend le premier ecart.
    std::string violation(const json &schema, const json &root, const json &v, const std::string &path)
    {
        static const std::vector<std::string> known = {
            "$schema", "$id", "$defs", "title", "description", "type", "const", "enum", "required", "properties",
            "additionalProperties", "propertyNames", "items", "minItems", "maxItems", "minimum", "oneOf", "$ref"};
        for (auto it = schema.begin(); it != schema.end(); ++it)
            if (std::find(known.begin(), known.end(), it.key()) == known.end())
                return path + " : unsupported keyword " + it.key();
        if (schema.contains("$ref"))
            return violation(root["$defs"][schema["$ref"].get<std::string>().substr(std::string("#/$defs/").size())],
                             root, v, path);
        const json &s = schema;
        if (s.contains("oneOf"))
        {
            int valid = 0;
            std::string first;
            for (const json &sub : s["oneOf"])
            {
                const std::string e = violation(sub, root, v, path);
                if (e.empty())
                    ++valid;
                else if (first.empty())
                    first = e;
            }
            if (valid != 1)
                return path + " : oneOf matched " + std::to_string(valid) + (first.empty() ? std::string() : " (" + first + ")");
        }
        if (s.contains("const") && v != s["const"])
            return path + " : const";
        if (s.contains("enum") && std::find(s["enum"].begin(), s["enum"].end(), v) == s["enum"].end())
            return path + " : enum";
        if (s.contains("type"))
        {
            const std::string t = s["type"].get<std::string>();
            const bool ok = (t == "object" && v.is_object()) || (t == "array" && v.is_array())
                         || (t == "string" && v.is_string()) || (t == "boolean" && v.is_boolean())
                         || (t == "integer" && v.is_number_integer()) || (t == "number" && v.is_number());
            if (!ok)
                return path + " : type " + t;
        }
        if (s.contains("minimum") && v.is_number() && v.get<double>() < s["minimum"].get<double>())
            return path + " : minimum";
        if (v.is_object())
        {
            if (s.contains("required"))
                for (const json &k : s["required"])
                    if (!v.contains(k.get<std::string>()))
                        return path + " : missing " + k.get<std::string>();
            for (auto it = v.begin(); it != v.end(); ++it)
            {
                const std::string sub = path + "." + it.key();
                if (s.contains("propertyNames"))
                {
                    const std::string e = violation(s["propertyNames"], root, json(it.key()), sub + " (name)");
                    if (!e.empty())
                        return e;
                }
                std::string e;
                if (s.contains("properties") && s["properties"].contains(it.key()))
                    e = violation(s["properties"][it.key()], root, it.value(), sub);
                else if (s.contains("additionalProperties"))
                {
                    if (s["additionalProperties"].is_boolean())
                        e = s["additionalProperties"].get<bool>() ? std::string() : path + " : unknown " + it.key();
                    else
                        e = violation(s["additionalProperties"], root, it.value(), sub);
                }
                if (!e.empty())
                    return e;
            }
        }
        if (v.is_array())
        {
            if (s.contains("minItems") && v.size() < s["minItems"].get<std::size_t>())
                return path + " : minItems";
            if (s.contains("maxItems") && v.size() > s["maxItems"].get<std::size_t>())
                return path + " : maxItems";
            if (s.contains("items"))
                for (std::size_t i = 0; i < v.size(); ++i)
                {
                    const std::string e = violation(s["items"], root, v[i], path + "[" + std::to_string(i) + "]");
                    if (!e.empty())
                        return e;
                }
        }
        return std::string();
    }
}

// Le JSON du reseau des references respecte src/cgarchi/schemas/gothic-microarchitecture-network-v1.schema.
TEST(TEST_cgarchi_gothic_micro_export, network_json_matches_its_schema)
{
    const std::string text = readText("./src/cgarchi/schemas/gothic-microarchitecture-network-v1.schema");
    ASSERT_FALSE(text.empty());
    const json schema = json::parse(text);
    EXPECT_EQ(schema["properties"]["schema"]["const"].get<std::string>(), kMicroarchNetworkSchemaV1);
    for (const MicroarchSpec &spec : {loadFile(kD4Segment), loadFile(kC4Pinwheel), d4Arc()})
    {
        const Built b = build(spec);
        const json doc = json::parse(NetworkToJson(b.net, b.spec, b.report));
        EXPECT_EQ(violation(schema, schema, doc, "$"), "");
    }
    // Le validateur echoue sur des documents fautifs.
    const Built b = build(loadFile(kD4Segment));
    json bad = json::parse(NetworkToJson(b.net, b.spec, b.report));
    bad.erase("free_ends");
    EXPECT_NE(violation(schema, schema, bad, "$"), "");
    bad = json::parse(NetworkToJson(b.net, b.spec, b.report));
    bad["junctions"][0]["extra"] = 1;
    EXPECT_NE(violation(schema, schema, bad, "$"), "");
    bad = json::parse(NetworkToJson(b.net, b.spec, b.report));
    bad["curves"][0]["element"]["k"] = 0.5;
    EXPECT_NE(violation(schema, schema, bad, "$"), "");
    bad = json::parse(NetworkToJson(b.net, b.spec, b.report));
    bad["basic_curves"][0]["prc"]["rule"] = "nearest";
    EXPECT_NE(violation(schema, schema, bad, "$"), "");
    // Segment sans `end`, segment avec une cle d'arc, code de rapport inconnu.
    bad = json::parse(NetworkToJson(b.net, b.spec, b.report));
    bad["curves"][0]["plan"].erase("end");
    EXPECT_NE(violation(schema, schema, bad, "$"), "");
    bad = json::parse(NetworkToJson(b.net, b.spec, b.report));
    bad["curves"][0]["plan"]["radius"] = 1.0;
    EXPECT_NE(violation(schema, schema, bad, "$"), "");
    bad = json::parse(NetworkToJson(b.net, b.spec, b.report));
    bad["report"]["errors"]["no_such_code"] = 0;
    EXPECT_NE(violation(schema, schema, bad, "$"), "");
    bad = json::parse(NetworkToJson(b.net, b.spec, b.report));
    bad["report"]["warnings"]["prc_tie"] = -1;
    EXPECT_NE(violation(schema, schema, bad, "$"), "");
    // Arc sans `radius`.
    const Built a = build(d4Arc());
    bad = json::parse(NetworkToJson(a.net, a.spec, a.report));
    ASSERT_EQ(bad["curves"][0]["plan"]["type"].get<std::string>(), "arc");
    bad["curves"][0]["plan"].erase("radius");
    EXPECT_NE(violation(schema, schema, bad, "$"), "");
    // Un mot-cle que le validateur ne gere pas fait echouer, meme sur un document valide.
    json strange = schema;
    strange["properties"]["scale"]["exclusiveMinimum"] = 0;
    EXPECT_NE(violation(strange, strange, json::parse(NetworkToJson(b.net, b.spec, b.report)), "$"), "");

    // Les codes du schema sont exactement ceux de la bibliotheque.
    std::vector<std::string> errors, warnings;
    for (std::size_t k = 0; k < kMicroarchErrorCount; ++k)
        errors.push_back(MicroarchErrorName(static_cast<MicroarchError>(k)));
    for (std::size_t k = 0; k < kMicroarchWarningCount; ++k)
        warnings.push_back(MicroarchWarningName(static_cast<MicroarchWarning>(k)));
    EXPECT_EQ(schema["$defs"]["errorCode"]["enum"].get<std::vector<std::string>>(), errors);
    EXPECT_EQ(schema["$defs"]["warningCode"]["enum"].get<std::vector<std::string>>(), warnings);
}
