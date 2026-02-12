#pragma once

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "TVector3.h"

#include "lineintersect_utils.h"

enum line_type {LINE_POINT_DIRECTION, LINE_PLUECKER};

class Line;

/**
* class LineImpl
*/
class LineImpl
{
public:

	// fit
	virtual void fit (Vector3f *array, int n) = 0;
	virtual void fit (Line **array, int n) = 0;

	// getters setters
	virtual void get_direction (Vector3f &dir) = 0;
	virtual void get_direction (double &x, double &y, double &z) = 0;
	virtual void get_point (Vector3f &pt) = 0;
	virtual void get_point (double &x, double &y, double &z) = 0;

	// Projections on planes
	virtual void projection_on_oxy (void) = 0;

	// position
	virtual int get_position_with (Line* line) = 0;

	// distances
	virtual double distance_with (Line &line) = 0;
	virtual double distance_with (Vector3f &v) = 0;
	virtual	void   closest_point (Vector3f &pt, Vector3f &pt_on_line) = 0;
	virtual void   init_shortest_distance (Line* line1, Line* line2) = 0;

	// dump
	virtual void dump (void) = 0;

protected:
	LineImpl (){};
};

class LineImplPointDirection;
class LineImplPluecker;

/**
* Implement the line with a point and a direction
*/
class LineImplPointDirection : public LineImpl
{
	friend class LineImplPluecker;

public:

	// constructors & destructor
	LineImplPointDirection ();
	LineImplPointDirection (double par_pt_x, double par_pt_y, double par_pt_z,
							double par_dir_x, double par_dir_y, double par_dir_z);
	LineImplPointDirection (Vector3f par_pt, Vector3f par_dir);
	LineImplPointDirection (const LineImplPluecker &par_linePluecker);
	~LineImplPointDirection ();

	// fit
	virtual void fit (Vector3f *array, int n);
	virtual void fit (Line **lines, int n);

	// getters setters
	virtual void get_direction (Vector3f &dir);
	virtual void get_direction (double &x, double &y, double &z);
	virtual void get_point (Vector3f &pt);
	virtual void get_point (double &x, double &y, double &z);

	// Projections on planes
	virtual void projection_on_oxy (void);

	// position
	virtual int get_position_with (Line* line);

	// distances
	virtual double distance_with (Line &line);
	virtual double distance_with (Vector3f &pt);
	virtual	void   closest_point (Vector3f &pt, Vector3f &pt_on_line);
	virtual void   init_shortest_distance (Line* line1, Line* line2);

	// dump
	virtual void dump (void);

private:
	Vector3f m_pt, m_dir;
};

/**
* Implement the rotation with Pluecker coordinates
*
*/
class LineImplPluecker : public LineImpl
{
	friend class LineImplPointDirection;

public:

	// constructors & destructor
	LineImplPluecker ();
	LineImplPluecker (double par_l1, double par_l2, double par_l3,
					  double par_l4, double par_l5, double par_l6);
	LineImplPluecker (const LineImplPointDirection &par_linePointDirection);
	~LineImplPluecker ();

	// fit
	virtual void fit (Vector3f *array, int n);
	virtual void fit (Line **lines, int n);

	// getters setters
	virtual void get_direction (Vector3f &dir);
	virtual void get_direction (double &x, double &y, double &z);
	virtual void get_point (Vector3f &pt);
	virtual void get_point (double &x, double &y, double &z);

	// Projections on planes
	virtual void projection_on_oxy (void);

	// position
	virtual int get_position_with (Line* line);

	// distances
	virtual double distance_with (Line &line);
	virtual double distance_with (Vector3f &pt);
	virtual	void   closest_point (Vector3f &pt, Vector3f &pt_on_line);
	virtual void   init_shortest_distance (Line* line1, Line* line2);

	// dump
	virtual void dump (void);

private:
	double m_l1, m_l2, m_l3, m_l4, m_l5, m_l6;
};


/**
* class Cline
*/
class Line
{
	friend class LineImplPointDirection;
	friend class LineImplPluecker;

public:
	/**
	*	@name Constructor
	*/
	//@{
	Line (line_type par_type = LINE_POINT_DIRECTION);//!< constructor
	//@}

	/**
	*	@name Initialization
	*/
	//@{
	void init_point_direction (double pt_x, double pt_y, double pt_z,
				   double dir_x, double dir_y, double dir_z);	//!< initialize a LINE_POINT_DIRECTION
	void init_point_direction (Vector3f par_pt, Vector3f par_dir);		//!< initialize a LINE_POINT_DIRECTION
	void init_point_point (double pt1_x, double pt1_y, double pt1_z,
						   double pt2_x, double pt2_y, double pt2_z);		//!< initialize a LINE_POINT_DIRECTION
	void init_point_point (Vector3f point1, Vector3f point2);				//!< initialize a LINE_POINT_DIRECTION
	void init_pluecker (double l1, double l2, double l3,
						double l4, double l5, double l6);					//!< initialize a LINE_PLUECKER
	void init_shortest_distance (Line* line1, Line* line2);				//!< initialize the line with the shortest length between the two lines
	//@}

	/**
	*	@name Getters / setters
	*/
	//@{
	void get_point (Vector3f &pt);							//!< get a point belonging to the line
	void get_point (double &x, double &y, double &z);		//!< get a point belonging to the line
	void get_direction (Vector3f &dir);					//!< get the direction of the line
	void get_direction (double &x, double &y, double &z);	//!< get the direction of the line
	//@}

	/**
	*	@name Distances
	*/
	//@{
	double	distance_with (Line &line);	//!< distance between the line and another line
	double	distance_with (Vector3f &pt);	//!< distance between the line and a point
	void	closest_point (Vector3f &pt, Vector3f &pt_on_line); //!< compute the closest point from the line
	//@}

	/**
	*	@name Projections
	*/
	//@{
	void projection_on_oxy (void);
	//@}

	/**
	*	@name Fitting
	*/
	//@{
	void fit (Vector3d *array, int n);
	void fit (Vector3f *array, int n);
	void fit (Line **lines, int n);
	//@}

	/**
	*	@name Misc
	*/
	//@{
	int  convert (line_type par_type); //!< conversion between the different representations of a line
	bool is_colinear_with (Line* line, float epsilon); //!< colinearity
	int  get_position_with (Line* line); //!< position
	void dump (void); //!<dump
	//@}

protected:
	LineImpl* m_lineImpl;
	line_type m_line_type;
};
