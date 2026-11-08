#include <gtest/gtest.h>

#include <algorithm>
#include <charconv>
#include <clocale>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <functional>
#include <fstream>
#include <limits>
#include <locale>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include <cgarchi/architecture_gothic.h>
#include <cgarchi/classical_orders.h>
#include <cgarchi/parameterized_classical.h>
#include <cgarchi/parameterized_classical_json.h>
#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/parameterized.h"

using namespace cgarchi;   // fichier de test de cgarchi seul

// ===========================================================================
//  Colonne classique -- document "classical-column/v1"
// ===========================================================================
//
// Lecture stricte (LoadClassicalColumnFromJson) et ecriture canonique
// (ExportClassicalColumnJson). La fixture test/data/classical-column-instance.json
// est ecrite a la main ; elle ne se regenere jamais.

namespace
{
    using nlohmann::json;

    const char *const kInstancePath = "./test/data/classical-column-instance.json";
    const char *const kShortDecimalPath = "./test/data/classical-column-short-decimal.json";

    // Texte du fichier, sans les `\r` qu'ajoute core.autocrlf.
    std::string readText(const std::string &path)
    {
        std::ifstream in(path, std::ios::binary);
        std::ostringstream os;
        os << in.rdbuf();
        std::string text = os.str();
        text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
        return text;
    }

    // Fixture gelee : son texte, sans les `\r` ni une fin de ligne finale, que
    // l'ecriture canonique ne produit pas et qu'un editeur peut ajouter.
    std::string fixtureText(const std::string &path)
    {
        std::string text = readText(path);
        if (!text.empty() && text.back() == '\n')
            text.pop_back();
        return text;
    }

    // Double lu en locale classique.
    double classicDouble(const std::string &text)
    {
        std::istringstream is(text);
        is.imbue(std::locale::classic());
        double d = std::numeric_limits<double>::quiet_NaN();
        is >> d;
        return d;
    }

    void expectSameSettings(const ClassicalColumnSettings &a, const ClassicalColumnSettings &e)
    {
        EXPECT_EQ(a.order, e.order);
        EXPECT_EQ(std::memcmp(&a.module, &e.module, sizeof a.module), 0) << a.module << " contre " << e.module;
        EXPECT_EQ(a.pedestal, e.pedestal);
        EXPECT_EQ(a.entablature, e.entablature);
        EXPECT_EQ(a.doricCornice, e.doricCornice);
        EXPECT_EQ(a.envelope, e.envelope);
        EXPECT_EQ(a.volutes, e.volutes);
        EXPECT_EQ(a.flutes, e.flutes);
        EXPECT_EQ(a.doricFlutes, e.doricFlutes);
        EXPECT_EQ(a.radialSegments, e.radialSegments);
        EXPECT_EQ(a.curveSegments, e.curveSegments);
    }

    // Les memes sommets au bit pres et les memes faces.
    void expectSameMesh(const Mesh &a, const Mesh &b)
    {
        ASSERT_EQ(a.GetNVertices(), b.GetNVertices());
        ASSERT_EQ(a.GetNFaces(), b.GetNFaces());
        for (unsigned v = 0; v < a.GetNVertices(); ++v)
        {
            float pa[3], pb[3];
            a.GetVertex(v, pa);
            b.GetVertex(v, pb);
            ASSERT_EQ(std::memcmp(pa, pb, sizeof pa), 0) << "sommet " << v;
        }
        for (unsigned f = 0; f < a.GetNFaces(); ++f)
        {
            ASSERT_EQ(a.GetFaceNVertices(f), b.GetFaceNVertices(f)) << "face " << f;
            for (unsigned k = 0; k < static_cast<unsigned>(a.GetFaceNVertices(f)); ++k)
                ASSERT_EQ(a.GetFaceVertex(f, k), b.GetFaceVertex(f, k)) << "face " << f;
        }
    }

    // Reglages de la grille : les defauts, puis chaque reglage hors defaut, puis
    // deux combinaisons ; `order` est pose par l'appelant.
    std::vector<ClassicalColumnSettings> settingsGrid()
    {
        std::vector<ClassicalColumnSettings> grid;
        const ClassicalColumnSettings d;
        grid.push_back(d);
        for (const float module : {kClassicalModuleMin, 0.3f, 2.5f, 7.123457f, kClassicalModuleMax})
        {
            ClassicalColumnSettings s = d;
            s.module = module;
            grid.push_back(s);
        }
        auto with = [&](auto field, auto value) {
            ClassicalColumnSettings s = d;
            s.*field = value;
            grid.push_back(s);
        };
        with(&ClassicalColumnSettings::pedestal, true);
        with(&ClassicalColumnSettings::entablature, true);
        with(&ClassicalColumnSettings::doricCornice, 1);
        with(&ClassicalColumnSettings::envelope, true);
        with(&ClassicalColumnSettings::volutes, false);
        with(&ClassicalColumnSettings::flutes, true);
        with(&ClassicalColumnSettings::doricFlutes, 1);
        with(&ClassicalColumnSettings::radialSegments, kClassicalRadialSegmentsMin);
        with(&ClassicalColumnSettings::radialSegments, 64);
        with(&ClassicalColumnSettings::curveSegments, kClassicalCurveSegmentsMin);
        with(&ClassicalColumnSettings::curveSegments, 9);

        ClassicalColumnSettings all = d;
        all.module = 1.75f;
        all.pedestal = all.entablature = all.envelope = all.flutes = true;
        all.volutes = false;
        all.doricCornice = all.doricFlutes = 1;
        all.radialSegments = 40;
        all.curveSegments = 4;
        grid.push_back(all);

        ClassicalColumnSettings bounds = all;
        bounds.module = kClassicalModuleMax;
        bounds.radialSegments = kClassicalRadialSegmentsMax;
        bounds.curveSegments = kClassicalCurveSegmentsMax;
        bounds.flutes = false;
        grid.push_back(bounds);
        return grid;
    }

    json canonical()
    {
        return json::parse(fixtureText(kInstancePath));
    }

    struct Refusal
    {
        GothicBuildError code;
        std::string      path;
        std::string      message;
    };

    // Le document est refuse, avec ce seul diagnostic.
    void expectRefused(const std::string &doc, const Refusal &e)
    {
        GothicBuildReport report;
        const std::optional<ClassicalColumnSettings> s = LoadClassicalColumnFromJson(doc, &report);
        EXPECT_FALSE(s.has_value());
        ASSERT_EQ(report.entries.size(), 1u) << GothicBuildReportToJson(report);
        const GothicDiagnostic &d = report.entries.front();
        EXPECT_TRUE(d.error);
        EXPECT_STREQ(GothicBuildErrorName(static_cast<GothicBuildError>(d.code)), GothicBuildErrorName(e.code));
        EXPECT_EQ(d.parameter, e.path);
        EXPECT_EQ(d.message, e.message);
    }

    // Domaine d'un champ de schema JSON plat : type, const, enum, minimum, maximum.
    // `integer` au sens de JSON Schema : 2 et 2.0 le sont, 2.5 non.
    bool conforms(const json &value, const json &spec)
    {
        if (spec.contains("const") && value != spec["const"])
            return false;
        if (spec.contains("enum"))
        {
            bool found = false;
            for (const json &e : spec["enum"])
                found = found || (value == e);
            if (!found)
                return false;
        }
        if (spec.contains("type"))
        {
            const std::string type = spec["type"].get<std::string>();
            const bool typed = (type == "string" && value.is_string()) || (type == "boolean" && value.is_boolean())
                            || (type == "number" && value.is_number())
                            || (type == "integer" && value.is_number()
                                && std::isfinite(value.get<double>())
                                && std::floor(value.get<double>()) == value.get<double>());
            if (!typed)
                return false;
        }
        if (spec.contains("minimum") && value.get<double>() < spec["minimum"].get<double>())
            return false;
        if (spec.contains("maximum") && value.get<double>() > spec["maximum"].get<double>())
            return false;
        return true;
    }

    // Vide si `doc` respecte le schema plat `schema` ; sinon le premier ecart.
    std::string schemaViolation(const json &doc, const json &schema)
    {
        for (const json &name : schema["required"])
            if (!doc.contains(name.get<std::string>()))
                return "champ requis absent : " + name.get<std::string>();
        for (auto it = doc.begin(); it != doc.end(); ++it)
        {
            if (!schema["properties"].contains(it.key()))
                return "champ hors schema : " + it.key();
            if (!conforms(it.value(), schema["properties"][it.key()]))
                return "valeur hors schema : " + it.key() + " = " + it.value().dump();
        }
        return std::string();
    }

    json schemaFile()
    {
        const std::filesystem::path path = std::filesystem::path("src") / "cgarchi" / "schemas" / "classical-column-v1.schema";
        const std::string text = readText(path.string());
        if (text.empty())
            return json();
        return json::parse(text);
    }
}

// Pour chaque ordre et chaque reglage de la grille : les reglages relus sont
// les memes, champ par champ, et le maillage construit est le meme au bit pres.
TEST(TEST_cgarchi_classical_column_json, every_order_and_a_grid_of_settings_round_trip_without_loss)
{
    for (int o = 0; o < classical::kOrderCount; ++o)
    {
        int index = 0;
        for (ClassicalColumnSettings s : settingsGrid())
        {
            s.order = o;
            SCOPED_TRACE(std::string(classical::OrderKey(static_cast<classical::Order>(o))) + " #" + std::to_string(index++));
            const std::string text = ExportClassicalColumnJson(s);
            GothicBuildReport report;
            const std::optional<ClassicalColumnSettings> read = LoadClassicalColumnFromJson(text, &report);
            ASSERT_TRUE(read.has_value()) << GothicBuildReportToJson(report) << "\n" << text;
            expectSameSettings(*read, s);

            // Garde-fou de expectSameSettings : un reglage ajoute plus tard a
            // ClassicalColumnSettings, et oublie par la comparaison champ par
            // champ comme par le document, changerait le maillage.
            const std::unique_ptr<Mesh> expected = BuildClassicalColumnMesh(s);
            const std::unique_ptr<Mesh> actual = BuildClassicalColumnMesh(*read);
            ASSERT_TRUE(expected && actual);
            expectSameMesh(*actual, *expected);
        }
    }
}

// Tout float du domaine de `module` est ecrit par sa plus courte ecriture
// decimale et se relit a l'identique : les 4096 floats au-dessus de la borne
// basse, les 4096 au-dessous de la borne haute, et un balayage geometrique du
// domaine. Reference : std::to_chars sur le float, lue en double.
TEST(TEST_cgarchi_classical_column_json, every_module_float_is_written_by_its_shortest_decimal_and_reads_back_exactly)
{
    std::vector<float> modules;
    for (float f = kClassicalModuleMin; modules.size() < 4096; f = std::nextafter(f, kClassicalModuleMax))
        modules.push_back(f);
    for (float f = kClassicalModuleMax; modules.size() < 8192; f = std::nextafter(f, kClassicalModuleMin))
        modules.push_back(f);
    for (double f = kClassicalModuleMin; f <= kClassicalModuleMax; f *= 1.001)
        modules.push_back(static_cast<float>(f));

    std::size_t notShortest = 0, notExact = 0;
    std::string firstNotShortest;
    for (const float module : modules)
    {
        ClassicalColumnSettings s;
        s.module = module;
        const std::string text = ExportClassicalColumnJson(s);

        char shortest[64];
        const std::to_chars_result r = std::to_chars(shortest, shortest + sizeof shortest, module);
        ASSERT_EQ(r.ec, std::errc());
        const std::string reference(shortest, r.ptr);
        const json written = json::parse(text)["module"];
        if (written.get<double>() != classicDouble(reference) && notShortest++ == 0)
            firstNotShortest = written.dump() + " contre " + reference;

        const std::optional<ClassicalColumnSettings> read = LoadClassicalColumnFromJson(text);
        if (!read || std::memcmp(&read->module, &module, sizeof module) != 0)
            ++notExact;
    }
    EXPECT_EQ(notShortest, 0u) << "sur " << modules.size() << ", premier : " << firstNotShortest;
    EXPECT_EQ(notExact, 0u) << "sur " << modules.size();
}

// La fixture, ecrite a la main, est le document canonique de ses reglages.
TEST(TEST_cgarchi_classical_column_json, the_frozen_instance_is_rewritten_byte_for_byte)
{
    const std::string text = fixtureText(kInstancePath);
    ASSERT_FALSE(text.empty()) << kInstancePath << " introuvable";
    GothicBuildReport report;
    const std::optional<ClassicalColumnSettings> s = LoadClassicalColumnFromJson(text, &report);
    ASSERT_TRUE(s.has_value()) << GothicBuildReportToJson(report);

    ClassicalColumnSettings e;
    e.order = static_cast<int>(classical::Order::Ionic);
    e.module = 2.5f;
    e.pedestal = true;
    e.entablature = true;
    e.doricCornice = 1;
    e.envelope = false;
    e.volutes = true;
    e.flutes = true;
    e.doricFlutes = 1;
    e.radialSegments = 64;
    e.curveSegments = 8;
    expectSameSettings(*s, e);

    EXPECT_EQ(ExportClassicalColumnJson(*s), text);
}

// Seconde fixture, ecrite a la main : un module non representable en binaire
// (0,3) s'ecrit `0.3`, a l'octet.
TEST(TEST_cgarchi_classical_column_json, the_frozen_short_decimal_instance_is_rewritten_byte_for_byte)
{
    const std::string text = fixtureText(kShortDecimalPath);
    ASSERT_FALSE(text.empty()) << kShortDecimalPath << " introuvable";
    GothicBuildReport report;
    const std::optional<ClassicalColumnSettings> s = LoadClassicalColumnFromJson(text, &report);
    ASSERT_TRUE(s.has_value()) << GothicBuildReportToJson(report);

    ClassicalColumnSettings e;
    e.order = static_cast<int>(classical::Order::Tuscan);
    e.module = 0.3f;
    expectSameSettings(*s, e);

    EXPECT_EQ(ExportClassicalColumnJson(*s), text);
}

// Un refus par motif : code, chemin JSON et message exacts.
TEST(TEST_cgarchi_classical_column_json, each_refusal_names_its_code_path_and_message)
{
    const json base = canonical();
    auto with = [&](const char *key, const json &value) {
        json doc = base;
        doc[key] = value;
        return doc.dump();
    };
    auto without = [&](const char *key) {
        json doc = base;
        doc.erase(key);
        return doc.dump();
    };

    struct Case
    {
        const char *name;
        std::string doc;
        Refusal     refusal;
    };
    const std::vector<Case> cases = {
        {"champ inconnu", with("colour", "red"), {GothicBuildError::JsonSchema, "colour", "unknown field"}},
        {"nombre en texte", with("module", "2.5"), {GothicBuildError::JsonType, "module", "expected a number, not string"}},
        {"booleen en nombre", with("pedestal", 1), {GothicBuildError::JsonType, "pedestal", "expected a boolean, not number"}},
        {"entier fractionnaire", with("radialSegments", 64.5),
         {GothicBuildError::JsonType, "radialSegments", "expected an integer, not number"}},
        {"ordre par indice", with("order", 2), {GothicBuildError::JsonType, "order", "expected a string, not number"}},
        {"module sous la borne", with("module", 0.05), {GothicBuildError::JsonValue, "module", "0.05 (0.05 as a float) outside [0.1, 10]"}},
        {"module sur la borne", with("module", 10.5), {GothicBuildError::JsonValue, "module", "10.5 outside [0.1, 10]"}},
        {"segments radiaux", with("radialSegments", 7), {GothicBuildError::JsonValue, "radialSegments", "7 outside [8, 256]"}},
        {"segments de courbe", with("curveSegments", 33), {GothicBuildError::JsonValue, "curveSegments", "33 outside [2, 32]"}},
        {"non fini en float", with("module", 1e39), {GothicBuildError::JsonValue, "module", "1e+39 is not finite as a float"}},
        {"non fini en double", R"({"schema":"classical-column/v1","module":1e400})",
         {GothicBuildError::JsonValue, "", "[json.exception.out_of_range.406] number overflow parsing '1e400'"}},
        {"ordre inconnu", with("order", "ionic"),
         {GothicBuildError::JsonValue, "order",
          "unknown value \"ionic\" (expected one of: toscan, dorique, ionique, corinthien, composite)"}},
        {"corniche inconnue", with("doricCornice", "modillions"),
         {GothicBuildError::JsonValue, "doricCornice", "unknown value \"modillions\" (expected one of: dentils, mutules)"}},
        {"cannelure inconnue", with("doricFlutes", "A"),
         {GothicBuildError::JsonValue, "doricFlutes", "unknown value \"A\" (expected one of: segment60, semicircle)"}},
        {"reglage absent", without("volutes"), {GothicBuildError::JsonMissing, "volutes", "required field is missing"}},
        {"schema absent", without("schema"), {GothicBuildError::JsonMissing, "schema", "required field is missing"}},
        {"schema inconnu", with("schema", "gothic-window/v2"),
         {GothicBuildError::JsonSchema, "schema", "unsupported schema \"gothic-window/v2\" (expected classical-column/v1)"}},
        {"schema non texte", with("schema", 1), {GothicBuildError::JsonType, "schema", "expected a string, not number"}},
        {"cle en double",
         R"({"curveSegments":8,"doricCornice":"mutules","doricFlutes":"semicircle","entablature":true,"envelope":false,)"
         R"("flutes":true,"module":-1,"module":2.5,"order":"ionique","pedestal":true,"radialSegments":64,)"
         R"("schema":"classical-column/v1","volutes":true})",
         {GothicBuildError::JsonSchema, "module", "duplicate key"}},
        {"racine tableau", "[1, 2]", {GothicBuildError::JsonType, "", "the document root must be an object, not array"}},
    };
    for (const Case &c : cases)
    {
        SCOPED_TRACE(c.name);
        expectRefused(c.doc, c.refusal);
    }

    GothicBuildReport report;
    EXPECT_FALSE(LoadClassicalColumnFromJson("{\"schema\":", &report).has_value());
    ASSERT_EQ(report.entries.size(), 1u);
    EXPECT_EQ(report.entries.front().code, static_cast<int>(GothicBuildError::JsonParse));
    EXPECT_EQ(report.entries.front().parameter, "");

    // Un doublon suivi d'un texte tronque : deux diagnostics, dans cet ordre.
    GothicBuildReport truncated;
    EXPECT_FALSE(LoadClassicalColumnFromJson(R"({"module":1,"module":2,)", &truncated).has_value());
    ASSERT_EQ(truncated.entries.size(), 2u) << GothicBuildReportToJson(truncated);
    EXPECT_EQ(truncated.entries[0].code, static_cast<int>(GothicBuildError::JsonSchema));
    EXPECT_EQ(truncated.entries[0].parameter, "module");
    EXPECT_EQ(truncated.entries[1].code, static_cast<int>(GothicBuildError::JsonParse));
}

// Une version future ou anterieure est refusee, et le message nomme la version
// attendue.
TEST(TEST_cgarchi_classical_column_json, other_versions_are_refused_with_the_expected_version)
{
    for (const char *schema : {"classical-column/v2", "classical-column/v0"})
    {
        SCOPED_TRACE(schema);
        json doc = canonical();
        doc["schema"] = schema;
        expectRefused(doc.dump(), {GothicBuildError::JsonSchema, "schema",
                                   std::string("unsupported schema \"") + schema + "\" (expected classical-column/v1)"});
    }
}

// L'ordre est lu et ecrit par sa cle dans les donnees des cinq ordres, pas par
// son indice ni par son libelle d'interface.
TEST(TEST_cgarchi_classical_column_json, the_order_is_read_and_written_by_its_key)
{
    json doc = canonical();
    doc["order"] = "ionique";
    std::optional<ClassicalColumnSettings> s = LoadClassicalColumnFromJson(doc.dump());
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->order, static_cast<int>(classical::Order::Ionic));

    for (int o = 0; o < classical::kOrderCount; ++o)
    {
        const char *key = classical::OrderKey(static_cast<classical::Order>(o));
        SCOPED_TRACE(key);
        doc["order"] = key;
        s = LoadClassicalColumnFromJson(doc.dump());
        ASSERT_TRUE(s.has_value());
        EXPECT_EQ(s->order, o);
        EXPECT_EQ(classical::ClassicalOrders().Get(static_cast<classical::Order>(s->order)).key, key);
        EXPECT_EQ(json::parse(ExportClassicalColumnJson(*s))["order"], key);
    }

    doc["order"] = "Ionic";
    EXPECT_FALSE(LoadClassicalColumnFromJson(doc.dump()).has_value());
}

// L'ecriture refuse des reglages qu'elle ne pourrait pas relire.
TEST(TEST_cgarchi_classical_column_json, the_writer_refuses_settings_outside_their_domain)
{
    std::vector<ClassicalColumnSettings> invalid;
    auto add = [&](auto field, auto value) {
        ClassicalColumnSettings s;
        s.*field = value;
        invalid.push_back(s);
    };
    add(&ClassicalColumnSettings::order, -1);
    add(&ClassicalColumnSettings::order, classical::kOrderCount);
    add(&ClassicalColumnSettings::module, std::numeric_limits<float>::quiet_NaN());
    add(&ClassicalColumnSettings::module, std::nextafter(kClassicalModuleMin, 0.f));
    add(&ClassicalColumnSettings::module, std::nextafter(kClassicalModuleMax, 100.f));
    add(&ClassicalColumnSettings::doricCornice, 2);
    add(&ClassicalColumnSettings::doricFlutes, -1);
    add(&ClassicalColumnSettings::radialSegments, kClassicalRadialSegmentsMin - 1);
    add(&ClassicalColumnSettings::radialSegments, kClassicalRadialSegmentsMax + 1);
    add(&ClassicalColumnSettings::curveSegments, kClassicalCurveSegmentsMin - 1);
    add(&ClassicalColumnSettings::curveSegments, kClassicalCurveSegmentsMax + 1);
    for (std::size_t i = 0; i < invalid.size(); ++i)
        EXPECT_THROW(ExportClassicalColumnJson(invalid[i]), std::invalid_argument) << "cas " << i;
}

// Sous une locale C++ globale a virgule, le document ecrit et les nombres des
// messages gardent le point decimal ; le module 0,3, non exact en binaire,
// passe par l'ecriture de la plus courte decimale et sa relecture. Si une
// locale C a virgule est installee, LC_NUMERIC est aussi bascule : c'est la
// locale que lisent strtod et localeconv, donc le lexeur de nlohmann.
TEST(TEST_cgarchi_classical_column_json, the_document_ignores_the_global_locale)
{
    struct CommaPunct : std::numpunct<char>
    {
        char do_decimal_point() const override { return ','; }
        char do_thousands_sep() const override { return '.'; }
        std::string do_grouping() const override { return "\3"; }
    };
    ClassicalColumnSettings s;
    s.module = 0.3f;
    const std::string classic = ExportClassicalColumnJson(s);
    ASSERT_NE(classic.find("\"module\": 0.3,"), std::string::npos) << classic;
    json doc = canonical();
    doc["module"] = 10.5;

    const std::locale previous = std::locale::global(std::locale(std::locale::classic(), new CommaPunct));
    std::string comma;
    std::optional<ClassicalColumnSettings> reread;
    GothicBuildReport report;
    bool loaded = true;
    try
    {
        comma = ExportClassicalColumnJson(s);
        reread = LoadClassicalColumnFromJson(comma);
        loaded = LoadClassicalColumnFromJson(doc.dump(), &report).has_value();
    }
    catch (const std::exception &e)
    {
        ADD_FAILURE() << e.what();
    }
    std::locale::global(previous);

    EXPECT_EQ(comma, classic);
    ASSERT_TRUE(reread.has_value());
    EXPECT_EQ(reread->module, 0.3f);
    EXPECT_FALSE(loaded);
    ASSERT_EQ(report.entries.size(), 1u);
    EXPECT_EQ(report.entries.front().message, "10.5 outside [0.1, 10]");

    const char *current = std::setlocale(LC_NUMERIC, nullptr);
    const std::string savedC = current ? current : "C";
    const char *cName = nullptr;
    for (const char *name : {"de-DE", "de_DE.UTF-8", "fr_FR.UTF-8", "fr-FR"})
        if (std::setlocale(LC_NUMERIC, name) && std::localeconv()->decimal_point[0] == ',')
        {
            cName = name;
            break;
        }
    if (!cName)
    {
        std::setlocale(LC_NUMERIC, savedC.c_str());
        RecordProperty("c_comma_locale", "absente");
        return;
    }
    RecordProperty("c_comma_locale", cName);
    std::string commaC;
    std::optional<ClassicalColumnSettings> rereadC;
    try
    {
        commaC = ExportClassicalColumnJson(s);
        rereadC = LoadClassicalColumnFromJson(commaC);
    }
    catch (const std::exception &e)
    {
        ADD_FAILURE() << e.what();
    }
    std::setlocale(LC_NUMERIC, savedC.c_str());
    EXPECT_EQ(commaC, classic) << cName;
    ASSERT_TRUE(rereadC.has_value()) << cName;
    EXPECT_EQ(rereadC->module, 0.3f) << cName;
}

// Le schema JSON Schema porte les champs ecrits, tous requis, et les domaines
// des Parameter ; la fixture et chaque document ecrit le respectent. Le
// lecteur accepte chaque borne et refuse juste au-dela.
TEST(TEST_cgarchi_classical_column_json, the_schema_file_has_the_fields_and_domains_of_the_parameters)
{
    const json schema = schemaFile();
    ASSERT_TRUE(schema.is_object()) << "classical-column-v1.schema introuvable";

    std::set<std::string> written, properties, required;
    const json exported = json::parse(ExportClassicalColumnJson(ClassicalColumnSettings()));
    for (auto it = exported.begin(); it != exported.end(); ++it)
        written.insert(it.key());
    for (auto it = schema["properties"].begin(); it != schema["properties"].end(); ++it)
        properties.insert(it.key());
    for (const json &r : schema["required"])
        required.insert(r.get<std::string>());
    EXPECT_EQ(properties, written);
    EXPECT_EQ(required, written);
    EXPECT_EQ(schema["additionalProperties"], false);
    EXPECT_EQ(schema["properties"]["schema"]["const"], kClassicalColumnSchema);

    std::vector<std::string> orderKeys;
    for (int o = 0; o < classical::kOrderCount; ++o)
        orderKeys.push_back(classical::OrderKey(static_cast<classical::Order>(o)));
    EXPECT_EQ(schema["properties"]["order"]["enum"].get<std::vector<std::string>>(), orderKeys);

    // Bornes et nombre de choix : ceux des Parameter de la colonne.
    ParameterizedClassicalColumn column;
    std::size_t checked = 0;
    for (const Parameter &p : column.GetParameters())
    {
        SCOPED_TRACE(p.GetName());
        const std::string field = p.GetName() == "Order"             ? "order"
                                : p.GetName() == "Module"            ? "module"
                                : p.GetName() == "Doric cornice"     ? "doricCornice"
                                : p.GetName() == "Doric flutes"      ? "doricFlutes"
                                : p.GetName() == "Radial segments"   ? "radialSegments"
                                : p.GetName() == "Curve segments"    ? "curveSegments"
                                : std::string();
        if (field.empty())
        {
            EXPECT_EQ(p.GetType(), Parameter::BOOL);
            continue;
        }
        const json &spec = schema["properties"][field];
        switch (p.GetType())
        {
        case Parameter::ENUM:
            EXPECT_EQ(spec["enum"].size(), p.GetChoices().size());
            break;
        case Parameter::FLOAT:
            EXPECT_EQ(static_cast<float>(spec["minimum"].get<double>()), p.GetMinFloat());
            EXPECT_EQ(static_cast<float>(spec["maximum"].get<double>()), p.GetMaxFloat());
            break;
        case Parameter::INT:
            EXPECT_EQ(spec["minimum"].get<int>(), p.GetMinInt());
            EXPECT_EQ(spec["maximum"].get<int>(), p.GetMaxInt());
            break;
        case Parameter::BOOL:
        case Parameter::STRING:
            ADD_FAILURE() << "type inattendu";
            break;
        }
        ++checked;
    }
    EXPECT_EQ(checked, 6u);

    // La fixture et les documents ecrits respectent le schema ; un document
    // hors domaine ne le respecte pas.
    EXPECT_EQ(schemaViolation(canonical(), schema), "");
    for (int o = 0; o < classical::kOrderCount; ++o)
        for (ClassicalColumnSettings s : settingsGrid())
        {
            s.order = o;
            EXPECT_EQ(schemaViolation(json::parse(ExportClassicalColumnJson(s)), schema), "");
        }
    // Un document negatif par mot-cle du controleur.
    auto violates = [&](const std::function<void(json &)> &edit) {
        json d = canonical();
        edit(d);
        return schemaViolation(d, schema);
    };
    EXPECT_NE(violates([](json &d) { d["module"] = 11.0; }), "");                    // maximum
    EXPECT_NE(violates([](json &d) { d["curveSegments"] = 1; }), "");                // minimum
    EXPECT_NE(violates([](json &d) { d["order"] = "ionic"; }), "");                  // enum
    EXPECT_NE(violates([](json &d) { d["schema"] = "classical-column/v2"; }), "");   // const
    EXPECT_NE(violates([](json &d) { d["pedestal"] = 1; }), "");                     // type boolean
    EXPECT_NE(violates([](json &d) { d["module"] = "2.5"; }), "");                   // type number
    EXPECT_NE(violates([](json &d) { d["radialSegments"] = 64.5; }), "");            // type integer
    EXPECT_NE(violates([](json &d) { d.erase("volutes"); }), "");                    // required
    EXPECT_NE(violates([](json &d) { d["colour"] = "red"; }), "");                   // additionalProperties

    // `integer` au sens de JSON Schema : 64.0 respecte le schema, et le lecteur
    // l'accepte aussi.
    EXPECT_EQ(violates([](json &d) { d["radialSegments"] = 64.0; }), "");
    json integral = canonical();
    integral["radialSegments"] = 64.0;
    const std::optional<ClassicalColumnSettings> integralRead = LoadClassicalColumnFromJson(integral.dump());
    ASSERT_TRUE(integralRead.has_value());
    EXPECT_EQ(integralRead->radialSegments, 64);

    // Le lecteur accepte chaque borne du schema et refuse juste au-dela.
    for (const char *field : {"module", "radialSegments", "curveSegments"})
    {
        SCOPED_TRACE(field);
        const json &spec = schema["properties"][field];
        const bool integer = spec["type"] == "integer";
        const double step = integer ? 1.0 : 1e-3;
        const double lo = spec["minimum"].get<double>(), hi = spec["maximum"].get<double>();
        auto value = [&](double v) { return integer ? json(static_cast<long long>(v)) : json(v); };
        auto loads = [&](double v) {
            json doc = canonical();
            doc[field] = value(v);
            return LoadClassicalColumnFromJson(doc.dump()).has_value();
        };
        EXPECT_TRUE(loads(lo));
        EXPECT_TRUE(loads(hi));
        EXPECT_FALSE(loads(lo - step));
        EXPECT_FALSE(loads(hi + step));
    }
}
