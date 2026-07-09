#include <cmath>
#include <gtest/gtest.h>

#include "../src/cgmath/TMatrix2.h"

//
// Real tests for the 2x2 matrix operations the cgmesh code relies on:
//   - symmetric eigensystem (curvature tensors, polygon PCA)
//   - 2D rotation (surface_architecture)
// These replace a former no-assertion "ghost" test and pin the behavior across
// the mat2 -> TMatrix2 migration. TMatrix2::SolveEigensystem is the exact
// equivalent of the removed mat2_solve_eigensystem.
//

TEST(TEST_cgmath_TMatrix2, SolveEigensystemDiagonal) {
    Matrix2f m(2.f, 0.f, 0.f, 3.f);
    Vector2f ev1, ev2, evals;
    ASSERT_TRUE(m.SolveEigensystem(ev1, ev2, evals));
    EXPECT_FLOAT_EQ(evals[0], 2.f);
    EXPECT_FLOAT_EQ(evals[1], 3.f);
    EXPECT_FLOAT_EQ(ev1[0], 1.f);
    EXPECT_FLOAT_EQ(ev1[1], 0.f);
    EXPECT_FLOAT_EQ(ev2[0], 0.f);
    EXPECT_FLOAT_EQ(ev2[1], 1.f);
}

TEST(TEST_cgmath_TMatrix2, SolveEigensystemSymmetric) {
    // [[2,1],[1,2]] -> eigenvalues 3 and 1, eigenvectors (1,1) and (1,-1).
    Matrix2f m(2.f, 1.f, 1.f, 2.f);
    Vector2f ev1, ev2, evals;
    ASSERT_TRUE(m.SolveEigensystem(ev1, ev2, evals));
    EXPECT_NEAR(evals[0], 3.f, 1e-5f);
    EXPECT_NEAR(evals[1], 1.f, 1e-5f);
    const float s = 0.70710678f;
    EXPECT_NEAR(ev1[0], s, 1e-5f);
    EXPECT_NEAR(ev1[1], s, 1e-5f);
    EXPECT_NEAR(ev2[0], s, 1e-5f);
    EXPECT_NEAR(ev2[1], -s, 1e-5f);
}

TEST(TEST_cgmath_TMatrix2, RotationTransform) {
    // +90 deg rotation maps (1,0) to (0,1) (same semantics as mat2_transform).
    const float th = 1.57079632679f;
    Matrix2f rot(cosf(th), -sinf(th), sinf(th), cosf(th));
    Vector2f r = rot * Vector2f(1.f, 0.f);
    EXPECT_NEAR(r[0], 0.f, 1e-6f);
    EXPECT_NEAR(r[1], 1.f, 1e-6f);
}
