#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "../src/cgmesh/bundle.h"

//
// Characterization tests for Bundle::Load and Bundle::Load2.
//
// These pin the externally observable results of the matrix maths used by the
// bundle loader, so the mat4 -> TMatrix4 migration can be validated:
//   - Load  : reads a rotation R and stores its inverse in place (mat4_get_inverse).
//   - Load2 : computes Rinv = R^-1 (mat4_get_inverse) and the camera direction
//             d = Rinv * (0,0,-1) and position pos = Rinv * (-T) (mat4_transform).
//
// The test rotation is a +90 deg rotation about the Y axis, stored row-major:
//     R = ( 0 0 1 )      its inverse / transpose is   Rinv = ( 0 0 -1 )
//         ( 0 1 0 )                                          ( 0 1  0 )
//         (-1 0 0 )                                          ( 1 0  0 )
//

namespace {

std::string WriteTempFile(const std::string& name, const std::string& content)
{
    std::filesystem::path p = std::filesystem::temp_directory_path() / name;
    std::ofstream ofs(p, std::ios::binary | std::ios::trunc);
    ofs << content;
    ofs.close();
    return p.string();
}

void ExpectVec3Near(const float* v, float x, float y, float z, float eps = 1e-5f)
{
    EXPECT_NEAR(v[0], x, eps);
    EXPECT_NEAR(v[1], y, eps);
    EXPECT_NEAR(v[2], z, eps);
}

} // namespace

TEST(TEST_bundle, load_reads_camera_and_inverts_rotation_in_place)
{
    // bundle.out : 1 camera, 2 points. Camera rotation = Ry(+90).
    const std::string content =
        "# Bundle file v0.3\n"
        "1 2\n"
        "100 0.01 0.02\n"   // f_pxl k1 k2
        "0 0 1\n"           // R row 0
        "0 1 0\n"           // R row 1
        "-1 0 0\n"          // R row 2
        "1 2 3\n"           // T
        "0.5 0.6 0.7\n"     // point 0 position
        "10 20 30\n"        // point 0 color
        "0\n"               // point 0 view list (ignored)
        "1.5 1.6 1.7\n"     // point 1 position
        "40 50 60\n"        // point 1 color
        "0\n";              // point 1 view list (ignored)

    std::string path = WriteTempFile("tu_bundle_load.out", content);

    Bundle bundle;
    ASSERT_EQ(bundle.Load(path.data()), 0);

    ASSERT_EQ(bundle.cameras_n, 1u);
    BundleCamera* cam = bundle.cameras[0];
    ASSERT_NE(cam, nullptr);

    EXPECT_FLOAT_EQ(cam->f_pxl, 100.f);
    EXPECT_FLOAT_EQ(cam->k1, 0.01f);
    EXPECT_FLOAT_EQ(cam->k2, 0.02f);

    // Load stores the inverse of the input rotation directly into R.
    // inverse of Ry(+90) = transpose = ( 0 0 -1 / 0 1 0 / 1 0 0 ).
    EXPECT_NEAR(cam->R[0], 0.f, 1e-5f);
    EXPECT_NEAR(cam->R[1], 0.f, 1e-5f);
    EXPECT_NEAR(cam->R[2], -1.f, 1e-5f);
    EXPECT_NEAR(cam->R[4], 0.f, 1e-5f);
    EXPECT_NEAR(cam->R[5], 1.f, 1e-5f);
    EXPECT_NEAR(cam->R[6], 0.f, 1e-5f);
    EXPECT_NEAR(cam->R[8], 1.f, 1e-5f);
    EXPECT_NEAR(cam->R[9], 0.f, 1e-5f);
    EXPECT_NEAR(cam->R[10], 0.f, 1e-5f);

    ExpectVec3Near(cam->T, 1.f, 2.f, 3.f);

    // Points were loaded into the mesh.
    ASSERT_NE(bundle.mesh, nullptr);
    ASSERT_EQ(bundle.mesh->m_nVertices, 2u);
    EXPECT_NEAR(bundle.mesh->m_pVertices[0], 0.5f, 1e-5f);
    EXPECT_NEAR(bundle.mesh->m_pVertices[1], 0.6f, 1e-5f);
    EXPECT_NEAR(bundle.mesh->m_pVertices[2], 0.7f, 1e-5f);
    EXPECT_NEAR(bundle.mesh->m_pVertices[3], 1.5f, 1e-5f);
    EXPECT_NEAR(bundle.mesh->m_pVertexColors[0], 10.f / 255.f, 1e-5f);
}

TEST(TEST_bundle, load2_computes_inverse_direction_and_position)
{
    // bundle.out : 1 camera, 0 points (skip the point-parsing path).
    const std::string bundleContent =
        "# Bundle file v0.3\n"
        "1 0\n"
        "100 0.01 0.02\n"   // f_pxl k1 k2
        "0 0 1\n"           // R row 0   (Ry(+90))
        "0 1 0\n"           // R row 1
        "-1 0 0\n"          // R row 2
        "1 2 3\n";          // T

    const std::string listContent = "img1.jpg\n";

    std::string bundlePath = WriteTempFile("tu_bundle_load2.out", bundleContent);
    std::string listPath = WriteTempFile("tu_bundle_load2.list", listContent);

    Bundle bundle;
    ASSERT_EQ(bundle.Load2(bundlePath.data(), listPath.data(), nullptr), 0);

    ASSERT_EQ(bundle.cameras_n, 1u);
    BundleCamera* cam = bundle.cameras[0];
    ASSERT_NE(cam, nullptr);

    // Load2 keeps R as read and computes Rinv = R^-1 separately.
    EXPECT_NEAR(cam->R[0], 0.f, 1e-5f);
    EXPECT_NEAR(cam->R[2], 1.f, 1e-5f);
    EXPECT_NEAR(cam->R[8], -1.f, 1e-5f);

    // Rinv = transpose of Ry(+90) = ( 0 0 -1 / 0 1 0 / 1 0 0 ).
    EXPECT_NEAR(cam->Rinv[0], 0.f, 1e-5f);
    EXPECT_NEAR(cam->Rinv[2], -1.f, 1e-5f);
    EXPECT_NEAR(cam->Rinv[5], 1.f, 1e-5f);
    EXPECT_NEAR(cam->Rinv[8], 1.f, 1e-5f);

    // d = Rinv * (0,0,-1) = (1,0,0).
    ExpectVec3Near(cam->d, 1.f, 0.f, 0.f);

    // pos = Rinv * (-T) with T=(1,2,3) -> (3,-2,-1).
    ExpectVec3Near(cam->pos, 3.f, -2.f, -1.f);

    ExpectVec3Near(cam->T, 1.f, 2.f, 3.f);
}

TEST(TEST_bundle, load2_skips_camera_with_zero_translation)
{
    const std::string bundleContent =
        "# Bundle file v0.3\n"
        "1 0\n"
        "100 0.01 0.02\n"
        "1 0 0\n"
        "0 1 0\n"
        "0 0 1\n"
        "0 0 0\n";          // T == 0 -> camera considered invalid and skipped

    const std::string listContent = "img1.jpg\n";

    std::string bundlePath = WriteTempFile("tu_bundle_load2_skip.out", bundleContent);
    std::string listPath = WriteTempFile("tu_bundle_load2_skip.list", listContent);

    Bundle bundle;
    ASSERT_EQ(bundle.Load2(bundlePath.data(), listPath.data(), nullptr), 0);

    EXPECT_EQ(bundle.cameras_n, 0u);
}
