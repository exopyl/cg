#include <gtest/gtest.h>

#include "../src/cgmesh/import_svg.h"
#include "../src/cgmesh/mesh.h"

#include <cmath>
#include <memory>

namespace {

std::unique_ptr<Mesh> importExtruded(const char* path, float height)
{
    SvgExtrudeOptions opt;
    opt.height       = height;
    opt.flattenTol   = 0.5f;
    opt.centerAndFit = true;
    opt.invertY      = true;
    return std::unique_ptr<Mesh>(import_svg_extruded(path, opt));
}

} // namespace

TEST(TEST_cgmesh_svg, square_produces_extruded_solid)
{
    auto m = importExtruded("./test/data/svg/square.svg", 0.5f);
    ASSERT_NE(m, nullptr);

    // A rect path becomes 4 corner samples (rectangles are degenerate cubics
    // but nanosvg still emits four bezier endpoints). We expect:
    //   - bottom + top caps: 2 * 2 = 4 triangles (rectangle = 2 tri each)
    //   - 4 side quads = 8 triangles
    //   - total: 12 triangles
    //   - vertices: 4 bottom + 4 top = 8
    EXPECT_EQ(m->GetNVertices(), 8u);
    EXPECT_EQ(m->GetNFaces(),    12u);

    EXPECT_TRUE(m->IsTriangleMesh());
}

TEST(TEST_cgmesh_svg, square_bbox_height_matches_param)
{
    auto m = importExtruded("./test/data/svg/square.svg", 0.5f);
    ASSERT_NE(m, nullptr);

    m->computebbox();
    float vmin[3], vmax[3];
    m->bbox().GetMinMax(vmin, vmax);
    EXPECT_NEAR(vmax[2] - vmin[2], 0.5f, 1e-4f) << "extrusion height in Z";
}

TEST(TEST_cgmesh_svg, square_bbox_height_scales_with_param)
{
    auto m1 = importExtruded("./test/data/svg/square.svg", 0.1f);
    auto m2 = importExtruded("./test/data/svg/square.svg", 2.0f);
    ASSERT_NE(m1, nullptr);
    ASSERT_NE(m2, nullptr);

    m1->computebbox();
    m2->computebbox();
    float a[3], b[3], c[3], d[3];
    m1->bbox().GetMinMax(a, b);
    m2->bbox().GetMinMax(c, d);
    EXPECT_NEAR(b[2] - a[2], 0.1f, 1e-4f);
    EXPECT_NEAR(d[2] - c[2], 2.0f, 1e-4f);
}

TEST(TEST_cgmesh_svg, triangle_produces_extruded_solid)
{
    auto m = importExtruded("./test/data/svg/triangle.svg", 1.0f);
    ASSERT_NE(m, nullptr);

    // Triangle: 3 corners (sometimes 3 after redundancy trim).
    //   - caps: 2 * 1 = 2 triangles
    //   - 3 side quads = 6 triangles
    //   - total: 8 triangles
    //   - vertices: 3 + 3 = 6
    EXPECT_EQ(m->GetNVertices(), 6u);
    EXPECT_EQ(m->GetNFaces(),    8u);
}

TEST(TEST_cgmesh_svg, nonexistent_file_returns_null)
{
    auto m = importExtruded("./test/data/svg/does_not_exist.svg", 0.5f);
    EXPECT_EQ(m, nullptr);
}
