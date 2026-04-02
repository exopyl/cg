#include <gtest/gtest.h>

#include "../src/cgmath/geometry.h"

TEST(TEST_cgmath_sphere, DefaultConstructorInitializesBoundingBox)
{
    Sphere sphere;

    ASSERT_NE(sphere.m_pAABox, nullptr);
    EXPECT_FLOAT_EQ(sphere.m_pAABox->parameters[0].x, -1.f);
    EXPECT_FLOAT_EQ(sphere.m_pAABox->parameters[0].y, -1.f);
    EXPECT_FLOAT_EQ(sphere.m_pAABox->parameters[0].z, -1.f);
    EXPECT_FLOAT_EQ(sphere.m_pAABox->parameters[1].x, 1.f);
    EXPECT_FLOAT_EQ(sphere.m_pAABox->parameters[1].y, 1.f);
    EXPECT_FLOAT_EQ(sphere.m_pAABox->parameters[1].z, 1.f);
}

TEST(TEST_cgmath_sphere, DefaultBoundingBoxRejectsFarRay)
{
    Sphere sphere;
    vec3 origin = {5.f, 5.f, 5.f};
    vec3 direction = {1.f, 0.f, 0.f};

    EXPECT_FALSE(sphere.GetIntersectionBboxWithRay(origin, direction));
}

TEST(TEST_cgmath_sphere, SetCenterUpdatesBoundingBox)
{
    Sphere sphere;

    sphere.SetCenter(2.f, 3.f, 4.f);

    ASSERT_NE(sphere.m_pAABox, nullptr);
    EXPECT_FLOAT_EQ(sphere.m_pAABox->parameters[0].x, 1.f);
    EXPECT_FLOAT_EQ(sphere.m_pAABox->parameters[0].y, 2.f);
    EXPECT_FLOAT_EQ(sphere.m_pAABox->parameters[0].z, 3.f);
    EXPECT_FLOAT_EQ(sphere.m_pAABox->parameters[1].x, 3.f);
    EXPECT_FLOAT_EQ(sphere.m_pAABox->parameters[1].y, 4.f);
    EXPECT_FLOAT_EQ(sphere.m_pAABox->parameters[1].z, 5.f);
}

TEST(TEST_cgmath_sphere, SetRadiusUpdatesBoundingBox)
{
    Sphere sphere;
    sphere.SetCenter(2.f, 3.f, 4.f);

    sphere.SetRadius(2.5f);

    ASSERT_NE(sphere.m_pAABox, nullptr);
    EXPECT_FLOAT_EQ(sphere.m_pAABox->parameters[0].x, -0.5f);
    EXPECT_FLOAT_EQ(sphere.m_pAABox->parameters[0].y, 0.5f);
    EXPECT_FLOAT_EQ(sphere.m_pAABox->parameters[0].z, 1.5f);
    EXPECT_FLOAT_EQ(sphere.m_pAABox->parameters[1].x, 4.5f);
    EXPECT_FLOAT_EQ(sphere.m_pAABox->parameters[1].y, 5.5f);
    EXPECT_FLOAT_EQ(sphere.m_pAABox->parameters[1].z, 6.5f);
}
