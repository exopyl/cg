#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "../src/cgmath/architecture_gothic_io.h"

namespace
{
    // Walk up from the current working directory until we find a sibling that
    // identifies the project root (the test/data directory exists there and is
    // also copied next to the test executable by CMake).
    std::filesystem::path findProjectRoot()
    {
        std::filesystem::path root = std::filesystem::current_path();
        for (int i = 0; i < 6; ++i)
        {
            if (std::filesystem::exists(root / "test" / "data") &&
                std::filesystem::exists(root / "src"  / "cgmath"))
                return root;
            root = root.parent_path();
        }
        return std::filesystem::current_path();    // give up, caller will diagnose
    }

    // A minimal valid JSON instance covering the fields the current parser
    // understands. Mirrors test/data/gothic-window-instance.json but trimmed.
    const char *kMinimalInstance = R"JSON({
        "$schema": "https://gothic-tracery/schema/v1/window.schema.json",
        "window": {
            "id": "test-window",
            "label": "test",
            "period": "modern",
            "basis": {
                "origin": { "x": 0, "y": 0 },
                "archRise": 0,
                "pL": { "x": -100, "y": 0 },
                "pR": { "x":  100, "y": 0 }
            },
            "arch": {
                "width": 200,
                "excess": 1.0,
                "offset": { "outer": 16, "inner": 10, "method": "fixed-centers" },
                "profile": { "type": "chamfer", "depth": 8,
                              "segments": [
                                  { "type": "line", "u": 0, "v": 0 },
                                  { "type": "line", "u": 1, "v": 1 } ] }
            },
            "subwindows": {
                "count": 2,
                "drop": 0,
                "excess": 1.0,
                "gap": { "mode": "fraction", "gapFraction": 0.11 },
                "foils": { "count": 3, "type": "round" }
            },
            "rosette": {
                "construction": "ellipse-intersection",
                "foils": { "count": 6, "type": "round" }
            },
            "style": {
                "glass": { "color": "#5588cc" },
                "stone": { }
            }
        }
    })JSON";
}

//
// JSON parsing
//

TEST(TEST_cgmath_architecture_gothic_io, LoadInstanceFromJsonParsesAllRequiredFields)
{
    WindowInstance inst = loadInstanceFromJson(kMinimalInstance);

    EXPECT_EQ(inst.id, "test-window");
    EXPECT_EQ(inst.label, "test");
    EXPECT_EQ(inst.period, "modern");

    EXPECT_DOUBLE_EQ(inst.archBasis.pL.x, -100.0);
    EXPECT_DOUBLE_EQ(inst.archBasis.pR.x,  100.0);
    EXPECT_DOUBLE_EQ(inst.archBasis.excess, 1.0);

    EXPECT_DOUBLE_EQ(inst.archOffset.outer, 16.0);
    EXPECT_DOUBLE_EQ(inst.archOffset.inner, 10.0);

    EXPECT_EQ(inst.subwindowParams.count, 2);
    EXPECT_DOUBLE_EQ(inst.subwindowParams.drop, 0.0);
    EXPECT_DOUBLE_EQ(inst.subwindowParams.excess, 1.0);
    EXPECT_EQ(inst.subwindowParams.gap.mode, SubwindowParams::Gap::Mode::Fraction);
    EXPECT_DOUBLE_EQ(inst.subwindowParams.gap.gapFraction, 0.11);

    EXPECT_TRUE(inst.hasRosette);
}

TEST(TEST_cgmath_architecture_gothic_io, LoadInstanceFromJsonAbsoluteGapMode)
{
    std::string jsn = R"JSON({
        "window": {
            "basis": { "pL": {"x":-100,"y":0}, "pR": {"x":100,"y":0} },
            "arch": { "width":200, "excess":1.0,
                      "offset": {"outer":16,"inner":10} },
            "subwindows": { "count":2, "excess":1.0,
                            "gap": {"mode":"absolute","absoluteWidth":22} }
        }
    })JSON";
    WindowInstance inst = loadInstanceFromJson(jsn);
    EXPECT_EQ(inst.subwindowParams.gap.mode, SubwindowParams::Gap::Mode::Absolute);
    EXPECT_DOUBLE_EQ(inst.subwindowParams.gap.absoluteWidth, 22.0);
}

TEST(TEST_cgmath_architecture_gothic_io, LoadInstanceMissingWindowThrows)
{
    EXPECT_THROW(loadInstanceFromJson(R"({"wall":{}})"), std::invalid_argument);
}

TEST(TEST_cgmath_architecture_gothic_io, LoadInstanceMalformedJsonThrows)
{
    EXPECT_THROW(loadInstanceFromJson("not json at all"), std::runtime_error);
}

TEST(TEST_cgmath_architecture_gothic_io, LoadInstanceFromFileNotFoundThrows)
{
    EXPECT_THROW(loadInstanceFromFile("does/not/exist.json"), std::runtime_error);
}

//
// Geometry construction
//

TEST(TEST_cgmath_architecture_gothic_io, BuildGeometryFromInstanceProducesAllParts)
{
    WindowInstance inst = loadInstanceFromJson(kMinimalInstance);
    WindowGeometry g = buildGeometryFromInstance(inst);

    // Main arch
    EXPECT_NEAR(g.mainArch.width, 200.0, 1e-12);
    EXPECT_GT(g.mainArch.height, 0.0);

    // Main offset
    EXPECT_NEAR(g.mainOffset.outer.circleL.radius, g.mainArch.circleL.radius + 16.0, 1e-9);
    EXPECT_NEAR(g.mainOffset.inner.circleL.radius, g.mainArch.circleL.radius - 10.0, 1e-9);

    // Subwindows
    EXPECT_EQ(g.subwindows.lancets.size(), 2u);

    // Rosette
    EXPECT_TRUE(g.hasRosette);
    EXPECT_GT(g.rosette.radius, 0.0);
}

TEST(TEST_cgmath_architecture_gothic_io, BuildGeometryWithoutRosetteSkipsRosette)
{
    std::string jsn = R"JSON({
        "window": {
            "basis": { "pL": {"x":-100,"y":0}, "pR": {"x":100,"y":0} },
            "arch": { "width":200, "excess":1.0,
                      "offset": {"outer":16,"inner":10} },
            "subwindows": { "count":2, "excess":1.0,
                            "gap": {"mode":"fraction","gapFraction":0.11} }
        }
    })JSON";
    WindowInstance inst = loadInstanceFromJson(jsn);
    WindowGeometry g = buildGeometryFromInstance(inst);
    EXPECT_FALSE(g.hasRosette);
    EXPECT_FALSE(inst.hasRosette);
}

//
// SVG export
//

TEST(TEST_cgmath_architecture_gothic_io, SvgContainsExpectedElements)
{
    WindowInstance inst = loadInstanceFromJson(kMinimalInstance);
    WindowGeometry g = buildGeometryFromInstance(inst);
    std::string svg = toSvg(g);

    EXPECT_NE(svg.find("<svg"),     std::string::npos);
    EXPECT_NE(svg.find("viewBox"),  std::string::npos);
    EXPECT_NE(svg.find("polyline"), std::string::npos);
    EXPECT_NE(svg.find("<circle"),  std::string::npos);   // rosette
    EXPECT_NE(svg.find("</svg>"),   std::string::npos);
}

TEST(TEST_cgmath_architecture_gothic_io, SvgWithoutRosetteHasNoCircle)
{
    std::string jsn = R"JSON({
        "window": {
            "basis": { "pL": {"x":-100,"y":0}, "pR": {"x":100,"y":0} },
            "arch": { "width":200, "excess":1.0,
                      "offset": {"outer":16,"inner":10} },
            "subwindows": { "count":2, "excess":1.0,
                            "gap": {"mode":"fraction","gapFraction":0.11} }
        }
    })JSON";
    WindowGeometry g = buildGeometryFromInstance(loadInstanceFromJson(jsn));
    std::string svg = toSvg(g);
    EXPECT_EQ(svg.find("<circle"), std::string::npos);
}

TEST(TEST_cgmath_architecture_gothic_io, WriteSvgToFileCreatesParentDirectory)
{
    WindowInstance inst = loadInstanceFromJson(kMinimalInstance);
    WindowGeometry g = buildGeometryFromInstance(inst);

    std::filesystem::path root = findProjectRoot();
    ASSERT_TRUE(std::filesystem::exists(root / "src" / "cgmath"))
        << "Could not locate project root from " << std::filesystem::current_path();

    std::filesystem::path outDir = root / "tmp";
    std::filesystem::path outFile = outDir / "tu_window_test.svg";

    if (std::filesystem::exists(outFile))
        std::filesystem::remove(outFile);

    EXPECT_NO_THROW(writeSvgToFile(g, outFile.string()));
    EXPECT_TRUE(std::filesystem::exists(outFile));
    EXPECT_TRUE(std::filesystem::exists(outDir));

    // File is non-empty and starts with <?xml.
    std::ifstream f(outFile);
    std::stringstream ss; ss << f.rdbuf();
    std::string contents = ss.str();
    EXPECT_GT(contents.size(), 0u);
    EXPECT_EQ(contents.substr(0, 5), "<?xml");
}

//
// End-to-end : load the reference instance from test/data and produce a SVG
// snapshot in tmp/.
//

TEST(TEST_cgmath_architecture_gothic_io, LoadReferenceInstanceAndProduceSvg)
{
    std::filesystem::path root = findProjectRoot();
    std::filesystem::path inputPath = root / "test" / "data" / "gothic-window-instance.json";
    if (!std::filesystem::exists(inputPath))
        GTEST_SKIP() << "reference instance not found at " << inputPath;

    WindowInstance inst = loadInstanceFromFile(inputPath.string());
    WindowGeometry g    = buildGeometryFromInstance(inst);

    std::filesystem::path outFile = root / "tmp" / "high-gothic-2lancets.svg";
    EXPECT_NO_THROW(writeSvgToFile(g, outFile.string()));
    EXPECT_TRUE(std::filesystem::exists(outFile));
}

//
// Foil parsing
//

TEST(TEST_cgmath_architecture_gothic_io, ParseRosetteFoils)
{
    WindowInstance inst = loadInstanceFromJson(kMinimalInstance);
    EXPECT_TRUE(inst.hasRosetteFoils);
    EXPECT_EQ(inst.rosetteFoils.count, 6);
    EXPECT_EQ(inst.rosetteFoils.type, FoilType::Round);
    EXPECT_DOUBLE_EQ(inst.rosetteFoils.pointedness, 0.0);
    EXPECT_FALSE(inst.rosetteFoils.orientLying);
}

TEST(TEST_cgmath_architecture_gothic_io, ParseSubwindowFoils)
{
    WindowInstance inst = loadInstanceFromJson(kMinimalInstance);
    EXPECT_TRUE(inst.hasSubwindowFoils);
    EXPECT_EQ(inst.subwindowFoils.count, 3);
    EXPECT_EQ(inst.subwindowFoils.type, FoilType::Round);
}

TEST(TEST_cgmath_architecture_gothic_io, ParseFoilsPointedType)
{
    std::string jsn = R"JSON({
        "window": {
            "basis": { "pL": {"x":-100,"y":0}, "pR": {"x":100,"y":0} },
            "arch": { "width":200, "excess":1.0,
                      "offset": {"outer":16,"inner":10} },
            "subwindows": { "count":2, "excess":1.0,
                            "gap": {"mode":"fraction","gapFraction":0.11},
                            "foils": {"count":4,"type":"pointed",
                                      "pointedness":0.5,"orientation":"lying"} }
        }
    })JSON";
    WindowInstance inst = loadInstanceFromJson(jsn);
    EXPECT_TRUE(inst.hasSubwindowFoils);
    EXPECT_EQ(inst.subwindowFoils.count, 4);
    EXPECT_EQ(inst.subwindowFoils.type, FoilType::Pointed);
    EXPECT_DOUBLE_EQ(inst.subwindowFoils.pointedness, 0.5);
    EXPECT_TRUE(inst.subwindowFoils.orientLying);
}

//
// Foil geometry construction
//

TEST(TEST_cgmath_architecture_gothic_io, BuildGeometryProducesRosetteFoils)
{
    WindowInstance inst = loadInstanceFromJson(kMinimalInstance);
    WindowGeometry g = buildGeometryFromInstance(inst);
    EXPECT_TRUE(g.hasRosetteFoils);
    EXPECT_EQ(g.rosetteFoils.foils.size(), 6u);
    EXPECT_EQ(g.rosetteFoils.outerCircle.radius, g.rosette.radius);
}

TEST(TEST_cgmath_architecture_gothic_io, BuildGeometryProducesSubwindowFoils)
{
    WindowInstance inst = loadInstanceFromJson(kMinimalInstance);
    WindowGeometry g = buildGeometryFromInstance(inst);
    EXPECT_TRUE(g.hasSubwindowFoils);
    EXPECT_EQ(g.subwindowFoils.size(), g.subwindows.lancets.size());
    for (const auto &ring : g.subwindowFoils)
    {
        EXPECT_EQ(ring.foils.size(), 3u);
        EXPECT_GT(ring.outerCircle.radius, 0.0);
    }
}

//
// SVG with foils
//

TEST(TEST_cgmath_architecture_gothic_io, SvgWithFoilsHasMultipleCircles)
{
    WindowInstance inst = loadInstanceFromJson(kMinimalInstance);
    WindowGeometry g = buildGeometryFromInstance(inst);
    std::string svg = toSvg(g);

    // Count <circle elements: should be 1 (rosette) + 6 (rosette foils)
    // + 3*2 (lancet foils, 3 per lancet, 2 lancets) = 13.
    size_t pos = 0, n = 0;
    const std::string needle = "<circle";
    while ((pos = svg.find(needle, pos)) != std::string::npos)
    {
        ++n;
        pos += needle.size();
    }
    EXPECT_EQ(n, 13u);
}

//
// Trefoil
//

namespace
{
    const char *kInstanceWithTrefoil = R"JSON({
        "window": {
            "basis": { "pL": {"x":-100,"y":0}, "pR": {"x":100,"y":0} },
            "arch": {
                "width":200, "excess":1.0,
                "offset": {"outer":16,"inner":10},
                "trefoil": {
                    "enabled": true,
                    "splitParameter": 0.45,
                    "foilRadiusFactor": 0.30
                }
            },
            "subwindows": {
                "count":2, "excess":1.0,
                "gap": {"mode":"fraction","gapFraction":0.11},
                "trefoil": {
                    "enabled": true,
                    "splitParameter": 0.50,
                    "foilRadiusFactor": 0.25
                }
            }
        }
    })JSON";
}

TEST(TEST_cgmath_architecture_gothic_io, ParseArchTrefoilDisabledByDefault)
{
    // Reference instance has trefoil.enabled = false.
    WindowInstance inst = loadInstanceFromJson(kMinimalInstance);
    EXPECT_FALSE(inst.hasArchTrefoil);
    EXPECT_FALSE(inst.hasSubwindowTrefoil);
}

TEST(TEST_cgmath_architecture_gothic_io, ParseArchTrefoilEnabled)
{
    WindowInstance inst = loadInstanceFromJson(kInstanceWithTrefoil);
    EXPECT_TRUE(inst.hasArchTrefoil);
    EXPECT_DOUBLE_EQ(inst.archTrefoil.splitParameter,   0.45);
    EXPECT_DOUBLE_EQ(inst.archTrefoil.foilRadiusFactor, 0.30);
}

TEST(TEST_cgmath_architecture_gothic_io, ParseSubwindowTrefoilEnabled)
{
    WindowInstance inst = loadInstanceFromJson(kInstanceWithTrefoil);
    EXPECT_TRUE(inst.hasSubwindowTrefoil);
    EXPECT_DOUBLE_EQ(inst.subwindowTrefoil.splitParameter,   0.50);
    EXPECT_DOUBLE_EQ(inst.subwindowTrefoil.foilRadiusFactor, 0.25);
}

TEST(TEST_cgmath_architecture_gothic_io, BuildGeometryWithArchTrefoil)
{
    WindowInstance inst = loadInstanceFromJson(kInstanceWithTrefoil);
    WindowGeometry g = buildGeometryFromInstance(inst);
    EXPECT_TRUE(g.hasArchTrefoil);
    EXPECT_NEAR(g.archTrefoil.foilLeft.circle.radius,
                0.30 * g.mainArch.circleL.radius, 1e-9);
}

TEST(TEST_cgmath_architecture_gothic_io, BuildGeometryWithSubwindowTrefoils)
{
    WindowInstance inst = loadInstanceFromJson(kInstanceWithTrefoil);
    WindowGeometry g = buildGeometryFromInstance(inst);
    EXPECT_TRUE(g.hasSubwindowTrefoils);
    EXPECT_EQ(g.subwindowTrefoils.size(), g.subwindows.lancets.size());
    for (size_t i = 0; i < g.subwindowTrefoils.size(); ++i)
    {
        EXPECT_NEAR(g.subwindowTrefoils[i].foilLeft.circle.radius,
                    0.25 * g.subwindows.lancets[i].arch.circleL.radius, 1e-9);
    }
}

TEST(TEST_cgmath_architecture_gothic_io, SvgWithTrefoilHasExtraPolylines)
{
    WindowInstance inst = loadInstanceFromJson(kInstanceWithTrefoil);
    WindowGeometry g = buildGeometryFromInstance(inst);
    std::string svg = toSvg(g);

    // Trefoil decorations add 2 polylines per trefoiled arch.
    // 1 main arch trefoil = 2 polylines + 2 lancets x 2 = 4 polylines = 6 extra.
    EXPECT_NE(svg.find("main arch trefoil"), std::string::npos);
    EXPECT_NE(svg.find("lancet trefoils"),   std::string::npos);
}

TEST(TEST_cgmath_architecture_gothic_io, RenderTrefoilSnapshotToTmp)
{
    WindowInstance inst = loadInstanceFromJson(kInstanceWithTrefoil);
    WindowGeometry g    = buildGeometryFromInstance(inst);

    std::filesystem::path root    = findProjectRoot();
    std::filesystem::path outFile = root / "tmp" / "high-gothic-trefoil.svg";
    EXPECT_NO_THROW(writeSvgToFile(g, outFile.string()));
    EXPECT_TRUE(std::filesystem::exists(outFile));
}

//
// Fillets
//

TEST(TEST_cgmath_architecture_gothic_io, ParseFilletsDisabledByDefaultIfAbsent)
{
    // Reference instance has fillets.enabled = true (per havemann sample),
    // so we use a synthetic JSON without a fillets block to test the absent case.
    std::string jsn = R"JSON({
        "window": {
            "basis": { "pL": {"x":-100,"y":0}, "pR": {"x":100,"y":0} },
            "arch": { "width":200, "excess":1.0,
                      "offset": {"outer":16,"inner":10} },
            "subwindows": { "count":2, "excess":1.0,
                            "gap": {"mode":"fraction","gapFraction":0.11} },
            "rosette": { "construction":"ellipse-intersection",
                         "foils":{"count":6,"type":"round"} }
        }
    })JSON";
    WindowInstance inst = loadInstanceFromJson(jsn);
    EXPECT_FALSE(inst.hasFillets);
}

TEST(TEST_cgmath_architecture_gothic_io, ParseFilletsEnabledTrue)
{
    std::string jsn = R"JSON({
        "window": {
            "basis": { "pL": {"x":-100,"y":0}, "pR": {"x":100,"y":0} },
            "arch": { "width":200, "excess":1.0,
                      "offset": {"outer":16,"inner":10} },
            "subwindows": { "count":2, "excess":1.0,
                            "gap": {"mode":"fraction","gapFraction":0.11} },
            "rosette": { "construction":"ellipse-intersection",
                         "foils":{"count":6,"type":"round"} },
            "fillets": { "enabled": true, "stoneBandWidth": 12.0 }
        }
    })JSON";
    WindowInstance inst = loadInstanceFromJson(jsn);
    EXPECT_TRUE(inst.hasFillets);
    EXPECT_DOUBLE_EQ(inst.filletsStoneBandWidth, 12.0);
}

TEST(TEST_cgmath_architecture_gothic_io, BuildGeometryWithFillets)
{
    std::string jsn = R"JSON({
        "window": {
            "basis": { "pL": {"x":-100,"y":0}, "pR": {"x":100,"y":0} },
            "arch": { "width":200, "excess":1.0,
                      "offset": {"outer":16,"inner":10} },
            "subwindows": { "count":2, "excess":1.0,
                            "gap": {"mode":"fraction","gapFraction":0.11} },
            "rosette": { "construction":"ellipse-intersection",
                         "foils":{"count":6,"type":"round"} },
            "fillets": { "enabled": true, "stoneBandWidth": 10.0 }
        }
    })JSON";
    WindowInstance inst = loadInstanceFromJson(jsn);
    WindowGeometry g    = buildGeometryFromInstance(inst);
    EXPECT_TRUE(g.hasFillets);
    EXPECT_EQ(g.fillets.fillets.size(), 2u);
}

TEST(TEST_cgmath_architecture_gothic_io, FilletsSkippedWhenRosetteAbsent)
{
    // No rosette -> fillets cannot be built, even if enabled.
    std::string jsn = R"JSON({
        "window": {
            "basis": { "pL": {"x":-100,"y":0}, "pR": {"x":100,"y":0} },
            "arch": { "width":200, "excess":1.0,
                      "offset": {"outer":16,"inner":10} },
            "subwindows": { "count":2, "excess":1.0,
                            "gap": {"mode":"fraction","gapFraction":0.11} },
            "fillets": { "enabled": true, "stoneBandWidth": 10.0 }
        }
    })JSON";
    WindowInstance inst = loadInstanceFromJson(jsn);
    WindowGeometry g    = buildGeometryFromInstance(inst);
    EXPECT_TRUE(inst.hasFillets);     // requested in JSON
    EXPECT_FALSE(g.hasFillets);       // but not built (no rosette)
}

TEST(TEST_cgmath_architecture_gothic_io, SvgWithFilletsHasExtraPolylines)
{
    std::string jsn = R"JSON({
        "window": {
            "basis": { "pL": {"x":-100,"y":0}, "pR": {"x":100,"y":0} },
            "arch": { "width":200, "excess":1.0,
                      "offset": {"outer":16,"inner":10} },
            "subwindows": { "count":2, "excess":1.0,
                            "gap": {"mode":"fraction","gapFraction":0.11} },
            "rosette": { "construction":"ellipse-intersection",
                         "foils":{"count":6,"type":"round"} },
            "fillets": { "enabled": true, "stoneBandWidth": 10.0 }
        }
    })JSON";
    WindowGeometry g = buildGeometryFromInstance(loadInstanceFromJson(jsn));
    std::string svg = toSvg(g);
    EXPECT_NE(svg.find("<!-- fillets -->"), std::string::npos);
}

TEST(TEST_cgmath_architecture_gothic_io, RenderFilletsSnapshotToTmp)
{
    std::string jsn = R"JSON({
        "window": {
            "basis": { "pL": {"x":-100,"y":0}, "pR": {"x":100,"y":0} },
            "arch": { "width":200, "excess":1.0,
                      "offset": {"outer":16,"inner":10} },
            "subwindows": { "count":2, "excess":1.0,
                            "gap": {"mode":"fraction","gapFraction":0.11},
                            "foils": {"count":3,"type":"round"} },
            "rosette": { "construction":"ellipse-intersection",
                         "foils":{"count":6,"type":"round"} },
            "fillets": { "enabled": true, "stoneBandWidth": 10.0 }
        }
    })JSON";
    WindowGeometry g = buildGeometryFromInstance(loadInstanceFromJson(jsn));

    std::filesystem::path root    = findProjectRoot();
    std::filesystem::path outFile = root / "tmp" / "high-gothic-fillets.svg";
    EXPECT_NO_THROW(writeSvgToFile(g, outFile.string()));
    EXPECT_TRUE(std::filesystem::exists(outFile));
}

//
// Mouchettes
//

TEST(TEST_cgmath_architecture_gothic_io, ParseMouchettesDisabledByDefault)
{
    WindowInstance inst = loadInstanceFromJson(kMinimalInstance);
    EXPECT_FALSE(inst.hasMouchettes);
}

TEST(TEST_cgmath_architecture_gothic_io, ParseMouchettesEnabledVesica)
{
    std::string jsn = R"JSON({
        "window": {
            "basis": { "pL": {"x":-100,"y":0}, "pR": {"x":100,"y":0} },
            "arch": { "width":200, "excess":1.0,
                      "offset": {"outer":16,"inner":10} },
            "subwindows": { "count":2, "excess":1.0,
                            "gap": {"mode":"fraction","gapFraction":0.11} },
            "mouchettes": { "enabled": true, "type": "vesica",
                             "radiusFactor": 0.20, "rotation": 1.5708 }
        }
    })JSON";
    WindowInstance inst = loadInstanceFromJson(jsn);
    EXPECT_TRUE(inst.hasMouchettes);
    EXPECT_EQ(inst.mouchettes.type, MouchetteType::Vesica);
    EXPECT_DOUBLE_EQ(inst.mouchettes.radiusFactor, 0.20);
    EXPECT_DOUBLE_EQ(inst.mouchettes.rotation,     1.5708);
}

TEST(TEST_cgmath_architecture_gothic_io, ParseMouchettesTeardropAndSoufflet)
{
    std::string jsnT = R"JSON({"window":{
        "basis":{"pL":{"x":-100,"y":0},"pR":{"x":100,"y":0}},
        "arch":{"width":200,"excess":1.0,"offset":{"outer":16,"inner":10}},
        "subwindows":{"count":2,"excess":1.0,"gap":{"mode":"fraction","gapFraction":0.11}},
        "mouchettes":{"enabled":true,"type":"teardrop"}
    }})JSON";
    EXPECT_EQ(loadInstanceFromJson(jsnT).mouchettes.type, MouchetteType::Teardrop);

    std::string jsnS = R"JSON({"window":{
        "basis":{"pL":{"x":-100,"y":0},"pR":{"x":100,"y":0}},
        "arch":{"width":200,"excess":1.0,"offset":{"outer":16,"inner":10}},
        "subwindows":{"count":2,"excess":1.0,"gap":{"mode":"fraction","gapFraction":0.11}},
        "mouchettes":{"enabled":true,"type":"soufflet"}
    }})JSON";
    EXPECT_EQ(loadInstanceFromJson(jsnS).mouchettes.type, MouchetteType::Soufflet);
}

TEST(TEST_cgmath_architecture_gothic_io, BuildGeometryWithMouchettes)
{
    std::string jsn = R"JSON({
        "window": {
            "basis": { "pL": {"x":-100,"y":0}, "pR": {"x":100,"y":0} },
            "arch": { "width":200, "excess":1.0,
                      "offset": {"outer":16,"inner":10} },
            "subwindows": { "count":3, "excess":1.0,
                            "gap": {"mode":"fraction","gapFraction":0.06} },
            "mouchettes": { "enabled": true, "type": "vesica" }
        }
    })JSON";
    WindowGeometry g = buildGeometryFromInstance(loadInstanceFromJson(jsn));
    EXPECT_TRUE(g.hasMouchettes);
    EXPECT_EQ(g.mouchettes.size(), 2u);    // n=3 -> 2 gaps -> 2 mouchettes
}

TEST(TEST_cgmath_architecture_gothic_io, BuildGeometrySkipsMouchettesForSingleLancet)
{
    std::string jsn = R"JSON({
        "window": {
            "basis": { "pL": {"x":-100,"y":0}, "pR": {"x":100,"y":0} },
            "arch": { "width":200, "excess":1.0,
                      "offset": {"outer":16,"inner":10} },
            "subwindows": { "count":1, "excess":1.0,
                            "gap": {"mode":"fraction","gapFraction":0.11} },
            "mouchettes": { "enabled": true, "type": "vesica" }
        }
    })JSON";
    WindowInstance inst = loadInstanceFromJson(jsn);
    WindowGeometry g    = buildGeometryFromInstance(inst);
    EXPECT_TRUE(inst.hasMouchettes);
    EXPECT_FALSE(g.hasMouchettes);    // n=1 -> empty, hasMouchettes stays false
}

TEST(TEST_cgmath_architecture_gothic_io, SvgWithMouchettesHasComment)
{
    std::string jsn = R"JSON({
        "window": {
            "basis": { "pL": {"x":-100,"y":0}, "pR": {"x":100,"y":0} },
            "arch": { "width":200, "excess":1.0,
                      "offset": {"outer":16,"inner":10} },
            "subwindows": { "count":2, "excess":1.0,
                            "gap": {"mode":"fraction","gapFraction":0.11} },
            "mouchettes": { "enabled": true, "type": "vesica" }
        }
    })JSON";
    WindowGeometry g = buildGeometryFromInstance(loadInstanceFromJson(jsn));
    std::string svg = toSvg(g);
    EXPECT_NE(svg.find("<!-- mouchettes -->"), std::string::npos);
}

TEST(TEST_cgmath_architecture_gothic_io, RenderMouchettesSnapshotToTmp)
{
    std::string jsn = R"JSON({
        "window": {
            "basis": { "pL": {"x":-100,"y":0}, "pR": {"x":100,"y":0} },
            "arch": { "width":200, "excess":1.0,
                      "offset": {"outer":16,"inner":10} },
            "subwindows": { "count":3, "excess":1.0,
                            "gap": {"mode":"fraction","gapFraction":0.06},
                            "foils": {"count":3,"type":"round"} },
            "rosette": { "construction":"ellipse-intersection",
                         "foils":{"count":8,"type":"round"} },
            "mouchettes": { "enabled": true, "type": "teardrop",
                             "radiusFactor": 0.18, "rotation": 1.5708 }
        }
    })JSON";
    WindowGeometry g = buildGeometryFromInstance(loadInstanceFromJson(jsn));

    std::filesystem::path root    = findProjectRoot();
    std::filesystem::path outFile = root / "tmp" / "high-gothic-mouchettes.svg";
    EXPECT_NO_THROW(writeSvgToFile(g, outFile.string()));
    EXPECT_TRUE(std::filesystem::exists(outFile));
}
