#ifndef __SEGMENT_H__
#define __SEGMENT_H__

#include "line.h"
#include "common.h"

class Segment : public Line
{
public:
	Segment ();

	// initialize the line with the shortest length between the two lines
	void init_shortest_distance (Line* line1, Line* line2);

	// getters / setters
	void get_extremities	(double &par_begin, double &par_end);
	void get_begin		(double &par_begin);
	void get_end		(double &par_end);

	void get_extremities	(Vector3f &par_begin, Vector3f &par_end);
	void get_begin		(Vector3f &par_begin);
	void get_end		(Vector3f &par_end);

	void set_extremities	(double par_begin, double par_end);
	void set_begin		(double par_begin);
	void set_end		(double par_end);

	void get_point_from_position (double position, Vector3f &par_point);

	// distances
	double	distance_with (Segment &segment);	// distance between the segment and another segment
	double	distance_with (Vector3f &pt);		// distance between the segment and a point
	void	closest_point (Vector3f &pt, Vector3f &pt_on_line); // compute the closest point from the segment

	
	float closest_position (Vector3f &pt); // compute the closest position on the line (give the relative position with the origin)


private:
	double m_begin, m_end;
};

#endif /* __SEGMENT_H__ */
