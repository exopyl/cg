#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <locale>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include <cgarchi/architecture_gothic.h>
#include <cgarchi/gothic_microarchitecture.h>
#include <cgarchi/gothic_microarchitecture_json.h>
#include <cgarchi/json_reader.h>
#include <cgarchi/parameterized_gothic.h>
#include "../src/cgmesh/print_validation_json.h"

// ===========================================================================
//  Lecteurs JSON stricts de cgarchi : messages figes de la fenetre gothique et
//  de la microarchitecture. Chaque cas donne le rapport ENTIER, dans l'ordre :
//  un message, un chemin ou un code qui change fait echouer le test.
// ===========================================================================

namespace
{
    using nlohmann::json;

    std::string describe(const cgarchi::GothicBuildReport &r)
    {
        std::string out;
        for (const cgarchi::GothicDiagnostic &d : r.entries)
            out += std::string(d.error ? "E " : "W ")
                 + (d.error ? cgarchi::GothicBuildErrorName(static_cast<cgarchi::GothicBuildError>(d.code))
                            : cgarchi::GothicBuildWarningName(static_cast<cgarchi::GothicBuildWarning>(d.code)))
                 + " @" + d.parameter + " : " + d.message + "\n";
        return out;
    }

    std::string describe(const cgarchi::microarch::MicroarchReport &r)
    {
        using namespace cgarchi::microarch;
        std::string out;
        for (const MicroarchDiagnostic &d : r.entries)
            out += std::string(d.error ? "E " : "W ")
                 + (d.error ? MicroarchErrorName(static_cast<MicroarchError>(d.code))
                            : MicroarchWarningName(static_cast<MicroarchWarning>(d.code)))
                 + " @" + d.path + " : " + d.message + "\n";
        return out;
    }

    std::string readFile(const std::string &path)
    {
        std::ifstream in(path, std::ios::binary);
        std::ostringstream os;
        os << in.rdbuf();
        return os.str();
    }

    // Sans '\r' : la copie de travail Windows peut porter des fins de ligne CRLF.
    std::string withoutCr(std::string s)
    {
        s.erase(std::remove(s.begin(), s.end(), '\r'), s.end());
        return s;
    }

    // Reference gelee, ecrite par les lecteurs d'avant leur factorisation. En
    // cas d'ecart, la sortie obtenue est ecrite dans tmp/<nom>.actual, relatif
    // au repertoire de travail (TU_RUN_DIR sous ctest), pour etre comparee.
    void expectFrozen(const std::string &name, const std::string &actual)
    {
        const std::string expected = withoutCr(readFile("./test/data/json_reader_before_e0/" + name));
        if (expected == actual)
            return;
        const std::filesystem::path dir = "tmp";
        std::filesystem::create_directories(dir);
        const std::filesystem::path out = dir / (name + ".actual");
        std::ofstream(out, std::ios::binary) << actual;
        ADD_FAILURE() << "test/data/json_reader_before_e0/" << name << " differe de la sortie ("
                      << (expected.empty() ? "reference absente" : "voir " + out.generic_string()) << ")";
    }

    std::string loadGothic(const std::string &text)
    {
        cgarchi::ParameterizedGothicWindow shape;
        cgarchi::GothicBuildReport report;
        shape.LoadFromJson(text, &report);
        return describe(report);
    }

    std::string minimalV2(const std::string &extra = std::string())
    {
        return R"({"geometry":{"width":200,"excess":1,"bdOuter":16,"bdInner":10})"
             + (extra.empty() ? std::string() : "," + extra) + "}";
    }

    std::string loadMicro(const std::string &text)
    {
        cgarchi::microarch::MicroarchSpec spec;
        cgarchi::microarch::MicroarchReport report;
        cgarchi::microarch::LoadSpecFromJson(text, spec, report);
        return describe(report);
    }

    json microBase()
    {
        return json::parse(R"({
          "schema": "gothic-microarchitecture/v1",
          "symmetry": { "center": [0, 0], "rotation_order": 4, "reflection_axis_deg": 90 },
          "points": [
            { "id": "A", "xy": [4, 1], "ground": true },
            { "id": "B", "xy": [1, 5], "height": 10 } ],
          "curves": [ { "id": "l1", "type": "segment", "start": "A", "end": "B" } ] })");
    }

    struct Case
    {
        const char *name;
        std::string text;
        std::string expected;
    };
}

// ---------------------------------------------------------------------------
//  Fenetre gothique (ParameterizedGothicWindow::LoadFromJson)
// ---------------------------------------------------------------------------

TEST(TEST_cgarchi_json_reader_characterization, gothic_window_messages)
{
    const std::vector<Case> cases = {
        {"document valide, schema absent", minimalV2(), ""},
        {"schema attendu", R"({"schema":"gothic-window/v2",)" + minimalV2().substr(1), ""},
        {"champ inconnu a la racine", minimalV2(R"("bogus":1)"), "E json_schema @bogus : unknown field\n"},
        {"champ inconnu imbrique", minimalV2(R"("recursion":{"depth":1,"bogus":2})"),
         "E json_schema @recursion.bogus : unknown field\n"},
        {"seuil inconnu", minimalV2(R"("thresholds":{"bogus":1})"),
         "E json_schema @thresholds.bogus : unknown field\n"},
        {"nombre attendu", R"({"geometry":{"width":200,"excess":"x","bdOuter":16,"bdInner":10}})",
         "E json_type @geometry.excess : expected a number, not string\n"},
        {"objet attendu", minimalV2(R"("style":3)"), "E json_type @style : expected an object, not number\n"},
        {"entier attendu", minimalV2(R"("recursion":{"depth":1.5})"),
         "E json_type @recursion.depth : expected an integer, not number\n"},
        {"booleen attendu", minimalV2(R"("recursion":{"harmonic":1})"),
         "E json_type @recursion.harmonic : expected a boolean, not number\n"},
        {"borne entiere", minimalV2(R"("recursion":{"depth":7})"),
         "E json_value @recursion.depth : 7 outside [0, 2]\n"},
        {"borne reelle", minimalV2(R"("thresholds":{"minRingWidth":-1})"),
         "E json_value @thresholds.minRingWidth : -1 outside [0, inf]\n"},
        {"borne float ouverte", R"({"geometry":{"width":-3,"excess":1,"bdOuter":16,"bdInner":10}})",
         "E json_value @geometry.width : -3 outside (0, inf]\n"},
        {"borne float apres arrondi", R"({"geometry":{"width":200,"excess":0.5000000001,"bdOuter":16,"bdInner":10}})",
         "E json_value @geometry.excess : 0.5000000001 (0.5 as a float) outside (0.5, inf]\n"},
        {"debordement float", R"({"geometry":{"width":1e39,"excess":1,"bdOuter":16,"bdInner":10}})",
         "E json_value @geometry.width : 1e+39 is not finite as a float\n"},
        {"enumeration", minimalV2(R"("style":{"mainArch":{"profile":{"type":"cham"}}})"),
         "E json_value @style.mainArch.profile.type : unknown value \"cham\" "
         "(expected one of: flat, chamfer, roll, keel, ogee)\n"},
        {"champ requis", R"({"geometry":{"width":200,"excess":1,"bdOuter":16}})",
         "E json_missing @geometry.bdInner : required field is missing\n"},
        {"schema inconnu", R"({"schema":"other/v1",)" + minimalV2().substr(1),
         "E json_schema @schema : unsupported schema \"other/v1\" (expected gothic-window/v2)\n"},
        {"version future", R"({"schema":"gothic-window/v3",)" + minimalV2().substr(1),
         "E json_schema @schema : unsupported schema \"gothic-window/v3\" (expected gothic-window/v2)\n"},
        {"v1 par le champ schema (non migree)", R"({"schema":"gothic-window/v1",)" + minimalV2().substr(1),
         "E json_schema @schema : unsupported schema \"gothic-window/v1\" (expected gothic-window/v2)\n"},
        {"schema inconnu et champ inconnu", R"({"schema":"other/v1","bogus":1,)" + minimalV2().substr(1),
         "E json_schema @schema : unsupported schema \"other/v1\" (expected gothic-window/v2)\n"},
        {"schema non chaine", R"({"schema":2,)" + minimalV2().substr(1),
         "E json_type @schema : expected a string, not number\n"},
        {"nombre hors des doubles", R"({"geometry":{"width":1e400,"excess":1,"bdOuter":16,"bdInner":10}})",
         "E json_value @ : [json.exception.out_of_range.406] number overflow parsing '1e400'\n"},
        {"racine tableau", "[1,2]", "E json_type @ : the document root must be an object, not array\n"},
        {"illisible", "{",
         "E json_parse @ : [json.exception.parse_error.101] parse error at line 1, column 2: syntax error "
         "while parsing object key - unexpected end of input; expected string literal\n"},
        {"cle en double refusee", R"({"geometry":{"width":-1,"width":200,"excess":1,"bdOuter":16,"bdInner":10}})",
         "E json_schema @geometry.width : duplicate key\n"},
    };
    for (const Case &c : cases)
    {
        SCOPED_TRACE(c.name);
        EXPECT_EQ(loadGothic(c.text), c.expected) << c.text;
    }
}

// Valeur non finie : atteignable seulement par la conversion v1 (largeur
// pR - pL qui deborde les doubles).
TEST(TEST_cgarchi_json_reader_characterization, gothic_window_non_finite_value)
{
    json j = json::parse(readFile("./test/data/gothic-window-instance.json"));
    ASSERT_TRUE(j.is_object());
    j["window"]["arch"].erase("width");
    j["window"]["basis"]["pL"]["x"] = -1e308;
    j["window"]["basis"]["pR"]["x"] = 1e308;
    const std::string out = loadGothic(j.dump());
    EXPECT_NE(out.find("E json_value @geometry.width : non-finite value\n"), std::string::npos) << out;
}

// Migration v1 -> v2 : un champ v1 inconnu est nomme par son chemin v1.
TEST(TEST_cgarchi_json_reader_characterization, gothic_window_v1_unknown_field)
{
    json j = json::parse(readFile("./test/data/gothic-window-instance.json"));
    ASSERT_TRUE(j.is_object());
    j["bogus"] = 1;
    j["window"]["bogus"] = 2;
    const std::string out = loadGothic(j.dump());
    EXPECT_NE(out.find("E json_schema @bogus : unknown field\n"), std::string::npos) << out;
    EXPECT_NE(out.find("E json_schema @window.bogus : unknown field\n"), std::string::npos) << out;
}

// La fixture v1 : rapport de conversion et document v2 exporte, a l'identique.
TEST(TEST_cgarchi_json_reader_characterization, gothic_window_fixture_reads_back_identically)
{
    const std::string text = readFile("./test/data/gothic-window-instance.json");
    ASSERT_FALSE(text.empty());
    cgarchi::ParameterizedGothicWindow shape;
    cgarchi::GothicBuildReport report;
    const bool accepted = shape.LoadFromJson(text, &report);
    expectFrozen("gothic-window-instance.txt",
                 std::string(accepted ? "accepted\n" : "refused\n") + describe(report) + "---\n" + shape.ExportJson()
                     + "\n");
}

// ---------------------------------------------------------------------------
//  Microarchitecture (cgarchi::microarch::LoadSpecFromJson)
// ---------------------------------------------------------------------------

TEST(TEST_cgarchi_json_reader_characterization, microarchitecture_messages)
{
    auto with = [](const std::function<void(json &)> &edit) {
        json j = microBase();
        edit(j);
        return j.dump();
    };
    const std::vector<Case> cases = {
        {"document valide", microBase().dump(), ""},
        {"champ inconnu a la racine", with([](json &j) { j["bogus"] = 1; }),
         "E json_schema @bogus : unknown field\n"},
        {"champ inconnu imbrique", with([](json &j) { j["points"][0]["bogus"] = 1; }),
         "E json_schema @points[0].bogus : unknown field\n"},
        {"entier attendu", with([](json &j) { j["symmetry"]["rotation_order"] = "4"; }),
         "E json_type @symmetry.rotation_order : expected an integer, not string\n"},
        {"objet attendu", with([](json &j) { j["symmetry"] = 3; }),
         "E json_type @symmetry : expected an object, not number\n"},
        {"booleen attendu", with([](json &j) { j["points"][0]["ground"] = 1; }),
         "E json_type @points[0].ground : expected a boolean, not number\n"},
        {"borne entiere", with([](json &j) { j["symmetry"]["rotation_order"] = 0; }),
         "E json_value @symmetry.rotation_order : 0 outside [1, 64]\n"},
        {"borne reelle exclue", with([](json &j) { j["symmetry"]["reflection_axis_deg"] = 180; }),
         "E json_value @symmetry.reflection_axis_deg : 180 outside [0, 180)\n"},
        {"borne basse exclue", with([](json &j) { j["tolerances"] = json{{"point", 0}}; }),
         "E json_value @tolerances.point : 0 outside (0, 0.001]\n"},
        {"17 chiffres significatifs", with([](json &j) { j["tolerances"] = json{{"point", 0.0011}}; }),
         "E json_value @tolerances.point : 0.0011000000000000001 outside (0, 0.001]\n"},
        {"enumeration", with([](json &j) { j["curves"][0]["type"] = "spline"; }),
         "E json_value @curves[0].type : unknown value \"spline\" (expected one of: segment, arc)\n"},
        {"enumeration non chaine", with([](json &j) { j["curves"][0]["type"] = 3; }),
         "E json_type @curves[0].type : expected a string, not number\n"},
        {"champ requis", with([](json &j) { j.erase("symmetry"); }),
         "E json_missing @symmetry : required field is missing\n"},
        {"point attendu", with([](json &j) { j["symmetry"]["center"] = json::array({0}); }),
         "E json_type @symmetry.center : expected an array of 2 numbers\n"},
        {"tableau vide", with([](json &j) { j["points"] = json::array(); }),
         "E json_value @points : the array must not be empty\n"},
        {"schema absent", with([](json &j) { j.erase("schema"); }),
         "E json_missing @schema : required field is missing\n"},
        {"schema inconnu", with([](json &j) { j["schema"] = "other/v1"; }),
         "E json_schema @schema : unsupported schema \"other/v1\" (expected gothic-microarchitecture/v1)\n"},
        {"version future", with([](json &j) { j["schema"] = "gothic-microarchitecture/v2"; }),
         "E json_schema @schema : unsupported schema \"gothic-microarchitecture/v2\" "
         "(expected gothic-microarchitecture/v1)\n"},
        {"schema inconnu et champ inconnu", with([](json &j) { j["schema"] = "other/v1"; j["bogus"] = 1; }),
         "E json_schema @bogus : unknown field\n"
         "E json_schema @schema : unsupported schema \"other/v1\" (expected gothic-microarchitecture/v1)\n"},
        {"schema non chaine", with([](json &j) { j["schema"] = 1; }),
         "E json_type @schema : expected a string, not number\n"},
        {"nombre hors des doubles", R"({"schema":"gothic-microarchitecture/v1","x":1e400})",
         "E json_parse @ : [json.exception.out_of_range.406] number overflow parsing '1e400'\n"},
        {"racine tableau", "[1,2]", "E json_type @ : the document root must be an object, not array\n"},
        {"illisible", "{",
         "E json_parse @ : [json.exception.parse_error.101] parse error at line 1, column 2: syntax error "
         "while parsing object key - unexpected end of input; expected string literal\n"},
        {"cle en double refusee : schema trois fois, deux doublons",
         R"({"schema":"gothic-microarchitecture/v1","schema":"gothic-microarchitecture/v1",)" + microBase().dump().substr(1),
         "E json_schema @schema : duplicate key\n"
         "E json_schema @schema : duplicate key\n"},
    };
    for (const Case &c : cases)
    {
        SCOPED_TRACE(c.name);
        EXPECT_EQ(loadMicro(c.text), c.expected) << c.text;
    }
}

// Les deux documents de reference : rapport et document reecrit, a l'identique.
TEST(TEST_cgarchi_json_reader_characterization, microarchitecture_fixtures_read_back_identically)
{
    for (const char *name : {"gothic_micro_d4_segment", "gothic_micro_c4_pinwheel"})
    {
        SCOPED_TRACE(name);
        cgarchi::microarch::MicroarchSpec spec;
        cgarchi::microarch::MicroarchReport report;
        const bool accepted =
            cgarchi::microarch::LoadSpecFromFile(std::string("./test/data/") + name + ".json", spec, report);
        expectFrozen(std::string(name) + ".txt", std::string(accepted ? "accepted\n" : "refused\n") + describe(report)
                                                     + "---\n" + cgarchi::microarch::SpecToJson(spec) + "\n");
    }
}

// ---------------------------------------------------------------------------
//  Cles en double, fenetre gothique et microarchitecture : json_schema
//  « duplicate key » au chemin du doublon, puis la lecture continue sur la
//  derniere valeur ; un texte illisible apres un doublon donne les deux erreurs.
// ---------------------------------------------------------------------------

namespace
{
    std::string replaceOnce(std::string text, const std::string &from, const std::string &to)
    {
        const std::size_t at = text.find(from);
        if (at == std::string::npos || text.find(from, at + 1) != std::string::npos)
        {
            ADD_FAILURE() << "\"" << from << "\" doit apparaitre une seule fois";
            return text;
        }
        return text.replace(at, from.size(), to);
    }
}

TEST(TEST_cgarchi_json_reader_duplicate_keys, gothic_window_reports_the_duplicate_and_the_independent_errors)
{
    const std::vector<Case> cases = {
        {"doublon puis champ inconnu",
         R"({"geometry":{"width":200,"width":200,"excess":1,"bdOuter":16,"bdInner":10},"bogus":1})",
         "E json_schema @geometry.width : duplicate key\n"
         "E json_schema @bogus : unknown field\n"},
        {"la derniere valeur est lue", R"({"geometry":{"width":200,"width":-1,"excess":1,"bdOuter":16,"bdInner":10}})",
         "E json_schema @geometry.width : duplicate key\n"
         "E json_value @geometry.width : -1 outside (0, inf]\n"},
        {"doublon puis texte tronque", R"({"geometry":{"width":200,"width":200)",
         "E json_schema @geometry.width : duplicate key\n"
         "E json_parse @ : [json.exception.parse_error.101] parse error at line 1, column 37: syntax error "
         "while parsing object - unexpected end of input; expected '}'\n"},
    };
    for (const Case &c : cases)
    {
        SCOPED_TRACE(c.name);
        cgarchi::ParameterizedGothicWindow shape;
        cgarchi::GothicBuildReport report;
        EXPECT_FALSE(shape.LoadFromJson(c.text, &report)) << c.text;
        EXPECT_EQ(describe(report), c.expected) << c.text;
    }
}

// Document v1 : le doublon est nomme par son chemin v1, et la lecture v2 du
// document converti n'a pas lieu. La derniere valeur, 1e39, passe le controle
// v1 de `window.arch.width` mais pas la lecture float de v2 : seule, elle y est
// refusee (temoin) ; apres un doublon, le doublon est la seule erreur.
TEST(TEST_cgarchi_json_reader_duplicate_keys, gothic_window_v1_duplicate_is_named_by_its_v1_path)
{
    const auto errorsOf = [](const std::string &text) {
        cgarchi::ParameterizedGothicWindow shape;
        cgarchi::GothicBuildReport report;
        EXPECT_FALSE(shape.LoadFromJson(text, &report));
        std::string errors;
        for (const cgarchi::GothicDiagnostic &d : report.entries)
            if (d.error)
                errors += std::string(cgarchi::GothicBuildErrorName(static_cast<cgarchi::GothicBuildError>(d.code)))
                        + " @" + d.parameter + " : " + d.message + "\n";
        return errors;
    };
    const std::string fixture = readFile("./test/data/gothic-window-instance.json");
    ASSERT_FALSE(fixture.empty());

    const std::string witness = errorsOf(replaceOnce(fixture, R"("width": 200,)", R"("width": 1e39,)"));
    EXPECT_NE(witness.find("json_value @geometry.width : 1e+39 is not finite as a float\n"), std::string::npos)
        << witness;

    EXPECT_EQ(errorsOf(replaceOnce(fixture, R"("width": 200,)", R"("width": 200, "width": 1e39,)")),
              "json_schema @window.arch.width : duplicate key\n");
}

TEST(TEST_cgarchi_json_reader_duplicate_keys, microarchitecture_reports_the_duplicate_and_the_independent_errors)
{
    json base = microBase();
    base["points"][0]["bogus"] = 1;
    const std::vector<Case> cases = {
        {"doublon, derniere valeur lue, puis champ inconnu",
         replaceOnce(base.dump(), R"("rotation_order":4)", R"("rotation_order":4,"rotation_order":0)"),
         "E json_schema @symmetry.rotation_order : duplicate key\n"
         "E json_value @symmetry.rotation_order : 0 outside [1, 64]\n"
         "E json_schema @points[0].bogus : unknown field\n"},
        {"doublon dans un element de tableau",
         replaceOnce(microBase().dump(), R"("height":10)", R"("height":10,"height":10)"),
         "E json_schema @points[1].height : duplicate key\n"},
        {"id de point repete : la derniere valeur est lue, la reference cascade",
         replaceOnce(microBase().dump(), R"("id":"B")", R"("id":"B","id":"Z")"),
         "E json_schema @points[1].id : duplicate key\n"
         "E unknown_reference @curves[0].end : unknown point \"B\"\n"
         "W field_ignored @points[1] : point \"Z\" is not referenced by any curve\n"},
        {"doublon puis texte tronque", R"({"schema":1,"schema":1)",
         "E json_schema @schema : duplicate key\n"
         "E json_parse @ : [json.exception.parse_error.101] parse error at line 1, column 23: syntax error "
         "while parsing object - unexpected end of input; expected '}'\n"},
    };
    for (const Case &c : cases)
    {
        SCOPED_TRACE(c.name);
        cgarchi::microarch::MicroarchSpec spec;
        cgarchi::microarch::MicroarchReport report;
        EXPECT_FALSE(cgarchi::microarch::LoadSpecFromJson(c.text, spec, report)) << c.text;
        EXPECT_EQ(describe(report), c.expected) << c.text;
    }
}

// ---------------------------------------------------------------------------
//  Lecteur commun (cgarchi::json_reader)
// ---------------------------------------------------------------------------

namespace
{
    using namespace cgarchi::json_reader;

    // Refus consignes « <classe> @<chemin> : <message> ».
    class Collect final : public StrictReader
    {
    public:
        explicit Collect(int precision = 6) : StrictReader(precision) {}
        std::string log;

    private:
        void OnFault(JsonFault kind, const std::string &path, const std::string &message) override
        {
            const char *const names[] = {"schema", "type", "value", "missing"};
            log += std::string(names[static_cast<int>(kind)]) + " @" + path + " : " + message + "\n";
        }
    };
}

TEST(TEST_cgarchi_json_reader, paths_name_members_and_items)
{
    EXPECT_EQ(Join("", "geometry"), "geometry");
    EXPECT_EQ(Join("geometry", "width"), "geometry.width");
    EXPECT_EQ(Join(Indexed("points", 3), "id"), "points[3].id");
}

TEST(TEST_cgarchi_json_reader, unknown_fields_are_refused_by_their_path)
{
    Collect r;
    const json doc = json::parse(R"({"a":1,"b":{"c":2,"d":3},"e":4})");
    r.Known(doc, "", {"a", "b"});
    r.Known(doc["b"], "b", {"c"});
    EXPECT_EQ(r.log, "schema @e : unknown field\nschema @b.d : unknown field\n");
}

// ParseRejectingDuplicateKeys : chaque cle repetee est nommee par son chemin,
// dans les objets imbriques et les elements de tableau ; la derniere valeur est
// gardee. Les memes cles dans deux objets distincts ne sont pas des doublons.
TEST(TEST_cgarchi_json_reader, duplicate_keys_are_refused_by_their_path)
{
    Collect r;
    const json doc = r.ParseRejectingDuplicateKeys(
        R"({"a":1,"a":2,"b":{"c":1,"c":2},"d":[{"e":1},[3],{"e":1,"e":2}],"f":{"e":1},"g":{"e":1}})");
    EXPECT_EQ(r.log, "schema @a : duplicate key\nschema @b.c : duplicate key\nschema @d[2].e : duplicate key\n");
    EXPECT_EQ(doc["a"], 2);
    EXPECT_EQ(doc["d"][2]["e"], 2);

    Collect clean;
    clean.ParseRejectingDuplicateKeys(R"({"a":{"x":1},"b":{"x":1},"c":[{"x":1},{"x":1}]})");
    EXPECT_EQ(clean.log, "");
}

TEST(TEST_cgarchi_json_reader, required_fields_are_all_reported)
{
    Collect r;
    const json doc = json::parse(R"({"a":1})");
    EXPECT_TRUE(r.Require(doc, "p", {"a"}));
    EXPECT_FALSE(r.Require(doc, "p", {"a", "b", "c"}));
    EXPECT_EQ(r.log, "missing @p.b : required field is missing\nmissing @p.c : required field is missing\n");
}

TEST(TEST_cgarchi_json_reader, an_absent_field_is_neither_read_nor_reported)
{
    Collect r;
    const json doc = json::object();
    double d = 7.0;
    int i = 7;
    bool b = true;
    std::string s = "x";
    EXPECT_FALSE(r.Number(doc, "", "n", AnyNumber(), d));
    EXPECT_FALSE(r.Integer(doc, "", "n", 0, 9, i));
    EXPECT_FALSE(r.Boolean(doc, "", "n", b));
    EXPECT_FALSE(r.String(doc, "", "n", s));
    EXPECT_FALSE(r.Enum(doc, "", "n", {"x"}, i));
    EXPECT_EQ(r.Object(doc, "", "n"), nullptr);
    EXPECT_EQ(d, 7.0);
    EXPECT_EQ(i, 7);
    EXPECT_TRUE(b);
    EXPECT_EQ(s, "x");
    EXPECT_EQ(r.log, "");
}

TEST(TEST_cgarchi_json_reader, wrong_types_name_the_expected_type)
{
    Collect r;
    const json doc = json::parse(R"({"o":1,"n":"1","i":1.5,"b":0,"s":[]})");
    double d = 0.0;
    int i = 0;
    bool b = false;
    std::string s;
    EXPECT_EQ(r.Object(doc, "p", "o"), nullptr);
    EXPECT_FALSE(r.Number(doc, "p", "n", AnyNumber(), d));
    EXPECT_FALSE(r.Integer(doc, "p", "i", 0, 9, i));
    EXPECT_FALSE(r.Boolean(doc, "p", "b", b));
    EXPECT_FALSE(r.String(doc, "p", "s", s));
    EXPECT_EQ(r.log, "type @p.o : expected an object, not number\n"
                     "type @p.n : expected a number, not string\n"
                     "type @p.i : expected an integer, not number\n"
                     "type @p.b : expected a boolean, not number\n"
                     "type @p.s : expected a string, not array\n");
}

// Le parseur refuse les nombres non finis ; un document construit en memoire
// peut en porter.
TEST(TEST_cgarchi_json_reader, non_finite_numbers_are_refused)
{
    Collect r;
    json doc;
    doc["inf"] = std::numeric_limits<double>::infinity();
    doc["nan"] = std::numeric_limits<double>::quiet_NaN();
    double d = 1.0;
    int i = 1;
    EXPECT_FALSE(r.Number(doc, "", "inf", AnyNumber(), d));
    EXPECT_FALSE(r.Finite(doc, "", "nan", d));
    EXPECT_FALSE(r.Integer(doc, "", "inf", 0, 9, i));
    EXPECT_EQ(d, 1.0);
    EXPECT_EQ(r.log, "value @inf : non-finite value\nvalue @nan : non-finite value\n"
                     "type @inf : expected an integer, not number\n");
}

TEST(TEST_cgarchi_json_reader, bounds_follow_json_schema)
{
    Collect r;
    const json doc = json::parse(R"({"zero":0,"one":1,"two":2.0})");
    double d = 0.0;
    int i = 0;
    EXPECT_TRUE(r.Number(doc, "", "zero", AtLeast(0.0), d));
    EXPECT_FALSE(r.Number(doc, "", "zero", Above(0.0), d));
    EXPECT_TRUE(r.Number(doc, "", "one", Interval(0.0, true, 1.0, false), d));
    EXPECT_FALSE(r.Number(doc, "", "one", Interval(0.0, false, 1.0, true), d));
    EXPECT_TRUE(r.Integer(doc, "", "two", 2, 2, i));
    EXPECT_EQ(i, 2);
    EXPECT_FALSE(r.Integer(doc, "", "two", 3, 4, i));
    EXPECT_EQ(r.log, "value @zero : 0 outside (0, inf]\nvalue @one : 1 outside [0, 1)\n"
                     "value @two : 2 outside [3, 4]\n");
}

// Les deux lecteurs concrets citent les nombres avec une precision differente
// (6 chiffres pour la fenetre, 17 pour la microarchitecture).
TEST(TEST_cgarchi_json_reader, the_precision_of_the_messages_is_a_parameter)
{
    const json doc = json{{"x", 0.1 + 0.2}};
    double d = 0.0;
    Collect six(6), seventeen(17);
    EXPECT_FALSE(six.Number(doc, "", "x", Interval(0.0, false, 0.3, false), d));
    EXPECT_FALSE(seventeen.Number(doc, "", "x", Interval(0.0, false, 0.3, false), d));
    EXPECT_EQ(six.log, "value @x : 0.3 outside [0, 0.3]\n");
    EXPECT_EQ(seventeen.log, "value @x : 0.30000000000000004 outside [0, 0.29999999999999999]\n");
}

TEST(TEST_cgarchi_json_reader, enumerations_give_the_index_or_list_the_values)
{
    Collect r;
    const json doc = json::parse(R"({"a":"arc","b":"Arc"})");
    int index = -1;
    EXPECT_TRUE(r.Enum(doc, "c", "a", {"segment", "arc"}, index));
    EXPECT_EQ(index, 1);
    EXPECT_FALSE(r.Enum(doc, "c", "b", {"segment", "arc"}, index));
    EXPECT_EQ(index, 1);
    EXPECT_EQ(r.log, "value @c.b : unknown value \"Arc\" (expected one of: segment, arc)\n");
}

// Toutes les valeurs de `schema` du depot sont de la forme « nom/vN ».
TEST(TEST_cgarchi_json_reader, every_schema_of_the_repository_is_name_slash_vN)
{
    struct Tag { std::string text; const char *name; int version; };
    const Tag tags[] = {
        {json::parse(cgarchi::ParameterizedGothicWindow().ExportJson())["schema"].get<std::string>(), "gothic-window", 2},
        {cgarchi::microarch::kMicroarchSchemaV1, "gothic-microarchitecture", 1},
        {cgarchi::microarch::kMicroarchNetworkSchemaV1, "gothic-microarchitecture-network", 1},
        {kPrintConstraintsSchema, "print-constraints", 1},
        {kPrintReportSchema, "print-report", 1},
    };
    for (const Tag &t : tags)
    {
        SCOPED_TRACE(t.text);
        const std::optional<SchemaId> id = ParseSchemaId(t.text);
        ASSERT_TRUE(id.has_value());
        EXPECT_EQ(id->name, t.name);
        EXPECT_EQ(id->version, t.version);
    }
    const std::optional<SchemaId> large = ParseSchemaId("x/v2147483647");
    ASSERT_TRUE(large.has_value());
    EXPECT_EQ(large->version, std::numeric_limits<int>::max());
}

TEST(TEST_cgarchi_json_reader, malformed_schema_ids_are_not_parsed)
{
    for (const char *text : {"", "gothic-window", "/v1", "gothic-window/", "gothic-window/v", "gothic-window/v0",
                             "gothic-window/v01", "gothic-window/2", "gothic-window/V2", "gothic-window/v2x",
                             "gothic-window/v-2", "a/b/v1", "x/v2147483648"})
        EXPECT_FALSE(ParseSchemaId(text).has_value()) << "\"" << text << "\"";
}

TEST(TEST_cgarchi_json_reader, the_schema_field_is_read_and_its_version_extracted)
{
    {
        Collect r;
        const SchemaRead s = ReadSchema(r, json::object(), "doc/v2");
        EXPECT_EQ(s.status, SchemaStatus::Absent);
        EXPECT_FALSE(s.id.has_value());
        EXPECT_EQ(r.log, "");
    }
    {
        Collect r;
        const SchemaRead s = ReadSchema(r, json{{"schema", 2}}, "doc/v2");
        EXPECT_EQ(s.status, SchemaStatus::NotAString);
        EXPECT_EQ(r.log, "type @schema : expected a string, not number\n");
    }
    {
        Collect r;
        const SchemaRead s = ReadSchema(r, json{{"schema", "doc/v2"}}, "doc/v2");
        EXPECT_EQ(s.status, SchemaStatus::Expected);
        ASSERT_TRUE(s.id.has_value());
        EXPECT_EQ(s.id->version, 2);
        EXPECT_EQ(r.log, "");
    }
    {
        Collect r;
        const SchemaRead s = ReadSchema(r, json{{"schema", "doc/v3"}}, "doc/v2");
        EXPECT_EQ(s.status, SchemaStatus::Unsupported);
        ASSERT_TRUE(s.id.has_value());
        EXPECT_EQ(s.id->name, "doc");
        EXPECT_EQ(s.id->version, 3);
        EXPECT_EQ(r.log, "schema @schema : unsupported schema \"doc/v3\" (expected doc/v2)\n");
    }
    {
        Collect r;
        const SchemaRead s = ReadSchema(r, json{{"schema", "doc"}}, "doc/v2");
        EXPECT_EQ(s.status, SchemaStatus::Unsupported);
        EXPECT_FALSE(s.id.has_value());
        EXPECT_EQ(r.log, "schema @schema : unsupported schema \"doc\" (expected doc/v2)\n");
    }
}

// Les nombres cites gardent le point decimal sous une locale globale a virgule,
// dans le lecteur commun comme dans la lecture float de la fenetre.
TEST(TEST_cgarchi_json_reader, messages_ignore_the_global_locale)
{
    struct CommaPunct : std::numpunct<char>
    {
        char do_decimal_point() const override { return ','; }
        char do_thousands_sep() const override { return '.'; }
        std::string do_grouping() const override { return "\3"; }
    };
    const std::locale previous = std::locale::global(std::locale(std::locale::classic(), new CommaPunct));
    Collect r(6);
    double d = 0.0;
    r.Number(json{{"x", 1.5}}, "", "x", Interval(0.0, false, 0.25, false), d);
    const std::string window = loadGothic(R"({"geometry":{"width":200,"excess":0.5000000001,"bdOuter":16,"bdInner":10}})");
    std::locale::global(previous);
    EXPECT_EQ(r.log, "value @x : 1.5 outside [0, 0.25]\n");
    EXPECT_EQ(window, "E json_value @geometry.excess : 0.5000000001 (0.5 as a float) outside (0.5, inf]\n");
}

// ---------------------------------------------------------------------------
//  Rejet des cles en double : chemins, plafond, cout
// ---------------------------------------------------------------------------

// Cles vides : le chemin suit Join et Indexed, qui n'ajoutent pas de point
// derriere un chemin vide.
TEST(TEST_cgarchi_json_reader, duplicate_key_paths_with_empty_keys_follow_join_and_indexed)
{
    Collect r;
    r.ParseRejectingDuplicateKeys(R"({"a":{"":1,"":2},"b":{"":{"c":1,"c":2}},"":{"d":1,"d":2},)"
                                  R"("e":[{"":[{"f":1,"f":2}]}],"":[{"g":1,"g":2}]})");
    const std::string expected =
        "schema @" + Join("a", "") + " : duplicate key\n"
        + "schema @" + Join(Join("b", ""), "c") + " : duplicate key\n"
        + "schema @" + Join(Join("", ""), "d") + " : duplicate key\n"
        + "schema @" + Join(Indexed(Join(Indexed("e", 0), ""), 0), "f") + " : duplicate key\n"
        + "schema @" + Join("", "") + " : duplicate key\n"
        + "schema @" + Join(Indexed(Join("", ""), 0), "g") + " : duplicate key\n";
    EXPECT_EQ(r.log, expected);
    EXPECT_EQ(r.log, "schema @a. : duplicate key\nschema @b..c : duplicate key\nschema @d : duplicate key\n"
                     "schema @e[0].[0].f : duplicate key\nschema @ : duplicate key\nschema @[0].g : duplicate key\n");
}

namespace
{
    // n objets imbriques, chacun avec la cle `a` en double : {"a":0,"a":{...}}.
    std::string deepDuplicates(std::size_t n)
    {
        std::string t;
        for (std::size_t i = 1; i < n; ++i)
            t += R"({"a":0,"a":)";
        t += R"({"a":0,"a":0})";
        return t + std::string(n - 1, '}');
    }

    // Tableau de n objets vides.
    std::string emptyObjects(std::size_t n)
    {
        std::string t = "[";
        for (std::size_t i = 0; i < n; ++i)
            t += i ? ",{}" : "{}";
        return t + "]";
    }

    // Objet de n membres distincts.
    std::string wideObject(std::size_t n)
    {
        std::string t = "{";
        for (std::size_t i = 0; i < n; ++i)
            t += (i ? ",\"k" : "\"k") + std::to_string(i) + "\":0";
        return t + "}";
    }
}

// Au-dela de kDuplicateKeyPathLimit doublons, le chemin n'est plus transmis ;
// le rapport, plafonne au meme nombre d'entrees, n'en perd aucune, et ses
// compteurs comptent chaque doublon.
TEST(TEST_cgarchi_json_reader, duplicates_beyond_the_path_limit_are_counted_without_path)
{
    const std::size_t n = kDuplicateKeyPathLimit + 6;
    Collect r;
    r.ParseRejectingDuplicateKeys(deepDuplicates(n));
    std::string expected, path;
    for (std::size_t i = 0; i < n; ++i)
    {
        path = Join(path, "a");
        expected += "schema @" + (i < kDuplicateKeyPathLimit ? path : std::string()) + " : duplicate key\n";
    }
    EXPECT_EQ(r.log, expected);

    cgarchi::ParameterizedGothicWindow shape;
    cgarchi::GothicBuildReport report;
    EXPECT_FALSE(shape.LoadFromJson(minimalV2(R"("bogus":)" + deepDuplicates(n)), &report));
    EXPECT_EQ(report.errorCounts[static_cast<std::size_t>(cgarchi::GothicBuildError::JsonSchema)], n + 1);
    ASSERT_EQ(report.entries.size(), cgarchi::kGothicBuildReportMaxEntries);
    path = "bogus";
    for (std::size_t i = 0; i < report.entries.size(); ++i)
    {
        path = Join(path, "a");
        EXPECT_EQ(report.entries[i].parameter, path) << i;
        EXPECT_EQ(report.entries[i].message, "duplicate key") << i;
    }
}

namespace
{
    // Refus comptes, sans chemin garde.
    class CountFaults final : public StrictReader
    {
    public:
        CountFaults() : StrictReader(6) {}
        std::size_t faults = 0;

    private:
        void OnFault(JsonFault, const std::string &, const std::string &) override { ++faults; }
    };

    double elapsedMs(const std::function<void()> &f)
    {
        const auto t0 = std::chrono::steady_clock::now();
        f();
        return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    }

    // Croissance du cout de `load` quand le texte passe de n a 4n : un cout
    // lineaire donne un rapport proche de 4, un cout quadratique de 16 ; le
    // seuil de 8 separe les deux sans dependre de la vitesse de la machine.
    // Les deux tailles sont mesurees en alternance, et le rapport est celui des
    // MINIMA : une preemption (ctest -j) ne fait qu'allonger une mesure.
    void expectLinearGrowth(const std::string &name, const std::function<void(const std::string &)> &load,
                            const std::string &small, const std::string &large)
    {
        std::vector<double> msSmall, msLarge;
        for (int k = 0; k < 7; ++k)
        {
            msSmall.push_back(elapsedMs([&] { load(small); }));
            msLarge.push_back(elapsedMs([&] { load(large); }));
        }
        const double minSmall = *std::min_element(msSmall.begin(), msSmall.end());
        const double minLarge = *std::min_element(msLarge.begin(), msLarge.end());
        const double growth = minLarge / minSmall;
        ::testing::Test::RecordProperty(name + "_ms_n", std::to_string(minSmall));
        ::testing::Test::RecordProperty(name + "_ms_4n", std::to_string(minLarge));
        ::testing::Test::RecordProperty(name + "_growth", std::to_string(growth));
#if defined(NDEBUG) && !defined(CG_SANITIZERS_ENABLED) && !defined(__SANITIZE_ADDRESS__)
        // Asserte sur build optimisee sans sanitizer seulement ; mesure partout.
        EXPECT_LT(growth, 8.0) << name << " : " << minSmall << " ms, puis " << minLarge << " ms pour 4 fois plus";
#else
        (void)growth;
#endif
    }
}

// Trois formes, de n = 10 000 puis 40 000 : tableau large, objet large, et
// imbrication profonde avec un doublon a chaque niveau. Trois lecteurs : le
// commun, la fenetre (la forme est un champ inconnu) et la microarchitecture
// (la forme est dans `meta`, de contenu libre).
TEST(TEST_cgarchi_json_reader, duplicate_key_rejection_cost_grows_linearly_with_the_text)
{
    struct Shape
    {
        const char *name;
        std::function<std::string(std::size_t)> make;
        std::size_t duplicates;   // pour n = 1 ; deepDuplicates en a n
    };
    const std::size_t n = 10000;
    const std::vector<Shape> shapes = {
        {"wide_array", emptyObjects, 0},
        {"wide_object", wideObject, 0},
        {"deep_duplicates", deepDuplicates, 1},
    };
    for (const Shape &shape : shapes)
    {
        SCOPED_TRACE(shape.name);
        const std::string small = shape.make(n), large = shape.make(4 * n);

        const auto common = [&](const std::string &t) {
            CountFaults r;
            const json doc = r.ParseRejectingDuplicateKeys(t);
            EXPECT_EQ(r.faults, (t.size() == small.size() ? n : 4 * n) * shape.duplicates);
        };
        expectLinearGrowth(std::string("strict_reader_") + shape.name, common, small, large);

        const auto window = [&](const std::string &t) {
            cgarchi::ParameterizedGothicWindow w;
            cgarchi::GothicBuildReport report;
            EXPECT_FALSE(w.LoadFromJson(t, &report));
        };
        expectLinearGrowth(std::string("gothic_window_") + shape.name, window,
                           minimalV2(R"("bogus":)" + small), minimalV2(R"("bogus":)" + large));

        const auto micro = [&](const std::string &t) {
            cgarchi::microarch::MicroarchSpec spec;
            cgarchi::microarch::MicroarchReport report;
            EXPECT_EQ(cgarchi::microarch::LoadSpecFromJson(t, spec, report), shape.duplicates == 0);
        };
        const std::string base = microBase().dump();
        expectLinearGrowth(std::string("microarchitecture_") + shape.name, micro,
                           R"({"meta":{"pad":)" + small + "}," + base.substr(1),
                           R"({"meta":{"pad":)" + large + "}," + base.substr(1));
    }
}
