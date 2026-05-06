#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "../src/cgmesh/architecture_gothic.h"

namespace
{
    // Walk up from CWD until the project root is found (test/data + src/cgmath both exist).
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
        return std::filesystem::current_path();
    }

    // A typical 2-lancet equilateral instance, NO rosette, NO foils.
    // Phase 1 polygon : outer + 2 lancet inner holes = 3 contours.
    WindowGeometry buildTypicalGeom()
    {
        const char *jsn = R"JSON({
            "window": {
                "basis": { "pL": {"x":-100,"y":0}, "pR": {"x":100,"y":0} },
                "arch": { "width":200, "excess":1.0,
                          "offset": {"outer":16,"inner":10} },
                "subwindows": { "count":2, "excess":1.0,
                                "gap": {"mode":"fraction","gapFraction":0.05} }
            }
        })JSON";
        return buildGeometryFromInstance(loadInstanceFromJson(jsn));
    }

    // A rich instance with rosette + foils (rosette and lancet).
    // Phase 2 polygon contains additional holes for these voids.
    WindowGeometry buildRichGeom()
    {
        const char *jsn = R"JSON({
            "window": {
                "basis": { "pL": {"x":-100,"y":0}, "pR": {"x":100,"y":0} },
                "arch": { "width":200, "excess":1.0,
                          "offset": {"outer":16,"inner":10} },
                "subwindows": { "count":2, "excess":1.0,
                                "gap": {"mode":"fraction","gapFraction":0.11},
                                "foils": {"count":3,"type":"round"} },
                "rosette": { "construction":"ellipse-intersection",
                             "foils":{"count":6,"type":"round"} }
            }
        })JSON";
        return buildGeometryFromInstance(loadInstanceFromJson(jsn));
    }
}

//
// Polygon construction
//

TEST(TEST_cgmesh_architecture_gothic, BuildBayPolygonHasOneOuterPlusLancetHoles)
{
    WindowGeometry g = buildTypicalGeom();
    Polygon2 poly = buildBayStonePolygon(g);
    EXPECT_EQ(poly.get_n_contours(), 1 + (int) g.subwindows.lancets.size());
}

TEST(TEST_cgmesh_architecture_gothic, BuildBayPolygonOuterContourIsCcw)
{
    WindowGeometry g = buildTypicalGeom();
    Polygon2 poly = buildBayStonePolygon(g);

    // Compute signed area of contour 0 only (outer) using a manual shoelace.
    int    nPts = poly.get_n_points(0);
    float *pts  = poly.get_points(0);
    double signedArea = 0.0;
    for (int i = 0; i < nPts; ++i)
    {
        int    j  = (i + 1) % nPts;
        signedArea += (double) pts[2*i] * (double) pts[2*j+1]
                    - (double) pts[2*j] * (double) pts[2*i+1];
    }
    signedArea *= 0.5;
    EXPECT_GT(signedArea, 0.0) << "outer contour should be CCW (positive area)";
}

TEST(TEST_cgmesh_architecture_gothic, BuildBayPolygonHoleContoursAreCw)
{
    WindowGeometry g = buildTypicalGeom();
    Polygon2 poly = buildBayStonePolygon(g);

    int n = poly.get_n_contours();
    for (int c = 1; c < n; ++c)
    {
        int    nPts = poly.get_n_points(c);
        float *pts  = poly.get_points(c);
        double signedArea = 0.0;
        for (int i = 0; i < nPts; ++i)
        {
            int j = (i + 1) % nPts;
            signedArea += (double) pts[2*i] * (double) pts[2*j+1]
                        - (double) pts[2*j] * (double) pts[2*i+1];
        }
        signedArea *= 0.5;
        EXPECT_LT(signedArea, 0.0)
            << "hole contour " << c << " should be CW (negative area)";
    }
}

TEST(TEST_cgmesh_architecture_gothic, BuildBayPolygonOuterAreaExceedsHoleAreas)
{
    // Manually compute signed areas for each contour (Polygon2::area only works
    // for single-contour polygons). The outer contour area should be larger in
    // magnitude than the sum of hole areas.
    WindowGeometry g = buildTypicalGeom();
    Polygon2 poly = buildBayStonePolygon(g);

    auto signedArea = [&](int c) {
        int    nPts = poly.get_n_points(c);
        float *pts  = poly.get_points(c);
        double sum  = 0.0;
        for (int i = 0; i < nPts; ++i)
        {
            int j = (i + 1) % nPts;
            sum += (double) pts[2*i] * (double) pts[2*j+1]
                 - (double) pts[2*j] * (double) pts[2*i+1];
        }
        return 0.5 * sum;
    };

    double outerArea = signedArea(0);
    double holesAreaMagnitude = 0.0;
    for (int c = 1; c < poly.get_n_contours(); ++c)
        holesAreaMagnitude += std::fabs(signedArea(c));

    EXPECT_GT(outerArea, holesAreaMagnitude);
}

//
// Tessellation
//

TEST(TEST_cgmesh_architecture_gothic, TessellateProducesNonEmptyMesh)
{
    WindowGeometry g = buildTypicalGeom();
    Polygon2 poly = buildBayStonePolygon(g);
    Mesh mesh;
    tessellateToMesh(poly, mesh, 0.0);
    EXPECT_GT(mesh.GetNVertices(), 0u);
    EXPECT_GT(mesh.GetNFaces(),    0u);
}

TEST(TEST_cgmesh_architecture_gothic, TessellateMeshHasZeroZByDefault)
{
    WindowGeometry g = buildTypicalGeom();
    Polygon2 poly = buildBayStonePolygon(g);
    Mesh mesh;
    tessellateToMesh(poly, mesh, 0.0);

    unsigned int n = mesh.GetNVertices();
    ASSERT_GT(n, 0u);
    for (unsigned int i = 0; i < n; ++i)
    {
        float v[3];
        ASSERT_EQ(mesh.GetVertex(i, v), 0);
        EXPECT_FLOAT_EQ(v[2], 0.0f);
    }
}

TEST(TEST_cgmesh_architecture_gothic, TessellateAcceptsCustomZ)
{
    WindowGeometry g = buildTypicalGeom();
    Polygon2 poly = buildBayStonePolygon(g);
    Mesh mesh;
    tessellateToMesh(poly, mesh, 5.0);

    unsigned int n = mesh.GetNVertices();
    ASSERT_GT(n, 0u);
    for (unsigned int i = 0; i < n; ++i)
    {
        float v[3];
        ASSERT_EQ(mesh.GetVertex(i, v), 0);
        EXPECT_FLOAT_EQ(v[2], 5.0f);
    }
}

//
// File output
//

TEST(TEST_cgmesh_architecture_gothic, WriteBayMeshObj)
{
    WindowGeometry g = buildTypicalGeom();
    std::filesystem::path root    = findProjectRoot();
    std::filesystem::path outFile = root / "tmp" / "high-gothic-bay.obj";

    if (std::filesystem::exists(outFile))
        std::filesystem::remove(outFile);

    EXPECT_NO_THROW(writeBayMesh(g, outFile.string()));
    EXPECT_TRUE(std::filesystem::exists(outFile));

    // Sanity : file is non-empty and starts with vertex declarations.
    std::ifstream f(outFile);
    std::stringstream ss; ss << f.rdbuf();
    std::string contents = ss.str();
    EXPECT_GT(contents.size(), 0u);
    EXPECT_NE(contents.find("v "), std::string::npos);
    EXPECT_NE(contents.find("f "), std::string::npos);
}

TEST(TEST_cgmesh_architecture_gothic, WriteBayMeshStl)
{
    WindowGeometry g = buildTypicalGeom();
    std::filesystem::path root    = findProjectRoot();
    std::filesystem::path outFile = root / "tmp" / "high-gothic-bay.stl";

    if (std::filesystem::exists(outFile))
        std::filesystem::remove(outFile);

    EXPECT_NO_THROW(writeBayMesh(g, outFile.string()));
    EXPECT_TRUE(std::filesystem::exists(outFile));

    // ASCII STL : begins with "solid", contains "facet normal" and "endsolid".
    std::ifstream f(outFile);
    std::stringstream ss; ss << f.rdbuf();
    std::string contents = ss.str();
    EXPECT_EQ(contents.substr(0, 5), "solid");
    EXPECT_NE(contents.find("facet normal"), std::string::npos);
    EXPECT_NE(contents.find("endsolid"),     std::string::npos);
}

TEST(TEST_cgmesh_architecture_gothic, WriteBayMeshUnknownExtensionThrows)
{
    WindowGeometry g = buildTypicalGeom();
    std::filesystem::path root    = findProjectRoot();
    std::filesystem::path outFile = root / "tmp" / "high-gothic-bay.unknown_ext";

    EXPECT_THROW(writeBayMesh(g, outFile.string()), std::runtime_error);
}

//
// Phase 2 voids : rosette + foils
//

TEST(TEST_cgmesh_architecture_gothic, BuildBayPolygonRichHasExpectedContourCount)
{
    // Outer + 2 lancet inner + 1 rosette + 6 rosette foils + 2*3 lancet foils = 16.
    WindowGeometry g = buildRichGeom();
    Polygon2 poly = buildBayStonePolygon(g);
    EXPECT_EQ(poly.get_n_contours(), 1 + 2 + 1 + 6 + 6);
}

TEST(TEST_cgmesh_architecture_gothic, BuildBayPolygonRichAllHolesAreCw)
{
    WindowGeometry g = buildRichGeom();
    Polygon2 poly = buildBayStonePolygon(g);

    // Every hole (contour 1..n) has negative signed area.
    int n = poly.get_n_contours();
    for (int c = 1; c < n; ++c)
    {
        int    nPts = poly.get_n_points(c);
        float *pts  = poly.get_points(c);
        double sum  = 0.0;
        for (int i = 0; i < nPts; ++i)
        {
            int j = (i + 1) % nPts;
            sum += (double) pts[2*i] * (double) pts[2*j+1]
                 - (double) pts[2*j] * (double) pts[2*i+1];
        }
        EXPECT_LT(sum, 0.0) << "hole contour " << c << " should be CW";
    }
}

TEST(TEST_cgmesh_architecture_gothic, RichTessellationProducesMoreFacesThanPhase1)
{
    // The richer polygon (with rosette + foils) should produce more triangles.
    Polygon2 polyT = buildBayStonePolygon(buildTypicalGeom());
    Polygon2 polyR = buildBayStonePolygon(buildRichGeom());

    Mesh meshT, meshR;
    tessellateToMesh(polyT, meshT, 0.0);
    tessellateToMesh(polyR, meshR, 0.0);

    EXPECT_GT(meshR.GetNFaces(), meshT.GetNFaces());
}

TEST(TEST_cgmesh_architecture_gothic, WriteRichBayMeshObj)
{
    // End-to-end : load a rich JSON, write tmp/high-gothic-bay-rich.obj
    WindowGeometry g = buildRichGeom();
    std::filesystem::path root    = findProjectRoot();
    std::filesystem::path outFile = root / "tmp" / "high-gothic-bay-rich.obj";
    if (std::filesystem::exists(outFile))
        std::filesystem::remove(outFile);
    EXPECT_NO_THROW(writeBayMesh(g, outFile.string()));
    EXPECT_TRUE(std::filesystem::exists(outFile));
}

TEST(TEST_cgmesh_architecture_gothic, WriteRichBayMeshStl)
{
    WindowGeometry g = buildRichGeom();
    std::filesystem::path root    = findProjectRoot();
    std::filesystem::path outFile = root / "tmp" / "high-gothic-bay-rich.stl";
    if (std::filesystem::exists(outFile))
        std::filesystem::remove(outFile);
    EXPECT_NO_THROW(writeBayMesh(g, outFile.string()));
    EXPECT_TRUE(std::filesystem::exists(outFile));
}

//
// Phase 3 : extrusion
//

TEST(TEST_cgmesh_architecture_gothic, ExtrudeProducesDoubledVertexCount)
{
    WindowGeometry g = buildTypicalGeom();
    Polygon2 poly = buildBayStonePolygon(g);

    Mesh meshFlat;
    tessellateToMesh(poly, meshFlat, 0.0);

    Mesh meshExt;
    extrudeToMesh(poly, meshExt, 0.0, 5.0);

    // Extruded mesh has top + bottom = 2x the flat vertex count.
    EXPECT_EQ(meshExt.GetNVertices(), 2u * meshFlat.GetNVertices());
}

TEST(TEST_cgmesh_architecture_gothic, ExtrudeMeshHasFacesAtTopAndBottomZ)
{
    WindowGeometry g = buildTypicalGeom();
    Polygon2 poly = buildBayStonePolygon(g);
    Mesh mesh;
    extrudeToMesh(poly, mesh, 0.0, 5.0);

    bool sawTop = false, sawBot = false, sawSide = false;
    unsigned int n = mesh.GetNFaces();
    ASSERT_GT(n, 0u);
    for (unsigned int i = 0; i < n; ++i)
    {
        int a = mesh.GetFaceVertex(i, 0);
        int b = mesh.GetFaceVertex(i, 1);
        int c = mesh.GetFaceVertex(i, 2);
        ASSERT_GE(a, 0); ASSERT_GE(b, 0); ASSERT_GE(c, 0);

        float va[3], vb[3], vc[3];
        mesh.GetVertex((unsigned int) a, va);
        mesh.GetVertex((unsigned int) b, vb);
        mesh.GetVertex((unsigned int) c, vc);

        bool allTop = (va[2] == 5.0f && vb[2] == 5.0f && vc[2] == 5.0f);
        bool allBot = (va[2] == 0.0f && vb[2] == 0.0f && vc[2] == 0.0f);
        if (allTop) sawTop = true;
        else if (allBot) sawBot = true;
        else            sawSide = true;
    }
    EXPECT_TRUE(sawTop)  << "no top-cap triangles detected";
    EXPECT_TRUE(sawBot)  << "no bottom-cap triangles detected";
    EXPECT_TRUE(sawSide) << "no side-wall triangles detected (mixed-z faces)";
}

TEST(TEST_cgmesh_architecture_gothic, ExtrudeFlipsBottomCapNormals)
{
    // For a CCW outer polygon, top-cap triangles have +z normal, bottom-cap
    // triangles should have -z normal (= reversed winding).
    WindowGeometry g = buildTypicalGeom();
    Polygon2 poly = buildBayStonePolygon(g);
    Mesh mesh;
    extrudeToMesh(poly, mesh, 0.0, 5.0);

    int positiveZNormals = 0, negativeZNormals = 0;
    unsigned int n = mesh.GetNFaces();
    for (unsigned int i = 0; i < n; ++i)
    {
        int a = mesh.GetFaceVertex(i, 0);
        int b = mesh.GetFaceVertex(i, 1);
        int c = mesh.GetFaceVertex(i, 2);
        float va[3], vb[3], vc[3];
        mesh.GetVertex((unsigned int) a, va);
        mesh.GetVertex((unsigned int) b, vb);
        mesh.GetVertex((unsigned int) c, vc);

        // Only count triangles whose all vertices share the same z (caps).
        if (va[2] != vb[2] || vb[2] != vc[2]) continue;

        // Triangle normal = (b-a) x (c-a).
        float ux = vb[0]-va[0], uy = vb[1]-va[1];
        float vx = vc[0]-va[0], vy = vc[1]-va[1];
        float nz = ux*vy - uy*vx;
        if (nz > 0.0f) ++positiveZNormals;
        else if (nz < 0.0f) ++negativeZNormals;
    }
    EXPECT_GT(positiveZNormals, 0);
    EXPECT_GT(negativeZNormals, 0);
}

TEST(TEST_cgmesh_architecture_gothic, WriteBayMeshExtrudedObj)
{
    WindowGeometry g = buildRichGeom();
    GothicMeshParams params;
    params.zHeight = 8.0;

    std::filesystem::path root    = findProjectRoot();
    std::filesystem::path outFile = root / "tmp" / "high-gothic-bay-rich-extruded.obj";
    if (std::filesystem::exists(outFile))
        std::filesystem::remove(outFile);
    EXPECT_NO_THROW(writeBayMesh(g, outFile.string(), params));
    EXPECT_TRUE(std::filesystem::exists(outFile));
}

TEST(TEST_cgmesh_architecture_gothic, WriteBayMeshExtrudedStl)
{
    WindowGeometry g = buildRichGeom();
    GothicMeshParams params;
    params.zHeight = 8.0;

    std::filesystem::path root    = findProjectRoot();
    std::filesystem::path outFile = root / "tmp" / "high-gothic-bay-rich-extruded.stl";
    if (std::filesystem::exists(outFile))
        std::filesystem::remove(outFile);
    EXPECT_NO_THROW(writeBayMesh(g, outFile.string(), params));
    EXPECT_TRUE(std::filesystem::exists(outFile));
}

//
// Pointed foils as voids
//

namespace
{
    WindowGeometry buildGeomWithPointedFoils()
    {
        const char *jsn = R"JSON({
            "window": {
                "basis": { "pL": {"x":-100,"y":0}, "pR": {"x":100,"y":0} },
                "arch": { "width":200, "excess":1.0,
                          "offset": {"outer":16,"inner":10} },
                "subwindows": { "count":2, "excess":1.0,
                                "gap": {"mode":"fraction","gapFraction":0.11},
                                "foils": {"count":3,"type":"pointed",
                                          "pointedness":0.5} },
                "rosette": { "construction":"ellipse-intersection",
                             "foils":{"count":6,"type":"pointed",
                                      "pointedness":0.5} }
            }
        })JSON";
        return buildGeometryFromInstance(loadInstanceFromJson(jsn));
    }
}

TEST(TEST_cgmesh_architecture_gothic, BuildBayPolygonWithPointedFoilsHasExpectedContourCount)
{
    // Outer + 2 lancet inner + 1 rosette + 6 rosette pointed foils + 2*3 lancet pointed foils = 16.
    WindowGeometry g = buildGeomWithPointedFoils();
    Polygon2 poly = buildBayStonePolygon(g);
    EXPECT_EQ(poly.get_n_contours(), 1 + 2 + 1 + 6 + 6);
}

TEST(TEST_cgmesh_architecture_gothic, PointedFoilContoursAreCw)
{
    // Auto-orientation should produce CW (negative-area) contours for all
    // pointed foil holes in the polygon.
    WindowGeometry g = buildGeomWithPointedFoils();
    Polygon2 poly = buildBayStonePolygon(g);

    int n = poly.get_n_contours();
    for (int c = 1; c < n; ++c)
    {
        int    nPts = poly.get_n_points(c);
        float *pts  = poly.get_points(c);
        double sum  = 0.0;
        for (int i = 0; i < nPts; ++i)
        {
            int j = (i + 1) % nPts;
            sum += (double) pts[2*i] * (double) pts[2*j+1]
                 - (double) pts[2*j] * (double) pts[2*i+1];
        }
        EXPECT_LT(sum, 0.0) << "hole contour " << c << " should be CW";
    }
}

TEST(TEST_cgmesh_architecture_gothic, WriteBayMeshWithPointedFoilsObj)
{
    WindowGeometry g = buildGeomWithPointedFoils();
    GothicMeshParams params;
    params.zHeight = 8.0;
    std::filesystem::path root    = findProjectRoot();
    std::filesystem::path outFile = root / "tmp" / "high-gothic-bay-pointed-foils.obj";
    if (std::filesystem::exists(outFile))
        std::filesystem::remove(outFile);
    EXPECT_NO_THROW(writeBayMesh(g, outFile.string(), params));
    EXPECT_TRUE(std::filesystem::exists(outFile));
}

//
// B2 — Profile sweep
//

TEST(TEST_cgmesh_architecture_gothic, RectangularProfileHasFourPoints)
{
    auto p = rectangularProfile();
    EXPECT_EQ(p.size(), 4u);
}

TEST(TEST_cgmesh_architecture_gothic, SweepProducesNonEmptyMesh)
{
    WindowGeometry g = buildTypicalGeom();
    Mesh m;
    sweepProfileAlongArc(g.mainOffset.inner.arcLeft, rectangularProfile(),
                          /*scale_u=*/8.0, /*scale_v=*/12.0, m);
    EXPECT_GT(m.GetNVertices(), 0u);
    EXPECT_GT(m.GetNFaces(), 0u);
}

TEST(TEST_cgmesh_architecture_gothic, SweepVertexCountMatchesPathTimesProfile)
{
    WindowGeometry g = buildTypicalGeom();
    auto profile = rectangularProfile();
    auto pathPts = g.mainOffset.inner.arcLeft.tessellateAdaptive(3.14159265358979323846 / 180.0);

    Mesh m;
    sweepProfileAlongArc(g.mainOffset.inner.arcLeft, profile, 8.0, 12.0, m);
    EXPECT_EQ(m.GetNVertices(), pathPts.size() * profile.size());
}

TEST(TEST_cgmesh_architecture_gothic, SweepProfileTooSmallThrows)
{
    WindowGeometry g = buildTypicalGeom();
    std::vector<Vector2d> tinyProfile = { {0, 0}, {1, 0} };   // only 2 points
    Mesh m;
    EXPECT_THROW(sweepProfileAlongArc(g.mainOffset.inner.arcLeft, tinyProfile, 8.0, 12.0, m),
                 std::invalid_argument);
}

TEST(TEST_cgmesh_architecture_gothic, SweepDegenerateArcThrows)
{
    Arc deg;   // default-constructed : zero radius, zero span -> length 0
    Mesh m;
    EXPECT_THROW(sweepProfileAlongArc(deg, rectangularProfile(), 8.0, 12.0, m),
                 std::invalid_argument);
}

TEST(TEST_cgmesh_architecture_gothic, SweepProducesObjFile)
{
    WindowGeometry g = buildTypicalGeom();
    Mesh leftSweep;
    sweepProfileAlongArc(g.mainOffset.inner.arcLeft, rectangularProfile(),
                          8.0, 12.0, leftSweep);

    std::filesystem::path root    = findProjectRoot();
    std::filesystem::path outFile = root / "tmp" / "high-gothic-bay-sweep-left.obj";
    if (std::filesystem::exists(outFile))
        std::filesystem::remove(outFile);

    int rc = leftSweep.save(outFile.string().c_str());
    EXPECT_GE(rc, 0);
    EXPECT_TRUE(std::filesystem::exists(outFile));
}
