#include <gtest/gtest.h>
#include <math.h>

#include "../src/cgmath/sqrt.h"

// =====================
// sqrt1 - Babylonian + IEEE trick
// =====================

TEST(TEST_cgmath_sqrt, Sqrt1_PerfectSquare)
{
	EXPECT_NEAR(sqrt1(4.f), 2.f, 0.01f);
}

TEST(TEST_cgmath_sqrt, Sqrt1_General)
{
	EXPECT_NEAR(sqrt1(2.f), sqrtf(2.f), 0.01f);
}

TEST(TEST_cgmath_sqrt, Sqrt1_Large)
{
	EXPECT_NEAR(sqrt1(10000.f), 100.f, 0.1f);
}

TEST(TEST_cgmath_sqrt, Sqrt1_Small)
{
	EXPECT_NEAR(sqrt1(0.25f), 0.5f, 0.01f);
}

// =====================
// sqrt2 - Quake 3 fast inverse sqrt
// =====================

TEST(TEST_cgmath_sqrt, Sqrt2_PerfectSquare)
{
	EXPECT_NEAR(sqrt2(4.f), 2.f, 0.05f);
}

TEST(TEST_cgmath_sqrt, Sqrt2_General)
{
	EXPECT_NEAR(sqrt2(2.f), sqrtf(2.f), 0.05f);
}

TEST(TEST_cgmath_sqrt, Sqrt2_One)
{
	EXPECT_NEAR(sqrt2(1.f), 1.f, 0.05f);
}

// =====================
// sqrt3 - Log base 2 approximation
// =====================

TEST(TEST_cgmath_sqrt, Sqrt3_PerfectSquare)
{
	EXPECT_NEAR(sqrt3(4.f), 2.f, 0.5f);
}

TEST(TEST_cgmath_sqrt, Sqrt3_General)
{
	EXPECT_NEAR(sqrt3(100.f), 10.f, 1.f);
}

// =====================
// sqrt4 - Bakhshali Approximation
// =====================

TEST(TEST_cgmath_sqrt, Sqrt4_PerfectSquare)
{
	EXPECT_NEAR(sqrt4(9.f), 3.f, 0.01f);
}

TEST(TEST_cgmath_sqrt, Sqrt4_General)
{
	EXPECT_NEAR(sqrt4(2.f), sqrtf(2.f), 0.01f);
}

TEST(TEST_cgmath_sqrt, Sqrt4_Large)
{
	EXPECT_NEAR(sqrt4(625.f), 25.f, 0.01f);
}

// =====================
// sqrt5 - Babylonian Method
// =====================

TEST(TEST_cgmath_sqrt, Sqrt5_PerfectSquare)
{
	EXPECT_NEAR(sqrt5(16.f), 4.f, 0.001f);
}

TEST(TEST_cgmath_sqrt, Sqrt5_General)
{
	EXPECT_NEAR(sqrt5(2.f), sqrtf(2.f), 0.001f);
}

TEST(TEST_cgmath_sqrt, Sqrt5_Small)
{
	EXPECT_NEAR(sqrt5(0.25f), 0.5f, 0.01f);
}

// =====================
// sqrt6 - IEEE 64-bit trick
// =====================

TEST(TEST_cgmath_sqrt, Sqrt6_PerfectSquare)
{
	if (sizeof(void*) == 8) // only works on 64-bit platforms
		EXPECT_NEAR(sqrt6(4.0), 2.0, 0.01);
}

TEST(TEST_cgmath_sqrt, Sqrt6_General)
{
	if (sizeof(void*) == 8) // only works on 64-bit platforms
		EXPECT_NEAR(sqrt6(2.0), sqrt(2.0), 0.01);
}

// =====================
// sqrt7 - IEEE 32-bit bit manipulation
// =====================

TEST(TEST_cgmath_sqrt, Sqrt7_PerfectSquare)
{
	EXPECT_NEAR(sqrt7(4.f), 2.f, 0.5f);
}

TEST(TEST_cgmath_sqrt, Sqrt7_General)
{
	EXPECT_NEAR(sqrt7(100.f), 10.f, 2.f);
}

// =====================
// sqrt9 - Babylonian (double)
// =====================

TEST(TEST_cgmath_sqrt, Sqrt9_PerfectSquare)
{
	EXPECT_NEAR(sqrt9(25.0), 5.0, 1e-10);
}

TEST(TEST_cgmath_sqrt, Sqrt9_General)
{
	EXPECT_NEAR(sqrt9(2.0), sqrt(2.0), 1e-10);
}

TEST(TEST_cgmath_sqrt, Sqrt9_Large)
{
	EXPECT_NEAR(sqrt9(1000000.0), 1000.0, 1e-8);
}

// =====================
// sqrt10 - Babylonian with tolerance
// =====================

TEST(TEST_cgmath_sqrt, Sqrt10_PerfectSquare)
{
	EXPECT_NEAR(sqrt10(49.0), 7.0, 1e-6);
}

TEST(TEST_cgmath_sqrt, Sqrt10_General)
{
	EXPECT_NEAR(sqrt10(2.0), sqrt(2.0), 1e-6);
}

// =====================
// sqrt11 - Newton's bisection
// =====================

TEST(TEST_cgmath_sqrt, Sqrt11_PerfectSquare)
{
	EXPECT_NEAR(sqrt11(36.0), 6.0, 0.001);
}

TEST(TEST_cgmath_sqrt, Sqrt11_General)
{
	EXPECT_NEAR(sqrt11(2.0), sqrt(2.0), 0.001);
}

TEST(TEST_cgmath_sqrt, Sqrt11_LessThanOne)
{
	EXPECT_NEAR(sqrt11(0.25), 0.5, 0.001);
}

// =====================
// sqrt12 - Newton's Approximation Method
// =====================

TEST(TEST_cgmath_sqrt, Sqrt12_NonPerfect)
{
	// returns floor of sqrt for non-perfect squares
	double val = sqrt12(26.0);
	EXPECT_NEAR(val, sqrt(26.0), 0.001);
}

TEST(TEST_cgmath_sqrt, Sqrt12_Zero)
{
	EXPECT_DOUBLE_EQ(sqrt12(0), 0.0);
}

TEST(TEST_cgmath_sqrt, Sqrt12_One)
{
	EXPECT_DOUBLE_EQ(sqrt12(1), 1.0);
}

// =====================
// sqrt13 - Babylonian (int overload)
// =====================

TEST(TEST_cgmath_sqrt, Sqrt13_PerfectSquare)
{
	EXPECT_NEAR(sqrt13(16), 4.0, 0.001);
}

TEST(TEST_cgmath_sqrt, Sqrt13_General)
{
	EXPECT_NEAR(sqrt13(2), sqrt(2.0), 0.01);
}

TEST(TEST_cgmath_sqrt, Sqrt13_Large)
{
	EXPECT_NEAR(sqrt13(100), 10.0, 0.001);
}
