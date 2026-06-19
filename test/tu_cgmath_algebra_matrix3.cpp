#include <gtest/gtest.h>
#include "../cgmath/cgmath.h"

TEST(TEST_cgmath_matrix3, DeterminantAndTranspose) {
    mat3 m;
    mat3_init(m, 1, 2, 3, 4, 5, 6, 7, 8, 10);
    EXPECT_NEAR(mat3_determinant(m), -3.0f, 1e-5f);

    mat3_transpose(m);
    EXPECT_FLOAT_EQ(m[0][1], 4.0f);
    EXPECT_FLOAT_EQ(m[1][0], 2.0f);
}

//
// Characterization of mat3_init_rotation_from_vec3_to_vec3.
// The function builds a rotation R such that R * normalize(dir1) = normalize(dir2)
// (it normalizes dir1/dir2 in place). The non-degenerate branch goes through a
// quaternion; these tests pin its behavior across the quaternion implementation
// change. The transform convention is r[i] = sum_j m[i][j] v[j] (mat3_transform).
//

namespace {

void ExpectRotationMapsDir1ToDir2(float x1, float y1, float z1,
                                  float x2, float y2, float z2)
{
    vec3 d1 = {x1, y1, z1};
    vec3 d2 = {x2, y2, z2};
    mat3 m;
    mat3_init_rotation_from_vec3_to_vec3(m, d1, d2);
    // d1 and d2 are now normalized in place.
    vec3 r;
    mat3_transform(r, m, d1);
    EXPECT_NEAR(r[0], d2[0], 1e-5f);
    EXPECT_NEAR(r[1], d2[1], 1e-5f);
    EXPECT_NEAR(r[2], d2[2], 1e-5f);

    // A rotation matrix is orthonormal with determinant +1.
    EXPECT_NEAR(mat3_determinant(m), 1.0f, 1e-4f);
}

} // namespace

TEST(TEST_cgmath_matrix3, RotationFromVec3ToVec3_AxisAligned) {
    ExpectRotationMapsDir1ToDir2(1.f, 0.f, 0.f, 0.f, 1.f, 0.f);
}

TEST(TEST_cgmath_matrix3, RotationFromVec3ToVec3_General) {
    ExpectRotationMapsDir1ToDir2(1.f, 2.f, 3.f, -2.f, 0.5f, 1.f);
}

TEST(TEST_cgmath_matrix3, RotationFromVec3ToVec3_AnotherGeneral) {
    ExpectRotationMapsDir1ToDir2(0.3f, -1.2f, 0.7f, 1.f, 1.f, 1.f);
}

TEST(TEST_cgmath_matrix3, RotationFromVec3ToVec3_ParallelGivesIdentity) {
    vec3 d1 = {0.f, 0.f, 1.f};
    vec3 d2 = {0.f, 0.f, 2.f}; // same direction
    mat3 m;
    mat3_init_rotation_from_vec3_to_vec3(m, d1, d2);
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            EXPECT_NEAR(m[i][j], (i == j) ? 1.f : 0.f, 1e-5f);
}
