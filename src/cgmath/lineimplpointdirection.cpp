#include "line.h"
#include "common.h"
#include "TMatrix3.h"

/**
* Implement the line with a point and a direction
*/

LineImplPointDirection::LineImplPointDirection ()
{
	m_pt.Set (0.0, 0.0, 0.0);
	m_dir.Set (0.0, 0.0, 1.0);
}

LineImplPointDirection::LineImplPointDirection (double par_pt_x, double par_pt_y, double par_pt_z,
												double par_dir_x, double par_dir_y, double par_dir_z)
{
	m_pt.Set (par_pt_x, par_pt_y, par_pt_z);
	m_dir.Set (par_dir_x, par_dir_y, par_dir_z);
}

LineImplPointDirection::LineImplPointDirection (Vector3f par_pt, Vector3f par_dir)
{
	m_pt = par_pt;
	m_dir = par_dir;
}

/**
* Constructor.
* Initializes a line from a line given by Pluecker coordinates.
*/
LineImplPointDirection::LineImplPointDirection (const LineImplPluecker &par_pluecker)
{
	double l1 = par_pluecker.m_l1;
	double l2 = par_pluecker.m_l2;
	double l3 = par_pluecker.m_l3;
	double l4 = par_pluecker.m_l4;
	double l5 = par_pluecker.m_l5;
	double l6 = par_pluecker.m_l6;

	// direction
	m_dir.Set (l1, l2, l3);

	// point
	if (l3 != 0.0)
	{
		m_pt.Set (-l5/l3, l4/l3, 0.0);
	}
	else if (l2 != 0.0)
	{
		m_pt.Set (l6/l2, 0.0, -l4/l2);
	}
	else if (l1 != 0.0)
	{
		m_pt.Set (0.0, -l6/l1, l5/l1);
	}
}

/**
* Destructor.
*/
LineImplPointDirection::~LineImplPointDirection ()
{
}

/**
* fits a line through a set of points.
*/
void LineImplPointDirection::fit (Vector3f *array, int n)
{
  Vector3f vector_walk;
  int i;

  float XX = 0.0;
  float XY = 0.0;
  float XZ = 0.0;
  float YY = 0.0;
  float YZ = 0.0;
  float ZZ = 0.0;

  // average of the points
  m_pt.Set (0.0, 0.0, 0.0);
  for (i=0; i<n; i++) m_pt += array[i];
  m_pt.x /= n;
  m_pt.y /= n;
  m_pt.z /= n;

  // sums of products
  vector_walk.Set (0.0, 0.0, 0.0);
  for (i=0; i<n; i++)
    {
      vector_walk = array[i] - m_pt;
      XX += vector_walk.x * vector_walk.x;
      XY += vector_walk.x * vector_walk.y;
      XZ += vector_walk.x * vector_walk.z;
      YY += vector_walk.y * vector_walk.y;
      YZ += vector_walk.y * vector_walk.z;
      ZZ += vector_walk.z * vector_walk.z;
    }

  // setup the eigensolver
  Matrix3f m (YY+ZZ, -XY, -XZ,
	      -XY, XX+ZZ, -YZ,
	      -XZ, -YZ, XX+YY);
  Vector3f e1, e2, e3, v;
  m.SolveEigensystem (e1, e2, e3, v);
  m_dir = e3;
  m_dir.Normalize ();
  
  /* determine the begin and end parameters */
  /*
  begin = closest_point (array[0]);
  end   = closest_point (array[0]);
  for (i=0; i<n; i++)
    {
      float pos = closest_point (array[i]);
      if (pos < begin) begin = pos;
      if (pos > end)   end = pos;
    }
	*/
}

void LineImplPointDirection::fit (Line **lines, int n)
{
}

// getters setters
void
LineImplPointDirection::get_direction (Vector3f &dir)
{
	dir.Set (m_dir.x, m_dir.y, m_dir.z);
}

void
LineImplPointDirection::get_direction (double &x, double &y, double &z)
{
	x = m_dir.x;
	y = m_dir.y;
	z = m_dir.z;
}

void
LineImplPointDirection::get_point (Vector3f &pt)
{
	pt.Set (m_pt.x, m_pt.y, m_pt.z);
}

void
LineImplPointDirection::get_point (double &x, double &y, double &z)
{
	x = m_pt.x;
	y = m_pt.y;
	z = m_pt.z;
}

/**
* position
*/
int
LineImplPointDirection::get_position_with (Line* line)
{
	assert (0);
	return 0;
}

/**
* compute the closest point from the line
*/
double
LineImplPointDirection::distance_with (Line &line)
{
	assert (line.m_line_type == LINE_POINT_DIRECTION);

	Vector3f direction1, origin1;
	Vector3f direction2, origin2;

	direction1 = m_dir;
	origin1    = m_pt;
	direction2 = ((LineImplPointDirection*)line.m_lineImpl)->m_dir;
	origin2    = ((LineImplPointDirection*)line.m_lineImpl)->m_pt;

	Vector3f n;
	n.x = direction1.y*direction2.z - direction2.y*direction1.z;
	n.y = direction2.x*direction1.z - direction1.x*direction2.z;
	n.z = direction1.x*direction2.y - direction2.x*direction1.y;
	n.Normalize ();

	Vector3f v = origin2 - origin1;

	float distance = v.x*n.x + v.y*n.y + v.z*n.z;
	return fabs(distance);
}

double
LineImplPointDirection::distance_with (Vector3f &pt)
{
	/*
	CVector3d h, pth;
	double position = this->closest_point (v);
	//this->v3d_from_position (position, h);
	pth = h - pt;
	return pth.length ();
	*/
	assert (0);
	return 0.0;
}

void
LineImplPointDirection::closest_point (Vector3f &pt, Vector3f &pt_on_line)
{
	double norm_dir = m_dir.getLength ();
	Vector3f tmp = pt - m_pt;
	double position = (tmp * m_dir) / (norm_dir * norm_dir);
	pt_on_line.Set (m_pt.x + position * m_dir.x,
					 m_pt.y + position * m_dir.y,
					 m_pt.z + position * m_dir.z);
}

void
LineImplPointDirection::init_shortest_distance (Line* line1, Line* line2)
{
  double A1x, A1y, A1z, A2x, A2y, A2z, B1x, B1y, B1z, B2x, B2y, B2z;
  float a[3], b[3], inter[3], vect[3];
  bool true_intersection;
  
  Vector3f tmp1, tmp2;
  bool infinite_lines = true;
  float epsilon = 0.0001;

  line1->get_point (A1x, A1y, A1z);
  line1->get_direction (tmp2);
  A2x = A1x + tmp2.x;
  A2y = A1y + tmp2.y;
  A2z = A1z + tmp2.z;
  
  line2->get_point (B1x, B1y, B1z);
  line2->get_direction (tmp2);
  B2x = B1x + tmp2.x;
  B2y = B1y + tmp2.y;
  B2z = B1z + tmp2.z;

  IntersectLineSegments (A1x, A1y, A1z, A2x, A2y, A2z,
			 B1x, B1y, B1z, B2x, B2y, B2z,
			 infinite_lines, epsilon,
			 a[0], a[1], a[2], b[0], b[1], b[2],
			 inter[0], inter[1], inter[2], vect[0], vect[1], vect[2],
			 true_intersection);

  m_pt.Set (a[0], a[1], a[2]);
  m_dir.Set (vect[0], vect[1], vect[2]);
}

/**
* Projections on planes
*/
void
LineImplPointDirection::projection_on_oxy (void)
{
	m_pt.z = 0.0;
	m_dir.z = 0.0;
}

/**
* Dump
*/
void
LineImplPointDirection::dump (void)
{
	printf ("Line Point Direction\n");
	printf ("point : %f %f %f\n", m_pt.x, m_pt.y, m_pt.z);
	printf ("direction : %f %f %f\n", m_dir.x, m_dir.y, m_dir.z);
}
