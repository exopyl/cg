#include <gtest/gtest.h>

#include "../src/cgmath/cgmath.h"


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
