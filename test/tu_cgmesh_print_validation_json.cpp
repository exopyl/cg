#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <locale>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include <cgarchi/parameterized_gothic.h>
#include "../src/cgmesh/print_validation.h"
#include "../src/cgmesh/print_validation_json.h"
#include <cgarchi/gothic_print_json.h>

// ===========================================================================
//  Serialisation du controle de fabrication : print-constraints/v1 (lecture
//  stricte, aller-retour exact) et print-report/v1 (verdict de la garde)
// ===========================================================================

namespace
{
    using nlohmann::json;

    // Contraintes dont aucun champ n'a sa valeur par defaut.
    PrintConstraints everyFieldSet()
    {
        PrintConstraints c;
        c.mmPerUnit = 1.0 / 3.0;
        c.minWallMm = 0.45;
        c.minHoleMm = 0.55;
        c.maxTipLengthMm = 1.06;
        c.noiseDepthRatio = 0.2;
        c.minDetailMm = 0.35;
        c.envelopeMm = std::array<double, 3>{218.0, 123.0, 250.0};
        c.maxRecordsPerKind = 7;
        return c;
    }

    void expectSameConstraints(const PrintConstraints &a, const PrintConstraints &b)
    {
        EXPECT_EQ(a.mmPerUnit, b.mmPerUnit);
        EXPECT_EQ(a.minWallMm, b.minWallMm);
        EXPECT_EQ(a.minHoleMm, b.minHoleMm);
        EXPECT_EQ(a.maxTipLengthMm, b.maxTipLengthMm);
        EXPECT_EQ(a.noiseDepthRatio, b.noiseDepthRatio);
        EXPECT_EQ(a.minDetailMm, b.minDetailMm);
        EXPECT_EQ(a.envelopeMm, b.envelopeMm);
        EXPECT_EQ(a.maxRecordsPerKind, b.maxRecordsPerKind);
    }

    std::string doc(const std::string &fields)
    {
        return std::string("{\"schema\": \"print-constraints/v1\"") + (fields.empty() ? "" : ", " + fields) + "}";
    }

    // Une locale a virgule decimale, si le systeme en a une.
    std::optional<std::locale> commaLocale()
    {
        for (const char *name : {"fr_FR.UTF-8", "fr_FR", "fr-FR", "French_France.1252", "de_DE.UTF-8", "de-DE"})
        {
            try
            {
                return std::locale(name);
            }
            catch (const std::runtime_error &)
            {
            }
        }
        return std::nullopt;
    }

    struct Window
    {
        cgarchi::ParameterizedGothicWindow shape;
        std::unique_ptr<Mesh>     mesh;

        explicit Window(const cgarchi::GothicWindowSettings &s = cgarchi::GothicWindowSettings())
        {
            shape.SetSettings(s);
            shape.Regenerate();
            mesh.reset(shape.TakeMesh());
        }
    };

    PrintConstraints at(double mmPerUnit)
    {
        PrintConstraints c;
        c.mmPerUnit = mmPerUnit;
        c.maxRecordsPerKind = 1000;
        return c;
    }
}

// --- print-constraints/v1 -------------------------------------------------------

TEST(TEST_cgmesh_print_validation_json, constraints_round_trip_identically)
{
    const PrintConstraints c = everyFieldSet();
    const std::string text = PrintConstraintsToJson(c);
    PrintConstraints back;
    const std::vector<PrintJsonError> errors = PrintConstraintsFromJson(text, back);
    ASSERT_TRUE(errors.empty()) << errors[0].path << ": " << errors[0].message;
    expectSameConstraints(back, c);
    EXPECT_EQ(PrintConstraintsToJson(back), text);
    EXPECT_EQ(json::parse(text).at("schema"), "print-constraints/v1");
}

TEST(TEST_cgmesh_print_validation_json, writer_writes_every_threshold_and_omits_absent_options)
{
    const json j = json::parse(PrintConstraintsToJson(PrintConstraints()));
    for (const char *key : {"schema", "minWallMm", "minHoleMm", "maxTipLengthMm", "noiseDepthRatio", "minDetailMm",
                            "maxRecordsPerKind"})
        EXPECT_TRUE(j.contains(key)) << key;
    EXPECT_FALSE(j.contains("mmPerUnit"));
    EXPECT_FALSE(j.contains("envelopeMm"));
    EXPECT_EQ(j.at("minWallMm").get<double>(), kDefaultMinWallMm);
    EXPECT_EQ(j.size(), 7u);
}

TEST(TEST_cgmesh_print_validation_json, absent_fields_take_no_hidden_value)
{
    // Ni l'echelle ni l'enveloppe n'ont de defaut ; un seuil absent prend son
    // defaut constexpr, pas la valeur anterieure.
    PrintConstraints out = everyFieldSet();
    const std::vector<PrintJsonError> errors = PrintConstraintsFromJson(doc(""), out);
    ASSERT_TRUE(errors.empty()) << errors[0].path << ": " << errors[0].message;
    EXPECT_FALSE(out.mmPerUnit.has_value());
    EXPECT_FALSE(out.envelopeMm.has_value());
    expectSameConstraints(out, PrintConstraints());

    PrintConstraints partial;
    ASSERT_TRUE(PrintConstraintsFromJson(doc("\"mmPerUnit\": 0.1, \"minWallMm\": 0.6"), partial).empty());
    ASSERT_TRUE(partial.mmPerUnit.has_value());
    EXPECT_EQ(*partial.mmPerUnit, 0.1);
    EXPECT_EQ(partial.minWallMm, 0.6);
    EXPECT_EQ(partial.minHoleMm, kDefaultMinHoleMm);
    EXPECT_FALSE(partial.envelopeMm.has_value());
}

TEST(TEST_cgmesh_print_validation_json, refusals_are_named_and_leave_constraints_unchanged)
{
    struct Case { std::string text; std::string path; };
    const Case cases[] = {
        {doc("\"printer\": 1"), "printer"},
        {doc("\"mmPerUnit\": 0"), "mmPerUnit"},
        {doc("\"mmPerUnit\": -0.1"), "mmPerUnit"},
        {doc("\"mmPerUnit\": \"0.1\""), "mmPerUnit"},
        {doc("\"mmPerUnit\": null"), "mmPerUnit"},
        {doc("\"envelopeMm\": [218, 123]"), "envelopeMm"},
        {doc("\"envelopeMm\": [218, 0, 250]"), "envelopeMm[1]"},
        {doc("\"envelopeMm\": [218, true, 250]"), "envelopeMm[1]"},
        {doc("\"envelopeMm\": {\"x\": 1}"), "envelopeMm"},
        {doc("\"minWallMm\": 0.001"), "minWallMm"},
        {doc("\"minHoleMm\": 1001"), "minHoleMm"},
        {doc("\"maxTipLengthMm\": 0"), "maxTipLengthMm"},
        {doc("\"noiseDepthRatio\": 1"), "noiseDepthRatio"},
        {doc("\"noiseDepthRatio\": -0.1"), "noiseDepthRatio"},
        {doc("\"minDetailMm\": 0"), "minDetailMm"},
        {doc("\"maxRecordsPerKind\": -1"), "maxRecordsPerKind"},
        {doc("\"maxRecordsPerKind\": 2.5"), "maxRecordsPerKind"},
        {doc("\"minWallMm\": 1e400"), ""},   // nombre non fini : refuse a la lecture du texte
        {doc("\"minWallMm\": NaN"), ""},
        {"{\"mmPerUnit\": 0.1}", "schema"},
        {"{\"schema\": \"print-constraints/v2\"}", "schema"},
        {"{\"schema\": 1}", "schema"},
        {"[]", ""},
        {"not json", ""},
    };
    for (const Case &k : cases)
    {
        PrintConstraints out = everyFieldSet();
        const std::vector<PrintJsonError> errors = PrintConstraintsFromJson(k.text, out);
        ASSERT_FALSE(errors.empty()) << k.text;
        EXPECT_EQ(errors[0].path, k.path) << k.text << " -> " << errors[0].message;
        EXPECT_FALSE(errors[0].message.empty()) << k.text;
        expectSameConstraints(out, everyFieldSet());
    }
}

TEST(TEST_cgmesh_print_validation_json, every_bad_field_is_reported)
{
    PrintConstraints out;
    const std::vector<PrintJsonError> errors =
        PrintConstraintsFromJson(doc("\"mmPerUnit\": 0, \"minWallMm\": 0, \"extra\": 1"), out);
    ASSERT_EQ(errors.size(), 3u);
    std::vector<std::string> paths;
    for (const PrintJsonError &e : errors)
        paths.push_back(e.path);
    for (const char *p : {"mmPerUnit", "minWallMm", "extra"})
        EXPECT_NE(std::find(paths.begin(), paths.end(), p), paths.end()) << p;
}

TEST(TEST_cgmesh_print_validation_json, numbers_ignore_the_global_locale)
{
    const std::optional<std::locale> comma = commaLocale();
    if (!comma)
        GTEST_SKIP() << "no locale with a decimal comma on this system";
    const std::locale previous = std::locale::global(*comma);
    const PrintConstraints c = everyFieldSet();
    const std::string text = PrintConstraintsToJson(c);
    PrintConstraints back;
    const std::vector<PrintJsonError> ok = PrintConstraintsFromJson(text, back);
    PrintConstraints out;
    const std::vector<PrintJsonError> bad = PrintConstraintsFromJson(doc("\"minWallMm\": 0.0015"), out);
    cgarchi::GothicPrintVerdict v;
    v.check.print.mmPerUnit = 0.25;
    v.check.print.evaluated = true;
    v.printedSizeMm = std::array<double, 3>{12.5, 1.5, 0.5};
    const std::string report = cgarchi::GothicPrintVerdictToJson(v);
    std::locale::global(previous);

    EXPECT_NE(text.find("0.45"), std::string::npos) << text;
    EXPECT_EQ(text.find("0,45"), std::string::npos) << text;
    ASSERT_TRUE(ok.empty());
    expectSameConstraints(back, c);
    ASSERT_EQ(bad.size(), 1u);
    EXPECT_NE(bad[0].message.find("0.0015"), std::string::npos) << bad[0].message;
    EXPECT_NE(bad[0].message.find("[0.002, 1000]"), std::string::npos) << bad[0].message;
    EXPECT_EQ(json::parse(report).at("mmPerUnit").get<double>(), 0.25);
    EXPECT_EQ(report.find("0,25"), std::string::npos) << report;
}

// --- print-report/v1 ----------------------------------------------------------

TEST(TEST_cgmesh_print_validation_json, report_lists_the_ten_categories_zero_included)
{
    const Window w;
    const cgarchi::GothicPrintVerdict v = cgarchi::CheckGothicExportAndPrint(w.shape, *w.mesh, at(0.1));
    const json j = json::parse(cgarchi::GothicPrintVerdictToJson(v));
    EXPECT_EQ(j.at("schema"), "print-report/v1");
    EXPECT_EQ(j.at("scope"), "complete");
    EXPECT_EQ(j.at("printable"), false);
    EXPECT_EQ(j.at("exportAllowed"), true);
    EXPECT_EQ(j.at("geometry").at("allowed"), true);
    EXPECT_EQ(j.at("geometry").at("strict"), true);
    EXPECT_EQ(j.at("evaluated"), true);
    EXPECT_EQ(j.at("mmPerUnit").get<double>(), 0.1);
    ASSERT_TRUE(j.at("printedSizeMm").is_array());
    EXPECT_EQ(j.at("printedSizeMm").size(), 3u);
    EXPECT_TRUE(j.at("error").is_null());
    EXPECT_EQ(j.at("constraints").at("mmPerUnit").get<double>(), 0.1);
    EXPECT_FALSE(j.at("constraints").contains("schema"));
    for (const char *key : {"counts", "displayCounts"})
    {
        const json &counts = j.at(key);
        ASSERT_EQ(counts.size(), kPrintDefectCount) << key;
        for (std::size_t k = 0; k < kPrintDefectCount; ++k)
        {
            const PrintDefect kind = static_cast<PrintDefect>(k);
            ASSERT_TRUE(counts.contains(PrintDefectName(kind))) << key << " " << PrintDefectName(kind);
            EXPECT_EQ(counts.at(PrintDefectName(kind)).get<std::size_t>(), std::string(key) == "counts"
                      ? v.check.print.Count(kind) : PrintDisplayCounts(v.check.print)[k]);
        }
    }
    EXPECT_EQ(j.at("counts").at("detail_too_small"), 0u);
    EXPECT_EQ(j.at("counts").at("thin_wall"), v.check.print.Count(PrintDefect::ThinWall));
    ASSERT_EQ(j.at("records").size(), v.check.print.records.size());
    const json &r0 = j.at("records").at(0);
    EXPECT_EQ(r0.at("kind"), PrintDefectName(v.check.print.records[0].kind));
    EXPECT_EQ(r0.at("boxMm").size(), 4u);
    EXPECT_EQ(r0.at("slice"), -1);
    EXPECT_TRUE(j.at("slices").empty());
}

TEST(TEST_cgmesh_print_validation_json, report_shows_the_maximum_per_slice)
{
    // Chamfer a 20 mm : les six nervures minces dans chacune des deux tranches.
    cgarchi::GothicWindowSettings s;
    s.profile = 1;
    const Window w(s);
    PrintConstraints c = at(0.1);
    c.maxTipLengthMm = 2.0;
    const cgarchi::GothicPrintVerdict v = cgarchi::CheckGothicExportAndPrint(w.shape, *w.mesh, c);
    const json j = json::parse(cgarchi::GothicPrintVerdictToJson(v));
    EXPECT_EQ(j.at("counts").at("thin_wall"), 12u);
    EXPECT_EQ(j.at("displayCounts").at("thin_wall"), 6u);
    ASSERT_EQ(j.at("slices").size(), 3u);
    EXPECT_EQ(j.at("slices").at(2).at("silhouette"), true);
    EXPECT_EQ(j.at("slices").at(0).at("counts").at("thin_wall"), 6u);
}

TEST(TEST_cgmesh_print_validation_json, report_without_scale_has_null_scale)
{
    const Window w;
    const cgarchi::GothicPrintVerdict v = cgarchi::CheckGothicExportAndPrint(w.shape, *w.mesh, std::nullopt);
    const json j = json::parse(cgarchi::GothicPrintVerdictToJson(v));
    EXPECT_EQ(j.at("scope"), "not_run");
    EXPECT_EQ(j.at("reason"), "print scale not set");
    EXPECT_EQ(j.at("evaluated"), false);
    EXPECT_TRUE(j.at("mmPerUnit").is_null());
    EXPECT_TRUE(j.at("printedSizeMm").is_null());
    EXPECT_TRUE(j.at("constraints").is_null());
    EXPECT_EQ(j.at("counts").at("scale_unset"), 1u);
    EXPECT_EQ(j.at("counts").size(), kPrintDefectCount);
}

TEST(TEST_cgmesh_print_validation_json, report_carries_the_error_and_survives_invalid_text)
{
    cgarchi::GothicPrintVerdict v;
    v.check.geometry.allowed = true;
    v.check.geometry.message = std::string("bad byte \xff here");
    v.printError = "CheckPrintRegion: half-extent 1740 mm exceeds 1048.58 mm";
    v.check.reason = "print check error";
    v.check.print.counts[static_cast<std::size_t>(PrintDefect::EnvelopeExceeded)] = 1;
    std::string text;
    ASSERT_NO_THROW(text = cgarchi::GothicPrintVerdictToJson(v));
    const json j = json::parse(text);
    EXPECT_EQ(j.at("error"), v.printError);
    EXPECT_EQ(j.at("counts").at("envelope_exceeded"), 1u);
    EXPECT_EQ(j.at("displayCounts").at("envelope_exceeded"), 1u);
    EXPECT_EQ(j.at("printable"), false);
    // allowed et strict different ici : chacun doit aller a sa cle.
    EXPECT_EQ(j.at("geometry").at("allowed"), true);
    EXPECT_EQ(j.at("geometry").at("strict"), false);
    EXPECT_EQ(j.at("exportAllowed"), true);
    EXPECT_EQ(j.at("scope"), "not_run");
    EXPECT_EQ(j.at("reason"), "print check error");
}

TEST(TEST_cgmesh_print_validation_json, generic_report_writes_every_envelope_field)
{
    // Valeurs toutes distinctes : une cle alimentee par le mauvais champ se voit.
    PrintReportEnvelope e;
    e.scope = "scope_name";
    e.reason = "reason text";
    e.printable = true;
    e.exportAllowed = false;
    e.geometry.allowed = true;
    e.geometry.strict = false;
    e.geometry.message = "geometry text";
    e.printedSizeMm = std::array<double, 3>{1.5, 2.5, 3.5};
    e.error = "error text";
    e.constraints = everyFieldSet();

    PrintabilityReport r;
    r.evaluated = true;
    r.mmPerUnit = 0.25;
    r.counts[static_cast<std::size_t>(PrintDefect::ThinWall)] = 3;
    r.counts[static_cast<std::size_t>(PrintDefect::NarrowOpening)] = 4;
    PrintDefectRecord d;
    d.kind = PrintDefect::NarrowOpening;
    d.boxMm = {1.0, 2.0, 3.0, 4.0};
    d.depthMm = 5.0;
    d.areaMm2 = 6.0;
    d.contacts = 7;
    d.widthBelowMm = 8.0;
    d.slice = 0;
    d.zLowMm = 9.0;
    d.zHighMm = 10.0;
    r.records.push_back(d);
    PrintSliceSummary s;
    s.zLowMm = 11.0;
    s.zHighMm = 12.0;
    s.zCutMm = 13.0;
    s.areaMm2 = 14.0;
    s.silhouette = true;
    s.counts[static_cast<std::size_t>(PrintDefect::ThinWall)] = 2;
    r.slices.push_back(s);

    const json j = json::parse(PrintReportToJson(e, r));

    std::vector<std::string> keys;
    for (auto it = j.begin(); it != j.end(); ++it)
        keys.push_back(it.key());
    const std::vector<std::string> expected = {"constraints", "counts", "displayCounts", "error", "evaluated",
                                               "exportAllowed", "geometry", "mmPerUnit", "printable",
                                               "printedSizeMm", "reason", "records", "schema", "scope", "slices"};
    EXPECT_EQ(keys, expected);

    EXPECT_EQ(j.at("schema"), "print-report/v1");
    EXPECT_EQ(j.at("scope"), "scope_name");
    EXPECT_EQ(j.at("reason"), "reason text");
    EXPECT_EQ(j.at("printable"), true);
    EXPECT_EQ(j.at("exportAllowed"), false);
    EXPECT_EQ(j.at("geometry").size(), 3u);
    EXPECT_EQ(j.at("geometry").at("allowed"), true);
    EXPECT_EQ(j.at("geometry").at("strict"), false);
    EXPECT_EQ(j.at("geometry").at("message"), "geometry text");
    EXPECT_EQ(j.at("evaluated"), true);
    EXPECT_EQ(j.at("mmPerUnit").get<double>(), 0.25);
    EXPECT_EQ(j.at("printedSizeMm"), json::array({1.5, 2.5, 3.5}));
    EXPECT_EQ(j.at("counts").size(), kPrintDefectCount);
    EXPECT_EQ(j.at("counts").at("thin_wall"), 3u);
    EXPECT_EQ(j.at("counts").at("narrow_opening"), 4u);
    EXPECT_EQ(j.at("displayCounts").size(), kPrintDefectCount);
    for (std::size_t k = 0; k < kPrintDefectCount; ++k)
        EXPECT_EQ(j.at("displayCounts").at(PrintDefectName(static_cast<PrintDefect>(k))).get<std::size_t>(),
                  PrintDisplayCounts(r)[k]) << k;
    ASSERT_EQ(j.at("records").size(), 1u);
    const json &rec = j.at("records").at(0);
    EXPECT_EQ(rec.at("kind"), "narrow_opening");
    EXPECT_EQ(rec.at("boxMm"), json::array({1.0, 2.0, 3.0, 4.0}));
    EXPECT_EQ(rec.at("depthMm").get<double>(), 5.0);
    EXPECT_EQ(rec.at("areaMm2").get<double>(), 6.0);
    EXPECT_EQ(rec.at("contacts"), 7u);
    EXPECT_EQ(rec.at("widthBelowMm").get<double>(), 8.0);
    EXPECT_EQ(rec.at("slice"), 0);
    EXPECT_EQ(rec.at("zLowMm").get<double>(), 9.0);
    EXPECT_EQ(rec.at("zHighMm").get<double>(), 10.0);
    ASSERT_EQ(j.at("slices").size(), 1u);
    const json &sl = j.at("slices").at(0);
    EXPECT_EQ(sl.at("zLowMm").get<double>(), 11.0);
    EXPECT_EQ(sl.at("zHighMm").get<double>(), 12.0);
    EXPECT_EQ(sl.at("zCutMm").get<double>(), 13.0);
    EXPECT_EQ(sl.at("areaMm2").get<double>(), 14.0);
    EXPECT_EQ(sl.at("silhouette"), true);
    EXPECT_EQ(sl.at("counts").at("thin_wall"), 2u);
    EXPECT_EQ(j.at("error"), "error text");
    const json &c = j.at("constraints");
    EXPECT_FALSE(c.contains("schema"));
    EXPECT_EQ(c.at("mmPerUnit").get<double>(), 1.0 / 3.0);
    EXPECT_EQ(c.at("minWallMm").get<double>(), 0.45);
    EXPECT_EQ(c.at("maxRecordsPerKind"), 7u);
    EXPECT_EQ(c.at("envelopeMm"), json::array({218.0, 123.0, 250.0}));
}

TEST(TEST_cgmesh_print_validation_json, generic_report_writes_null_for_absent_fields)
{
    const json j = json::parse(PrintReportToJson(PrintReportEnvelope{}, PrintabilityReport{}));
    EXPECT_EQ(j.at("scope"), "");
    EXPECT_EQ(j.at("reason"), "");
    EXPECT_EQ(j.at("evaluated"), false);
    EXPECT_TRUE(j.at("mmPerUnit").is_null());
    EXPECT_TRUE(j.at("printedSizeMm").is_null());
    EXPECT_TRUE(j.at("error").is_null());
    EXPECT_TRUE(j.at("constraints").is_null());
    EXPECT_TRUE(j.at("records").is_array() && j.at("records").empty());
    EXPECT_TRUE(j.at("slices").is_array() && j.at("slices").empty());
}

TEST(TEST_cgmesh_print_validation_json, duplicate_keys_are_refused)
{
    struct Case { std::string text; std::string path; };
    const Case cases[] = {
        {"{\"schema\": \"print-constraints/v1\", \"minWallMm\": 0.4, \"minWallMm\": 5}", "minWallMm"},
        {"{\"schema\": \"bogus\", \"schema\": \"print-constraints/v1\"}", "schema"},
        {"{\"schema\": \"print-constraints/v1\", \"mmPerUnit\": 0.1, \"mmPerUnit\": 0.1}", "mmPerUnit"},
    };
    for (const Case &k : cases)
    {
        PrintConstraints out = everyFieldSet();
        const std::vector<PrintJsonError> errors = PrintConstraintsFromJson(k.text, out);
        ASSERT_FALSE(errors.empty()) << k.text;
        bool named = false;
        for (const PrintJsonError &e : errors)
            named = named || (e.path == k.path && e.message.find("duplicate") != std::string::npos);
        EXPECT_TRUE(named) << k.text << " -> " << errors[0].path << ": " << errors[0].message;
        expectSameConstraints(out, everyFieldSet());
    }
}
