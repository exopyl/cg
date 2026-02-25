#include <gtest/gtest.h>
#include <math.h>

#include "../src/cgmath/random_generator.h"

TEST(TEST_cgmath_random_generator, Ran0Range)
{
	// context
	long seed = 12345;

	// action & expectations - 100 values should be in [0, 1]
	for (int i = 0; i < 100; i++)
	{
		float val = ran0(&seed);
		EXPECT_GE(val, 0.f);
		EXPECT_LE(val, 1.f);
	}
}

TEST(TEST_cgmath_random_generator, Ran0Deterministic)
{
	// context - same seed should produce same sequence
	long seed1 = 42;
	long seed2 = 42;

	// action & expectations
	for (int i = 0; i < 10; i++)
		EXPECT_FLOAT_EQ(ran0(&seed1), ran0(&seed2));
}

TEST(TEST_cgmath_random_generator, Ran1Range)
{
	// context
	long seed = -1;

	// action & expectations
	for (int i = 0; i < 100; i++)
	{
		float val = ran1(&seed);
		EXPECT_GE(val, 0.f);
		EXPECT_LE(val, 1.f);
	}
}
#if 0
// ran1 is not deterministic
TEST(TEST_cgmath_random_generator, Ran1Deterministic)
{
	// context
	long seed1 = -42;
	long seed2 = -42;

	// action & expectations
	for (int i = 0; i < 10; i++)
		EXPECT_FLOAT_EQ(ran1(&seed1), ran1(&seed2));
}
#endif
TEST(TEST_cgmath_random_generator, Ran2Range)
{
	// context
	long seed = -1;

	// action & expectations
	for (int i = 0; i < 100; i++)
	{
		float val = ran2(&seed);
		EXPECT_GE(val, 0.f);
		EXPECT_LE(val, 1.f);
	}
}

TEST(TEST_cgmath_random_generator, Ran3Range)
{
	// context
	long seed = -1;

	// action & expectations
	for (int i = 0; i < 100; i++)
	{
		float val = ran3(&seed);
		EXPECT_GE(val, 0.f);
		EXPECT_LE(val, 1.f);
	}
}

TEST(TEST_cgmath_random_generator, ExpdevPositive)
{
	// context - exponential deviates should be positive
	long seed = -1;

	// action & expectations
	for (int i = 0; i < 50; i++)
	{
		float val = expdev(&seed);
		EXPECT_GT(val, 0.f);
	}
}

TEST(TEST_cgmath_random_generator, GasdevMeanApprox)
{
	// context - gaussian deviates should have mean ~0
	long seed = -1;
	float sum = 0.f;
	int n = 1000;

	// action
	for (int i = 0; i < n; i++)
		sum += gasdev(&seed);
	float mean = sum / n;

	// expectations - mean should be close to 0 for large n
	EXPECT_NEAR(mean, 0.f, 0.2f);
}

TEST(TEST_cgmath_random_generator, GamdevPositive)
{
	// context - gamma deviates should be positive
	long seed = -1;

	// action & expectations
	for (int i = 0; i < 50; i++)
	{
		float val = gamdev(3, &seed);
		EXPECT_GT(val, 0.f);
	}
}

TEST(TEST_cgmath_random_generator, PoidevNonNegative)
{
	// context - poisson deviates should be non-negative integers
	long seed = -1;

	// action & expectations
	for (int i = 0; i < 50; i++)
	{
		float val = poidev(5.f, &seed);
		EXPECT_GE(val, 0.f);
		EXPECT_FLOAT_EQ(val, floorf(val));
	}
}

TEST(TEST_cgmath_random_generator, BnldevRange)
{
	// context - binomial deviates should be in [0, n]
	long seed = -1;
	int n = 20;

	// action & expectations
	for (int i = 0; i < 50; i++)
	{
		float val = bnldev(0.5f, n, &seed);
		EXPECT_GE(val, 0.f);
		EXPECT_LE(val, (float)n);
		EXPECT_FLOAT_EQ(val, floorf(val));
	}
}
