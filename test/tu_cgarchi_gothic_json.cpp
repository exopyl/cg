#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include <cgarchi/architecture_gothic.h>
#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/parameterized.h"
#include <cgarchi/parameterized_gothic.h>

using namespace cgarchi;   // fichier de test de cgarchi seul

// ===========================================================================
//  Fenetre gothique -- chargement JSON (v2 strict, conversion v1) et export
// ===========================================================================

namespace
{
    void setParam(IParameterized &shape, const std::string &name, double value)
    {
        for (Parameter &p : shape.GetParameters())
        {
            if (p.GetName() != name)
                continue;
            switch (p.GetType())
            {
            case Parameter::FLOAT: p.SetFloat(static_cast<float>(value)); return;
            case Parameter::INT:
            case Parameter::ENUM:  p.SetInt(static_cast<int>(value)); return;
            case Parameter::BOOL:  p.SetBool(value != 0.0); return;
            case Parameter::STRING: break;
            }
            ADD_FAILURE() << "parametre non numerique : " << name;
            return;
        }
        ADD_FAILURE() << "parametre inconnu : " << name;
    }

    std::unique_ptr<Mesh> regenerate(ParameterizedGothicWindow &shape)
    {
        shape.Regenerate();
        return std::unique_ptr<Mesh>(shape.TakeMesh());
    }

    std::string readFile(const std::string &path)
    {
        std::ifstream in(path, std::ios::binary);
        std::ostringstream os;
        os << in.rdbuf();
        return os.str();
    }

    const char *const kV1InstancePath = "./test/data/gothic-window-instance.json";

    // Les memes sommets et les memes faces, a l'octet.
    void expectSameMesh(ParameterizedGothicWindow &a, ParameterizedGothicWindow &b)
    {
        const auto ma = regenerate(a);
        const auto mb = regenerate(b);
        ASSERT_TRUE(ma);
        ASSERT_TRUE(mb);
        EXPECT_EQ(ma->GetNVertices(), mb->GetNVertices());
        EXPECT_EQ(ma->GetVertices(), mb->GetVertices());
        ASSERT_EQ(ma->GetNFaces(), mb->GetNFaces());
        for (unsigned f = 0; f < ma->GetNFaces(); ++f)
            for (unsigned k = 0; k < 3; ++k)
                ASSERT_EQ(ma->FaceAt(f)->GetVertex(k), mb->FaceAt(f)->GetVertex(k)) << "face " << f;
    }

    bool sameThresholds(const GothicFeatureThresholds &a, const GothicFeatureThresholds &b)
    {
        return a.minRosetteRadius == b.minRosetteRadius && a.minRingWidth == b.minRingWidth
            && a.minEyeletInset == b.minEyeletInset && a.minSubRosetteRadius == b.minSubRosetteRadius
            && a.minMouchetteRadius == b.minMouchetteRadius && a.minFilletInnerOffset == b.minFilletInnerOffset
            && a.maxMouchetteInset == b.maxMouchetteInset
            && a.chamferWidthAdd == b.chamferWidthAdd && a.minChamferWidth == b.minChamferWidth
            && a.barRadiusAdd == b.barRadiusAdd && a.minBarRadius == b.minBarRadius
            && a.minBand == b.minBand;
    }

    // Chemins des entrees d'un code donne.
    std::set<std::string> pathsOf(const GothicBuildReport &r, GothicBuildWarning kind)
    {
        std::set<std::string> out;
        for (const GothicDiagnostic &d : r.entries)
            if (!d.error && d.code == static_cast<int>(kind))
                out.insert(d.parameter);
        return out;
    }

    std::set<std::string> pathsOf(const GothicBuildReport &r, GothicBuildError kind)
    {
        std::set<std::string> out;
        for (const GothicDiagnostic &d : r.entries)
            if (d.error && d.code == static_cast<int>(kind))
                out.insert(d.parameter);
        return out;
    }

    // Toutes les longueurs de la fenetre par defaut divisees par 10.
    void applyDiv10(ParameterizedGothicWindow &s)
    {
        setParam(s, "Width", 20);
        setParam(s, "Offset outer", 1.6);
        setParam(s, "Offset inner", 1.0);
        setParam(s, "Body height", 26);
        setParam(s, "Extrusion (zHeight)", 2);
    }

    // Une fenetre valide minimale, completee par `extra` (membres de la racine).
    std::string minimalV2(const std::string &extra = std::string())
    {
        return R"({"geometry":{"width":200,"excess":1,"bdOuter":16,"bdInner":10})"
             + (extra.empty() ? std::string() : "," + extra) + "}";
    }
}

// ---------------------------------------------------------------------------
//  Lecteur v2 strict
// ---------------------------------------------------------------------------

// Fillets 0 s'exporte (fill « none ») et se relit.
TEST(TEST_cgarchi_gothic_json, fillets_off_survives_a_round_trip)
{
    ParameterizedGothicWindow source;
    setParam(source, "Fillets", 0);
    ParameterizedGothicWindow loaded;
    ASSERT_TRUE(loaded.LoadFromJson(source.ExportJson()));
    EXPECT_FALSE(loaded.Settings().fillets);
    expectSameMesh(source, loaded);
}

// Les seuils s'exportent et se relisent : la baie reduite d'un facteur 10,
// seuils reduits, garde ses mouchettes apres relecture sur une fenetre neuve.
TEST(TEST_cgarchi_gothic_json, thresholds_survive_a_round_trip)
{
    ParameterizedGothicWindow source;
    applyDiv10(source);
    setParam(source, "Mouchettes", 1);
    source.SetFeatureThresholds(GothicFeatureThresholds().Scaled(0.1));

    ParameterizedGothicWindow loaded;
    ASSERT_TRUE(loaded.LoadFromJson(source.ExportJson()));
    EXPECT_TRUE(sameThresholds(loaded.FeatureThresholds(), source.FeatureThresholds()));
    GothicBuildReport report;
    loaded.BuildStonePolygon(&report);
    EXPECT_EQ(report.Count(GothicBuildWarning::FeatureDropped), 0u);
}

// Le plancher de bande s'exporte et se relit.
TEST(TEST_cgarchi_gothic_json, min_band_survives_a_round_trip)
{
    ParameterizedGothicWindow source;
    GothicFeatureThresholds t;
    t.minBand = 0.125;
    source.SetFeatureThresholds(t);
    const nlohmann::json j = nlohmann::json::parse(source.ExportJson());
    ASSERT_TRUE(j["thresholds"].contains("minBand"));
    EXPECT_EQ(j["thresholds"]["minBand"].get<double>(), 0.125);
    ParameterizedGothicWindow loaded;
    ASSERT_TRUE(loaded.LoadFromJson(source.ExportJson()));
    EXPECT_EQ(loaded.FeatureThresholds().minBand, 0.125);
    EXPECT_FALSE(loaded.LastLoadReport().HasWarnings());
}

// fallbackFilletInset est sans effet (retrait = max(bdInner,
// minFilletInnerOffset)) : le champ est lu, borne a >= 0, et signale
// field_ignored ; l'export ne l'ecrit pas.
TEST(TEST_cgarchi_gothic_json, fallback_fillet_inset_is_read_and_ignored)
{
    ParameterizedGothicWindow shape;
    const nlohmann::json exported = nlohmann::json::parse(shape.ExportJson());
    EXPECT_FALSE(exported["thresholds"].contains("fallbackFilletInset"));

    ASSERT_TRUE(shape.LoadFromJson(minimalV2(R"("thresholds":{"fallbackFilletInset":4})")));
    EXPECT_EQ(pathsOf(shape.LastLoadReport(), GothicBuildWarning::FieldIgnored).count("thresholds.fallbackFilletInset"), 1u);

    EXPECT_FALSE(shape.LoadFromJson(minimalV2(R"("thresholds":{"fallbackFilletInset":-1})")));
    EXPECT_EQ(pathsOf(shape.LastLoadReport(), GothicBuildError::JsonValue).count("thresholds.fallbackFilletInset"), 1u);
}

// L'export ecrit la taille de mouchette que le generateur utilise.
TEST(TEST_cgarchi_gothic_json, mouchette_size_is_the_value_used)
{
    ParameterizedGothicWindow shape;
    const nlohmann::json j = nlohmann::json::parse(shape.ExportJson());
    EXPECT_EQ(j["style"]["fillet"]["mouchette"]["size"].get<double>(), 0.92);
}

// Refus sans effet de bord : ni les reglages, ni les seuils, meme quand
// l'erreur suit un champ valide, ou quand seuls les seuils sont valides.
TEST(TEST_cgarchi_gothic_json, refused_document_leaves_the_window_unchanged)
{
    struct Case { const char *name; std::string json; GothicBuildError kind; const char *path; };
    const std::vector<Case> cases = {
        {"excess texte", R"({"geometry":{"width":300,"excess":"x","bdOuter":16,"bdInner":10}})",
         GothicBuildError::JsonType, "geometry.excess"},
        {"seuil negatif apres une geometrie valide",
         R"({"geometry":{"width":300,"excess":1,"bdOuter":16,"bdInner":10},"thresholds":{"minRingWidth":-1}})",
         GothicBuildError::JsonValue, "thresholds.minRingWidth"},
        {"seuils valides, geometrie fausse",
         R"({"geometry":{"width":-3,"excess":1,"bdOuter":16,"bdInner":10},"thresholds":{"minRingWidth":0.3}})",
         GothicBuildError::JsonValue, "geometry.width"},
    };
    for (const Case &c : cases)
    {
        SCOPED_TRACE(c.name);
        ParameterizedGothicWindow shape;
        const std::string before = shape.ExportJson();
        GothicBuildReport report;
        EXPECT_FALSE(shape.LoadFromJson(c.json, &report));
        EXPECT_TRUE(shape.Settings() == GothicWindowSettings());
        EXPECT_TRUE(sameThresholds(shape.FeatureThresholds(), GothicFeatureThresholds()));
        EXPECT_EQ(shape.ExportJson(), before);
        EXPECT_EQ(pathsOf(report, c.kind).count(c.path), 1u);
        EXPECT_EQ(pathsOf(shape.LastLoadReport(), c.kind), pathsOf(report, c.kind));
    }
}

// Types faux, enumerations inconnues, valeurs hors schema, version inconnue :
// refus nomme, fenetre inchangee.
TEST(TEST_cgarchi_gothic_json, out_of_schema_documents_are_refused)
{
    struct Case { const char *name; std::string json; GothicBuildError kind; const char *path; };
    const std::vector<Case> cases = {
        {"Chamfer", minimalV2(R"("style":{"mainArch":{"profile":{"type":"Chamfer"}}})"),
         GothicBuildError::JsonValue, "style.mainArch.profile.type"},
        {"depth 7", minimalV2(R"("recursion":{"depth":7})"), GothicBuildError::JsonValue, "recursion.depth"},
        {"racine tableau", "[1,2]", GothicBuildError::JsonType, ""},
        {"objet vide", "{}", GothicBuildError::JsonMissing, "geometry"},
        {"schema v9", R"({"schema":"gothic-window/v9","geometry":{"width":250,"excess":1,"bdOuter":16,"bdInner":10}})",
         GothicBuildError::JsonSchema, "schema"},
        {"1e300", R"({"geometry":{"width":1e300,"excess":1,"bdOuter":16,"bdInner":10}})",
         GothicBuildError::JsonValue, "geometry.width"},
        {"illisible", "{\"geometry\":", GothicBuildError::JsonParse, ""},
        {"1e400 (refuse par le parseur)", R"({"geometry":{"width":1e400,"excess":1,"bdOuter":16,"bdInner":10}})",
         GothicBuildError::JsonValue, ""},
        {"champ inconnu", minimalV2(R"("bogus":1)"), GothicBuildError::JsonSchema, "bogus"},
        {"seuil inconnu", minimalV2(R"("thresholds":{"bogus":1})"), GothicBuildError::JsonSchema, "thresholds.bogus"},
        {"prefixe d'enumeration", minimalV2(R"("style":{"mainArch":{"profile":{"type":"cham"}}})"),
         GothicBuildError::JsonValue, "style.mainArch.profile.type"},
        {"largeur nulle en float", R"({"geometry":{"width":1e-50,"excess":1,"bdOuter":16,"bdInner":10}})",
         GothicBuildError::JsonValue, "geometry.width"},
        {"excess 0,5 en float", R"({"geometry":{"width":200,"excess":0.5000000001,"bdOuter":16,"bdInner":10}})",
         GothicBuildError::JsonValue, "geometry.excess"},
        {"entier attendu", minimalV2(R"("recursion":{"depth":1.5})"), GothicBuildError::JsonType, "recursion.depth"},
        {"champ requis", R"({"geometry":{"width":200,"excess":1,"bdOuter":16}})",
         GothicBuildError::JsonMissing, "geometry.bdInner"},
    };
    for (const Case &c : cases)
    {
        SCOPED_TRACE(c.name);
        ParameterizedGothicWindow shape;
        const std::string before = shape.ExportJson();
        EXPECT_FALSE(shape.LoadFromJson(c.json));
        EXPECT_TRUE(shape.Settings() == GothicWindowSettings());
        EXPECT_EQ(shape.ExportJson(), before);
        EXPECT_EQ(pathsOf(shape.LastLoadReport(), c.kind).count(c.path), 1u);
    }
}

// Un champ connu que la baie ne consomme pas est accepte et liste en
// avertissement, nomme par son chemin. Le document exporte n'en produit aucun.
TEST(TEST_cgarchi_gothic_json, unconsumed_fields_are_listed_as_warnings)
{
    const std::string json = R"({
        "schema": "gothic-window/v2",
        "geometry": {"width": 200, "excess": 1, "bdOuter": 16, "bdInner": 10, "wallSetback": 5,
                     "subwindows": {"count": 3, "layout": "uniform",
                                    "gap": {"mode": "absolute", "gapFraction": 0.1, "absoluteWidth": 12}}},
        "style": {"mainArch": {"profile": {"type": "chamfer", "depth": 8},
                               "trefoil": {"enabled": true}},
                  "subArch": {"profile": {"type": "flat"}},
                  "rosette": {"profile": {"type": "chamfer"},
                              "foils": {"count": 6, "orientation": "lying"}},
                  "fillet": {"mouchette": {"type": "vesica", "size": 0.5}}},
        "recursion": {"depth": 0, "harmonic": true}
    })";
    ParameterizedGothicWindow shape;
    GothicBuildReport report;
    ASSERT_TRUE(shape.LoadFromJson(json, &report)) << GothicBuildReportToJson(report);
    EXPECT_FALSE(report.HasErrors());
    const std::set<std::string> expected = {
        "geometry.wallSetback", "geometry.subwindows.count", "geometry.subwindows.gap.mode",
        "geometry.subwindows.gap.absoluteWidth", "style.mainArch.profile.depth",
        "style.mainArch.trefoil.enabled", "style.subArch.profile.type",
        "style.rosette.foils.orientation", "style.fillet.mouchette.size", "recursion.harmonic"};
    EXPECT_EQ(pathsOf(report, GothicBuildWarning::FieldIgnored), expected);
    EXPECT_EQ(report.Count(GothicBuildWarning::FieldIgnored), expected.size());
    // Les champs lus sont poses.
    EXPECT_EQ(shape.Settings().profile, 1);
    EXPECT_EQ(shape.Settings().subCount, 3);
    EXPECT_TRUE(shape.Settings().archTrefoil);
    EXPECT_EQ(shape.Settings().mouchetteType, 0);
    EXPECT_EQ(shape.Settings().gapFraction, 0.1f);

    ParameterizedGothicWindow reloaded;
    GothicBuildReport clean;
    ASSERT_TRUE(reloaded.LoadFromJson(ParameterizedGothicWindow().ExportJson(), &clean));
    EXPECT_FALSE(clean.HasWarnings()) << GothicBuildReportToJson(clean);
    EXPECT_FALSE(clean.HasErrors());
}

// Un champ absent garde sa valeur.
TEST(TEST_cgarchi_gothic_json, absent_fields_keep_their_value)
{
    ParameterizedGothicWindow shape;
    setParam(shape, "Lancet layout", 0);
    setParam(shape, "Mouchettes", 1);
    setParam(shape, "Recursion", 2);
    setParam(shape, "Extrusion (zHeight)", 33);
    GothicWindowSettings expected = shape.Settings();
    ASSERT_TRUE(shape.LoadFromJson(R"({"geometry":{"width":200,"excess":1,"bdOuter":16,"bdInner":10,
                                                     "subwindows":{"count":2}},
                                        "style":{"fillet":{}}})"))
        << GothicBuildReportToJson(shape.LastLoadReport());
    expected.width = 200.f; expected.excess = 1.f; expected.offsetOuter = 16.f; expected.offsetInner = 10.f;
    EXPECT_TRUE(shape.Settings() == expected);
}

// Export(Load(Export(x))) == Export(x), et le meme maillage, sur une matrice
// de reglages non par defaut.
TEST(TEST_cgarchi_gothic_json, exported_document_round_trips_on_a_settings_matrix)
{
    using Setup = std::function<void(ParameterizedGothicWindow &)>;
    auto params = [](std::vector<std::pair<const char *, double>> list) -> Setup {
        return [list](ParameterizedGothicWindow &s) { for (const auto &[n, v] : list) setParam(s, n, v); };
    };
    const std::vector<std::pair<const char *, Setup>> cases = {
        {"defaut", params({})},
        {"Fillets 0", params({{"Fillets", 0}})},
        {"Rosette foils 0", params({{"Rosette foils", 0}})},
        {"Rosette 0", params({{"Rosette", 0}})},
        {"Mouchettes Vesica", params({{"Mouchettes", 1}, {"Mouchette type", 0}})},
        {"Mouchettes Teardrop", params({{"Mouchettes", 1}, {"Mouchette type", 1}})},
        {"Mouchettes Soufflet", params({{"Mouchettes", 1}, {"Mouchette type", 2}})},
        {"Uniform", params({{"Lancet layout", 0}, {"Gap fraction", 0.15}})},
        {"Prototype, Gap 0,05, Offset inner 40", params({{"Lancet layout", 1}, {"Gap fraction", 0.05}, {"Offset inner", 40}})},
        {"Recursion 1", params({{"Recursion", 1}})},
        {"Recursion 2", params({{"Recursion", 2}})},
        {"Chamfer", params({{"Profile", 1}})},
        {"Keel bar", params({{"Profile", 3}})},
        {"Lancet head plain", params({{"Lancet head", 0}})},
        {"Lancet head foils 6", params({{"Lancet head foils", 6}})},
        {"Rosette round, 9 foils", params({{"Rosette foil type", 0}, {"Rosette foil count", 9}})},
        {"Rosette pointedness 1,5", params({{"Rosette pointedness", 1.5}})},
        {"Excess 1,3, lancet excess 1,2", params({{"Excess (arch)", 1.3}, {"Lancet excess", 1.2}})},
        {"Body 0, drop 30", params({{"Body height", 0}, {"Lancet drop", 30}})},
        {"Extrusion 0", params({{"Extrusion (zHeight)", 0}})},
        {"Width 321, offsets 8 / 4", params({{"Width", 321}, {"Offset outer", 8}, {"Offset inner", 4}})},
        {"seuils / 10, mouchettes", [](ParameterizedGothicWindow &s) {
             applyDiv10(s);
             setParam(s, "Mouchettes", 1);
             s.SetFeatureThresholds(GothicFeatureThresholds().Scaled(0.1));
         }},
    };
    for (const auto &[name, setup] : cases)
    {
        SCOPED_TRACE(name);
        ParameterizedGothicWindow source;
        setup(source);
        const std::string exported = source.ExportJson();
        ParameterizedGothicWindow loaded;
        GothicBuildReport report;
        ASSERT_TRUE(loaded.LoadFromJson(exported, &report)) << GothicBuildReportToJson(report);
        EXPECT_FALSE(report.HasWarnings()) << GothicBuildReportToJson(report);
        EXPECT_EQ(loaded.ExportJson(), exported);
        EXPECT_TRUE(loaded.Settings() == source.Settings());
        EXPECT_TRUE(sameThresholds(loaded.FeatureThresholds(), source.FeatureThresholds()));
        expectSameMesh(source, loaded);
    }
}

// ---------------------------------------------------------------------------
//  Conversion v1 -> v2
// ---------------------------------------------------------------------------

// Le descripteur v1 converti, exporte puis relu, redonne le meme document et le
// meme maillage, sans avertissement a la relecture.
TEST(TEST_cgarchi_gothic_json, v1_conversion_is_idempotent_through_export)
{
    const std::string v1 = readFile(kV1InstancePath);
    ASSERT_FALSE(v1.empty()) << kV1InstancePath << " introuvable";
    ParameterizedGothicWindow converted;
    ASSERT_TRUE(converted.LoadFromJson(v1));
    ASSERT_FALSE(converted.Settings() == GothicWindowSettings()) << "rien n'a ete converti";
    const std::string exported = converted.ExportJson();

    ParameterizedGothicWindow reloaded;
    GothicBuildReport report;
    ASSERT_TRUE(reloaded.LoadFromJson(exported, &report));
    EXPECT_FALSE(report.HasWarnings()) << GothicBuildReportToJson(report);
    EXPECT_EQ(reloaded.ExportJson(), exported);
    expectSameMesh(converted, reloaded);
}

// Champ v1 requis absent, ou de type faux : refus nomme par le chemin v1.
TEST(TEST_cgarchi_gothic_json, v1_invalid_document_is_refused)
{
    const std::string v1 = readFile(kV1InstancePath);
    ASSERT_FALSE(v1.empty()) << kV1InstancePath << " introuvable";
    struct Case { const char *name; std::function<void(nlohmann::json &)> edit; GothicBuildError kind; const char *path; };
    const std::vector<Case> cases = {
        {"rosette absente", [](nlohmann::json &j) { j["window"].erase("rosette"); },
         GothicBuildError::JsonMissing, "window.rosette"},
        {"basis absent", [](nlohmann::json &j) { j["window"].erase("basis"); },
         GothicBuildError::JsonMissing, "window.basis"},
        {"style absent", [](nlohmann::json &j) { j["window"].erase("style"); },
         GothicBuildError::JsonMissing, "window.style"},
        {"archRise absent", [](nlohmann::json &j) { j["window"]["basis"].erase("archRise"); },
         GothicBuildError::JsonMissing, "window.basis.archRise"},
        {"largeur texte", [](nlohmann::json &j) { j["window"]["arch"]["width"] = "x"; },
         GothicBuildError::JsonType, "window.arch.width"},
        {"profil inconnu", [](nlohmann::json &j) { j["window"]["arch"]["profile"]["type"] = "roll"; },
         GothicBuildError::JsonValue, "window.arch.profile.type"},
        {"champ inconnu", [](nlohmann::json &j) { j["window"]["arch"]["bogus"] = 1; },
         GothicBuildError::JsonSchema, "window.arch.bogus"},
        {"champ inconnu de window", [](nlohmann::json &j) { j["window"]["bogus"] = 1; },
         GothicBuildError::JsonSchema, "window.bogus"},
    };
    for (const Case &c : cases)
    {
        SCOPED_TRACE(c.name);
        nlohmann::json j = nlohmann::json::parse(v1);
        c.edit(j);
        ParameterizedGothicWindow shape;
        const std::string before = shape.ExportJson();
        EXPECT_FALSE(shape.LoadFromJson(j.dump()));
        EXPECT_TRUE(shape.Settings() == GothicWindowSettings());
        EXPECT_EQ(shape.ExportJson(), before);
        EXPECT_EQ(pathsOf(shape.LastLoadReport(), c.kind).count(c.path), 1u)
            << GothicBuildReportToJson(shape.LastLoadReport());
    }
}

// Les conversions de la table v1 -> v2 : valeur retenue et avertissement nomme.
TEST(TEST_cgarchi_gothic_json, v1_fields_are_converted_to_their_nearest_equivalent)
{
    const std::string v1 = readFile(kV1InstancePath);
    ASSERT_FALSE(v1.empty()) << kV1InstancePath << " introuvable";
    nlohmann::json j = nlohmann::json::parse(v1);
    nlohmann::json &w = j["window"];
    w["arch"]["excess"] = 0.5;
    w["arch"]["profile"]["type"] = "bead";
    w["style"]["archStyle"]["molding"] = "bead";
    w["subwindows"]["gap"] = {{"mode", "absolute"}, {"absoluteWidth", 30}};
    w["subwindows"]["foils"]["count"] = 12;
    w["mouchettes"] = {{"enabled", true}, {"type", "teardrop"}};
    w["style"]["recursive"] = {{"enabled", true}, {"maxDepth", 4}};
    w["tessellation"]["angularResolution"] = 120;
    w["basis"]["origin"] = {{"x", 5}, {"y", 0}};

    ParameterizedGothicWindow shape;
    GothicBuildReport report;
    ASSERT_TRUE(shape.LoadFromJson(j.dump(), &report)) << GothicBuildReportToJson(report);
    const GothicWindowSettings &s = shape.Settings();
    EXPECT_EQ(s.excess, 0.7f);
    EXPECT_EQ(s.profile, 2);                    // bead -> roll
    EXPECT_EQ(s.lancetLayout, 0);               // uniform
    EXPECT_EQ(s.gapFraction, 0.15f);            // 30 / 200
    EXPECT_EQ(s.lancetHead, 1);
    EXPECT_EQ(s.lancetHeadFoils, 8);            // 12 borne a 8
    EXPECT_TRUE(s.fillets);
    EXPECT_TRUE(s.mouchettes);
    EXPECT_EQ(s.mouchetteType, 1);
    EXPECT_EQ(s.recursion, 2);
    EXPECT_DOUBLE_EQ(s.maxAngleRad, 2.0 * 3.14159265358979323846 / 120.0);
    EXPECT_EQ(s.bodyHeight, 180.f);
    EXPECT_EQ(s.rosetteFoilCount, 6);
    EXPECT_EQ(s.rosetteFoilType, 0);
    const std::set<std::string> converted = {
        "window.arch.excess", "window.arch.profile.type", "window.subwindows.gap.absoluteWidth",
        "window.subwindows.foils", "window.style.recursive.maxDepth"};
    EXPECT_EQ(pathsOf(report, GothicBuildWarning::FieldConverted), converted);
    EXPECT_EQ(pathsOf(report, GothicBuildWarning::FieldIgnored).count("window.basis.origin"), 1u);

    // Fillets coupes : remplissage « none », la mouchette demandee est perdue.
    w["fillets"]["enabled"] = false;
    ParameterizedGothicWindow off;
    GothicBuildReport offReport;
    ASSERT_TRUE(off.LoadFromJson(j.dump(), &offReport)) << GothicBuildReportToJson(offReport);
    EXPECT_FALSE(off.Settings().fillets);
    EXPECT_FALSE(off.Settings().mouchettes);
    EXPECT_EQ(pathsOf(offReport, GothicBuildWarning::FieldIgnored).count("window.mouchettes.enabled"), 1u);
}

// Les variantes ne sont pas appliquees, et le rapport le dit.
TEST(TEST_cgarchi_gothic_json, v1_variants_are_reported)
{
    const std::string v1 = readFile(kV1InstancePath);
    ASSERT_FALSE(v1.empty()) << kV1InstancePath << " introuvable";
    ParameterizedGothicWindow shape;
    ASSERT_TRUE(shape.LoadFromJson(v1));
    bool found = false;
    for (const GothicDiagnostic &d : shape.LastLoadReport().entries)
        if (!d.error && d.code == static_cast<int>(GothicBuildWarning::FieldIgnored) && d.parameter == "variants")
        {
            found = true;
            EXPECT_NE(d.message.find("5 variant"), std::string::npos) << d.message;
        }
    EXPECT_TRUE(found);
}

// Detection de version : un document v1 se reconnait a son `$schema`, ou, sans
// lui, a son objet `window` sans `geometry`.
TEST(TEST_cgarchi_gothic_json, v1_is_detected_by_its_schema_or_its_window_object)
{
    const std::string v1 = readFile(kV1InstancePath);
    ASSERT_FALSE(v1.empty()) << kV1InstancePath << " introuvable";
    ParameterizedGothicWindow reference;
    ASSERT_TRUE(reference.LoadFromJson(v1));

    nlohmann::json withoutSchema = nlohmann::json::parse(v1);
    withoutSchema.erase("$schema");
    ParameterizedGothicWindow byWindow;
    ASSERT_TRUE(byWindow.LoadFromJson(withoutSchema.dump())) << GothicBuildReportToJson(byWindow.LastLoadReport());
    EXPECT_TRUE(byWindow.Settings() == reference.Settings());

    // `$schema` v1 l'emporte : `geometry` est alors un champ v1 inconnu.
    nlohmann::json withGeometry = nlohmann::json::parse(v1);
    withGeometry["geometry"] = nlohmann::json::object();
    ParameterizedGothicWindow bySchema;
    EXPECT_FALSE(bySchema.LoadFromJson(withGeometry.dump()));
    EXPECT_EQ(pathsOf(bySchema.LastLoadReport(), GothicBuildError::JsonSchema).count("geometry"), 1u)
        << GothicBuildReportToJson(bySchema.LastLoadReport());
}

// v1 : des foils ronds ont une pointedness nulle (if / then du schema v1).
TEST(TEST_cgarchi_gothic_json, v1_round_foils_must_have_zero_pointedness)
{
    const std::string v1 = readFile(kV1InstancePath);
    ASSERT_FALSE(v1.empty()) << kV1InstancePath << " introuvable";
    nlohmann::json j = nlohmann::json::parse(v1);
    j["window"]["rosette"]["foils"]["pointedness"] = 0.5;
    ParameterizedGothicWindow shape;
    EXPECT_FALSE(shape.LoadFromJson(j.dump()));
    EXPECT_TRUE(shape.Settings() == GothicWindowSettings());
    EXPECT_EQ(pathsOf(shape.LastLoadReport(), GothicBuildError::JsonValue).count("window.rosette.foils.pointedness"), 1u)
        << GothicBuildReportToJson(shape.LastLoadReport());
}

// v1 : le trefle des lancettes n'est pas construit, et le rapport le dit.
TEST(TEST_cgarchi_gothic_json, v1_lancet_trefoil_is_reported_as_ignored)
{
    const std::string v1 = readFile(kV1InstancePath);
    ASSERT_FALSE(v1.empty()) << kV1InstancePath << " introuvable";
    nlohmann::json j = nlohmann::json::parse(v1);
    ParameterizedGothicWindow off;
    ASSERT_TRUE(off.LoadFromJson(j.dump()));
    EXPECT_EQ(pathsOf(off.LastLoadReport(), GothicBuildWarning::FieldIgnored).count("window.subwindows.trefoil"), 0u);
    j["window"]["subwindows"]["trefoil"]["enabled"] = true;
    ParameterizedGothicWindow on;
    ASSERT_TRUE(on.LoadFromJson(j.dump())) << GothicBuildReportToJson(on.LastLoadReport());
    EXPECT_EQ(pathsOf(on.LastLoadReport(), GothicBuildWarning::FieldIgnored).count("window.subwindows.trefoil"), 1u);
}

// v1 sans arch.width, centres a +-1e308 : la largeur deduite est infinie et
// refusee ; l'erreur porte le chemin v2 du champ produit par la conversion.
TEST(TEST_cgarchi_gothic_json, v1_overflowing_width_is_refused)
{
    const std::string v1 = readFile(kV1InstancePath);
    ASSERT_FALSE(v1.empty()) << kV1InstancePath << " introuvable";
    nlohmann::json j = nlohmann::json::parse(v1);
    j["window"]["arch"].erase("width");
    j["window"]["basis"]["pL"]["x"] = -1e308;
    j["window"]["basis"]["pR"]["x"] = 1e308;
    ParameterizedGothicWindow shape;
    EXPECT_FALSE(shape.LoadFromJson(j.dump()));
    EXPECT_TRUE(shape.Settings() == GothicWindowSettings());
    EXPECT_EQ(pathsOf(shape.LastLoadReport(), GothicBuildError::JsonValue).count("geometry.width"), 1u)
        << GothicBuildReportToJson(shape.LastLoadReport());
}

// heightBott au-dessus de la naissance : corps borne a 0, avertissement nomme.
TEST(TEST_cgarchi_gothic_json, positive_heightBott_gives_an_empty_body_with_a_warning)
{
    ParameterizedGothicWindow shape;
    GothicBuildReport report;
    ASSERT_TRUE(shape.LoadFromJson(R"({"geometry":{"width":200,"excess":1,"bdOuter":16,"bdInner":10,"heightBott":5}})",
                                   &report));
    EXPECT_EQ(shape.Settings().bodyHeight, 0.f);
    EXPECT_EQ(pathsOf(report, GothicBuildWarning::ParameterClamped).count("geometry.heightBott"), 1u)
        << GothicBuildReportToJson(report);
}

// Chaque champ numerique borne du schema v2 : le lecteur accepte une borne
// incluse et refuse juste au-dela ; il refuse une borne exclue et accepte juste
// en deca. Les bornes sont lues dans le schema, pas recopiees.
TEST(TEST_cgarchi_gothic_json, reader_bounds_are_the_schema_bounds)
{
    using nlohmann::json;
    const std::filesystem::path schemaPath = std::filesystem::path("src") / "cgarchi" / "schemas" / "gothic-window-v2.schema";
    const std::string text = readFile(schemaPath.string());
    ASSERT_FALSE(text.empty()) << schemaPath.string() << " introuvable";
    const json schema = json::parse(text);

    // Champs numeriques bornes, par pointeur JSON.
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
    };
    walk(schema, "");
    EXPECT_EQ(fields.size(), 35u) << "champs bornes du schema v2";

    const json base = json::parse(ParameterizedGothicWindow().ExportJson());
    auto check = [&](const std::string &pointer, const json &value, bool accepted) {
        json doc = base;
        doc[json::json_pointer(pointer)] = value;
        ParameterizedGothicWindow shape;
        const bool loaded = shape.LoadFromJson(doc.dump());
        std::string dotted = pointer.substr(1);
        std::replace(dotted.begin(), dotted.end(), '/', '.');
        EXPECT_EQ(loaded, accepted) << dotted << " = " << value.dump() << " : "
                                    << GothicBuildReportToJson(shape.LastLoadReport());
        if (!accepted)
            EXPECT_EQ(pathsOf(shape.LastLoadReport(), GothicBuildError::JsonValue).count(dotted), 1u) << dotted;
    };
    for (const auto &[pointer, spec] : fields)
    {
        const bool integer = spec["type"] == "integer";
        auto step = [&](double b) { return integer ? 1.0 : 1e-3 * std::max(1.0, std::abs(b)); };
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
