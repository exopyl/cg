#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <limits>
#include <memory>
#include <regex>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "../src/cgmesh/architecture_gothic.h"
#include "../src/cgmesh/contour_validation.h"
#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/mesh_validation.h"
#include "../src/cgmesh/parameterized.h"
#include "../src/cgmesh/parameterized_gothic.h"
#include "../src/cgmesh/polygon2.h"

// ===========================================================================
//  Fenetre gothique -- reglages et canal d'erreur
// ===========================================================================

namespace
{
    // Ecrit un parametre par son nom, quel que soit son type. Un nom inconnu est
    // une faute du test.
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
}

// ---------------------------------------------------------------------------
//  Reglages
// ---------------------------------------------------------------------------

// Les parametres exposes ecrivent dans les reglages ; une copie des reglages
// posee sur une autre fenetre en reproduit le maillage.
TEST(TEST_cgmesh_gothic_build_report, settings_hold_every_exposed_parameter)
{
    ParameterizedGothicWindow shape;
    EXPECT_TRUE(shape.Settings() == GothicWindowSettings());

    setParam(shape, "Width", 321);
    setParam(shape, "Recursion", 1);
    setParam(shape, "Mouchettes", 1);
    setParam(shape, "Profile", 1);
    EXPECT_FALSE(shape.Settings() == GothicWindowSettings());
    EXPECT_EQ(shape.Settings().width, 321.f);
    EXPECT_EQ(shape.Settings().recursion, 1);
    EXPECT_TRUE(shape.Settings().mouchettes);
    EXPECT_EQ(shape.Settings().profile, 1);

    const GothicWindowSettings copy = shape.Settings();
    EXPECT_TRUE(copy == shape.Settings());

    ParameterizedGothicWindow other;
    other.SetSettings(copy);
    EXPECT_TRUE(other.Settings() == copy);
    const auto a = regenerate(shape);
    const auto b = regenerate(other);
    ASSERT_TRUE(a);
    ASSERT_TRUE(b);
    EXPECT_EQ(a->GetVertices(), b->GetVertices());
    EXPECT_EQ(a->GetTriangles(), b->GetTriangles());
}

// operator== compare chaque champ : un seul champ different suffit.
TEST(TEST_cgmesh_gothic_build_report, settings_equality_compares_every_field)
{
    const GothicWindowSettings d;
    std::vector<GothicWindowSettings> variants;
    auto add = [&](auto mutate) { GothicWindowSettings s; mutate(s); variants.push_back(s); };
    add([](GothicWindowSettings &s) { s.width += 1.f; });
    add([](GothicWindowSettings &s) { s.excess += 0.1f; });
    add([](GothicWindowSettings &s) { s.offsetOuter += 1.f; });
    add([](GothicWindowSettings &s) { s.offsetInner += 1.f; });
    add([](GothicWindowSettings &s) { s.bodyHeight += 1.f; });
    add([](GothicWindowSettings &s) { s.subCount += 1; });
    add([](GothicWindowSettings &s) { s.subDrop += 1.f; });
    add([](GothicWindowSettings &s) { s.subExcess += 0.1f; });
    add([](GothicWindowSettings &s) { s.gapFraction += 0.01f; });
    add([](GothicWindowSettings &s) { s.lancetLayout = 1; });
    add([](GothicWindowSettings &s) { s.recursion = 2; });
    add([](GothicWindowSettings &s) { s.lancetHead = 0; });
    add([](GothicWindowSettings &s) { s.lancetHeadFoils += 1; });
    add([](GothicWindowSettings &s) { s.rosette = !s.rosette; });
    add([](GothicWindowSettings &s) { s.rosetteFoils = !s.rosetteFoils; });
    add([](GothicWindowSettings &s) { s.rosetteFoilCount += 1; });
    add([](GothicWindowSettings &s) { s.rosetteFoilType = 0; });
    add([](GothicWindowSettings &s) { s.rosetteFoilPointed += 0.1f; });
    add([](GothicWindowSettings &s) { s.subFoils = !s.subFoils; });
    add([](GothicWindowSettings &s) { s.subFoilCount += 1; });
    add([](GothicWindowSettings &s) { s.subFoilType = 1; });
    add([](GothicWindowSettings &s) { s.subFoilPointed += 0.1f; });
    add([](GothicWindowSettings &s) { s.archTrefoil = !s.archTrefoil; });
    add([](GothicWindowSettings &s) { s.trefoilSplit += 0.1f; });
    add([](GothicWindowSettings &s) { s.trefoilFoilRadius += 0.1f; });
    add([](GothicWindowSettings &s) { s.mouchettes = !s.mouchettes; });
    add([](GothicWindowSettings &s) { s.mouchetteType = 2; });
    add([](GothicWindowSettings &s) { s.mouchetteRadius += 0.1f; });
    add([](GothicWindowSettings &s) { s.fillets = !s.fillets; });
    add([](GothicWindowSettings &s) { s.zHeight += 1.f; });
    add([](GothicWindowSettings &s) { s.maxAngleRad *= 2.0; });
    add([](GothicWindowSettings &s) { s.profile = 3; });
    for (std::size_t i = 0; i < variants.size(); ++i)
    {
        EXPECT_FALSE(variants[i] == d) << "champ " << i;
        EXPECT_TRUE(variants[i] != d) << "champ " << i;
    }
}

// ---------------------------------------------------------------------------
//  Rapport de construction
// ---------------------------------------------------------------------------

namespace
{
    using ParamList = std::vector<std::pair<const char *, double>>;

    void applyParams(ParameterizedGothicWindow &shape, const ParamList &params)
    {
        for (const auto &[name, value] : params)
            setParam(shape, name, value);
    }

    // Entrees du rapport d'un code donne.
    std::vector<GothicDiagnostic> entriesOf(const GothicBuildReport &r, GothicBuildWarning kind)
    {
        std::vector<GothicDiagnostic> out;
        for (const GothicDiagnostic &d : r.entries)
            if (!d.error && d.code == static_cast<int>(kind))
                out.push_back(d);
        return out;
    }

    bool samePolygon(const Polygon2 &a, const Polygon2 &b)
    {
        if (a.get_n_contours() != b.get_n_contours())
            return false;
        for (int c = 0; c < a.get_n_contours(); ++c)
        {
            if (a.get_n_points(c) != b.get_n_points(c))
                return false;
            for (unsigned i = 0; i < 2 * a.get_n_points(c); ++i)
                if (a.get_points(c)[i] != b.get_points(c)[i])
                    return false;
        }
        return true;
    }

    // Baies d'un tirage aleatoire de parametres (graine 7), voir
    // tu_cgmesh_gothic_baseline.cpp.

    // rand50 : buildRosette ne trouve pas d'intersection de l'ellipse avec
    // l'axe, la construction leve.
    const ParamList kRosetteConstructionFailureBay = {
        {"Width", 38.3008}, {"Excess (arch)", 2.33445}, {"Body height", 313.104},
        {"Offset outer", 30.9418}, {"Offset inner", 26.8407}, {"Lancet head", 0},
        {"Lancet head foils", 4}, {"Recursion", 1}, {"Lancet drop", 94.4747},
        {"Lancet excess", 1.61888}, {"Gap fraction", 0.0931348}, {"Lancet layout", 0},
        {"Fillets", 0}, {"Mouchettes", 1}, {"Mouchette type", 1}, {"Rosette", 1},
        {"Rosette foils", 1}, {"Rosette foil count", 9}, {"Rosette foil type", 1},
        {"Rosette pointedness", 0.5503}, {"Extrusion (zHeight)", 96.7996}};

    // rand6 : disposition Prototype, l'offset interieur demande depasse ce que
    // les sous-lancettes admettent (buildArchOffset leve sans la borne).
    const ParamList kInnerOffsetCollapseBay = {
        {"Width", 94.1173}, {"Excess (arch)", 2.38304}, {"Body height", 90.8693},
        {"Offset outer", 31.8631}, {"Offset inner", 17.1813}, {"Lancet head", 0},
        {"Lancet head foils", 7}, {"Recursion", 0}, {"Lancet drop", 77.6148},
        {"Lancet excess", 0.822707}, {"Gap fraction", 0.112108}, {"Lancet layout", 1},
        {"Fillets", 1}, {"Mouchettes", 1}, {"Mouchette type", 1}, {"Rosette", 0},
        {"Rosette foils", 0}, {"Rosette foil count", 10}, {"Rosette foil type", 1},
        {"Rosette pointedness", 0.258922}, {"Extrusion (zHeight)", 11.7588}};

    // rand27 : aucun cercle tangent a l'arc ne passe par l'apex des lancettes ;
    // la rosace demandee n'est pas construite.
    const ParamList kUnsupportedRosetteBay = {
        {"Width", 77.8691}, {"Excess (arch)", 0.728777}, {"Body height", 329.13},
        {"Offset outer", 26.4028}, {"Offset inner", 17.1729}, {"Lancet head", 1},
        {"Lancet head foils", 7}, {"Recursion", 1}, {"Lancet drop", 52.3885},
        {"Lancet excess", 2.27508}, {"Gap fraction", 0.0817563}, {"Lancet layout", 0},
        {"Fillets", 1}, {"Mouchettes", 1}, {"Mouchette type", 1}, {"Rosette", 1},
        {"Rosette foils", 0}, {"Rosette foil count", 19}, {"Rosette foil type", 1},
        {"Rosette pointedness", 1.60697}, {"Extrusion (zHeight)", 96.3655}};

    // rand2 : les deux unites recursives n'ont pas de sous-rosace.
    const ParamList kUnsupportedSubRosettesBay = {
        {"Width", 167.355}, {"Excess (arch)", 1.40253}, {"Body height", 543.01},
        {"Offset outer", 39.0841}, {"Offset inner", 6.73863}, {"Lancet head", 1},
        {"Lancet head foils", 4}, {"Recursion", 1}, {"Lancet drop", 93.9659},
        {"Lancet excess", 0.961611}, {"Gap fraction", 0.105635}, {"Lancet layout", 0},
        {"Fillets", 0}, {"Mouchettes", 0}, {"Mouchette type", 1}, {"Rosette", 0},
        {"Rosette foils", 1}, {"Rosette foil count", 4}, {"Rosette foil type", 0},
        {"Rosette pointedness", 1.57491}, {"Extrusion (zHeight)", 17.7693}};

    // rand127 : un soufflet decoupe a son champ d'ecoincon est vide.
    const ParamList kClippedAwayMouchetteBay = {
        {"Width", 418.565}, {"Excess (arch)", 1.54817}, {"Body height", 101.422},
        {"Offset outer", 28.9327}, {"Offset inner", 1.57417}, {"Lancet head", 1},
        {"Lancet head foils", 5}, {"Recursion", 1}, {"Lancet drop", 17.0247},
        {"Lancet excess", 1.4227}, {"Gap fraction", 0.0571949}, {"Lancet layout", 1},
        {"Fillets", 1}, {"Mouchettes", 1}, {"Mouchette type", 2}, {"Rosette", 1},
        {"Rosette foils", 0}, {"Rosette foil count", 15}, {"Rosette foil type", 0},
        {"Rosette pointedness", 0.377313}, {"Extrusion (zHeight)", 37.2512}};
}

// Noms stables : snake_case, uniques entre avertissements et erreurs, dans
// l'ordre des enumerations.
TEST(TEST_cgmesh_gothic_build_report, names_are_stable_unique_snake_case)
{
    const std::vector<std::string> warnings = {
        "depth_fallback", "feature_dropped", "morphological_guard", "guard_closing_fallback",
        "guard_insufficient", "parameter_clamped", "region_invalid", "profile_fallback",
        "moulding_dropped", "field_ignored", "field_converted", "profile_reduced", "region_repaired",
        "multiple_shells", "moulding_not_fused", "moulding_simplified"};
    const std::vector<std::string> errors = {
        "invalid_parameter", "construction_failed", "json_parse", "json_schema",
        "json_type", "json_value", "json_missing", "mesh_not_strict"};
    ASSERT_EQ(kGothicBuildWarningCount, warnings.size());
    ASSERT_EQ(kGothicBuildErrorCount, errors.size());
    const std::regex snake("[a-z]+(_[a-z]+)*");
    std::set<std::string> seen;
    for (std::size_t k = 0; k < kGothicBuildWarningCount; ++k)
    {
        const std::string name = GothicBuildWarningName(static_cast<GothicBuildWarning>(k));
        EXPECT_EQ(name, warnings[k]);
        EXPECT_TRUE(std::regex_match(name, snake)) << name;
        EXPECT_TRUE(seen.insert(name).second) << name;
    }
    for (std::size_t k = 0; k < kGothicBuildErrorCount; ++k)
    {
        const std::string name = GothicBuildErrorName(static_cast<GothicBuildError>(k));
        EXPECT_EQ(name, errors[k]);
        EXPECT_TRUE(std::regex_match(name, snake)) << name;
        EXPECT_TRUE(seen.insert(name).second) << name;
    }
}

// Les compteurs comptent tout ; les entrees s'arretent au plafond. Record sans
// message ne cree pas d'entree. Merge additionne et prolonge, plafond compris.
TEST(TEST_cgmesh_gothic_build_report, counters_count_every_event_and_entries_are_capped)
{
    GothicBuildReport r;
    EXPECT_FALSE(r.HasErrors());
    EXPECT_FALSE(r.HasWarnings());

    r.Record(GothicBuildWarning::DepthFallback);
    EXPECT_TRUE(r.HasWarnings());
    EXPECT_TRUE(r.entries.empty());

    const std::size_t n = kGothicBuildReportMaxEntries + 10;
    for (std::size_t i = 0; i < n; ++i)
        r.Record(GothicBuildWarning::ParameterClamped, "Width", "w" + std::to_string(i));
    EXPECT_EQ(r.Count(GothicBuildWarning::ParameterClamped), n);
    ASSERT_EQ(r.entries.size(), kGothicBuildReportMaxEntries);
    EXPECT_EQ(r.entries.front().message, "w0");
    EXPECT_FALSE(r.HasErrors());

    GothicBuildReport e;
    e.Record(GothicBuildError::ConstructionFailed, "", "boom");
    EXPECT_TRUE(e.HasErrors());
    EXPECT_FALSE(e.HasWarnings());
    ASSERT_EQ(e.entries.size(), 1u);
    EXPECT_TRUE(e.entries[0].error);
    EXPECT_EQ(e.entries[0].code, static_cast<int>(GothicBuildError::ConstructionFailed));

    GothicBuildReport m = e;
    m.Merge(r);
    EXPECT_EQ(m.Count(GothicBuildError::ConstructionFailed), 1u);
    EXPECT_EQ(m.Count(GothicBuildWarning::ParameterClamped), n);
    EXPECT_EQ(m.Count(GothicBuildWarning::DepthFallback), 1u);
    ASSERT_EQ(m.entries.size(), kGothicBuildReportMaxEntries);
    EXPECT_EQ(m.entries[0].message, "boom");
    EXPECT_EQ(m.entries[1].message, "w0");
}

TEST(TEST_cgmesh_gothic_build_report, json_lists_every_category_and_the_entries)
{
    GothicBuildReport r;
    r.Record(GothicBuildWarning::FeatureDropped, "Rosette", "rosette: gone");
    r.Record(GothicBuildWarning::FeatureDropped);
    r.Record(GothicBuildError::InvalidParameter, "Width", "non-finite value");
    const nlohmann::json j = nlohmann::json::parse(GothicBuildReportToJson(r));
    ASSERT_TRUE(j.contains("warnings"));
    ASSERT_TRUE(j.contains("errors"));
    EXPECT_EQ(j["warnings"].size(), kGothicBuildWarningCount);
    EXPECT_EQ(j["errors"].size(), kGothicBuildErrorCount);
    EXPECT_EQ(j["warnings"]["feature_dropped"].get<std::size_t>(), 2u);
    EXPECT_EQ(j["warnings"]["depth_fallback"].get<std::size_t>(), 0u);
    EXPECT_EQ(j["errors"]["invalid_parameter"].get<std::size_t>(), 1u);
    ASSERT_EQ(j["entries"].size(), 2u);
    EXPECT_EQ(j["entries"][0]["severity"], "warning");
    EXPECT_EQ(j["entries"][0]["code"], "feature_dropped");
    EXPECT_EQ(j["entries"][0]["parameter"], "Rosette");
    EXPECT_EQ(j["entries"][1]["severity"], "error");
    EXPECT_EQ(j["entries"][1]["code"], "invalid_parameter");
    EXPECT_EQ(j["entries"][1]["message"], "non-finite value");
}

// ---------------------------------------------------------------------------
//  Non-finis hors des parametres exposes : seuils et profils explicites
// ---------------------------------------------------------------------------
//
// Un seuil non fini est refuse avant tout calcul, comme un parametre :
// triangle de repli, une seule invalid_parameter nommee « thresholds.<champ> »,
// et BuildStonePolygon leve. Un profil explicite non fini est refuse par
// Regenerate (« Splay profile », « Bar profile ») ; la region de pierre, qui
// ne le lit pas, se construit.

namespace
{
    using ThresholdField = double GothicFeatureThresholds::*;

    const std::vector<std::pair<const char *, ThresholdField>> &thresholdFields()
    {
        static const std::vector<std::pair<const char *, ThresholdField>> fields = {
            {"thresholds.minRosetteRadius",     &GothicFeatureThresholds::minRosetteRadius},
            {"thresholds.minRingWidth",         &GothicFeatureThresholds::minRingWidth},
            {"thresholds.minEyeletInset",       &GothicFeatureThresholds::minEyeletInset},
            {"thresholds.minSubRosetteRadius",  &GothicFeatureThresholds::minSubRosetteRadius},
            {"thresholds.minMouchetteRadius",   &GothicFeatureThresholds::minMouchetteRadius},
            {"thresholds.minFilletInnerOffset", &GothicFeatureThresholds::minFilletInnerOffset},
            {"thresholds.maxMouchetteInset",    &GothicFeatureThresholds::maxMouchetteInset},
            {"thresholds.chamferWidthAdd",      &GothicFeatureThresholds::chamferWidthAdd},
            {"thresholds.minChamferWidth",      &GothicFeatureThresholds::minChamferWidth},
            {"thresholds.barRadiusAdd",         &GothicFeatureThresholds::barRadiusAdd},
            {"thresholds.minBarRadius",         &GothicFeatureThresholds::minBarRadius},
            {"thresholds.minBand",              &GothicFeatureThresholds::minBand}};
        return fields;
    }

    // Triangle de repli et une seule erreur invalid_parameter, nommee `name`.
    void expectPlaceholderWithOneInvalidParameter(ParameterizedGothicWindow &shape, const std::string &name)
    {
        const auto mesh = regenerate(shape);
        ASSERT_TRUE(mesh);
        EXPECT_EQ(mesh->GetNVertices(), 3u);
        EXPECT_EQ(mesh->GetNFaces(), 1u);
        const GothicBuildReport &r = shape.LastBuildReport();
        EXPECT_EQ(r.Count(GothicBuildError::InvalidParameter), 1u);
        EXPECT_EQ(r.Count(GothicBuildError::ConstructionFailed), 0u);
        std::size_t named = 0;
        for (const GothicDiagnostic &d : r.entries)
            named += (d.error && d.parameter == name);
        EXPECT_EQ(named, 1u);
    }
}

TEST(TEST_cgmesh_gothic_build_report, non_finite_thresholds_are_reported_as_errors)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    struct Case { const char *config; ParamList params; double value; };
    std::vector<Case> cases;
    for (const double v : {nan, inf, -inf})
        cases.push_back({"defaut", {}, v});
    for (const auto &[field, member] : thresholdFields())
    {
        for (const Case &c : cases)
        {
            SCOPED_TRACE(std::string(field) + " = " + std::to_string(c.value));
            GothicFeatureThresholds t;
            t.*member = c.value;
            ParameterizedGothicWindow shape;
            shape.SetFeatureThresholds(t);
            expectPlaceholderWithOneInvalidParameter(shape, field);
            EXPECT_THROW(shape.BuildStonePolygon(), std::invalid_argument);
        }
    }
}

// Seuils non finis dans des configurations ou ils sont lus : sans le controle,
// minRingWidth NaN et minChamferWidth +inf (Chamfer) font planter Regenerate.
TEST(TEST_cgmesh_gothic_build_report, non_finite_thresholds_read_by_the_bay_are_reported)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    const std::vector<std::pair<const char *, ParamList>> ringConfigs = {
        {"defaut", {}}, {"Chamfer", {{"Profile", 1}}}, {"Roll bar", {{"Profile", 2}}},
        {"Mouchettes", {{"Mouchettes", 1}}}, {"Offset inner 0,5", {{"Offset inner", 0.5}}}};
    for (const auto &[config, params] : ringConfigs)
    {
        SCOPED_TRACE(std::string("thresholds.minRingWidth = NaN, ") + config);
        GothicFeatureThresholds t;
        t.minRingWidth = nan;
        ParameterizedGothicWindow shape;
        applyParams(shape, params);
        shape.SetFeatureThresholds(t);
        expectPlaceholderWithOneInvalidParameter(shape, "thresholds.minRingWidth");
    }
    {
        SCOPED_TRACE("thresholds.minChamferWidth = inf, Chamfer");
        GothicFeatureThresholds t;
        t.minChamferWidth = inf;
        ParameterizedGothicWindow shape;
        setParam(shape, "Profile", 1);
        shape.SetFeatureThresholds(t);
        expectPlaceholderWithOneInvalidParameter(shape, "thresholds.minChamferWidth");
    }
}

TEST(TEST_cgmesh_gothic_build_report, non_finite_explicit_profiles_are_reported_as_errors)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    for (const double v : {nan, inf})
    {
        {
            SCOPED_TRACE("Splay profile, " + std::to_string(v));
            Profile2D splay;
            splay.points = {Vector2d(0.0, 0.0), Vector2d(v, 5.0), Vector2d(6.0, 10.0)};
            ParameterizedGothicWindow shape;
            shape.SetSplayProfile(&splay);
            expectPlaceholderWithOneInvalidParameter(shape, "Splay profile");
            EXPECT_NO_THROW(shape.BuildStonePolygon());
        }
        {
            SCOPED_TRACE("Bar profile, " + std::to_string(v));
            Profile2D bar;
            bar.points = {Vector2d(0.0, -2.0), Vector2d(2.0, v), Vector2d(0.0, 2.0)};
            ParameterizedGothicWindow shape;
            shape.SetBarProfile(&bar);
            expectPlaceholderWithOneInvalidParameter(shape, "Bar profile");
            EXPECT_NO_THROW(shape.BuildStonePolygon());
        }
    }
}

// Une construction qui leve rend le triangle de repli ; le rapport porte
// construction_failed, avec le texte de l'exception, et garde les
// avertissements enregistres avant elle.
// Corps fini mais hors de la plage de coordonnees de Clipper2 : le booleen leve.
TEST(TEST_cgmesh_gothic_build_report, construction_failure_is_reported)
{
    ParameterizedGothicWindow shape;
    GothicWindowSettings s = shape.Settings();
    s.bodyHeight  = 1e18f;
    s.offsetOuter = -1.f;
    shape.SetSettings(s);
    const auto mesh = regenerate(shape);
    ASSERT_TRUE(mesh);
    EXPECT_EQ(mesh->GetNVertices(), 3u);
    EXPECT_EQ(mesh->GetNFaces(), 1u);
    const GothicBuildReport &r = shape.LastBuildReport();
    EXPECT_EQ(r.Count(GothicBuildError::ConstructionFailed), 1u);
    EXPECT_EQ(r.Count(GothicBuildError::InvalidParameter), 0u);
    EXPECT_EQ(r.Count(GothicBuildWarning::ParameterClamped), 1u);
    bool found = false;
    for (const GothicDiagnostic &d : r.entries)
        if (d.error && d.code == static_cast<int>(GothicBuildError::ConstructionFailed))
        {
            found = true;
            EXPECT_NE(d.message.find("exceed"), std::string::npos) << d.message;
        }
    EXPECT_TRUE(found);
}

// Clamps : enregistres, nommes, quand la valeur utilisee differe du reglage ;
// la fenetre par defaut et les bornes de l'interface n'en produisent aucun.
TEST(TEST_cgmesh_gothic_build_report, clamped_parameters_are_reported)
{
    {
        ParameterizedGothicWindow shape;
        GothicBuildReport r;
        shape.BuildStonePolygon(&r);
        EXPECT_FALSE(r.HasWarnings());
        EXPECT_FALSE(r.HasErrors());
    }
    for (const double gap : {0.2, 0.01})
    {
        SCOPED_TRACE("borne de l'interface : Gap fraction = " + std::to_string(gap));
        ParameterizedGothicWindow shape;
        setParam(shape, "Gap fraction", gap);
        GothicBuildReport r;
        shape.BuildStonePolygon(&r);
        EXPECT_EQ(r.Count(GothicBuildWarning::ParameterClamped), 0u);
    }
    const ParamList outOfDomain = {
        {"Width", 0.5}, {"Excess (arch)", 3.0}, {"Excess (arch)", 0.5}, {"Offset outer", -1.0},
        {"Offset inner", 40.0}, {"Offset inner", -1.0}, {"Lancet drop", -1.0},
        {"Lancet excess", 3.0}, {"Gap fraction", 0.0}, {"Gap fraction", 0.5},
        {"Recursion", 3}, {"Recursion", -1}, {"Rosette foil count", 30}, {"Rosette foil count", 2},
        {"Lancet head foils", 9}, {"Lancet head foils", 2}, {"Mouchette type", 5},
        {"Rosette pointedness", 2.5}, {"Rosette pointedness", -0.5},
        {"Extrusion (zHeight)", -1.0}};
    for (const auto &[name, value] : outOfDomain)
    {
        SCOPED_TRACE(std::string(name) + " = " + std::to_string(value));
        ParameterizedGothicWindow shape;
        setParam(shape, name, value);
        GothicBuildReport r;
        shape.BuildStonePolygon(&r);
        // Un clamp peut en entrainer d'autres (Width 0,5 borne aussi l'offset
        // interieur) : seul celui du parametre modifie est attendu ici.
        std::size_t named = 0;
        for (const GothicDiagnostic &d : entriesOf(r, GothicBuildWarning::ParameterClamped))
            if (d.parameter == name)
            {
                ++named;
                EXPECT_FALSE(d.message.empty());
            }
        EXPECT_EQ(named, 1u);
    }
}

// Une rosace demandee qui ne peut prendre appui sur les lancettes est absente
// (la region est celle de la baie sans rosace) et enregistree, nommee
// « Rosette ».
TEST(TEST_cgmesh_gothic_build_report, unsupported_rosette_is_reported)
{
    ParameterizedGothicWindow with, without;
    applyParams(with, kUnsupportedRosetteBay);
    applyParams(without, kUnsupportedRosetteBay);
    setParam(without, "Rosette", 0);
    GothicBuildReport r;
    const Polygon2 a = with.BuildStonePolygon(&r);
    EXPECT_TRUE(samePolygon(a, without.BuildStonePolygon())) << "gabarit non discriminant : la rosace existe";
    std::size_t named = 0;
    for (const GothicDiagnostic &d : entriesOf(r, GothicBuildWarning::FeatureDropped))
        named += (d.parameter == "Rosette");
    EXPECT_EQ(named, 1u);
}

// Sous-rosaces non construites : une par unite recursive, enregistree.
TEST(TEST_cgmesh_gothic_build_report, unsupported_sub_rosettes_are_reported)
{
    ParameterizedGothicWindow shape;
    applyParams(shape, kUnsupportedSubRosettesBay);
    GothicBuildReport r;
    shape.BuildStonePolygon(&r);
    std::size_t sub = 0;
    for (const GothicDiagnostic &d : entriesOf(r, GothicBuildWarning::FeatureDropped))
        sub += (d.message.rfind("sub-rosette:", 0) == 0);
    EXPECT_EQ(sub, 2u);
    EXPECT_EQ(r.Count(GothicBuildWarning::FeatureDropped), 2u);
}

// Une mouchette entierement decoupee par son champ laisse le champ simple,
// enregistre.
TEST(TEST_cgmesh_gothic_build_report, clipped_away_mouchette_is_reported)
{
    ParameterizedGothicWindow shape;
    applyParams(shape, kClippedAwayMouchetteBay);
    GothicBuildReport r;
    shape.BuildStonePolygon(&r);
    std::size_t clipped = 0;
    for (const GothicDiagnostic &d : entriesOf(r, GothicBuildWarning::FeatureDropped))
        clipped += (d.message == "mouchette: empty once clipped to its field");
    EXPECT_EQ(clipped, 1u);
}

// Un profil d'ebrasement inextrudable retombe sur les parois droites : le
// maillage est celui de la baie plate, et le repli est enregistre.
TEST(TEST_cgmesh_gothic_build_report, splay_profile_fallback_is_reported)
{
    Profile2D bad;
    bad.points = {Vector2d(1.0, 1.0), Vector2d(2.0, 3.0)};   // ne part pas de (0, 0)
    ParameterizedGothicWindow shape;
    shape.SetSplayProfile(&bad);
    const auto mesh = regenerate(shape);
    ParameterizedGothicWindow flat;
    const auto reference = regenerate(flat);
    ASSERT_TRUE(mesh);
    ASSERT_TRUE(reference);
    EXPECT_EQ(mesh->GetVertices(), reference->GetVertices());
    const GothicBuildReport &r = shape.LastBuildReport();
    EXPECT_EQ(r.Count(GothicBuildWarning::ProfileFallback), 1u);
    const std::vector<GothicDiagnostic> d = entriesOf(r, GothicBuildWarning::ProfileFallback);
    ASSERT_EQ(d.size(), 1u);
    EXPECT_EQ(d[0].parameter, "Profile");
    EXPECT_NE(d[0].message.find("extrudeProfiledToMesh"), std::string::npos) << d[0].message;
}

// Barres de moulure : chaque contour recursif qui ne se construit pas est
// enregistre. A Recursion = 2, les quatre sous-lancettes de la fenetre par
// defaut ne se subdivisent pas (depth_fallback = 4) : leurs moulures non plus.
TEST(TEST_cgmesh_gothic_build_report, dropped_moulding_outlines_are_reported)
{
    ParameterizedGothicWindow shape;
    applyParams(shape, {{"Recursion", 2}, {"Profile", 2}});
    const auto mesh = regenerate(shape);
    ASSERT_TRUE(mesh);
    const GothicBuildReport &r = shape.LastBuildReport();
    EXPECT_EQ(r.Count(GothicBuildWarning::DepthFallback), 4u);
    EXPECT_EQ(r.Count(GothicBuildWarning::MouldingDropped), 4u);

    ParameterizedGothicWindow oneLevel;
    applyParams(oneLevel, {{"Recursion", 1}, {"Profile", 2}});
    regenerate(oneLevel);
    EXPECT_EQ(oneLevel.LastBuildReport().Count(GothicBuildWarning::MouldingDropped), 0u);
}

// ---------------------------------------------------------------------------
//  Matrice des extremes de curseurs
// ---------------------------------------------------------------------------
//
// Chaque parametre expose (sauf Recursion) a son minimum puis a son maximum
// d'interface, chaque enumeration a toutes ses valeurs, a Recursion 0, 1 et 2 :
// (16 x 2 + 2 + 2 + 3 + 2 + 5) x 3 = 138 cas. Chaque cas rend un maillage sain, ou
// un rapport qui nomme le defaut : une erreur, region_invalid ou
// guard_insufficient. Sain : region de pierre sans defaut de contour, maillage
// d'un seul tenant, sans aucun defaut, faces degenerees comprises. Au niveau 0,
// region_invalid accompagne une region que la reparation n'a pas rendue valide ;
// son maillage est strict, sinon le rapport porte aussi mesh_not_strict.
// Une lecture propre a un mode : Extrusion 0, feuille plane, ses aretes de bord
// sont attendues. Les profils en barre sont fusionnes a la plaque et juges
// comme les autres, sans repli (moulding_not_fused) ni niveau saute
// (moulding_simplified).
// Les cas ni sains ni nommes sont listes par famille (kKnownExtremeFailures) ;
// un cas liste qui devient sain ou nomme fait echouer le test.

namespace
{
    struct ExtremeCase
    {
        std::string name;
        double      value = 0.0;
        int         recursion = 0;
    };

    struct KnownExtremeFailure
    {
        const char *family;
        const char *name;
        double      value;
        int         recursion;
    };

    // Aucun cas aujourd'hui.
    const std::vector<KnownExtremeFailure> kKnownExtremeFailures = {};

    bool isKnownExtremeFailure(const ExtremeCase &c)
    {
        for (const KnownExtremeFailure &k : kKnownExtremeFailures)
            if (c.name == k.name && c.value == k.value && c.recursion == k.recursion)
                return true;
        return false;
    }

    std::vector<ExtremeCase> sliderExtremes()
    {
        std::vector<ExtremeCase> out;
        ParameterizedGothicWindow proto;
        for (int rec = 0; rec <= 2; ++rec)
            for (const Parameter &p : proto.GetParameters())
            {
                if (p.GetName() == "Recursion")
                    continue;
                std::vector<double> values;
                switch (p.GetType())
                {
                case Parameter::FLOAT: values = {p.GetMinFloat(), p.GetMaxFloat()}; break;
                case Parameter::INT:   values = {static_cast<double>(p.GetMinInt()), static_cast<double>(p.GetMaxInt())}; break;
                case Parameter::ENUM:
                    for (std::size_t k = 0; k < p.GetChoices().size(); ++k)
                        values.push_back(static_cast<double>(k));
                    break;
                case Parameter::BOOL:  values = {0.0, 1.0}; break;
                case Parameter::STRING: break;
                }
                for (const double v : values)
                    out.push_back({p.GetName(), v, rec});
            }
        return out;
    }

    // Le rapport nomme le defaut de la baie : une erreur, ou un avertissement
    // qui dit que la region rendue garde un defaut de contour (region_invalid
    // au niveau 0, guard_insufficient en recursion).
    bool namesADefect(const GothicBuildReport &report)
    {
        return report.HasErrors()
            || report.Count(GothicBuildWarning::RegionInvalid) > 0
            || report.Count(GothicBuildWarning::GuardInsufficient) > 0;
    }
}

TEST(TEST_cgmesh_gothic_build_report, slider_extremes_give_a_sound_mesh_or_a_named_defect)
{
    const std::vector<ExtremeCase> cases = sliderExtremes();
    ASSERT_EQ(cases.size(), 138u);
    std::size_t sound = 0, named = 0;
    for (const ExtremeCase &c : cases)
    {
        SCOPED_TRACE("Recursion " + std::to_string(c.recursion) + ", " + c.name + " = " + std::to_string(c.value));
        ParameterizedGothicWindow shape;
        setParam(shape, "Recursion", c.recursion);
        setParam(shape, c.name, c.value);
        const auto mesh = regenerate(shape);
        ASSERT_TRUE(mesh);
        const GothicBuildReport &report = shape.LastBuildReport();

        const bool flat = shape.Settings().zHeight <= 0.f;
        bool contoursOk = false;
        try { contoursOk = ValidateContours(shape.BuildStonePolygon()).IsValid(); } catch (...) {}
        MeshValidationOptions options;
        options.weldTolerance         = 5e-4;
        options.checkSelfIntersection = c.recursion == 0;
        const MeshValidationReport r = ValidateMesh(*mesh, options);
        bool meshOk = r.components == 1u && mesh->GetNFaces() > 1u;
        for (std::size_t k = 0; k < kMeshDefectCount; ++k)
        {
            const auto kind = static_cast<MeshDefect>(k);
            if (kind == MeshDefect::UnexpectedComponentCount)
                continue;
            if (flat && kind == MeshDefect::BoundaryEdge)
                continue;
            if (r.Count(kind))
                meshOk = false;
        }

        const bool diagnosed = namesADefect(report);
        EXPECT_EQ(report.Count(GothicBuildWarning::MouldingNotFused), 0u);
        EXPECT_EQ(report.Count(GothicBuildWarning::MouldingSimplified), 0u);
        if (contoursOk && meshOk)
            ++sound;
        else
            ++named;
        if (isKnownExtremeFailure(c))
            EXPECT_FALSE((contoursOk && meshOk) || diagnosed)
                << "cas liste devenu sain ou nomme : le retirer de kKnownExtremeFailures";
        else
            EXPECT_TRUE((contoursOk && meshOk) || diagnosed)
                << "contours " << contoursOk << ", maillage " << meshOk << " sans diagnostic nomme";
    }
    RecordProperty("sound", static_cast<int>(sound));
    RecordProperty("named_defects", static_cast<int>(named));
}

// ---------------------------------------------------------------------------
//  Tableau de couverture par paires
// ---------------------------------------------------------------------------
//
// 39 baies, 22 parametres : chacun des 1 009 couples de valeurs des 231 paires
// de parametres figure dans au moins une ligne. Valeurs : extremes des
// curseurs, toutes les valeurs d'enumeration, Recursion 0, 1 et 2, Profile
// Flat ou Chamfer, Extrusion 1 ou 100. Tableau engendre une fois par un
// algorithme glouton (gothic_implementation.md, M2-a), dedoublonne, et fige
// ici. Chaque ligne
// rend un maillage strict ou un rapport qui nomme le defaut (namesADefect).
// Strict : une composante, fermee, manifold, orientee, sans auto-intersection
// ni face degeneree, soudure a 5e-4 x W / 200 (la tolerance suit la baie, dont
// les contours sont rendus a l'echelle W / 200).
//
// Les lignes qui ne sont aujourd'hui ni l'un ni l'autre sont listees par
// famille de defaut. Une ligne listee qui devient stricte ou nommee fait
// echouer le test : la retirer de la liste.

namespace
{
    const char *const kCoverParams[] = {
        "Excess (arch)", "Width", "Body height", "Offset outer", "Offset inner", "Lancet head",
        "Lancet head foils", "Recursion", "Lancet drop", "Lancet excess", "Gap fraction",
        "Lancet layout", "Fillets", "Mouchettes", "Mouchette type", "Rosette", "Rosette foils",
        "Rosette foil count", "Rosette foil type", "Rosette pointedness", "Extrusion (zHeight)",
        "Profile"};
    constexpr std::size_t kCoverParamCount = sizeof(kCoverParams) / sizeof(kCoverParams[0]);

    struct CoverRow
    {
        const char *name;
        std::array<double, kCoverParamCount> values;
    };

    const std::vector<CoverRow> &coverRows()
    {
        static const std::vector<CoverRow> rows = {
        {"cov7_0", {2.5, 600, 600, 40, 40, 1, 8, 2, 0, 2.5, 0.2, 1, 1, 1, 2, 1, 1, 24, 1, 2, 100, 1}},
        {"cov7_1", {0.7, 20, 0, 0, 0, 0, 3, 1, 100, 0.7, 0.01, 0, 0, 0, 1, 0, 0, 3, 0, 0.01, 1, 1}},
        {"cov7_2", {2.5, 600, 600, 40, 40, 1, 8, 0, 100, 0.7, 0.01, 0, 1, 0, 0, 0, 0, 3, 1, 2, 1, 0}},
        {"cov7_3", {0.7, 20, 0, 0, 0, 0, 3, 0, 0, 2.5, 0.2, 1, 0, 1, 0, 1, 1, 24, 0, 0.01, 100, 0}},
        {"cov7_4", {0.7, 600, 0, 40, 0, 1, 8, 2, 100, 2.5, 0.2, 1, 0, 0, 2, 1, 0, 3, 0, 0.01, 1, 0}},
        {"cov7_5", {2.5, 20, 600, 0, 40, 0, 3, 1, 0, 0.7, 0.01, 1, 1, 1, 1, 0, 1, 24, 1, 2, 100, 0}},
        {"cov7_6", {2.5, 20, 600, 0, 40, 0, 8, 2, 0, 0.7, 0.2, 0, 0, 0, 2, 0, 1, 24, 0, 0.01, 1, 1}},
        {"cov7_7", {0.7, 600, 0, 40, 0, 1, 3, 1, 100, 2.5, 0.01, 0, 1, 1, 1, 1, 0, 24, 1, 2, 100, 1}},
        {"cov7_8", {2.5, 600, 600, 0, 0, 1, 8, 1, 100, 0.7, 0.2, 1, 1, 1, 1, 1, 1, 3, 0, 2, 100, 1}},
        {"cov7_9", {0.7, 600, 600, 40, 40, 0, 3, 2, 0, 2.5, 0.01, 1, 0, 0, 2, 0, 0, 3, 1, 2, 100, 1}},
        {"cov7_10", {2.5, 20, 0, 40, 40, 1, 8, 0, 100, 2.5, 0.2, 1, 1, 1, 2, 1, 1, 24, 1, 0.01, 1, 1}},
        {"cov7_11", {2.5, 600, 600, 40, 40, 1, 8, 2, 100, 2.5, 0.2, 1, 1, 1, 0, 1, 1, 24, 1, 2, 100, 1}},
        {"cov7_12", {2.5, 600, 600, 40, 40, 1, 8, 1, 100, 2.5, 0.2, 1, 1, 1, 2, 1, 1, 24, 1, 2, 100, 1}},
        {"cov7_13", {2.5, 600, 600, 40, 40, 1, 8, 2, 100, 2.5, 0.2, 1, 1, 1, 1, 1, 1, 24, 1, 2, 100, 1}},
        {"cov7_14", {2.5, 600, 600, 40, 40, 1, 8, 0, 100, 2.5, 0.2, 1, 1, 1, 1, 1, 1, 24, 1, 2, 100, 1}},
        {"cov7_15", {2.5, 600, 600, 40, 40, 1, 8, 1, 100, 2.5, 0.2, 1, 1, 1, 0, 1, 1, 24, 1, 2, 100, 1}},
        {"cov8_0", {2.5, 600, 600, 40, 40, 1, 8, 2, 100, 2.5, 0.2, 0, 1, 1, 2, 1, 1, 24, 1, 2, 100, 1}},
        {"cov8_1", {0.7, 20, 0, 0, 0, 0, 3, 1, 0, 0.7, 0.2, 1, 0, 0, 1, 0, 0, 3, 0, 0.01, 1, 0}},
        {"cov8_2", {0.7, 600, 600, 40, 0, 0, 8, 0, 0, 0.7, 0.01, 1, 1, 1, 0, 0, 1, 24, 1, 0.01, 100, 0}},
        {"cov8_3", {2.5, 20, 0, 0, 40, 1, 3, 0, 100, 2.5, 0.01, 0, 0, 0, 0, 1, 0, 3, 0, 2, 1, 1}},
        {"cov8_4", {2.5, 600, 0, 0, 40, 1, 8, 2, 0, 0.7, 0.01, 1, 0, 0, 2, 1, 1, 3, 0, 0.01, 100, 0}},
        {"cov8_5", {2.5, 20, 600, 40, 0, 0, 3, 1, 100, 2.5, 0.01, 0, 1, 1, 1, 0, 0, 24, 1, 2, 1, 1}},
        {"cov8_6", {0.7, 20, 0, 0, 0, 1, 3, 2, 0, 0.7, 0.2, 0, 1, 1, 2, 0, 1, 3, 1, 2, 1, 1}},
        {"cov8_7", {0.7, 600, 600, 40, 40, 1, 8, 1, 100, 2.5, 0.2, 1, 0, 0, 1, 1, 0, 24, 0, 0.01, 100, 1}},
        {"cov8_8", {2.5, 600, 0, 40, 40, 0, 3, 2, 100, 2.5, 0.2, 1, 1, 1, 0, 1, 0, 24, 0, 2, 100, 0}},
        {"cov8_9", {2.5, 20, 600, 0, 0, 0, 8, 0, 0, 2.5, 0.2, 0, 0, 1, 2, 1, 0, 24, 1, 0.01, 100, 0}},
        {"cov8_10", {2.5, 600, 600, 40, 40, 1, 8, 1, 100, 0.7, 0.2, 1, 1, 0, 2, 0, 1, 3, 1, 2, 1, 1}},
        {"cov9_0", {2.5, 600, 600, 40, 40, 1, 8, 2, 100, 2.5, 0.2, 1, 1, 1, 2, 1, 1, 24, 1, 2, 100, 1}},
        {"cov9_1", {0.7, 20, 0, 0, 0, 0, 3, 1, 0, 0.7, 0.01, 0, 0, 0, 1, 0, 0, 3, 1, 0.01, 1, 0}},
        {"cov9_2", {0.7, 20, 0, 0, 0, 0, 3, 0, 100, 0.7, 0.2, 1, 0, 1, 0, 1, 1, 24, 0, 2, 100, 1}},
        {"cov9_3", {2.5, 600, 600, 40, 40, 1, 8, 0, 0, 2.5, 0.01, 0, 1, 0, 0, 0, 0, 3, 0, 0.01, 1, 0}},
        {"cov9_4", {2.5, 20, 0, 0, 40, 1, 8, 2, 100, 0.7, 0.01, 1, 0, 0, 2, 0, 0, 24, 0, 0.01, 1, 1}},
        {"cov9_5", {0.7, 600, 600, 40, 0, 0, 8, 1, 0, 2.5, 0.2, 0, 1, 1, 1, 1, 1, 3, 0, 2, 100, 0}},
        {"cov9_6", {0.7, 600, 0, 40, 0, 1, 3, 2, 0, 0.7, 0.2, 0, 1, 1, 2, 0, 0, 3, 1, 2, 1, 1}},
        {"cov9_7", {2.5, 600, 600, 0, 40, 1, 3, 1, 100, 2.5, 0.01, 1, 0, 0, 1, 1, 1, 24, 1, 0.01, 100, 0}},
        {"cov9_8", {2.5, 20, 600, 40, 40, 0, 8, 2, 100, 2.5, 0.2, 0, 1, 0, 2, 1, 1, 24, 1, 2, 1, 0}},
        {"cov9_9", {2.5, 600, 600, 40, 0, 1, 8, 0, 100, 0.7, 0.01, 1, 0, 1, 1, 0, 1, 3, 1, 2, 100, 1}},
        {"cov9_10", {0.7, 600, 0, 0, 40, 1, 8, 1, 0, 2.5, 0.2, 1, 1, 1, 0, 1, 0, 24, 1, 0.01, 100, 1}},
        {"cov9_14", {2.5, 600, 600, 40, 40, 1, 8, 0, 100, 2.5, 0.2, 1, 1, 1, 2, 1, 1, 24, 1, 2, 100, 1}},
        };
        return rows;
    }

    struct KnownCoverFailures
    {
        const char              *family;
        std::vector<std::string> rows;
    };

    // Lignes ni strictes ni nommees, par famille (gothic_implementation.md, M2).
    // Aucune aujourd'hui.
    const std::vector<KnownCoverFailures> &knownCoverFailures()
    {
        static const std::vector<KnownCoverFailures> known = {};
        return known;
    }

    MeshValidationReport validateStrict(const Mesh &mesh, double width)
    {
        MeshValidationOptions options;
        options.weldTolerance      = 5e-4 * width / 200.0;
        options.expectedComponents = 1u;
        return ValidateMesh(mesh, options);
    }

    bool isStrict(const Mesh &mesh, const MeshValidationReport &r)
    {
        if (mesh.GetNFaces() <= 1u || r.components != 1u || r.selfIntersectionTruncated)
            return false;
        for (std::size_t k = 0; k < kMeshDefectCount; ++k)
            if (r.counts[k])
                return false;
        return true;
    }

    std::string describeDefects(const MeshValidationReport &r)
    {
        std::string out = "composantes " + std::to_string(r.components);
        for (std::size_t k = 0; k < kMeshDefectCount; ++k)
            if (r.counts[k])
                out += std::string(", ") + MeshDefectName(static_cast<MeshDefect>(k)) + " " + std::to_string(r.counts[k]);
        return out;
    }
}

TEST(TEST_cgmesh_gothic_build_report, pairwise_covering_array_gives_a_strict_mesh_or_a_named_error)
{
    ASSERT_EQ(coverRows().size(), 39u);
    std::set<std::string> listed;
    for (const KnownCoverFailures &f : knownCoverFailures())
        listed.insert(f.rows.begin(), f.rows.end());
    for (const std::string &name : listed)
        EXPECT_TRUE(std::any_of(coverRows().begin(), coverRows().end(),
                                [&name](const CoverRow &row) { return name == row.name; }))
            << "ligne listee inconnue : " << name;

    std::size_t strict = 0, named = 0, failing = 0;
    for (const CoverRow &row : coverRows())
    {
        SCOPED_TRACE(row.name);
        ParameterizedGothicWindow shape;
        for (std::size_t k = 0; k < kCoverParamCount; ++k)
            setParam(shape, kCoverParams[k], row.values[k]);
        const auto mesh = regenerate(shape);
        ASSERT_TRUE(mesh);
        const MeshValidationReport r = validateStrict(*mesh, shape.Settings().width);
        const bool ok        = isStrict(*mesh, r);
        const bool diagnosed = namesADefect(shape.LastBuildReport());
        const bool fails     = !ok && !diagnosed;
        strict  += ok;
        named   += !ok && diagnosed;
        failing += fails;
        if (listed.count(row.name))
            EXPECT_TRUE(fails) << "ligne listee devenue stricte ou nommee : la retirer de knownCoverFailures";
        else
            EXPECT_FALSE(fails) << "ni strict ni nomme : " << describeDefects(r);
    }
    RecordProperty("strict", static_cast<int>(strict));
    RecordProperty("named", static_cast<int>(named));
    RecordProperty("listed_failures", static_cast<int>(failing));
}

// API bas niveau : une unite recursive dont l'offset ne se construit pas n'a
// pas de vide ; elle est enregistree.
TEST(TEST_cgmesh_gothic_build_report, recursive_unit_without_offset_is_reported)
{
    WindowInstance wi;
    wi.archBasis.pL = Vector2d(-100.0, 0.0);
    wi.archBasis.pR = Vector2d( 100.0, 0.0);
    wi.archBasis.excess = 1.0;
    wi.archOffset.outer = 16.0;
    wi.archOffset.inner = 10.0;
    wi.subwindowParams.count = 2;
    wi.subwindowParams.gap.mode = SubwindowParams::Gap::Mode::Fraction;
    wi.subwindowParams.gap.gapFraction = 0.11;
    wi.hasRosette = false;
    const WindowGeometry geom = buildGeometryFromInstance(wi);
    ASSERT_EQ(geom.subwindows.lancets.size(), 2u);

    GothicMeshParams mp;
    mp.recursionDepth  = 1;
    mp.recursionOffset = wi.archOffset;
    mp.recursionOffset.inner = 1000.0;
    mp.recursionSub    = wi.subwindowParams;
    GothicBuildReport r;
    EXPECT_NO_THROW(buildBayStonePolygon(geom, mp, &r));
    std::size_t unit = 0;
    for (const GothicDiagnostic &d : entriesOf(r, GothicBuildWarning::FeatureDropped))
        unit += (d.message.rfind("recursive unit offset:", 0) == 0);
    EXPECT_EQ(unit, 2u);
}

// ---------------------------------------------------------------------------
//  Region de niveau 0, bandes minimales
// ---------------------------------------------------------------------------

namespace
{
    std::vector<Vector2d> rectRing(double x0, double y0, double x1, double y1)
    {
        return {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
    }

    // Cercle en sens horaire (trou), premier sommet a l'angle 0.
    std::vector<Vector2d> holeCircle(double cx, double cy, double r, int n = 72)
    {
        std::vector<Vector2d> ring;
        for (int i = 0; i < n; ++i)
        {
            const double t = -2.0 * 3.14159265358979323846 * i / n;
            ring.emplace_back(cx + r * std::cos(t), cy + r * std::sin(t));
        }
        return ring;
    }

    double polygonArea(const Polygon2 &p)
    {
        double s = 0.0;
        for (int c = 0; c < p.get_n_contours(); ++c)
        {
            const float *q = p.get_points(c);
            const unsigned n = p.get_n_points(c);
            for (unsigned i = 0; i < n; ++i)
            {
                const unsigned j = (i + 1) % n;
                s += 0.5 * (static_cast<double>(q[2 * i]) * q[2 * j + 1] - static_cast<double>(q[2 * j]) * q[2 * i + 1]);
            }
        }
        return s;
    }

    // Maillage strict (validateStrict / isStrict) et rapport sans erreur.
    void expectStrictMesh(ParameterizedGothicWindow &shape)
    {
        const auto mesh = regenerate(shape);
        ASSERT_TRUE(mesh);
        const MeshValidationReport r = validateStrict(*mesh, shape.Settings().width);
        EXPECT_TRUE(isStrict(*mesh, r)) << describeDefects(r);
        EXPECT_FALSE(shape.LastBuildReport().HasErrors()) << GothicBuildReportToJson(shape.LastBuildReport());
    }

    std::size_t clampsOf(const GothicBuildReport &r, const std::string &parameter)
    {
        std::size_t n = 0;
        for (const GothicDiagnostic &d : entriesOf(r, GothicBuildWarning::ParameterClamped))
            n += (d.parameter == parameter);
        return n;
    }

    // rand9 : Prototype, l'offset interieur consume les sous-lancettes.
    const ParamList kPrototypeInnerOverflowBay = {
        {"Width", 63.4856}, {"Excess (arch)", 1.76268}, {"Body height", 42.0131},
        {"Offset outer", 9.28533}, {"Offset inner", 29.6697}, {"Lancet head", 0},
        {"Lancet head foils", 3}, {"Recursion", 2}, {"Lancet drop", 64.0345},
        {"Lancet excess", 1.87165}, {"Gap fraction", 0.0834936}, {"Lancet layout", 1},
        {"Fillets", 0}, {"Mouchettes", 1}, {"Mouchette type", 0}, {"Rosette", 0},
        {"Rosette foils", 0}, {"Rosette foil count", 6}, {"Rosette foil type", 0},
        {"Rosette pointedness", 0.391369}, {"Extrusion (zHeight)", 3.02419}};
}

// Niveau 0 : l'assemblage direct est garde s'il est valide ; sinon la region
// est un seul booleen, garde comprise, enregistre region_repaired. La rosace a
// lobes ronds de la fenetre par defaut se pince ; sans plancher de bande,
// l'offset exterieur nul colle les lancettes au cadre. La region rendue est
// valide avec ou sans rapport, et le maillage strict.
TEST(TEST_cgmesh_gothic_build_report, level_0_region_defect_is_repaired)
{
    GothicFeatureThresholds noBand;
    noBand.minBand = 0.0;
    struct Case { const char *name; ParamList params; GothicFeatureThresholds thresholds; std::size_t repaired; };
    const std::vector<Case> cases = {
        {"defaut", {}, {}, 0},
        {"Recursion 1", {{"Recursion", 1}}, {}, 0},
        {"rosace a lobes ronds", {{"Rosette foil type", 0}}, {}, 1},
        {"Offset outer 0, sans plancher", {{"Offset outer", 0}}, noBand, 1},
    };
    for (const Case &c : cases)
    {
        SCOPED_TRACE(c.name);
        ParameterizedGothicWindow shape;
        applyParams(shape, c.params);
        shape.SetFeatureThresholds(c.thresholds);
        GothicBuildReport r;
        const Polygon2 reported = shape.BuildStonePolygon(&r);
        EXPECT_EQ(r.Count(GothicBuildWarning::RegionRepaired), c.repaired);
        EXPECT_EQ(r.Count(GothicBuildWarning::RegionInvalid), 0u);
        const std::vector<GothicDiagnostic> d = entriesOf(r, GothicBuildWarning::RegionRepaired);
        ASSERT_EQ(d.size(), c.repaired);
        if (c.repaired)
            EXPECT_FALSE(d[0].message.empty());

        const Polygon2 silent = shape.BuildStonePolygon();
        EXPECT_TRUE(ValidateContours(silent).IsValid());
        EXPECT_TRUE(samePolygon(silent, reported)) << "la region depend du rapport";

        expectStrictMesh(shape);
        EXPECT_EQ(shape.LastBuildReport().Count(GothicBuildWarning::RegionRepaired), c.repaired);
    }
}

// Une region de niveau 0 encore invalide apres le booleen et la garde est
// rendue, sans exception, et enregistree region_invalid (ses defauts) : un vide
// tangent au cadre, en un sommet, n'est decolle par aucun des deux. Temoins :
// le meme vide decolle du cadre (assemblage direct garde, identique), deux
// vides tangents entre eux (repares).
TEST(TEST_cgmesh_gothic_build_report, level_0_region_still_invalid_is_reported)
{
    const std::vector<Vector2d> frame = rectRing(0, 0, 100, 100);
    {
        SCOPED_TRACE("vide tangent au cadre");
        const std::vector<std::vector<Vector2d>> holes = {holeCircle(50, 15, 15)};
        GothicBuildReport r;
        Polygon2 p;
        ASSERT_NO_THROW(p = buildLevel0StoneRegion(frame, holes, &r));
        EXPECT_GE(p.get_n_contours(), 1);
        EXPECT_FALSE(ValidateContours(p).IsValid());
        EXPECT_EQ(r.Count(GothicBuildWarning::RegionRepaired), 0u);
        const std::vector<GothicDiagnostic> d = entriesOf(r, GothicBuildWarning::RegionInvalid);
        ASSERT_EQ(d.size(), 1u);
        EXPECT_FALSE(d[0].message.empty());
        EXPECT_FALSE(r.HasErrors());
        Polygon2 silent;
        ASSERT_NO_THROW(silent = buildLevel0StoneRegion(frame, holes));
        EXPECT_TRUE(samePolygon(silent, p)) << "la region depend du rapport";

        // La region rendue est celle du booleen et de la garde, pas
        // l'assemblage direct (cadre et trou tels quels).
        Polygon2 direct;
        direct.alloc_contours(2);
        for (int c = 0; c < 2; ++c)
        {
            const std::vector<Vector2d> &ring = (c == 0) ? frame : holes[0];
            std::vector<float> packed;
            for (const Vector2d &v : ring)
            {
                packed.push_back(static_cast<float>(v.x));
                packed.push_back(static_cast<float>(v.y));
            }
            direct.add_contour(static_cast<unsigned>(c), static_cast<unsigned>(ring.size()), packed.data());
        }
        EXPECT_FALSE(samePolygon(p, direct)) << "l'assemblage direct est rendu au lieu de la region reparee";
    }
    {
        SCOPED_TRACE("vide decolle");
        const std::vector<std::vector<Vector2d>> holes = {holeCircle(50, 30, 15)};
        GothicBuildReport r;
        const Polygon2 p = buildLevel0StoneRegion(frame, holes, &r);
        EXPECT_FALSE(r.HasWarnings());
        ASSERT_EQ(p.get_n_contours(), 2);
        EXPECT_EQ(p.get_n_points(1), 72u);
        EXPECT_EQ(p.get_points(1)[0], 65.f);
    }
    {
        SCOPED_TRACE("vides tangents entre eux");
        const std::vector<std::vector<Vector2d>> holes = {holeCircle(30, 50, 15), holeCircle(60, 50, 15)};
        GothicBuildReport r;
        const Polygon2 p = buildLevel0StoneRegion(frame, holes, &r);
        EXPECT_EQ(r.Count(GothicBuildWarning::RegionRepaired), 1u);
        EXPECT_TRUE(ValidateContours(p).IsValid());
    }
}

// Par Regenerate : une region de niveau 0 qui reste invalide apres reparation
// ne fait pas lever la construction. Le maillage est rendu ; strict, le rapport
// porte region_invalid seul ; non strict, il porte aussi l'erreur
// mesh_not_strict (la garde d'export refuse alors). Jamais un maillage
// defectueux sans erreur, jamais de triangle de repli. Baies : rosace elancee a
// 21 lobes ronds dans les bornes de l'interface (corps canonique ≈ 5 655), et
// baie par defaut a corps 6 000, 7 000 (auto-intersection 2D), 8 000, 1e5 et
// 1e6 (aretes courtes devant la tolerance relative du validateur ; a 1e6 la
// pierre n'est plus stricte).
TEST(TEST_cgmesh_gothic_build_report, level_0_invalid_region_errs_only_if_the_mesh_is_not_strict)
{
    std::vector<std::pair<std::string, GothicWindowSettings>> bays;
    {
        ParameterizedGothicWindow shape;
        applyParams(shape, {
            {"Width", 20.3285446}, {"Excess (arch)", 1.69816482}, {"Body height", 574.609436},
            {"Offset outer", 28.2489223}, {"Offset inner", 17.8723812}, {"Lancet head", 1},
            {"Lancet head foils", 8}, {"Recursion", 0}, {"Lancet drop", 11.0167351},
            {"Lancet excess", 1.9512527}, {"Gap fraction", 0.197894603}, {"Lancet layout", 0},
            {"Fillets", 1}, {"Mouchettes", 0}, {"Mouchette type", 2}, {"Rosette", 1},
            {"Rosette foils", 1}, {"Rosette foil count", 21}, {"Rosette foil type", 0},
            {"Rosette pointedness", 1.23699617}, {"Extrusion (zHeight)", 20}});
        bays.emplace_back("21 lobes ronds", shape.Settings());
    }
    for (const float body : {6000.f, 7000.f, 8000.f, 1e5f, 1e6f})
    {
        GothicWindowSettings s;
        s.bodyHeight = body;
        bays.emplace_back("corps " + std::to_string(body), s);
    }
    std::size_t warningBranch = 0, errorBranch = 0;
    for (const auto &[name, settings] : bays)
    {
        SCOPED_TRACE(name);
        ParameterizedGothicWindow shape;
        shape.SetSettings(settings);
        const auto mesh = regenerate(shape);
        ASSERT_TRUE(mesh);
        EXPECT_GT(mesh->GetNFaces(), 1u);
        const GothicBuildReport &r = shape.LastBuildReport();
        EXPECT_EQ(r.Count(GothicBuildError::ConstructionFailed), 0u);
        EXPECT_EQ(r.Count(GothicBuildWarning::RegionInvalid), 1u) << "gabarit non discriminant";
        const MeshValidationReport v = validateStrict(*mesh, shape.Settings().width);
        const bool strict = isStrict(*mesh, v);
        EXPECT_EQ(r.Count(GothicBuildError::MeshNotStrict), strict ? 0u : 1u)
            << describeDefects(v) << " " << GothicBuildReportToJson(r);
        EXPECT_EQ(r.HasErrors(), !strict);
        warningBranch += (r.Count(GothicBuildWarning::RegionInvalid) == 1u && !r.HasErrors());
        errorBranch   += (r.Count(GothicBuildError::MeshNotStrict) == 1u);
        GothicBuildReport polygonReport;
        EXPECT_NO_THROW(shape.BuildStonePolygon(&polygonReport));
        EXPECT_EQ(polygonReport.Count(GothicBuildWarning::RegionInvalid), 1u);
    }
    EXPECT_GE(warningBranch, 1u) << "couverture perdue : aucune baie ne prend la branche region_invalid seul (pierre stricte)";
    EXPECT_GE(errorBranch, 1u) << "couverture perdue : aucune baie ne prend la branche mesh_not_strict (pierre non stricte)";
}

// Le verdict porte sur la pierre seule : les barres d'un profil en barre
// (coques posees, multiple_shells) n'en font pas un maillage non strict, et une
// feuille plane (Extrusion 0) admet ses aretes de bord. Corps 7 000 : region
// invalide, pierre stricte.
TEST(TEST_cgmesh_gothic_build_report, level_0_verdict_judges_the_stone_only)
{
    for (const auto &[name, profile, zHeight] : {std::make_tuple("Roll bar", 2, 20.f),
                                                 std::make_tuple("Extrusion 0", 0, 0.f)})
    {
        SCOPED_TRACE(name);
        GothicWindowSettings s;
        s.bodyHeight = 7000.f;
        s.profile    = profile;
        s.zHeight    = zHeight;
        ParameterizedGothicWindow shape;
        shape.SetSettings(s);
        const auto mesh = regenerate(shape);
        ASSERT_TRUE(mesh);
        const GothicBuildReport &r = shape.LastBuildReport();
        EXPECT_EQ(r.Count(GothicBuildWarning::RegionInvalid), 1u) << "gabarit non discriminant";
        EXPECT_FALSE(r.HasErrors()) << GothicBuildReportToJson(r);
    }
}

// Disposition Prototype : l'offset exterieur est borne a 0,4 x largeur et
// l'offset interieur (le meneau) a (largeur - 2 x exterieur) / 6. Les baies
// rand6 et rand9, dont les offsets demandes depassent ces bornes, sont strictes
// et le clamp est nomme.
TEST(TEST_cgmesh_gothic_build_report, prototype_offsets_are_bounded)
{
    const std::vector<std::pair<const char *, ParamList>> bays = {
        {"rand6", kInnerOffsetCollapseBay}, {"rand9", kPrototypeInnerOverflowBay}};
    for (const auto &[name, params] : bays)
    {
        SCOPED_TRACE(name);
        ParameterizedGothicWindow shape;
        applyParams(shape, params);
        expectStrictMesh(shape);
        EXPECT_EQ(clampsOf(shape.LastBuildReport(), "Offset inner"), 1u);
    }
    {
        SCOPED_TRACE("Offset outer 0,45 x largeur");
        ParameterizedGothicWindow shape;
        applyParams(shape, {{"Lancet layout", 1}, {"Width", 80}, {"Offset outer", 36}, {"Offset inner", 1}});
        GothicBuildReport r;
        EXPECT_NO_THROW(shape.BuildStonePolygon(&r));
        EXPECT_EQ(clampsOf(r, "Offset outer"), 1u);
    }
}

// Plancher de bande (minBand, unites monde) : l'offset exterieur, et en
// disposition Prototype le meneau (l'offset interieur), n'y descendent pas ;
// le clamp est nomme. A minBand nul, aucun plancher.
TEST(TEST_cgmesh_gothic_build_report, band_floor_clamps_the_outer_offset_and_the_prototype_mullion)
{
    {
        SCOPED_TRACE("Offset outer 0");
        ParameterizedGothicWindow shape;
        setParam(shape, "Offset outer", 0);
        GothicBuildReport r;
        shape.BuildStonePolygon(&r);
        EXPECT_EQ(clampsOf(r, "Offset outer"), 1u);
        expectStrictMesh(shape);
    }
    {
        SCOPED_TRACE("Offset outer 0,2, largeur 20");
        ParameterizedGothicWindow shape;
        applyParams(shape, {{"Width", 20}, {"Offset outer", 0.2}, {"Offset inner", 1}});
        GothicBuildReport r;
        shape.BuildStonePolygon(&r);
        EXPECT_EQ(clampsOf(r, "Offset outer"), 1u);
    }
    {
        SCOPED_TRACE("Prototype, Offset inner 0");
        ParameterizedGothicWindow shape;
        applyParams(shape, {{"Lancet layout", 1}, {"Offset inner", 0}});
        GothicBuildReport r;
        shape.BuildStonePolygon(&r);
        EXPECT_EQ(clampsOf(r, "Offset inner"), 1u);
        expectStrictMesh(shape);
    }
    {
        SCOPED_TRACE("Uniform, Offset inner 0 : pas de meneau Prototype");
        ParameterizedGothicWindow shape;
        setParam(shape, "Offset inner", 0);
        GothicBuildReport r;
        shape.BuildStonePolygon(&r);
        EXPECT_EQ(clampsOf(r, "Offset inner"), 0u);
    }
    {
        SCOPED_TRACE("minBand 0");
        GothicFeatureThresholds t;
        t.minBand = 0.0;
        ParameterizedGothicWindow shape;
        applyParams(shape, {{"Offset outer", 0}, {"Lancet layout", 1}, {"Offset inner", 0}});
        shape.SetFeatureThresholds(t);
        GothicBuildReport r;
        shape.BuildStonePolygon(&r);
        EXPECT_EQ(clampsOf(r, "Offset outer"), 0u);
        EXPECT_EQ(clampsOf(r, "Offset inner"), 0u);
    }
}

namespace
{
    // rand163 : buildRosette ne trouve pas d'intersection de l'ellipse avec
    // l'axe entre les apex (disposition Uniform).
    const ParamList kRosetteConstructionFailureBay2 = {
        {"Width", 21.5907}, {"Excess (arch)", 1.08408}, {"Body height", 521.027},
        {"Offset outer", 9.31625}, {"Offset inner", 38.2191}, {"Lancet head", 0},
        {"Lancet head foils", 7}, {"Recursion", 1}, {"Lancet drop", 27.7488},
        {"Lancet excess", 1.45733}, {"Gap fraction", 0.0984745}, {"Lancet layout", 0},
        {"Fillets", 1}, {"Mouchettes", 0}, {"Mouchette type", 0}, {"Rosette", 1},
        {"Rosette foils", 0}, {"Rosette foil count", 5}, {"Rosette foil type", 0},
        {"Rosette pointedness", 1.59322}, {"Extrusion (zHeight)", 89.4946}};

    // rand58 : disposition Prototype, offsets bornes ; buildRosette leve.
    const ParamList kRosetteConstructionFailureBay3 = {
        {"Width", 38.7266}, {"Excess (arch)", 0.775579}, {"Body height", 386.694},
        {"Offset outer", 15.2847}, {"Offset inner", 21.1296}, {"Lancet head", 0},
        {"Lancet head foils", 5}, {"Recursion", 0}, {"Lancet drop", 41.2538},
        {"Lancet excess", 2.26412}, {"Gap fraction", 0.0885809}, {"Lancet layout", 1},
        {"Fillets", 1}, {"Mouchettes", 1}, {"Mouchette type", 2}, {"Rosette", 1},
        {"Rosette foils", 1}, {"Rosette foil count", 21}, {"Rosette foil type", 0},
        {"Rosette pointedness", 0.978325}, {"Extrusion (zHeight)", 5.60959}};
}

// Une rosace que buildRosette ne sait pas construire est abandonnee : la baie
// est celle sans rosace, stricte, et feature_dropped nomme « Rosette » avec le
// texte de l'exception.
TEST(TEST_cgmesh_gothic_build_report, rosette_construction_failure_drops_the_rosette)
{
    const std::vector<std::pair<const char *, ParamList>> bays = {
        {"rand50", kRosetteConstructionFailureBay}, {"rand163", kRosetteConstructionFailureBay2},
        {"rand58", kRosetteConstructionFailureBay3}};
    for (const auto &[name, params] : bays)
    {
        SCOPED_TRACE(name);
        ParameterizedGothicWindow with, without;
        applyParams(with, params);
        applyParams(without, params);
        setParam(without, "Rosette", 0);
        expectStrictMesh(with);
        std::size_t named = 0;
        for (const GothicDiagnostic &d : entriesOf(with.LastBuildReport(), GothicBuildWarning::FeatureDropped))
            if (d.parameter == "Rosette")
            {
                ++named;
                EXPECT_NE(d.message.find("buildRosette"), std::string::npos) << d.message;
            }
        EXPECT_EQ(named, 1u);
        EXPECT_TRUE(samePolygon(with.BuildStonePolygon(), without.BuildStonePolygon()));
    }
}

// Retrait des ecoincons : max(offset interieur, minFilletInnerOffset), monotone.
// La region de pierre ne perd pas de surface quand l'offset interieur croit, en
// particulier au passage du seuil.
TEST(TEST_cgmesh_gothic_build_report, spandrel_inset_is_monotone_in_the_inner_offset)
{
    double previous = 0.0;
    for (const double inner : {0.0, 0.5, 0.999, 1.0, 1.001, 1.5, 4.0})
    {
        SCOPED_TRACE("Offset inner " + std::to_string(inner));
        ParameterizedGothicWindow shape;
        setParam(shape, "Offset inner", inner);
        const double area = polygonArea(shape.BuildStonePolygon());
        EXPECT_GE(area, previous);
        previous = area;
    }
}

// ---------------------------------------------------------------------------
//  Barres de moulure (O1)
// ---------------------------------------------------------------------------

namespace
{
    // Distance de (x, y) au contour 0 de `p` s'il est dehors, 0 dedans.
    double distanceOutsideContour0(const Polygon2 &p, double x, double y)
    {
        const float *q = p.get_points(0);
        const unsigned n = p.get_n_points(0);
        bool inside = false;
        double d = std::numeric_limits<double>::infinity();
        for (unsigned i = 0, j = n - 1; i < n; j = i++)
        {
            const double xi = q[2 * i], yi = q[2 * i + 1], xj = q[2 * j], yj = q[2 * j + 1];
            if (((yi > y) != (yj > y)) && (x < (xj - xi) * (y - yi) / (yj - yi) + xi))
                inside = !inside;
            const double dx = xj - xi, dy = yj - yi, l2 = dx * dx + dy * dy;
            double t = l2 > 0.0 ? ((x - xi) * dx + (y - yi) * dy) / l2 : 0.0;
            t = std::min(1.0, std::max(0.0, t));
            d = std::min(d, std::hypot(xi + t * dx - x, yi + t * dy - y));
        }
        return inside ? 0.0 : d;
    }

    // rand232 : Recursion 2, une sous-lancette descend sous l'appui.
    const ParamList kRecursiveBarBelowThePlateBay = {
        {"Width", 248.623}, {"Excess (arch)", 0.764553}, {"Body height", 129.55},
        {"Offset outer", 3.36303}, {"Offset inner", 0.109013}, {"Lancet head", 0},
        {"Lancet head foils", 4}, {"Recursion", 2}, {"Lancet drop", 92.3358},
        {"Lancet excess", 0.713169}, {"Gap fraction", 0.0435457}, {"Lancet layout", 0},
        {"Fillets", 1}, {"Mouchettes", 1}, {"Mouchette type", 1}, {"Rosette", 1},
        {"Rosette foils", 0}, {"Rosette foil count", 8}, {"Rosette foil type", 1},
        {"Rosette pointedness", 0.497155}, {"Extrusion (zHeight)", 86.9402}};

    // rand297 : Recursion 2, Prototype ; un contour deborde de sa fenetre de
    // moins d'un dixieme de son etendue.
    const ParamList kRecursiveBarSlightlyOffItsWindowBay = {
        {"Width", 439.616}, {"Excess (arch)", 1.92983}, {"Body height", 137.21},
        {"Offset outer", 3.39566}, {"Offset inner", 37.4011}, {"Lancet head", 1},
        {"Lancet head foils", 6}, {"Recursion", 2}, {"Lancet drop", 68.8143},
        {"Lancet excess", 2.32245}, {"Gap fraction", 0.0834157}, {"Lancet layout", 1},
        {"Fillets", 0}, {"Mouchettes", 0}, {"Mouchette type", 2}, {"Rosette", 1},
        {"Rosette foils", 1}, {"Rosette foil count", 12}, {"Rosette foil type", 0},
        {"Rosette pointedness", 1.20685}, {"Extrusion (zHeight)", 32.5167}};

    // rand260 : Recursion 1.
    const ParamList kRecursion1BarOffThePlateBay = {
        {"Width", 72.2615}, {"Excess (arch)", 1.3956}, {"Body height", 45.5078},
        {"Offset outer", 22.7049}, {"Offset inner", 6.69535}, {"Lancet head", 0},
        {"Lancet head foils", 4}, {"Recursion", 1}, {"Lancet drop", 27.0192},
        {"Lancet excess", 1.3526}, {"Gap fraction", 0.0285681}, {"Lancet layout", 0},
        {"Fillets", 0}, {"Mouchettes", 1}, {"Mouchette type", 0}, {"Rosette", 0},
        {"Rosette foils", 1}, {"Rosette foil count", 3}, {"Rosette foil type", 0},
        {"Rosette pointedness", 0.699592}, {"Extrusion (zHeight)", 68.4045}};
}

// Les contours des moulures recursives sont bornes a leur fenetre, comme les
// sous-vides de la pierre : aucune barre ne deborde de la plaque de plus que
// son rayon. Barre explicite de rayon 1 (unites monde), fusionnee a la plaque :
// aucun sommet du maillage n'est a plus du rayon hors du contour exterieur de
// la pierre, a 5 pas de la grille de fusion pres (largeur / 200 / 128 par pas,
// en unites monde : arrondi, simplification et ouverture des niveaux).
TEST(TEST_cgmesh_gothic_build_report, recursive_mouldings_stay_on_the_plate)
{
    const std::vector<std::pair<const char *, ParamList>> bays = {
        {"rand232", kRecursiveBarBelowThePlateBay}, {"rand297", kRecursiveBarSlightlyOffItsWindowBay},
        {"rand260", kRecursion1BarOffThePlateBay}};
    for (const auto &[name, params] : bays)
    {
        SCOPED_TRACE(name);
        ParameterizedGothicWindow flat;
        applyParams(flat, params);
        const auto plate = regenerate(flat);
        ASSERT_TRUE(plate);
        const Polygon2 stone = flat.BuildStonePolygon();
        const double step = flat.Settings().width / 200.0 / 128.0;

        ParameterizedGothicWindow barred;
        applyParams(barred, params);
        const Profile2D bar = rollBarProfile(1.0);
        barred.SetBarProfile(&bar);
        const auto mesh = regenerate(barred);
        ASSERT_TRUE(mesh);
        EXPECT_EQ(barred.LastBuildReport().Count(GothicBuildWarning::MouldingNotFused), 0u);
        double worst = 0.0;
        for (unsigned i = 0; i < mesh->GetNVertices(); ++i)
        {
            float v[3];
            mesh->GetVertex(i, v);
            worst = std::max(worst, distanceOutsideContour0(stone, v[0], v[1]));
        }
        EXPECT_GT(worst, 0.5) << "aucune barre ne deborde : le cas ne mesure rien";
        EXPECT_LE(worst, 1.0 + 5.0 * step);
    }
}

// Un contour de moulure recursif entierement hors de sa fenetre n'a pas de
// barre : il est enregistre moulding_dropped.
TEST(TEST_cgmesh_gothic_build_report, moulding_outline_outside_its_window_is_reported)
{
    GothicWindowSettings s;
    s.width = 469.651367f; s.excess = 0.725439072f; s.bodyHeight = 39.0467529f;
    s.offsetOuter = 36.5019646f; s.offsetInner = 6.45578766f; s.lancetHead = 1; s.lancetHeadFoils = 7;
    s.recursion = 1; s.subDrop = 79.3601303f; s.subExcess = 1.12045622f; s.gapFraction = 0.0430123918f;
    s.lancetLayout = 1; s.fillets = false; s.mouchettes = false; s.mouchetteType = 1; s.rosette = false;
    s.rosetteFoils = true; s.rosetteFoilCount = 7; s.rosetteFoilType = 1; s.rosetteFoilPointed = 1.27657163f;
    s.zHeight = 89.4607315f; s.profile = 2;
    ParameterizedGothicWindow shape;
    shape.SetSettings(s);
    const auto mesh = regenerate(shape);
    ASSERT_TRUE(mesh);
    std::size_t outside = 0;
    for (const GothicDiagnostic &d : entriesOf(shape.LastBuildReport(), GothicBuildWarning::MouldingDropped))
        outside += (d.message == "moulding outline: outside its window");
    EXPECT_GE(outside, 1u);
}

// Les barres des positions Roll, Keel et Ogee du menu sont fusionnees a la
// plaque : ni multiple_shells ni moulding_not_fused, et sans barre (Flat,
// Chamfer), rien.
TEST(TEST_cgmesh_gothic_build_report, menu_bars_are_fused_without_warning)
{
    for (int profile = 0; profile <= 4; ++profile)
    {
        SCOPED_TRACE("Profile " + std::to_string(profile));
        ParameterizedGothicWindow shape;
        setParam(shape, "Profile", profile);
        const auto mesh = regenerate(shape);
        ASSERT_TRUE(mesh);
        EXPECT_EQ(shape.LastBuildReport().Count(GothicBuildWarning::MultipleShells), 0u);
        EXPECT_EQ(shape.LastBuildReport().Count(GothicBuildWarning::MouldingNotFused), 0u);
    }
}

// Issue d'une fusion dans le rapport (« Profile ») : niveaux sautes par la garde
// d'anneau -> moulding_simplified, leur nombre dans le message ; echec ->
// moulding_not_fused seul, sa cause dans le message, meme si des niveaux ont ete
// sautes avant l'echec ; fusion complete -> rien.
TEST(TEST_cgmesh_gothic_build_report, fusion_outcome_is_recorded_by_name)
{
    {
        SCOPED_TRACE("niveaux sautes");
        FusedBarsResult r;
        r.skippedLevels = 2;
        GothicBuildReport report;
        recordFusedBarsOutcome(r, &report);
        const std::vector<GothicDiagnostic> d = entriesOf(report, GothicBuildWarning::MouldingSimplified);
        ASSERT_EQ(d.size(), 1u);
        EXPECT_EQ(d[0].parameter, "Profile");
        EXPECT_NE(d[0].message.find('2'), std::string::npos) << d[0].message;
        EXPECT_EQ(report.Count(GothicBuildWarning::MouldingNotFused), 0u);
    }
    {
        SCOPED_TRACE("echec");
        FusedBarsResult r;
        r.failed        = true;
        r.skippedLevels = 1;
        r.failure       = "plate and bar footprint noding did not converge";
        GothicBuildReport report;
        recordFusedBarsOutcome(r, &report);
        const std::vector<GothicDiagnostic> d = entriesOf(report, GothicBuildWarning::MouldingNotFused);
        ASSERT_EQ(d.size(), 1u);
        EXPECT_EQ(d[0].parameter, "Profile");
        EXPECT_NE(d[0].message.find("noding did not converge"), std::string::npos) << d[0].message;
        EXPECT_EQ(report.Count(GothicBuildWarning::MouldingSimplified), 0u);
    }
    {
        SCOPED_TRACE("fusion complete");
        GothicBuildReport report;
        recordFusedBarsOutcome(FusedBarsResult(), &report);
        EXPECT_TRUE(report.entries.empty());
    }
}

// Barres posees sans fusion (O1) : chacune est une coque fermee distincte, et
// le rapport le dit (multiple_shells, une fois, « Profile »).
//   - ebrasement et barre branches ensemble (ports d'ebrasement et de barre) :
//     la fusion ne combine pas l'ebrasement, O1 sans moulding_not_fused ;
//   - repli : la fusion echoue (profil de barre enfonce plus bas que le fond de
//     la plaque) ; O1, et moulding_not_fused (« Profile ») en nomme la cause.
TEST(TEST_cgmesh_gothic_build_report, unfused_bars_are_named_as_separate_shells)
{
    const Profile2D splay = chamferSplayProfile(2.0, 3.0);
    const Profile2D sunk = translatedInU(rollBarProfile(4.0), -30.0);
    struct Case { const char *name; const Profile2D *splay; Profile2D bar; std::size_t notFused; };
    const std::vector<Case> cases = {
        {"ebrasement + barre", &splay, rollBarProfile(4.0), 0u},
        {"repli", nullptr, sunk, 1u},
    };
    for (const Case &c : cases)
    {
        SCOPED_TRACE(c.name);
        ParameterizedGothicWindow shape;
        shape.SetSplayProfile(c.splay);
        shape.SetBarProfile(&c.bar);
        const auto mesh = regenerate(shape);
        ASSERT_TRUE(mesh);
        const GothicBuildReport &report = shape.LastBuildReport();
        EXPECT_FALSE(report.HasErrors());
        const std::vector<GothicDiagnostic> shells = entriesOf(report, GothicBuildWarning::MultipleShells);
        ASSERT_EQ(shells.size(), 1u);
        EXPECT_EQ(shells[0].parameter, "Profile");
        const std::vector<GothicDiagnostic> notFused = entriesOf(report, GothicBuildWarning::MouldingNotFused);
        ASSERT_EQ(notFused.size(), c.notFused);
        if (c.notFused)
        {
            EXPECT_EQ(notFused[0].parameter, "Profile");
            EXPECT_NE(notFused[0].message.find("height"), std::string::npos) << notFused[0].message;
        }
        MeshValidationOptions options;
        options.weldTolerance = 5e-4;
        EXPECT_GT(ValidateMesh(*mesh, options).components, 1u);
    }
}

// ---------------------------------------------------------------------------
//  Garde d'export (maker)
// ---------------------------------------------------------------------------

// Refus si le rapport porte une erreur (triangle de repli), avec son motif ;
// sinon export permis, strict ou non, les defauts nommes quand il ne l'est pas.
TEST(TEST_cgmesh_gothic_build_report, export_check_names_refusal_and_strictness)
{
    struct Case { const char *name; ParamList params; float body; bool allowed; bool strict; };
    const std::vector<Case> cases = {
        {"defaut", {}, 0.f, true, true},
        {"Roll bar", {{"Profile", 2}}, 0.f, true, true},
        {"Extrusion 0", {{"Extrusion (zHeight)", 0}}, 0.f, true, false},
        {"corps 1e18", {}, 1e18f, false, false},
    };
    for (const Case &c : cases)
    {
        SCOPED_TRACE(c.name);
        ParameterizedGothicWindow shape;
        applyParams(shape, c.params);
        if (c.body > 0.f)
        {
            GothicWindowSettings s = shape.Settings();
            s.bodyHeight = c.body;
            shape.SetSettings(s);
        }
        const auto mesh = regenerate(shape);
        ASSERT_TRUE(mesh);
        const GothicExportCheck check = CheckGothicExport(shape, *mesh);
        EXPECT_EQ(check.allowed, c.allowed);
        EXPECT_EQ(check.strict, c.strict);
        EXPECT_EQ(check.message.empty(), c.allowed && c.strict) << check.message;
        if (!c.allowed)
            EXPECT_NE(check.message.find("construction_failed"), std::string::npos) << check.message;
    }
}
