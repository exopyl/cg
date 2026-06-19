#include <gtest/gtest.h>

#include "../src/cgmesh/bundle_camera.h"

//
// Characterization tests for BundleCamera.
//
// The constructor initializes R and Rinv to identity (mat4_set_identity) and
// T/d/pos to the zero vector. These tests pin that contract so it survives the
// mat4 -> TMatrix4 migration.
//

namespace {

// Identity matrix stored row-major in a float[16]: 1 on the diagonal, 0 elsewhere.
void ExpectIdentity4(const float* m)
{
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 4; ++col)
        {
            float expected = (row == col) ? 1.f : 0.f;
            EXPECT_FLOAT_EQ(m[4 * row + col], expected)
                << "at (" << row << "," << col << ")";
        }
}

} // namespace

TEST(TEST_bundle_camera, default_constructor_sets_identity_rotations)
{
    BundleCamera cam;

    ExpectIdentity4(cam.R);
    ExpectIdentity4(cam.Rinv);
}

TEST(TEST_bundle_camera, default_constructor_zeroes_vectors_and_scalars)
{
    BundleCamera cam;

    EXPECT_FLOAT_EQ(cam.T[0], 0.f);
    EXPECT_FLOAT_EQ(cam.T[1], 0.f);
    EXPECT_FLOAT_EQ(cam.T[2], 0.f);

    EXPECT_FLOAT_EQ(cam.d[0], 0.f);
    EXPECT_FLOAT_EQ(cam.d[1], 0.f);
    EXPECT_FLOAT_EQ(cam.d[2], 0.f);

    EXPECT_FLOAT_EQ(cam.pos[0], 0.f);
    EXPECT_FLOAT_EQ(cam.pos[1], 0.f);
    EXPECT_FLOAT_EQ(cam.pos[2], 0.f);

    EXPECT_FLOAT_EQ(cam.f_pxl, 0.f);
    EXPECT_FLOAT_EQ(cam.f_mm, 0.f);
    EXPECT_FLOAT_EQ(cam.CCDWidth_mm, 0.f);
    EXPECT_FLOAT_EQ(cam.CCDHeight_mm, 0.f);
    EXPECT_FLOAT_EQ(cam.k1, 0.f);
    EXPECT_FLOAT_EQ(cam.k2, 0.f);

    EXPECT_EQ(cam.filename, nullptr);
    EXPECT_EQ(cam.w, 0u);
    EXPECT_EQ(cam.h, 0u);
}

TEST(TEST_bundle_camera, dump_does_not_crash)
{
    BundleCamera cam;
    // Smoke test: Dump dereferences R/Rinv/T/d; just make sure it runs.
    cam.Dump();
    SUCCEED();
}
