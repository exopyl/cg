#include "line.h"
#include "common.h"

/**
* Implement the line with a point and a direction
*/

/**
* Constructor by default.
* Initializes all the Pluecker coordinates to zero.
*/
LineImplPluecker::LineImplPluecker ()
{
	m_l1 = m_l2 = m_l3 = m_l4 = m_l5 = m_l6 = 0.0;
}

/**
* Constructor.
* Initializes the Pluecker coordinates with the ones given.
*/
LineImplPluecker::LineImplPluecker (double l1, double l2, double l3,
									double l4, double l5, double l6)
{
	m_l1 = l1;
	m_l2 = l2;
	m_l3 = l3;
	m_l4 = l4;
	m_l5 = l5;
	m_l6 = l6;
}

/**
* Constructor.
* Initializes the line from a line defined by a point and a direction.
*/
LineImplPluecker::LineImplPluecker (const LineImplPointDirection &par_pointDirection)
{
	Vector3f pt = par_pointDirection.m_pt;
	Vector3f dir = par_pointDirection.m_dir;
	float l;

	dir.Normalize();
	m_l1 = dir.x;
	m_l2 = dir.y;
	m_l3 = dir.z;

	Vector3f tmp = pt ^ dir;
	m_l4 = tmp.x;
	m_l5 = tmp.y;
	m_l6 = tmp.z;

	// normalization
	l = sqrt (m_l1*m_l1+m_l2*m_l2+m_l3*m_l3+m_l4*m_l4+m_l5*m_l5+m_l6*m_l6);
	assert (l!=0);
	m_l1 /= l;
	m_l2 /= l;
	m_l3 /= l;
	m_l4 /= l;
	m_l5 /= l;
	m_l6 /= l;
}

/**
* Destructor.
*/
LineImplPluecker::~LineImplPluecker ()
{
}

/**
* fits a line through a set of points.
*/
void LineImplPluecker::fit (Vector3f *array, int n)
{
	LineImplPointDirection implPointDirection;
	LineImplPluecker *implPluecker;

	implPointDirection.fit (array, n);
	implPluecker = new LineImplPluecker (implPointDirection);

	m_l1 = implPluecker->m_l1;
	m_l2 = implPluecker->m_l2;
	m_l3 = implPluecker->m_l3;
	m_l4 = implPluecker->m_l4;
	m_l5 = implPluecker->m_l5;
	m_l6 = implPluecker->m_l6;

	delete implPluecker;
}

void LineImplPluecker::fit (Line **lines, int n)
{
	float l11, l22, l33, l44, l55, l66;
	float l14, l24, l34, l45, l46;
	float l15, l25, l35, l56;
	float l16, l26, l36;
	float l12, l13;
	float l23;
	l11 = l22 = l33 = l44 = l55 = l66 = 0.0;
	l14 = l24 = l34 = l45 = l46 = 0.0;
	l15 = l25 = l35 = l56 = 0.0;
	l16 = l26 = l36 = 0.0;
	l12 = l13 = 0.0;
	l23 = 0.0;

	for (int i=0; i<n; i++)
	{
		LineImplPluecker *line_walk = (LineImplPluecker*)lines[i];
		l11 += line_walk->m_l1 * line_walk->m_l1;
		l22 += line_walk->m_l2 * line_walk->m_l2;
		l33 += line_walk->m_l3 * line_walk->m_l3;
		l44 += line_walk->m_l4 * line_walk->m_l4;
		l55 += line_walk->m_l5 * line_walk->m_l5;
		l66 += line_walk->m_l6 * line_walk->m_l6;
		l14 += line_walk->m_l1 * line_walk->m_l4;
		l24 += line_walk->m_l2 * line_walk->m_l4;
		l34 += line_walk->m_l3 * line_walk->m_l4;
		l45 += line_walk->m_l4 * line_walk->m_l5;
		l46 += line_walk->m_l4 * line_walk->m_l6;
		l15 += line_walk->m_l1 * line_walk->m_l5;
		l25 += line_walk->m_l2 * line_walk->m_l5;
		l35 += line_walk->m_l3 * line_walk->m_l5;
		l56 += line_walk->m_l5 * line_walk->m_l6;
		l16 += line_walk->m_l1 * line_walk->m_l6;
		l26 += line_walk->m_l2 * line_walk->m_l6;
		l36 += line_walk->m_l3 * line_walk->m_l6;
		l12 += line_walk->m_l1 * line_walk->m_l2;
		l13 += line_walk->m_l1 * line_walk->m_l3;
		l23 += line_walk->m_l2 * line_walk->m_l3;
	}

	float data[36] = {	l44, l45, l46, l14, l24, l34,
		l45, l55, l56, l15, l25, l35,
		l46, l56, l66, l16, l26, l36,
		l14, l15, l16, l11, l12, l13,
		l24, l25, l26, l12, l22, l23,
		l34, l35, l36, l13, l23, l33 };
	//SquareMatrix m (6, data);
	//m.SolveEigenSystem ();
}

// getters setters
void
LineImplPluecker::get_direction (Vector3f &dir)
{
	dir.Set (m_l1, m_l2, m_l3);
}

void
LineImplPluecker::get_direction (double &x, double &y, double &z)
{
	x = m_l1;
	y = m_l2;
	z = m_l3;
}

void
LineImplPluecker::get_point (Vector3f &pt)
{
	if (m_l3 != 0.0)
	{
	  pt.Set (-m_l5/m_l3, m_l4/m_l3, 0.0);
	}
	else if (m_l2 != 0.0)
	{
	  pt.Set (m_l6/m_l2, 0.0, -m_l4/m_l2);
	}
	else if (m_l1 != 0.0)
	{
	  pt.Set (0.0, -m_l6/m_l1, m_l5/m_l1);
	}
}

void
LineImplPluecker::get_point (double &x, double &y, double &z)
{
	if (m_l3 != 0.0)
	{
		x = -m_l5/m_l3;
		y = m_l4/m_l3;
		z = 0.0;
	}
	else if (m_l2 != 0.0)
	{
	  x = m_l6/m_l2;
	  y = 0.0;
	  z = -m_l4/m_l2;
	}
	else if (m_l1 != 0.0)
	{
		x = 0.0;
		y = -m_l6/m_l1;
		z = m_l5/m_l1;
	}
}

/**
* position
*/
int
LineImplPluecker::get_position_with (Line* line)
{
	if (line->m_line_type != LINE_PLUECKER)
		assert (0);

	LineImplPluecker* implline = (LineImplPluecker*)line->m_lineImpl;
/*
	double tmp =  this->m_l1 * implline->m_l6
				- this->m_l2 * implline->m_l5
				+ this->m_l3 * implline->m_l4
				+ this->m_l4 * implline->m_l3
				- this->m_l5 * implline->m_l2
				+ this->m_l6 * implline->m_l1;
*/
	double tmp =  this->m_l1 * implline->m_l4
				+ this->m_l2 * implline->m_l5
				+ this->m_l3 * implline->m_l6
				+ this->m_l4 * implline->m_l1
				+ this->m_l5 * implline->m_l2
				+ this->m_l6 * implline->m_l3;

	if (tmp == 0.0) return 0;	// intersection
	else if (tmp > 0.0) return 1;	// clockwise
	else return -1;	// counterclockwise
}

/**
* compute the closest point from the line
*/
double
LineImplPluecker::distance_with (Line &line)
{
	assert (0);
	return 0.0;
}

double
LineImplPluecker::distance_with (Vector3f &pt)
{
	assert (0);
	return 0.0;
}

void
LineImplPluecker::closest_point (Vector3f &pt, Vector3f &pt_on_line)
{
	assert (0);
}

void
LineImplPluecker::init_shortest_distance (Line* line1, Line* line2)
{
	assert (0);
}

/**
* Projections on planes
*/
void
LineImplPluecker::projection_on_oxy (void)
{
	assert (0);
}

/**
* Dump
*/
void
LineImplPluecker::dump (void)
{
	printf ("Line Pluecker\n");
	printf ("%.3f %.3f %.3f %.3f %.3f %.3f\n",
		m_l1, m_l2, m_l3, m_l4, m_l5, m_l6);
}
