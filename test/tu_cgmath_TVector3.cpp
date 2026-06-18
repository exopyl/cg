#include <gtest/gtest.h>
#include <cmath>
#include <type_traits>

#include "../src/cgmath/TVector3.h"
// Legacy C API used as the reference oracle for the drop-in replacement.
#include "../src/cgmath/algebra_vector3.h"

// Migration prerequisite (Phase 1): TVector3 must stay trivially copyable so that
// arrays of Vector3 can be malloc/realloc/memcpy'd safely during the vec3 ->
// TVector3 migration. Re-introducing a user-declared copy/move/dtor would break
// this and fail the build here.
static_assert(std::is_trivially_copyable<Vector3>::value,
              "TVector3<float> must remain trivially copyable");
static_assert(std::is_trivially_copyable<Vector3d>::value,
              "TVector3<double> must remain trivially copyable");

//
// evaluate_triangle_normal returns the UNNORMALIZED normal of triangle (v1,v2,v3):
//   n = (v2 - v1) x (v3 - v1)
// evaluate_triangle_area = 0.5 * |n|. Both mirror vec3_triangle_normal /
// vec3_triangle_area (intermediate accumulation in double, then cast to float).
//

TEST(TEST_cgmath_TVector3, TriangleNormalCanonicalXY)
{
    // Unit right triangle in the XY plane, CCW -> +Z by the right-hand rule.
    Vector3 v1(0, 0, 0), v2(1, 0, 0), v3(0, 1, 0);
    Vector3 n = Vector3::evaluate_triangle_normal(v1, v2, v3);
    EXPECT_FLOAT_EQ(n.x, 0.0f);
    EXPECT_FLOAT_EQ(n.y, 0.0f);
    EXPECT_FLOAT_EQ(n.z, 1.0f);
}

TEST(TEST_cgmath_TVector3, TriangleNormalIsUnnormalized)
{
    // Legs of length 2 and 3 -> |n| must be 6 (= 2*area), NOT 1.
    Vector3 v1(0, 0, 0), v2(2, 0, 0), v3(0, 3, 0);
    Vector3 n = Vector3::evaluate_triangle_normal(v1, v2, v3);
    EXPECT_FLOAT_EQ(n.x, 0.0f);
    EXPECT_FLOAT_EQ(n.y, 0.0f);
    EXPECT_FLOAT_EQ(n.z, 6.0f);
    EXPECT_FLOAT_EQ(n.getLength(), 6.0f);
}

TEST(TEST_cgmath_TVector3, TriangleNormalGeneral3D)
{
    // u=(1,0,0), v=(0,0,1) -> u x v = (0,-1,0)
    Vector3 v1(0, 0, 0), v2(1, 0, 0), v3(0, 0, 1);
    Vector3 n = Vector3::evaluate_triangle_normal(v1, v2, v3);
    EXPECT_FLOAT_EQ(n.x, 0.0f);
    EXPECT_FLOAT_EQ(n.y, -1.0f);
    EXPECT_FLOAT_EQ(n.z, 0.0f);
}

TEST(TEST_cgmath_TVector3, TriangleNormalWindingFlipsSign)
{
    // Swapping v2 and v3 reverses the orientation -> opposite normal.
    Vector3 v1(0.5f, -1.0f, 2.0f), v2(3.0f, 0.0f, 1.0f), v3(-1.0f, 4.0f, 0.0f);
    Vector3 n  = Vector3::evaluate_triangle_normal(v1, v2, v3);
    Vector3 nr = Vector3::evaluate_triangle_normal(v1, v3, v2);
    EXPECT_FLOAT_EQ(nr.x, -n.x);
    EXPECT_FLOAT_EQ(nr.y, -n.y);
    EXPECT_FLOAT_EQ(nr.z, -n.z);
}

TEST(TEST_cgmath_TVector3, TriangleNormalDegenerateIsZero)
{
    // Collinear vertices -> zero normal.
    Vector3 v1(0, 0, 0), v2(1, 1, 1), v3(2, 2, 2);
    Vector3 n = Vector3::evaluate_triangle_normal(v1, v2, v3);
    EXPECT_FLOAT_EQ(n.x, 0.0f);
    EXPECT_FLOAT_EQ(n.y, 0.0f);
    EXPECT_FLOAT_EQ(n.z, 0.0f);
}

TEST(TEST_cgmath_TVector3, TriangleAreaRightTriangle)
{
    // legs 3 and 4 -> area = 0.5 * 3 * 4 = 6
    Vector3 v1(0, 0, 0), v2(3, 0, 0), v3(0, 4, 0);
    EXPECT_FLOAT_EQ(Vector3::evaluate_triangle_area(v1, v2, v3), 6.0f);
}

TEST(TEST_cgmath_TVector3, TriangleAreaTranslationInvariant)
{
    // Same triangle as above, translated by (10,-5,3): area must be unchanged.
    Vector3 w1(10.0f, -5.0f, 3.0f), w2(13.0f, -5.0f, 3.0f), w3(10.0f, -1.0f, 3.0f);
    EXPECT_FLOAT_EQ(Vector3::evaluate_triangle_area(w1, w2, w3), 6.0f);
}

TEST(TEST_cgmath_TVector3, TriangleAreaDegenerateIsZero)
{
    Vector3 v1(0, 0, 0), v2(1, 1, 1), v3(2, 2, 2);
    EXPECT_FLOAT_EQ(Vector3::evaluate_triangle_area(v1, v2, v3), 0.0f);
}

TEST(TEST_cgmath_TVector3, AreaEqualsHalfNormalLength)
{
    // Consistency between the two helpers on an arbitrary 3D triangle.
    Vector3 v1(1, 2, 3), v2(4, 0, -1), v3(-2, 5, 2);
    Vector3 n = Vector3::evaluate_triangle_normal(v1, v2, v3);
    EXPECT_NEAR(Vector3::evaluate_triangle_area(v1, v2, v3),
                0.5f * n.getLength(), 1e-4f);
}

TEST(TEST_cgmath_TVector3, MatchesLegacyVec3Reference)
{
    // Drop-in equivalence: the new helpers must reproduce vec3_triangle_normal /
    // vec3_triangle_area, the C functions they are meant to replace.
    const float tris[][9] = {
        { 0, 0, 0,  1, 0, 0,  0, 1, 0 },               // canonical XY
        { 1, 2, 3,  4, 0, -1, -2, 5, 2 },              // arbitrary 3D
        { -3.5f, 1.0f, 2.0f, 0.0f, -2.0f, 4.0f, 5.0f, 5.0f, -1.0f },
        { 0, 0, 0,  2, 2, 2,  -1, -1, -1 },            // degenerate (collinear)
    };

    for (const auto& t : tris)
    {
        Vector3 a(t[0], t[1], t[2]);
        Vector3 b(t[3], t[4], t[5]);
        Vector3 c(t[6], t[7], t[8]);

        vec3 va = { t[0], t[1], t[2] };
        vec3 vb = { t[3], t[4], t[5] };
        vec3 vc = { t[6], t[7], t[8] };

        vec3 nref;
        vec3_triangle_normal(nref, va, vb, vc);

        Vector3 n = Vector3::evaluate_triangle_normal(a, b, c);
        EXPECT_FLOAT_EQ(n.x, nref[0]);
        EXPECT_FLOAT_EQ(n.y, nref[1]);
        EXPECT_FLOAT_EQ(n.z, nref[2]);

        EXPECT_FLOAT_EQ(Vector3::evaluate_triangle_area(a, b, c),
                        vec3_triangle_area(va, vb, vc));
    }
}
