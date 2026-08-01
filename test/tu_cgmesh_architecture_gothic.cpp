#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

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
    // Outer + 2 plain lancets + rosette flower + 6 rosette spandrel fillets = 10.
    // (The rosette foil ring is cut as ONE connected flower void plus one eyelet
    //  fillet per foil, between the foils and the ring.)
    WindowGeometry g = buildRichGeom();
    Polygon2 poly = buildBayStonePolygon(g);
    EXPECT_EQ(poly.get_n_contours(), 1 + 2 + 1 + 6);
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

TEST(TEST_cgmesh_architecture_gothic, ExtrudeProducesQuadrupledVertexCount)
{
    WindowGeometry g = buildTypicalGeom();
    Polygon2 poly = buildBayStonePolygon(g);

    Mesh meshFlat;
    tessellateToMesh(poly, meshFlat, 0.0);

    Mesh meshExt;
    extrudeToMesh(poly, meshExt, 0.0, 5.0);

    // Distinct vertices for cap-top, cap-bottom, wall-top, wall-bottom (so the
    // front/back-to-wall edges keep hard normals) = 4x the flat vertex count.
    EXPECT_EQ(meshExt.GetNVertices(), 4u * meshFlat.GetNVertices());
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
    // Outer + 2 plain lancets + rosette flower + 6 rosette spandrel fillets = 10.
    WindowGeometry g = buildGeomWithPointedFoils();
    Polygon2 poly = buildBayStonePolygon(g);
    EXPECT_EQ(poly.get_n_contours(), 1 + 2 + 1 + 6);
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

//
// Multi-arc sweep
//

TEST(TEST_cgmesh_architecture_gothic, SweepMultiArcsConcatTubes)
{
    WindowGeometry g = buildTypicalGeom();
    Mesh single;
    sweepProfileAlongArc(g.mainOffset.inner.arcLeft, rectangularProfile(),
                          8.0, 12.0, single);

    Mesh multi;
    sweepProfileAlongArcs({ g.mainOffset.inner.arcLeft, g.mainOffset.inner.arcRight },
                          rectangularProfile(), 8.0, 12.0, multi);

    // Multi-arc tube has roughly twice as many vertices/faces as single-arc.
    EXPECT_GT(multi.GetNVertices(), single.GetNVertices());
    EXPECT_GT(multi.GetNFaces(),    single.GetNFaces());
}

TEST(TEST_cgmesh_architecture_gothic, SweepMultiArcsEmptyThrows)
{
    Mesh m;
    EXPECT_THROW(sweepProfileAlongArcs({}, rectangularProfile(), 8.0, 12.0, m),
                 std::invalid_argument);
}

TEST(TEST_cgmesh_architecture_gothic, SweepMultiArcsProfileTooSmallThrows)
{
    WindowGeometry g = buildTypicalGeom();
    std::vector<Vector2d> tiny = { {0, 0}, {1, 0} };
    Mesh m;
    EXPECT_THROW(sweepProfileAlongArcs({ g.mainOffset.inner.arcLeft }, tiny, 8.0, 12.0, m),
                 std::invalid_argument);
}

//
// Classical profile factories
//

TEST(TEST_cgmesh_architecture_gothic, ChamferProfileHasFivePoints)
{
    auto p = chamferProfile();
    EXPECT_EQ(p.size(), 5u);
    // First point on outside face, just above chamfer.
    EXPECT_DOUBLE_EQ(p[0].x, 0.0);
    EXPECT_DOUBLE_EQ(p[0].y, 0.3);
    // Second point on bottom face, at end of chamfer.
    EXPECT_DOUBLE_EQ(p[1].x, 0.3);
    EXPECT_DOUBLE_EQ(p[1].y, 0.0);
}

TEST(TEST_cgmesh_architecture_gothic, CavettoProfileHasExpectedPointCount)
{
    auto p = cavettoProfile(0.3, 6);
    // 6 curve segments produce 7 curve samples + 3 corners = 10.
    EXPECT_EQ(p.size(), 10u);
}

TEST(TEST_cgmesh_architecture_gothic, ChamferProfileSweepsSuccessfully)
{
    WindowGeometry g = buildTypicalGeom();
    Mesh m;
    EXPECT_NO_THROW(sweepProfileAlongArc(g.mainOffset.inner.arcLeft,
                                          chamferProfile(), 8.0, 12.0, m));
    EXPECT_GT(m.GetNVertices(), 0u);
}

//
// appendMesh helper
//

TEST(TEST_cgmesh_architecture_gothic, AppendMeshIncreasesCounts)
{
    WindowGeometry g = buildTypicalGeom();
    Mesh m1, m2;
    sweepProfileAlongArc(g.mainOffset.inner.arcLeft, rectangularProfile(), 8.0, 12.0, m1);
    sweepProfileAlongArc(g.mainOffset.inner.arcRight, rectangularProfile(), 8.0, 12.0, m2);

    unsigned int origNV = m1.GetNVertices();
    unsigned int origNF = m1.GetNFaces();
    unsigned int srcNV  = m2.GetNVertices();
    unsigned int srcNF  = m2.GetNFaces();

    appendMesh(m1, m2);

    EXPECT_EQ(m1.GetNVertices(), origNV + srcNV);
    EXPECT_EQ(m1.GetNFaces(),    origNF + srcNF);
}

//
// End-to-end : unified mesh = bay extrusion + sweep moldings
//

TEST(TEST_cgmesh_architecture_gothic, WriteUnifiedBayPlusMoldingObj)
{
    WindowGeometry g = buildRichGeom();

    // 1. Bay : flat tessellation (no extrusion for cleaner visualization).
    GothicMeshParams params;
    params.zHeight = 0.0;
    Polygon2 poly = buildBayStonePolygon(g, params);
    Mesh combined;
    tessellateToMesh(poly, combined, 0.0);

    // 2. Sweep : chamfer profile along main inner offset arcs.
    Mesh moldings;
    sweepProfileAlongArcs({ g.mainOffset.inner.arcLeft, g.mainOffset.inner.arcRight },
                          chamferProfile(0.4), /*scale_u=*/8.0, /*scale_v=*/10.0, moldings);

    // 3. Merge bay + moldings.
    appendMesh(combined, moldings);

    std::filesystem::path root    = findProjectRoot();
    std::filesystem::path outFile = root / "tmp" / "high-gothic-bay-unified.obj";
    if (std::filesystem::exists(outFile))
        std::filesystem::remove(outFile);

    int rc = combined.save(outFile.string().c_str());
    EXPECT_GE(rc, 0);
    EXPECT_TRUE(std::filesystem::exists(outFile));
}

// ===========================================================================
//  extrudeProfiledToMesh — extrusion a chanfrein
// ===========================================================================
//  Seule variante d'extrusion sans couverture jusqu'ici, et c'est la plus
//  delicate : elle decale chaque sommet le long d'une normale « vers la pierre »,
//  operation qui s'auto-intersecte sur un contour concave pointu. Le garde-fou
//  qui retombe alors sur une paroi verticale est verifie plus bas.

namespace
{
    // Bornes en z des sommets d'un maillage.
    void meshZRange (Mesh &m, float &zMin, float &zMax)
    {
        zMin = 0.f; zMax = 0.f;
        const unsigned int n = m.GetNVertices();
        for (unsigned int i = 0; i < n; ++i)
        {
            float v[3];
            m.GetVertex(i, v);
            if (i == 0) { zMin = zMax = v[2]; continue; }
            zMin = std::min(zMin, v[2]);
            zMax = std::max(zMax, v[2]);
        }
    }

    // Nombre de sommets situes exactement a une altitude donnee.
    unsigned int countVerticesAtZ (Mesh &m, float z, float tol = 1e-4f)
    {
        unsigned int c = 0;
        const unsigned int n = m.GetNVertices();
        for (unsigned int i = 0; i < n; ++i)
        {
            float v[3];
            m.GetVertex(i, v);
            if (std::fabs(v[2] - z) < tol) ++c;
        }
        return c;
    }

    // Volume signe (somme des tetraedres origine-triangle). Positif et proche du
    // volume attendu = surface fermee et normales sortantes.
    double signedVolume (Mesh &m)
    {
        double vol = 0.;
        std::vector<unsigned int> tris = m.GetTriangles();
        for (size_t t = 0; t + 2 < tris.size(); t += 3)
        {
            float a[3], b[3], c[3];
            m.GetVertex(tris[t], a);
            m.GetVertex(tris[t+1], b);
            m.GetVertex(tris[t+2], c);
            vol += ( (double)a[0]*((double)b[1]*c[2] - (double)b[2]*c[1])
                   - (double)a[1]*((double)b[0]*c[2] - (double)b[2]*c[0])
                   + (double)a[2]*((double)b[0]*c[1] - (double)b[1]*c[0]) ) / 6.0;
        }
        return vol;
    }
}

TEST(TEST_cgmesh_architecture_gothic, ExtrudeProfiledProducesGeometrySpanningTheGivenZ)
{
    WindowGeometry g = buildTypicalGeom();
    Polygon2 poly = buildBayStonePolygon(g);

    Mesh mesh;
    ASSERT_NO_THROW(extrudeProfiledToMesh(poly, mesh, 0.0, 10.0, /*chamW=*/2.0, /*chamD=*/4.0));
    ASSERT_GT(mesh.GetNVertices(), 0u);
    ASSERT_GT(mesh.GetNFaces(), 0u);

    float zMin = 0.f, zMax = 0.f;
    meshZRange(mesh, zMin, zMax);
    EXPECT_NEAR(zMin,  0.0f, 1e-4f);
    EXPECT_NEAR(zMax, 10.0f, 1e-4f);

    // La face avant reste PLATE a zTop : il doit y avoir des sommets exactement la.
    EXPECT_GT(countVerticesAtZ(mesh, 10.0f), 0u);
    EXPECT_GT(countVerticesAtZ(mesh,  0.0f), 0u);
}

// Le chanfrein insere un anneau de sommets supplementaire par contour : le
// maillage profile est donc plus lourd que l'extrusion droite, a contour egal.
TEST(TEST_cgmesh_architecture_gothic, ExtrudeProfiledAddsGeometryComparedToStraightExtrude)
{
    WindowGeometry g = buildTypicalGeom();

    Polygon2 polyA = buildBayStonePolygon(g);
    Mesh straight;
    ASSERT_NO_THROW(extrudeToMesh(polyA, straight, 0.0, 10.0));

    Polygon2 polyB = buildBayStonePolygon(g);
    Mesh profiled;
    ASSERT_NO_THROW(extrudeProfiledToMesh(polyB, profiled, 0.0, 10.0, 2.0, 4.0));

    EXPECT_GT(profiled.GetNVertices(), straight.GetNVertices());
    EXPECT_GT(profiled.GetNFaces(),    straight.GetNFaces());
}

// Un chanfrein NUL doit rester licite et redonner une piece de meme etendue que
// l'extrusion droite : c'est le cas degenere que traverse tout reglage a 0 dans
// l'UI.
TEST(TEST_cgmesh_architecture_gothic, ExtrudeProfiledWithZeroChamferIsWellFormed)
{
    WindowGeometry g = buildTypicalGeom();
    Polygon2 poly = buildBayStonePolygon(g);

    Mesh mesh;
    ASSERT_NO_THROW(extrudeProfiledToMesh(poly, mesh, 0.0, 10.0, 0.0, 0.0));
    ASSERT_GT(mesh.GetNVertices(), 0u);

    float zMin = 0.f, zMax = 0.f;
    meshZRange(mesh, zMin, zMax);
    EXPECT_NEAR(zMin,  0.0f, 1e-4f);
    EXPECT_NEAR(zMax, 10.0f, 1e-4f);
}

// Le volume reste POSITIF et borne par la boite englobante : c'est ce qui attrape
// une orientation de faces inversee ou des boucles parasites nees d'un offset
// par-sommet auto-intersectant (les « gouttes » historiques du chanfrein).
TEST(TEST_cgmesh_architecture_gothic, ExtrudeProfiledVolumeStaysPositiveAndBounded)
{
    WindowGeometry g = buildRichGeom();          // remplage riche = contours concaves pointus
    Polygon2 poly = buildBayStonePolygon(g);

    Mesh mesh;
    ASSERT_NO_THROW(extrudeProfiledToMesh(poly, mesh, 0.0, 10.0, 3.0, 6.0));
    ASSERT_GT(mesh.GetNVertices(), 0u);

    mesh.computebbox();
    float mn[3], mx[3];
    mesh.bbox().GetMinMax(mn, mx);
    const double boxVol = (double)(mx[0]-mn[0]) * (mx[1]-mn[1]) * (mx[2]-mn[2]);
    ASSERT_GT(boxVol, 0.0);

    const double vol = signedVolume(mesh);
    EXPECT_GT(vol, 0.0) << "volume negatif => faces orientees a l'envers";
    EXPECT_LT(vol, boxVol) << "volume superieur a la boite englobante => "
                              "geometrie repliee ou dupliquee";
}

// Un chanfrein demesure (plus large que la piece) ne doit ni lever ni produire un
// maillage vide : la fonction doit se replier sur une paroi droite.
TEST(TEST_cgmesh_architecture_gothic, ExtrudeProfiledSurvivesAnOversizedChamfer)
{
    WindowGeometry g = buildTypicalGeom();
    Polygon2 poly = buildBayStonePolygon(g);

    Mesh mesh;
    ASSERT_NO_THROW(extrudeProfiledToMesh(poly, mesh, 0.0, 10.0,
                                          /*chamW=*/500.0, /*chamD=*/500.0));
    EXPECT_GT(mesh.GetNVertices(), 0u);
    EXPECT_GT(mesh.GetNFaces(), 0u);
}

// ===========================================================================
//  buildBayMoulding — moulure balayee le long des bords de champ
// ===========================================================================
//  Autre fonction publique sans couverture. Contrat : elle balaie le profil le
//  long des VRAIES silhouettes que buildBayStonePolygon decoupe en vides, et
//  laisse `out` VIDE quand la geometrie n'offre rien de balayable -- ce dernier
//  point est un contrat explicite du header, donc un test a part entiere.

TEST(TEST_cgmesh_architecture_gothic, BuildBayMouldingProducesASweptMesh)
{
    WindowGeometry g = buildTypicalGeom();
    GothicMeshParams params;

    Mesh mesh;
    ASSERT_NO_THROW(buildBayMoulding(g, params, rectangularProfile(), mesh));
    EXPECT_GT(mesh.GetNVertices(), 0u);
    EXPECT_GT(mesh.GetNFaces(), 0u);

    // Le profil est un carre unite en (u, v) : u = profondeur z. La moulure sort
    // donc du plan, contrairement a une tesselation plate.
    float zMin = 0.f, zMax = 0.f;
    meshZRange(mesh, zMin, zMax);
    EXPECT_GT(zMax - zMin, 0.f) << "une moulure balayee doit avoir de l'epaisseur en z";
}

// Le nombre de sommets suit le profil : un profil plus riche donne un tube plus
// dense sur le MEME parcours. C'est ce qui verifie que le profil est reellement
// balaye, et pas simplement ignore.
TEST(TEST_cgmesh_architecture_gothic, BuildBayMouldingVertexCountScalesWithProfileSize)
{
    WindowGeometry g = buildTypicalGeom();
    GothicMeshParams params;

    Mesh rect, cavetto;
    ASSERT_NO_THROW(buildBayMoulding(g, params, rectangularProfile(), rect));
    ASSERT_NO_THROW(buildBayMoulding(g, params, cavettoProfile(0.3, 6), cavetto));
    ASSERT_GT(rect.GetNVertices(), 0u);
    ASSERT_GT(cavetto.GetNVertices(), 0u);

    // rectangularProfile = 4 points ; cavettoProfile(_, 6) = 6 + 4 = 10.
    EXPECT_GT(cavetto.GetNVertices(), rect.GetNVertices());
}

// Un remplage riche offre davantage de bords de champ (rosace, sous-arcs, tetes
// foliees) : la moulure doit y etre plus longue, donc plus lourde.
TEST(TEST_cgmesh_architecture_gothic, BuildBayMouldingFollowsMoreBordersOnRicherTracery)
{
    GothicMeshParams params;

    WindowGeometry plain = buildTypicalGeom();
    Mesh plainMesh;
    ASSERT_NO_THROW(buildBayMoulding(plain, params, rectangularProfile(), plainMesh));

    WindowGeometry rich = buildRichGeom();
    Mesh richMesh;
    ASSERT_NO_THROW(buildBayMoulding(rich, params, rectangularProfile(), richMesh));

    ASSERT_GT(plainMesh.GetNVertices(), 0u);
    EXPECT_GT(richMesh.GetNVertices(), plainMesh.GetNVertices());
}

// Un profil de moins de 3 points n'est pas une section fermee. Contrairement a
// sweepProfileAlongArc, qui LEVE std::invalid_argument, buildBayMoulding rejette
// EN SILENCE (`if (profile.size() < 3) return;`) et laisse `out` intact.
//
// Ce test fige le comportement REEL, pas le contrat du header -- lequel annonce
// « Result written into `out` (replaces previous content) » et « `out` is left
// empty » : ni l'un ni l'autre sur ce chemin. Ecart signale, non corrige : changer
// la semantique d'une fonction partagee sortait du perimetre « ecrire les tests
// manquants ».
TEST(TEST_cgmesh_architecture_gothic, BuildBayMouldingSilentlySkipsADegenerateProfile)
{
    WindowGeometry g = buildTypicalGeom();
    GothicMeshParams params;

    std::vector<Vector2d> tooSmall = { Vector2d(0, 0), Vector2d(1, 0) };
    Mesh mesh;
    EXPECT_NO_THROW(buildBayMoulding(g, params, tooSmall, mesh));
    EXPECT_EQ(mesh.GetNVertices(), 0u) << "rien ne doit etre produit depuis un profil degenere";
}

// Le piege que cree ce retour anticipe : sur un Mesh DEJA rempli, un profil
// degenere ne remet pas a zero -- l'appelant croit lire le nouveau resultat et
// relit l'ancien. Verrouille pour que la correction eventuelle du contrat soit un
// changement VOULU et non une surprise.
TEST(TEST_cgmesh_architecture_gothic, BuildBayMouldingLeavesStaleContentOnADegenerateProfile)
{
    WindowGeometry g = buildTypicalGeom();
    GothicMeshParams params;

    Mesh mesh;
    ASSERT_NO_THROW(buildBayMoulding(g, params, rectangularProfile(), mesh));
    const unsigned int valid = mesh.GetNVertices();
    ASSERT_GT(valid, 0u);

    std::vector<Vector2d> tooSmall = { Vector2d(0, 0), Vector2d(1, 0) };
    ASSERT_NO_THROW(buildBayMoulding(g, params, tooSmall, mesh));
    EXPECT_EQ(mesh.GetNVertices(), valid)
        << "comportement actuel : `out` reste INTACT (le header dit « left empty »)";
}

// `out` est remplace, pas complete : un second appel ne doit pas empiler sur le
// resultat du premier.
TEST(TEST_cgmesh_architecture_gothic, BuildBayMouldingReplacesPreviousContent)
{
    WindowGeometry g = buildTypicalGeom();
    GothicMeshParams params;

    Mesh mesh;
    ASSERT_NO_THROW(buildBayMoulding(g, params, rectangularProfile(), mesh));
    const unsigned int first = mesh.GetNVertices();
    ASSERT_GT(first, 0u);

    ASSERT_NO_THROW(buildBayMoulding(g, params, rectangularProfile(), mesh));
    EXPECT_EQ(mesh.GetNVertices(), first) << "le maillage a ete complete au lieu d'etre remplace";
}

// appendMesh est la voie documentee pour poser la moulure sur la plaque extrudee :
// on verifie que le total est bien la somme, indices de faces decales compris.
TEST(TEST_cgmesh_architecture_gothic, MouldingAppendsOntoTheExtrudedPlate)
{
    WindowGeometry g = buildTypicalGeom();
    GothicMeshParams params;

    Polygon2 poly = buildBayStonePolygon(g);
    Mesh plate;
    ASSERT_NO_THROW(extrudeToMesh(poly, plate, 0.0, 10.0));
    const unsigned int nvPlate = plate.GetNVertices();
    const unsigned int nfPlate = plate.GetNFaces();

    Mesh moulding;
    ASSERT_NO_THROW(buildBayMoulding(g, params, rectangularProfile(), moulding));
    const unsigned int nvMould = moulding.GetNVertices();
    const unsigned int nfMould = moulding.GetNFaces();
    ASSERT_GT(nvMould, 0u);

    appendMesh(plate, moulding);
    EXPECT_EQ(plate.GetNVertices(), nvPlate + nvMould);
    EXPECT_EQ(plate.GetNFaces(),    nfPlate + nfMould);

    // Aucun indice de face ne doit sortir du nouveau domaine : c'est la seule
    // maniere d'attraper un decalage oublie lors de la concatenation.
    std::vector<unsigned int> tris = plate.GetTriangles();
    ASSERT_FALSE(tris.empty());
    for (unsigned int idx : tris)
        ASSERT_LT(idx, plate.GetNVertices());
}
