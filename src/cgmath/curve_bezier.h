#ifndef __CURVE_BEZIER_H__
#define __CURVE_BEZIER_H__

#include "curve.h"
#include "algebra_vector3.h"

/**
* TODO
*
* Impl�menter intersection avec droites
* Faire inventaire des diff�rentes m�thodes d'interpolation
*/

/**
* Bezier curve
*/
class CurveBezier //: public Curve
{
public:
	CurveBezier ();
	CurveBezier (const CurveBezier &par_curveBezier);
	//CurveBezier* clone () { return new CurveBezier (*this); };
	void dump (void);
	void export_interpolated (char *filename, unsigned int n);

	// specific method
	~CurveBezier ();

	int getDegree (void) { return m_nControlPoints-1; };
	bool getControlPoint (int index, vec3 v)
	{
		if (index > m_nControlPoints)
			return false;
		
		v[0] = m_controlPoints[index][0];
		v[1] = m_controlPoints[index][1];
		v[2] = m_controlPoints[index][2];

		return true;
	}

	// Construction of the Bezier curve
	int addControlPoint (vec3 v);
	int addControlPoint (float x, float y, float z);
	
	// eval
	int eval (float t, vec3 pt);
	int eval_on_x (float x, vec3 pt);

	// Interpolation methods
	int computeInterpolation (int par_nPoints, vec3 **par_points);
	int computeInterpolation3 (int par_nPoints, vec3 **par_points);
	int computeInterpolation4 (int par_nPoints, vec3 **par_points);

	int computeInterpolationRecursive3 (int level, int &par_nPoints, vec3 **par_points);
	int computeInterpolationRecursive3aux (int par_level, vec3 **points, int pos);

	bool isSufficentlyFlat (float tolerance);
	int computeInterpolationRecursiveFlatness3 (float tolerance, int &par_nPoints, vec3 **par_points);

	// Normals

	// Length
	float length4 (void);

	// Intersection with a line


private:
	int m_nMaxControlPoints;
	int m_nControlPoints;
	vec3 *m_controlPoints;
};

#endif // __CURVE_BEZIER_H__
