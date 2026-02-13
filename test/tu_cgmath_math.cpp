#include <gtest/gtest.h>

#include "../src/cgmath/cgmath.h"

TEST(TEST_cgmath, sqrt1)
{
	auto res = sqrt1(4);
	EXPECT_EQ(res, 2.);
}

TEST(TEST_cgmath, quaternion)
{
	Vector3f org, dst (1.0, 1.0, 1.0);

	Vector3f axis (1.0, 1.0, 0.0);
	float angle = DEGTORAD (180.0);

	Quaternionf q (axis, angle);
	cout << q << endl;

	org.Set (2.0, 0.0, 0.0);
	q.rotate (dst, org);
	cout << "org : " << org << endl;
	cout << "dst : " << dst << endl;

	q.get_axis_angle (axis, &angle);
	cout << "axis : " << axis << endl;
	cout << RADTODEG(angle);
}

#if 0
void test_matrix2 ()
{
	printf ("+---------+\n");
	printf ("| matrix2 |\n");
	printf ("+---------+\n");
	printf ("old version\n");
	float mm2[4] = {1.0, 2.0, 3.0, 2.0};
	float eigenvectors[2][2];
	float eigenvalues[2];
	matrix2_solve_eigensystem (mm2, eigenvectors, eigenvalues);
	Vector2d ev1 (eigenvectors[0][0], eigenvectors[0][1]);
	Vector2d ev2 (eigenvectors[1][0], eigenvectors[1][1]);

	ev1.dump ();
	ev2.dump ();
	printf ("%f %f\n", eigenvalues[0], eigenvalues[1]);
	
	//printf ();

	Vector2d ttt1, ttt2;
	//matrix2_mult_v2d (ttt1, mm2, ev1);

	printf ("\n");

	printf ("new version\n");
	printf ("Cmatrix2\n");
	Matrix2 m2 (1.0, 2.0, 3.0, 2.0);
	m2.dump ();

	Vector2d e1, e2, v;
	m2.solve_eigensystem (e1, e2, v);

	printf ("eigenvectors :\n");
	e1.dump ();
	e2.dump ();
	printf ("eigenvalues :\n");
	v.dump ();

	m2.init (1.0, 2.0, 3.0, 2.0);
	printf ("m * e1\n");
	Vector2d tmp1 = m2 * e1;
	tmp1.dump ();
	e1 *= v.x;
	e1.dump ();

	printf ("m * e2\n");
	Vector2d tmp2 = m2 * e2;
	tmp2.dump ();
	e2 *= v.y;
	e2.dump ();
}

void
test_matrix3 ()
{
	printf ("+---------+\n");
	printf ("| matrix3 |\n");
	printf ("+---------+\n");
	Matrix3 m3 (1.0, 2.0, 3.0,
		    2.0, 1.0, 2.0,
		    3.0, 2.0, 1.0);

	Csquare_matrix sm (3, 1.0, 2.0, 3.0,
			   2.0, 1.0, 2.0,
			   3.0, 2.0, 1.0);

	m3.dump ();
	sm.dump ();

	float right[3] = {1.0, 3.0, 3.0};
	float res[3];
	//sm.solve_linear_system (right, res, Csquare_matrix::SOLVE_LINEAR_SYSTEM_GAUSS_JORDAN);
	sm.solve_linear_system (right, res, Csquare_matrix::SOLVE_LINEAR_SYSTEM_LU_DECOMPOSITION);

	printf ("right (%f %f %f)\n", right[0], right[1], right[2]);
	printf ("res   (%f %f %f)\n", res[0], res[1], res[2]);
	//SOLVE_LINEAR_SYSTEM_GAUSS_JORDAN);
}

void
test_matrix4 ()
{
	printf ("+---------+\n");
	printf ("| matrix4 |\n");
	printf ("+---------+\n");
	Matrix4 m4_1 (1, 2, 3, 4,
		      3, 5, 1, 3,
		      5, 3, 4, 5,
		      1, 4, 5, 2);
	Matrix4 m4_2 = m4_1;
	m4_1.dump ();

	m4_1.inverse ();
	m4_1.dump ();
	Matrix4 m4_3 = m4_1;
	m4_3 = m4_1 * m4_2;
	m4_3.dump ();
}

void
test_square_matrix (void)
{
	Csquare_matrix m (7,
			  4.0, 4.0, 2.0, 4.0, 5.0, 3.0, 9.0,
			  3.0, 4.0, 2.0, 8.0, 5.0, 0.0, 4.0,
			  2.0, 4.0, 5.0, 2.0, 8.0, 4.0, 2.0,
			  1.0, 3.0, 5.0, 4.0, 9.0, 6.0, 3.0,
			  1.0, 4.0, 7.0, 7.0, 4.0, 2.0, 5.0,
			  4.0, 2.0, 0.0, 8.0, 4.0, 5.0, 2.0,
			  3.0, 6.0, 4.0, 9.0, 1.0, 3.0, 0.0);
	Csquare_matrix m2 = m;
	m.inverse ();
	m2 *= m;
	//m.inverse ();
	m2.dump ();
}


void
main_matrix (void)
{
	test_matrix2 ();
	//test_matrix3 ();
	//test_matrix4 ();
	//test_square_matrix ();
}
#endif


//=============================================================================
// test vector2
//=============================================================================
TEST(TEST_cgmath, vector2)
{
	//
	Vector2f v1;
	cout << "Vector2 v1;" << endl;
	cout << "v1 : " << v1 << endl << endl;

	//
	Vector2f v2 (1.1, 2.1);
	cout << "Vector2 v2 (1.1, 2.1);" << endl;
	cout << "v2 : " << v2 << endl << endl;
	
	//
	Vector2f v3 (v2);
	cout << "Vector2 v3 (v2);" << endl;
	cout << "v3 : " << v3 << endl << endl;

	//
	v3 = v1;
	cout << "v3 = v1;" << endl;
	cout << "v3 : " << v3 << endl << endl;

	//
	v1 = v2 + v3;
	cout << "v1 = v2 + v3;" << endl;
	cout << "v1 : " << v1 << endl << endl;

	//
	v1 = v2 - v3;
	cout << "v1 = v2 - v3;" << endl;
	cout << "v1 : " << v1 << endl << endl;

	//
	v1 = v2 * 2.0;
	cout << "v1 = v2 * 2.0;" << endl;
	cout << "v1 : " << v1 << endl << endl;

	//
	v1 = v2 / 3.0;
	cout << "v1 = v2 / 3.0;" << endl;
	cout << "v1 : " << v1 << endl << endl;

	//
	v1 += v2;
	cout << "v1 += v2;" << endl;
	cout << "v1 : " << v1 << endl << endl;

	//
	v1 -= v2;
	cout << "v1 -= v2;" << endl;
	cout << "v1 : " << v1 << endl << endl;

	//
	v1 *= 2.0;
	cout << "v1 *= 2.0;" << endl;
	cout << "v1 : " << v1 << endl << endl;

	//
	v1 /= 3.0;
	cout << "v1 /= 3.0;" << endl;
	cout << "v1 : " << v1 << endl << endl;

	cout << v1[-1] << endl;
	cout << v1[0] << endl;
	cout << v1[1] << endl;
	cout << v1[2] << endl << endl;

	//
	cout << "v1 == v2" << endl;
	cout << (v1 == v2) << endl << endl;

	//
	cout << "v1 != v2" << endl;
	cout << (v1 != v2) << endl << endl;

	//
	v1.Set (2.0, 3.0);
	cout << "v1.Set (2.0, 3.0);" << endl;
	cout << "v1 : " << v1 << endl << endl;

	//
	v1.Clamp (-1.0, 1.0);
	cout << "v1.Clamp (-1.0, 1.0);" << endl;
	cout << "v1 : " << v1 << endl << endl;

	//
	cout << "v1.getLength2 ();" << endl;
	cout << v1.getLength2 () << endl << endl;

	//
	cout << "v1.getLength ();" << endl;
	cout << v1.getLength () << endl << endl;

	//
	v1.Normalize ();
	cout << "v1.Normalize ();" << endl;
	cout << v1 << endl << endl;

	//
	cout << "v1.DotProduct (v2);" << endl;
	cout << v1.DotProduct (v2) << endl << endl;

	//
	cout << "(v1 * v2);" << endl;
	cout << (v1 * v2) << endl << endl;

	//
	cout << "v1.getDistance (v2);" << endl;
	cout << v1.getDistance (v2) << endl << endl;

	//
	cout << "v1.getAngle (v2);" << endl;
	cout << v1.getAngle (v2) << endl << endl;
}

//=============================================================================
// test vector3
//=============================================================================
TEST(TEST_cgmath, vector3)
{
	//
	Vector3 v1;
	cout << "Vector3 v1;" << endl;
	cout << "v1 : " << v1 << endl << endl;

	//
	Vector3 v2 (1.0, 2.0, 3.0);
	cout << "Vector3 v2 (1.0, 2.0, 3.0);" << endl;
	cout << "v2 : " << v2 << endl << endl;
	
	//
	Vector3 v3 (v2);
	cout << "Vector3 v3 (v2);" << endl;
	cout << "v3 : " << v3 << endl << endl;

	//
	v3 = v1;
	cout << "v3 = v1;" << endl;
	cout << "v3 : " << v3 << endl << endl;

	//
	v1 = v2 + v3;
	cout << "v1 = v2 + v3;" << endl;
	cout << "v1 : " << v1 << endl << endl;

	//
	v1 = v2 - v3;
	cout << "v1 = v2 - v3;" << endl;
	cout << "v1 : " << v1 << endl << endl;

	//
	v1 = v2 * 2.0;
	cout << "v1 = v2 * 2.0;" << endl;
	cout << "v1 : " << v1 << endl << endl;

	//
	v1 = v2 / 3.0;
	cout << "v1 = v2 / 3.0;" << endl;
	cout << "v1 : " << v1 << endl << endl;

	//
	v1 += v2;
	cout << "v1 += v2;" << endl;
	cout << "v1 : " << v1 << endl << endl;

	//
	v1 -= v2;
	cout << "v1 -= v2;" << endl;
	cout << "v1 : " << v1 << endl << endl;

	//
	v1 *= 2.0;
	cout << "v1 *= 2.0;" << endl;
	cout << "v1 : " << v1 << endl << endl;

	//
	v1 /= 3.0;
	cout << "v1 /= 3.0;" << endl;
	cout << "v1 : " << v1 << endl << endl;

	cout << v1[-1] << endl;
	cout << v1[0] << endl;
	cout << v1[1] << endl;
	cout << v1[2] << endl;
	cout << v1[3] << endl << endl;

	//
	cout << "v1 == v2" << endl;
	cout << (v1 == v2) << endl << endl;

	//
	cout << "v1 != v2" << endl;
	cout << (v1 != v2) << endl << endl;

	//
	v1.Set (2.0, 3.0, 4.0);
	cout << "v1.Set (2.0, 3.0, 4.0);" << endl;
	cout << "v1 : " << v1 << endl << endl;

	//
	v1.Clamp (-1.0, 1.0);
	cout << "v1.Clamp (-1.0, 1.0);" << endl;
	cout << "v1 : " << v1 << endl << endl;

	//
	cout << "v1.getLength2 ();" << endl;
	cout << v1.getLength2 () << endl << endl;

	//
	cout << "v1.getLength ();" << endl;
	cout << v1.getLength () << endl << endl;

	//
	v1.Normalize ();
	cout << "v1.Normalize ();" << endl;
	cout << v1 << endl << endl;

	//
	cout << "v1.DotProduct (v2);" << endl;
	cout << v1.DotProduct (v2) << endl << endl;

	//
	cout << "(v1 * v2);" << endl;
	cout << (v1 * v2) << endl << endl;

	//
	cout << "v1.getDistance (v2);" << endl;
	cout << v1.getDistance (v2) << endl << endl;

	//
	cout << "v1.getAngle (v2);" << endl;
	cout << v1.getAngle (v2) << endl << endl;
}

//=============================================================================
// test vector4
//=============================================================================
TEST(TEST_cgmath, vector4)
{
	//
	Vector4 v1;
	cout << "Vector4 v1;" << endl;
	cout << "v1 : " << v1 << endl << endl;

	//
	Vector4 v2 (1.0, 2.0, 3.0, 4.0);
	cout << "Vector4 v2 (1.0, 2.0, 3.0, 4.0);" << endl;
	cout << "v2 : " << v2 << endl << endl;
	
	//
	Vector4 v3 (v2);
	cout << "Vector4 v3 (v2);" << endl;
	cout << "v3 : " << v3 << endl << endl;

	//
	v3 = v1;
	cout << "v3 = v1;" << endl;
	cout << "v3 : " << v3 << endl << endl;

	//
	v1 = v2 + v3;
	cout << "v1 = v2 + v3;" << endl;
	cout << "v1 : " << v1 << endl << endl;

	//
	v1 = v2 - v3;
	cout << "v1 = v2 - v3;" << endl;
	cout << "v1 : " << v1 << endl << endl;

	//
	v1 = v2 * 2.0;
	cout << "v1 = v2 * 2.0;" << endl;
	cout << "v1 : " << v1 << endl << endl;

	//
	v1 = v2 / 3.0;
	cout << "v1 = v2 / 3.0;" << endl;
	cout << "v1 : " << v1 << endl << endl;

	//
	v1 += v2;
	cout << "v1 += v2;" << endl;
	cout << "v1 : " << v1 << endl << endl;

	//
	v1 -= v2;
	cout << "v1 -= v2;" << endl;
	cout << "v1 : " << v1 << endl << endl;

	//
	v1 *= 2.0;
	cout << "v1 *= 2.0;" << endl;
	cout << "v1 : " << v1 << endl << endl;

	//
	v1 /= 3.0;
	cout << "v1 /= 3.0;" << endl;
	cout << "v1 : " << v1 << endl << endl;

	cout << v1[-1] << endl;
	cout << v1[0] << endl;
	cout << v1[1] << endl;
	cout << v1[2] << endl;
	cout << v1[3] << endl;
	cout << v1[4] << endl << endl;

	//
	cout << "v1 == v2" << endl;
	cout << (v1 == v2) << endl << endl;

	//
	cout << "v1 != v2" << endl;
	cout << (v1 != v2) << endl << endl;

	//
	v1.Set (2.0, 3.0, 4.0, 5.0);
	cout << "v1.Set (2.0, 3.0, 4.0, 5.0);" << endl;
	cout << "v1 : " << v1 << endl << endl;
}


TEST(TEST_cgmath, matrix2)
{
	cout << "m1 : constructor by default" << endl;
	Matrix2f m1;
	cout << m1 << endl << endl;

	cout << "m2 : constructor with values" << endl;
	Matrix2f m2 (1.0, 2.0, 3.0, 2.0);
	cout << m2 << endl << endl;

	cout << "m3 : constructor with array of values" << endl;
	float m3values[4] = {4.0, 2.0, 1.0, 2.0};
	Matrix2f m3 (m3values);
	cout << m3 << endl;
	cout << endl;

	cout << "m4 : initialization by affectation of an array of values" << endl;
	Matrix2f m4 = m3values;
	cout << m4 << endl << endl;

	cout << "m5 : initialization by affectation of a Matrix2" << endl;
	Matrix2f m5 = m4;
	cout << m5 << endl << endl;

	cout << "m4 == m5 ? : " << ((m4==m5)? 1 : 0) << endl;
	cout << "m2 == m4 ? : " << ((m2==m4)? 1 : 0) << endl;
	cout << "m4 != m5 ? : " << ((m4!=m5)? 1 : 0) << endl;
	cout << "m2 != m4 ? : " << ((m2!=m4)? 1 : 0) << endl;
	cout << endl << endl;

	cout << "m6 = m2 * m4" << endl;
	Matrix2 m6 = m2 * m4;
	cout << m6 << endl << endl;

	cout << "m6.Transpose" << endl;
	m6.Transpose ();
	cout << m6 << endl << endl;

	cout << "m6.Determinant" << endl;
	cout << m6.Determinant () << endl << endl;

	cout << "m6.GetInverse (m7)" << endl;
	Matrix2 m7;
	m6.GetInverse (m7);
	cout << m7 << endl << endl;

	cout << "m8 = m6 * m7" << endl;
	Matrix2 m8 = m6 * m7;
	cout << m8 << endl << endl;

	cout << "linear system : m6 * (x y) = (1.0 2.0)" << endl;
	Vector2 right (1.0, 2.0);
	Vector2 sol;
	m6.SolveLinearsystem (right, sol);
	cout << sol << endl;
	Vector2 res = m6 * sol;
	cout << res << endl << endl;

	cout << "eigen system" << endl;
	Matrix2 m9 (-4, 2, 3, 1);
	cout << m9 << endl << endl;
	Vector2 evector1, evector2, evalues;
	bool hr = m9.SolveEigensystem (evector1, evector2, evalues);
	cout << evector1 << endl;
	cout << evector2 << endl;
	cout << evalues << endl;
	cout << m9*evector1 << endl;
	cout << evector1*evalues.x << endl;
	cout << m9*evector2 << endl;
	cout << evector2*evalues.y << endl;
}

TEST(TEST_cgmath, matrix3)
{
	Matrix3 m;
	m.Set (-3., 5., 6.,
			-1., 2., 2.,
			1., -1., -1.);
	//cout << m << endl;
	Matrix3 minv = m;
	cout << m.Determinant() << endl;
	minv.Inverse ();
	Matrix3 i = m*minv;
	cout << "-----" << endl;
	cout << m << endl;
	cout << "-----" << endl;
	cout << minv << endl;
	cout << "-----" << endl;
	cout << i << endl;
	return;

	Matrix3 m2 = m;
	m2.m_Mat[0][0] = 0.;
	m2 *= 3.;
	cout << "m" << endl << m << endl;
	cout << "m2" << endl << m2 << endl;
	m2.Transpose();
	cout << "m2t" << endl << m2 << endl;

	Vector3 v (1., 2., 3.);
	Vector3 vv = m2*v;
	cout << "v" << endl << v << endl;
	cout << "vv" << endl << vv << endl;

	Matrix3 rot;
	rot.SetRotation (90., 0., 0., 1.);
	cout << rot << endl;
	Vector3 vrot (1., 0., 0.);
	cout << vrot << endl;
	cout << rot*vrot << endl;

	if (0)
	{
		Matrix3 m;
		cout << m << endl;

		// ref : http://www.softintegration.com/chhtml/lang/lib/libch/numeric/CGI_Eigen.html
		cout << "eigen system" << endl;
		Matrix3 m9 (3, 2, 3,
					2, 2, 1,
					3, 1, 3);
		cout << m9 << endl << endl;

		//Matrix3 m9copy (m9);
		Vector3 evector1, evector2, evector3, evalues;
		bool hr = m9.SolveEigensystem (evector1, evector2, evector3, evalues);
		if (hr)
		{
			cout << "result = " << hr << endl;
			cout << "evector1 : " << evector1 << endl;
			cout << "evector2 : " << evector2 << endl;
			cout << "evector3 : " << evector3 << endl;
			cout << "evalues : " << evalues << endl;
			cout << endl;

			cout << m9*evector1 << endl;
			cout << evector1*evalues.x << endl;
			cout << m9*evector2 << endl;
			cout << evector2*evalues.y << endl;
			cout << m9*evector3 << endl;
			cout << evector3*evalues.z << endl;
		}
		else
		{
			cout << "pas possible de résoudre le système propre" << endl;
		}
	}
}

TEST(TEST_cgmath, squarematrix)
{

	float m1[16] = {	1, 2, 3, 4, 
						2, 2, 5, 3,
						3, 5, 2, 6,
						4, 3, 6, 4	};

	float m2[16] = {	1, 2, 3, 4, 
						0, 1, 0, 0,
						0, 0, 1, 0,
						0, 0, 0, 1	};

	float m3[16] = {	1, 2, 3, 4, 
						5, 6, 7, 8,
						9, 10, 11, 12,
						13, 14, 15, 16	};


	float src[4] = {1.0, 2.0, 3.0, 4.0};
	float dst[4] = {0.0, 0.0, 0.0, 0.0};

	SquareMatrixf sq (	4, m3	);
	//sq.SetIdentity ();
	cout << sq << endl;

	SquareMatrixf sq1, sq2, sq3;
	// copy
	sq2 = sq;
	cout << sq2 << endl;

	// Transpose
	sq2.Transpose ();
	cout << sq2 << endl;

	// Set
	sq2.Set (4, m2);
	cout << sq2 << endl;

	// Multiply
	sq2.Multiply (src, dst);
	printf ("dst : %.3f %.3f %.3f %.3f\n", dst[0], dst[1], dst[2], dst[3]);

	//
	// eigensystem
	//
	float eigenvectors[4][4];
	float eigenvalues[4];
	sq.Set (4, m1);
	cout << sq << endl;
	sq.SolveEigenSystem ();
	sq.GetEigenValues (eigenvalues);

	int i, j, n = 4;
	printf ("eigenvalues :\n\t");
	for (i=0; i<n; i++) printf ("%.3f ", eigenvalues[i]);
	printf ("\n");

	printf ("eigenvectors :\n");
	for (i=0; i<n; i++)
	{
		sq.GetEigenVector (i, eigenvectors[i]);
		printf ("\t%d : ", i);
		for (j=0; j<n; j++)
			printf ("%.3f ", eigenvectors[i][j]);
		printf ("\n");
	}
	printf ("\n");

	//sq.Set (4, m1);
	cout << sq << endl;

	//
	cout << "check the relevance of result of the eigen system" << endl;
	for (int j=0; j<n; j++)
	{
		printf ("%d\n", j);
		sq.Multiply (eigenvectors[j], dst);
		for (i=0; i<n; i++) printf ("%.3f ", dst[i]);
		printf ("\n");
		for (i=0; i<n; i++) printf ("%.3f ", eigenvalues[j]*eigenvectors[j][i]);
		printf ("\n");
	}

	//
	// linearsystem
	//
	cout << endl << "Linear System" << endl;
	float right[4] = {1.0, 3.0, 0.0, 4.0};
	float res[4] = {0.0, 0.0, 0.0, 0.0};

	sq.Set (4, m1);
	cout << sq << endl;
	//sq.SolveLinearSystem (right, res, SOLVE_LINEAR_SYSTEM_LU_DECOMPOSITION);
	sq.SolveLinearSystem (right, res, SOLVE_LINEAR_SYSTEM_GAUSS_JORDAN);
	for (i=0; i<n; i++) printf ("%.3f ", res[i]);
	printf ("\n");

	sq.Set (4, m1);
	sq.Multiply (res, dst);
	for (i=0; i<n; i++) printf ("%.3f ", dst[i]);
	printf ("\n");


//	Csquare_matrix *sm = new Csquare_matrix (4, m1);
//	float a, b, c;
//	if (sm->solve_linear_system (right, res, Csquare_matrix::SOLVE_LINEAR_SYSTEM_GAUSS_JORDAN))
//	{
//		printf ("%.3f %.3f %.3f %.3f\n", res[0], res[1], res[2], res[3]);
//	}

	cout << "============================" << endl;
	cout << "Operators" << endl;
	sq1.Set (4, m1);
	sq2.Set (4, m2);
	cout << sq1 << endl;
	cout << sq2 << endl;
	//sq1 += sq2;
	//sq3 = (sq1 + sq2); // does not work
	cout << sq1 << endl;
	cout << sq2 << endl;
	cout << sq3 << endl;

	cout << "============================" << endl;
	cout << "Determinant" << endl;
	sq1.Set (4, m1);
	sq2.Set (4, m2);
	cout << "sq1.determinant () = " << sq1.Determinant () << endl;
	cout << "sq2.determinant () = " << sq2.Determinant () << endl;


	cout << "============================" << endl;
	cout << "Inverse" << endl;

	cout << "square matrix avec m1" << endl;
	//sq1.Set (4, m1);
	sq2.Set (4, m1);
	cout << sq2 << endl;

	sq2.GetInverse (sq3);
	//sq2 *= sq3;
	cout << sq2 << endl;
	cout << sq3 << endl;

	sq2.SetInverse ();
	cout << sq2 << endl;

//	Csquare_matrix mm (4, m1);
//	mm.inverse ();
//	cout << "result" << endl;
//	mm.dump ();

	sq2.SetRandomSymmetric ();
	cout << sq2 << endl;
}
