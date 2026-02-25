#include <gtest/gtest.h>
#include <cmath>

#include "../src/cgmath/TCamera.h"

static constexpr float PI = 3.14159265358979323846f;
static constexpr float EPSILON = 1e-5f;

//-----------------------------------------------------------------------------
// Constructeur par defaut
//-----------------------------------------------------------------------------

TEST(TEST_cgmath_tcamera, default_constructor_position)
{
	Cameraf cam;
	const auto& pos = cam.GetPosition();
	EXPECT_FLOAT_EQ(pos.x, 0.f);
	EXPECT_FLOAT_EQ(pos.y, 0.f);
	EXPECT_FLOAT_EQ(pos.z, 3.f);
}

TEST(TEST_cgmath_tcamera, default_constructor_target)
{
	Cameraf cam;
	const auto& target = cam.GetTarget();
	EXPECT_FLOAT_EQ(target.x, 0.f);
	EXPECT_FLOAT_EQ(target.y, 0.f);
	EXPECT_FLOAT_EQ(target.z, 0.f);
}

TEST(TEST_cgmath_tcamera, default_constructor_up)
{
	Cameraf cam;
	const auto& up = cam.GetUp();
	EXPECT_FLOAT_EQ(up.x, 0.f);
	EXPECT_FLOAT_EQ(up.y, 1.f);
	EXPECT_FLOAT_EQ(up.z, 0.f);
}

TEST(TEST_cgmath_tcamera, default_constructor_fov)
{
	Cameraf cam;
	EXPECT_NEAR(cam.GetFov(), PI / 4.f, EPSILON);
}

TEST(TEST_cgmath_tcamera, default_constructor_clipping_planes)
{
	Cameraf cam;
	EXPECT_FLOAT_EQ(cam.GetNearPlane(), 0.1f);
	EXPECT_FLOAT_EQ(cam.GetFarPlane(), 100.f);
}

//-----------------------------------------------------------------------------
// Setters scalaires
//-----------------------------------------------------------------------------

TEST(TEST_cgmath_tcamera, SetPosition_xyz)
{
	Cameraf cam;
	cam.SetPosition(1.f, 2.f, 3.f);
	const auto& pos = cam.GetPosition();
	EXPECT_FLOAT_EQ(pos.x, 1.f);
	EXPECT_FLOAT_EQ(pos.y, 2.f);
	EXPECT_FLOAT_EQ(pos.z, 3.f);
}

TEST(TEST_cgmath_tcamera, SetPosition_vector)
{
	Cameraf cam;
	TVector3<float> v(4.f, 5.f, 6.f);
	cam.SetPosition(v);
	const auto& pos = cam.GetPosition();
	EXPECT_FLOAT_EQ(pos.x, 4.f);
	EXPECT_FLOAT_EQ(pos.y, 5.f);
	EXPECT_FLOAT_EQ(pos.z, 6.f);
}

TEST(TEST_cgmath_tcamera, SetTarget_xyz)
{
	Cameraf cam;
	cam.SetTarget(1.f, 2.f, 3.f);
	const auto& target = cam.GetTarget();
	EXPECT_FLOAT_EQ(target.x, 1.f);
	EXPECT_FLOAT_EQ(target.y, 2.f);
	EXPECT_FLOAT_EQ(target.z, 3.f);
}

TEST(TEST_cgmath_tcamera, SetTarget_vector)
{
	Cameraf cam;
	TVector3<float> v(7.f, 8.f, 9.f);
	cam.SetTarget(v);
	const auto& target = cam.GetTarget();
	EXPECT_FLOAT_EQ(target.x, 7.f);
	EXPECT_FLOAT_EQ(target.y, 8.f);
	EXPECT_FLOAT_EQ(target.z, 9.f);
}

TEST(TEST_cgmath_tcamera, SetUp)
{
	Cameraf cam;
	cam.SetUp(0.f, 0.f, 1.f);
	const auto& up = cam.GetUp();
	EXPECT_FLOAT_EQ(up.x, 0.f);
	EXPECT_FLOAT_EQ(up.y, 0.f);
	EXPECT_FLOAT_EQ(up.z, 1.f);
}

TEST(TEST_cgmath_tcamera, SetFov)
{
	Cameraf cam;
	cam.SetFov(PI / 3.f);
	EXPECT_NEAR(cam.GetFov(), PI / 3.f, EPSILON);
}

TEST(TEST_cgmath_tcamera, SetClippingPlanes)
{
	Cameraf cam;
	cam.SetClippingPlanes(0.5f, 500.f);
	EXPECT_FLOAT_EQ(cam.GetNearPlane(), 0.5f);
	EXPECT_FLOAT_EQ(cam.GetFarPlane(), 500.f);
}

//-----------------------------------------------------------------------------
// GetViewMatrix
//-----------------------------------------------------------------------------

TEST(TEST_cgmath_tcamera, GetViewMatrix_default_matches_SetLookAt)
{
	// Default camera: eye(0,0,3), center(0,0,0), up(0,1,0)
	Cameraf cam;
	Matrix4f view = cam.GetViewMatrix();

	// Build expected via SetLookAt
	float eye[3] = {0.f, 0.f, 3.f};
	float center[3] = {0.f, 0.f, 0.f};
	float up[3] = {0.f, 1.f, 0.f};
	Matrix4f expected;
	expected.SetLookAt(eye, center, up);

	for (int i = 0; i < 4; ++i)
		for (int j = 0; j < 4; ++j)
			EXPECT_NEAR(view.at(i, j), expected.at(i, j), EPSILON);
}

TEST(TEST_cgmath_tcamera, GetViewMatrix_custom_position)
{
	Cameraf cam;
	cam.SetPosition(5.f, 0.f, 0.f);
	cam.SetTarget(0.f, 0.f, 0.f);
	Matrix4f view = cam.GetViewMatrix();

	float eye[3] = {5.f, 0.f, 0.f};
	float center[3] = {0.f, 0.f, 0.f};
	float up[3] = {0.f, 1.f, 0.f};
	Matrix4f expected;
	expected.SetLookAt(eye, center, up);

	for (int i = 0; i < 4; ++i)
		for (int j = 0; j < 4; ++j)
			EXPECT_NEAR(view.at(i, j), expected.at(i, j), EPSILON);
}

TEST(TEST_cgmath_tcamera, GetViewMatrix_off_axis)
{
	Cameraf cam;
	cam.SetPosition(3.f, 4.f, 5.f);
	cam.SetTarget(1.f, 1.f, 1.f);
	cam.SetUp(0.f, 1.f, 0.f);
	Matrix4f view = cam.GetViewMatrix();

	float eye[3] = {3.f, 4.f, 5.f};
	float center[3] = {1.f, 1.f, 1.f};
	float up[3] = {0.f, 1.f, 0.f};
	Matrix4f expected;
	expected.SetLookAt(eye, center, up);

	for (int i = 0; i < 4; ++i)
		for (int j = 0; j < 4; ++j)
			EXPECT_NEAR(view.at(i, j), expected.at(i, j), EPSILON);
}

//-----------------------------------------------------------------------------
// GetProjectionMatrix
//-----------------------------------------------------------------------------

TEST(TEST_cgmath_tcamera, GetProjectionMatrix_default_matches_SetPerspective)
{
	Cameraf cam;
	float aspect = 16.f / 9.f;
	Matrix4f proj = cam.GetProjectionMatrix(aspect);

	Matrix4f expected;
	expected.SetPerspective(PI / 4.f, aspect, 0.1f, 100.f);

	for (int i = 0; i < 4; ++i)
		for (int j = 0; j < 4; ++j)
			EXPECT_NEAR(proj.at(i, j), expected.at(i, j), EPSILON);
}

TEST(TEST_cgmath_tcamera, GetProjectionMatrix_custom_params)
{
	Cameraf cam;
	cam.SetFov(PI / 3.f);
	cam.SetClippingPlanes(1.f, 1000.f);
	float aspect = 4.f / 3.f;
	Matrix4f proj = cam.GetProjectionMatrix(aspect);

	Matrix4f expected;
	expected.SetPerspective(PI / 3.f, aspect, 1.f, 1000.f);

	for (int i = 0; i < 4; ++i)
		for (int j = 0; j < 4; ++j)
			EXPECT_NEAR(proj.at(i, j), expected.at(i, j), EPSILON);
}

TEST(TEST_cgmath_tcamera, GetProjectionMatrix_vulkan_y_flip)
{
	Cameraf cam;
	Matrix4f proj = cam.GetProjectionMatrix(1.f);
	// Vulkan convention: at(1,1) is negative (Y-flip)
	EXPECT_LT(proj.at(1, 1), 0.f);
}

TEST(TEST_cgmath_tcamera, GetProjectionMatrix_vulkan_depth_range)
{
	Cameraf cam;
	Matrix4f proj = cam.GetProjectionMatrix(1.f);
	// Vulkan convention: at(3,2) == -1 (perspective divide)
	EXPECT_FLOAT_EQ(proj.at(3, 2), -1.f);
	// at(3,3) == 0 (perspective, not ortho)
	EXPECT_FLOAT_EQ(proj.at(3, 3), 0.f);
}

//-----------------------------------------------------------------------------
// MVP coherence: Projection * View produit une matrice non-degeneree
//-----------------------------------------------------------------------------

TEST(TEST_cgmath_tcamera, mvp_pipeline_non_degenerate)
{
	Cameraf cam;
	cam.SetPosition(0.f, 0.f, 5.f);
	cam.SetTarget(0.f, 0.f, 0.f);
	cam.SetClippingPlanes(0.1f, 100.f);

	Matrix4f view = cam.GetViewMatrix();
	Matrix4f proj = cam.GetProjectionMatrix(16.f / 9.f);
	Matrix4f vp = proj * view;

	// VP should be invertible (non-degenerate)
	EXPECT_NE(vp.Determinant(), 0.f);
}

TEST(TEST_cgmath_tcamera, mvp_origin_projects_to_center)
{
	// A point at the origin (camera target) should project near screen center
	Cameraf cam;
	cam.SetPosition(0.f, 0.f, 5.f);
	cam.SetTarget(0.f, 0.f, 0.f);

	Matrix4f view = cam.GetViewMatrix();
	Matrix4f proj = cam.GetProjectionMatrix(1.f);
	Matrix4f vp = proj * view;

	// Transform origin (0,0,0,1) through VP
	TVector4<float> origin(0.f, 0.f, 0.f, 1.f);
	TVector4<float> clip = vp * origin;

	// After perspective divide, x and y should be 0 (center of screen)
	ASSERT_NE(clip.w, 0.f);
	float ndcX = clip.x / clip.w;
	float ndcY = clip.y / clip.w;
	EXPECT_NEAR(ndcX, 0.f, EPSILON);
	EXPECT_NEAR(ndcY, 0.f, EPSILON);
}

//-----------------------------------------------------------------------------
// Double precision (Camerad)
//-----------------------------------------------------------------------------

TEST(TEST_cgmath_tcamera, double_precision_alias)
{
	Camerad cam;
	cam.SetPosition(1.0, 2.0, 3.0);
	const auto& pos = cam.GetPosition();
	EXPECT_DOUBLE_EQ(pos.x, 1.0);
	EXPECT_DOUBLE_EQ(pos.y, 2.0);
	EXPECT_DOUBLE_EQ(pos.z, 3.0);

	// Verify view matrix is also double
	auto view = cam.GetViewMatrix();
	// Just check it compiles and runs with double
	EXPECT_NE(view.Determinant(), 0.0);
}

//-----------------------------------------------------------------------------
// Setters ne modifient que le champ cible
//-----------------------------------------------------------------------------

TEST(TEST_cgmath_tcamera, SetPosition_does_not_affect_target)
{
	Cameraf cam;
	cam.SetTarget(1.f, 2.f, 3.f);
	cam.SetPosition(10.f, 20.f, 30.f);
	const auto& target = cam.GetTarget();
	EXPECT_FLOAT_EQ(target.x, 1.f);
	EXPECT_FLOAT_EQ(target.y, 2.f);
	EXPECT_FLOAT_EQ(target.z, 3.f);
}

TEST(TEST_cgmath_tcamera, SetFov_does_not_affect_clipping)
{
	Cameraf cam;
	cam.SetClippingPlanes(0.5f, 200.f);
	cam.SetFov(PI / 6.f);
	EXPECT_FLOAT_EQ(cam.GetNearPlane(), 0.5f);
	EXPECT_FLOAT_EQ(cam.GetFarPlane(), 200.f);
}
