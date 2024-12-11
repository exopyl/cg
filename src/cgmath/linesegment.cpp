#include "linesegment.h"

/**
* Constructor
* Initializes the segment with a line defined as a LINE_POINT_DIRECTION.
* Constructor by default generates a line passing through the origin
* and with direction = (0,0,1). m_begin is set to 0 and m_end is set to 1.
*/
Segment::Segment ()
: Line (LINE_POINT_DIRECTION)
{
	m_begin = 0.0;
	m_end = 1.0;
}

/**
* getters / setters
*/
void
Segment::set_extremities (double par_begin, double par_end)
{
	m_begin = par_begin;
	m_end = par_end;
}

void
Segment::set_begin (double par_begin)
{
	m_begin = par_begin;
}

void
Segment::set_end (double par_end)
{
	m_end = par_end;
}

void
Segment::get_extremities (double &par_begin, double &par_end)
{
	par_begin = m_begin;
	par_end = m_end;
}

void
Segment::get_begin (double &par_begin)
{
	par_begin = m_begin;
}

void
Segment::get_end (double &par_end)
{
	par_end = m_end;
}

void
Segment::get_extremities (Vector3f &par_begin, Vector3f &par_end)
{
	get_point_from_position (m_begin, par_begin);
	get_point_from_position (m_end, par_end);
}

void
Segment::get_begin (Vector3f &par_begin)
{
	get_point_from_position (m_begin, par_begin);
}

void
Segment::get_end (Vector3f &par_end)
{
	get_point_from_position (m_end, par_end);
}

void
Segment::get_point_from_position (double position, Vector3f &par_point)
{
	Vector3f orig, dir;
	m_lineImpl->get_point (orig);
	m_lineImpl->get_direction (dir);
	par_point.Set ( orig.x + position * dir.x,
					orig.y + position * dir.y,
					orig.z + position * dir.z	);
}

/*
* distances
*/
double
Segment::distance_with (Vector3f &v)
{
	//assert (0);
	return 0.0;
}

// compute the closest point from the line
void
Segment::closest_point (Vector3f &pt_on_line, Vector3f &pt)
{
	assert (0);
}

// compute the closest position on the line (give the relative position with the origin)
float
Segment::closest_position (Vector3f &pt)
{
	assert (0);
	return 0.0;
}

