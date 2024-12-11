#include "line.h"

/**
* Constructors
*/
Line::Line (line_type par_type)
{
	m_line_type = par_type;
	switch (m_line_type)
	{
	case LINE_POINT_DIRECTION:
		m_lineImpl = new LineImplPointDirection ();
		break;
	case LINE_PLUECKER:
		m_lineImpl = new LineImplPluecker ();
		break;
	default:
		printf ("type unknown\n");
	}
}

/**
* Convert a representation to another one
*/
int Line::convert (line_type par_type)
{
	if (m_line_type == par_type) return 1;

	LineImpl *m_lineImpl_tmp;

	switch (m_line_type)
	{
	case LINE_POINT_DIRECTION:
		switch (par_type)
		{
		case LINE_PLUECKER:
			m_lineImpl_tmp = new LineImplPluecker ((LineImplPointDirection&)(*m_lineImpl));
			break;
		default:
			return 0;
			break;
		}
		break;
	case LINE_PLUECKER:
		switch (par_type)
		{
		case LINE_POINT_DIRECTION:
			m_lineImpl_tmp = new LineImplPointDirection ((LineImplPluecker&)(*m_lineImpl));
			break;
		default:
			return 0;
			break;
		}
		break;
	default:
		return 0;
	}
	delete m_lineImpl;
	m_lineImpl = m_lineImpl_tmp;
	m_line_type = par_type;
	return 1;
}

/* initialization */
void
Line::init_point_direction (double pt_x, double pt_y, double pt_z,
							 double dir_x, double dir_y, double dir_z)
{
	delete m_lineImpl;
	m_line_type = LINE_POINT_DIRECTION;
	m_lineImpl = new LineImplPointDirection (pt_x, pt_y, pt_z, dir_x, dir_y, dir_z);
}

void
Line::init_point_point (double pt1_x, double pt1_y, double pt1_z,
						 double pt2_x, double pt2_y, double pt2_z)
{
	delete m_lineImpl;
	m_line_type = LINE_POINT_DIRECTION;
	m_lineImpl = new LineImplPointDirection (pt1_x, pt1_y, pt1_z,
		pt2_x - pt1_x, pt2_y - pt1_y, pt2_z - pt1_z);
}

void
Line::init_point_direction (Vector3f pt, Vector3f dir)
{
	delete m_lineImpl;
	m_line_type = LINE_POINT_DIRECTION;
	m_lineImpl = new LineImplPointDirection (pt, dir);
}

void
Line::init_point_point (Vector3f pt1, Vector3f pt2)
{
	delete m_lineImpl;
	m_line_type = LINE_POINT_DIRECTION;
	m_lineImpl = new LineImplPointDirection (pt1, pt2-pt1);
}

void
Line::init_pluecker (double l1, double l2, double l3,
					  double l4, double l5, double l6)
{
	delete m_lineImpl;
	m_line_type = LINE_PLUECKER;
	m_lineImpl = new LineImplPluecker (l1, l2, l3, l4, l5, l6);
}

// fitting
void Line::fit (Vector3d *array, int n)
{
	//m_lineImpl->fit (array, n);
}

void Line::fit (Vector3f *array, int n)
{
	m_lineImpl->fit (array, n);
}

void Line::fit (Line **array, int n)
{
	m_lineImpl->fit (array, n);
}

// getters setters
void
Line::get_direction (Vector3f &dir)
{
	m_lineImpl->get_direction (dir);
}

void
Line::get_direction (double &x, double &y, double &z)
{
	m_lineImpl->get_direction (x, y, z);
}

void
Line::get_point (Vector3f &pt)
{
	m_lineImpl->get_point (pt);
}

void
Line::get_point (double &x, double &y, double &z)
{
	m_lineImpl->get_point (x, y, z);
}

/**
* Colinearity
*/
bool
Line::is_colinear_with (Line* line, float epsilon)
{
	Vector3f dir1, dir2;
	this->get_direction (dir1);
	line->get_direction (dir2);
	//float tmp = (dir1 * dir2);
	float tmp = (dir1.x*dir2.x + dir1.y*dir2.y + dir1.z*dir2.z);
	return (fabs (1.0 - tmp) < epsilon || fabs (-1.0 - tmp) < epsilon)? true : false;
}

/**
* position
*/
int
Line::get_position_with (Line* line)
{
	return m_lineImpl->get_position_with (line);
}

/**
* distances
*/
double
Line::distance_with (Line &line)
{
	return m_lineImpl->distance_with (line);
}

double
Line::distance_with (Vector3f &pt)
{
	return m_lineImpl->distance_with (pt);
}

void
Line::closest_point (Vector3f &pt, Vector3f &pt_on_line)
{
	m_lineImpl->closest_point (pt_on_line, pt);
}

void
Line::init_shortest_distance (Line* line1, Line* line2)
{
	m_lineImpl->init_shortest_distance (line1, line2);
}

/* Projections on planes */
void
Line::projection_on_oxy (void)
{
	m_lineImpl->projection_on_oxy ();
}

/**
* Dump the current implementation used
*/
void
Line::dump (void)
{
	m_lineImpl->dump ();
}

