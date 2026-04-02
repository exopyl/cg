#include <gtest/gtest.h>

#include "../src/cgmath/line.h"

TEST(TEST_cgmath_line, DefaultConstructorCreatesZAxisLine)
{
    Line line;
    Vector3f point;
    Vector3f direction;

    line.get_point(point);
    line.get_direction(direction);

    EXPECT_FLOAT_EQ(point.x, 0.f);
    EXPECT_FLOAT_EQ(point.y, 0.f);
    EXPECT_FLOAT_EQ(point.z, 0.f);
    EXPECT_FLOAT_EQ(direction.x, 0.f);
    EXPECT_FLOAT_EQ(direction.y, 0.f);
    EXPECT_FLOAT_EQ(direction.z, 1.f);
}

TEST(TEST_cgmath_line, InitPointPointUsesDeltaAsDirection)
{
    Line line;
    Vector3f point;
    Vector3f direction;

    line.init_point_point(1.0, 2.0, 3.0, 4.0, 6.0, 8.0);
    line.get_point(point);
    line.get_direction(direction);

    EXPECT_FLOAT_EQ(point.x, 1.f);
    EXPECT_FLOAT_EQ(point.y, 2.f);
    EXPECT_FLOAT_EQ(point.z, 3.f);
    EXPECT_FLOAT_EQ(direction.x, 3.f);
    EXPECT_FLOAT_EQ(direction.y, 4.f);
    EXPECT_FLOAT_EQ(direction.z, 5.f);
}

TEST(TEST_cgmath_line, ClosestPointProjectsPointOntoLine)
{
    Line line;
    Vector3f point(3.f, 4.f, 0.f);
    Vector3f projected(-1.f, -1.f, -1.f);

    line.init_point_direction(0.0, 0.0, 0.0, 1.0, 0.0, 0.0);
    line.closest_point(point, projected);

    EXPECT_FLOAT_EQ(projected.x, 3.f);
    EXPECT_FLOAT_EQ(projected.y, 0.f);
    EXPECT_FLOAT_EQ(projected.z, 0.f);
}

TEST(TEST_cgmath_line, DistanceBetweenIntersectingLinesIsZero)
{
    Line xAxis;
    Line yAxis;

    xAxis.init_point_direction(0.0, 0.0, 0.0, 1.0, 0.0, 0.0);
    yAxis.init_point_direction(0.0, 0.0, 0.0, 0.0, 1.0, 0.0);

    EXPECT_NEAR(xAxis.distance_with(yAxis), 0.0, 1e-6);
}

TEST(TEST_cgmath_line, ConvertToPlueckerAndBackPreservesLineThroughOrigin)
{
    Line line;
    Vector3f point;
    Vector3f direction;

    line.init_point_direction(0.0, 0.0, 0.0, 1.0, 0.0, 0.0);
    ASSERT_EQ(line.convert(LINE_PLUECKER), 1);
    ASSERT_EQ(line.convert(LINE_POINT_DIRECTION), 1);

    line.get_point(point);
    line.get_direction(direction);

    EXPECT_FLOAT_EQ(point.x, 0.f);
    EXPECT_FLOAT_EQ(point.y, 0.f);
    EXPECT_FLOAT_EQ(point.z, 0.f);
    EXPECT_NEAR(direction.x, 1.f, 1e-6);
    EXPECT_NEAR(direction.y, 0.f, 1e-6);
    EXPECT_NEAR(direction.z, 0.f, 1e-6);
}
