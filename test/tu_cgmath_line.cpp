#include <gtest/gtest.h>

#include <cmath>

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

// ===========================================================================
//  fit (Line**) : l'implementation se demande a la ligne, pas a son adresse
// ===========================================================================
//
// Le corps transtypait `lines[i]` -- un `Line*` -- directement en
// `LineImplPluecker*`, et lisait donc les coordonnees de Plucker a partir de
// l'adresse d'un `Line`. Comportement indefini, silencieux, dans du code que
// rien n'exercait. Ces cas exercent les deux branches du correctif : la ligne
// qui PORTE une implementation de Plucker, et celle qui n'en porte pas.

namespace {

// Une ligne rangee en representation de Plucker. On passe par point+direction
// puis convert : c'est le chemin que l'API expose, init_pluecker demandant les
// six coordonnees deja calculees.
void MakePlueckerLine (Line &line, float px, float py, float pz,
                       float dx, float dy, float dz)
{
	line.init_point_direction (px, py, pz, dx, dy, dz);
	line.convert (LINE_PLUECKER);
}

} // namespace

TEST (TEST_cgmath_line, fitting_pluecker_through_pluecker_lines_runs_on_their_impl)
{
	Line a, b, c;
	MakePlueckerLine (a, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f);
	MakePlueckerLine (b, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f);
	MakePlueckerLine (c, 0.f, 0.f, 0.f, 1.f, 1.f, 0.f);

	Line *lines[3] = { &a, &b, &c };
	Line fitted;
	fitted.convert (LINE_PLUECKER);
	fitted.fit (lines, 3);

	Vector3f direction;
	fitted.get_direction (direction);
	EXPECT_TRUE (std::isfinite (direction.x));
	EXPECT_TRUE (std::isfinite (direction.y));
	EXPECT_TRUE (std::isfinite (direction.z));
}

TEST (TEST_cgmath_line, fitting_pluecker_ignores_lines_held_in_another_representation)
{
	// LE CAS QUE LE TRANSTYPAGE EN C NE POUVAIT PAS VOIR. Une ligne rangee en
	// point + direction n'a aucune coordonnee de Plucker a offrir ; le
	// dynamic_cast le constate et la saute, au lieu de reinterpreter ses octets.
	Line pointDirection;
	pointDirection.init_point_direction (0.f, 0.f, 0.f, 1.f, 0.f, 0.f);
	Line pluecker;
	MakePlueckerLine (pluecker, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f);

	Line *lines[2] = { &pointDirection, &pluecker };
	Line fitted;
	fitted.convert (LINE_PLUECKER);
	fitted.fit (lines, 2);

	Vector3f point;
	fitted.get_point (point);
	EXPECT_TRUE (std::isfinite (point.x));
	EXPECT_TRUE (std::isfinite (point.y));
	EXPECT_TRUE (std::isfinite (point.z));
}
