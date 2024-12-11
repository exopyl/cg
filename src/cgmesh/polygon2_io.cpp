#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

#include "polygon2.h"
#include "../cgmath/cgmath.h"
#include "../cgimg/cgimg.h"

#define NPTS_QUADRIC 10
#define NPTS_CUBIC 30

///////////////////////
//
// parse a path
//
static int
_isCommand (char c)
{
	if (c == 'M' || c == 'm' ||
	    c == 'Z' || c == 'z' ||
	    c == 'L' || c == 'l' ||
	    c == 'H' || c == 'h' ||
	    c == 'V' || c == 'v' ||
	    c == 'C' || c == 'c' ||
	    c == 'S' || c == 's' ||
	    c == 'Q' || c == 'q' ||
	    c == 'T' || c == 't' ||
	    c == 'A' || c == 'a'    )
	return 1;
	else
		return 0;
}

static int _ParseParameters (char *str, int pos, float **parameters, int *nParameters)
{
	*parameters = NULL;
	*nParameters = 0;

	int i = pos, n = strlen (str);
	char* buffer = (char*)malloc(256*sizeof(char));
	int iBuf, iParam;

	float *_parameters = (float*)malloc(10*sizeof(float));

	for (i=pos, iBuf=0, iParam=0; i<n; i++)
	{
		if (i == n)
		{
			assert (0);
			*parameters = NULL;
			*nParameters = 0;
			free (buffer);
			return -1;
		}
		else if (isdigit (str[i]) || str[i] == '-' || str[i] == '.')
			buffer[iBuf++] = str[i];
		else if ( str[i] == ' ' || str[i] == ',' || str[i] == '\n' || _isCommand(str[i]) )
		{
			buffer[iBuf] = '\0';
			if (iBuf != 0)
			{
				sscanf (buffer, "%f", &_parameters[iParam]);
				iParam++;
			}
			if ( _isCommand(str[i]) )
			{
				*parameters = _parameters;
				*nParameters = iParam;
				free (buffer);
				return i;
			}
			else
			{
				iBuf=0;
			}
		}
	}
	free (_parameters);
	free (buffer);
	return -1;
}

// previous point : used for Bezier curves
static int bPreviousValid = 0;
static float Xprevious = 0.0;
static float Yprevious = 0.0;

// current point
static float Xcurrent = 0.0;
static float Ycurrent = 0.0;
static float Zcurrent = 0.0;

// point at the beginning to close the path
static float Xfirst = 0.0;
static float Yfirst = 0.0;
static float Zfirst = 0.0;

// polygon
static Polygon2 *_polygon = NULL;
static unsigned int _iCurrentContour = 0;
static unsigned int _iCurrentPoint = 0;
static int _bCountVertices = 0;

static void _AddPointToCurrentContour (float x, float y, float z)
{
	if (_bCountVertices != 1)
	{
		//dbg ("pContours[%d][%d]", _iCurrentContour, _iCurrentPoint);
		_polygon->m_pPoints[_iCurrentContour][2*_iCurrentPoint]   = x;
		_polygon->m_pPoints[_iCurrentContour][2*_iCurrentPoint+1] = y;
		//_polygon->m_pPoints[_iCurrentContour][3*_iCurrentPoint+2] = z;
	}

	_iCurrentPoint++;
	_polygon->m_nPoints[_iCurrentContour] = _iCurrentPoint;
}

static void _CloseCurrentContour (void)
{
	_iCurrentContour++;
	_polygon->m_nContours = _iCurrentContour;
	_iCurrentPoint = 0;
}

static void _ResetPolygon (void)
{
	delete _polygon;
	_polygon = NULL;
	_iCurrentContour = 0;
	_iCurrentPoint = 0;
}

// See Appendix F.6 Elliptical arc implementation notes
// http://www.w3.org/TR/SVG/implnote.html#ArcImplementationNotes
static void _Arc(float x1, float y1, float x2, float y2,
		 float rx, float ry,
		 float xrot, float fa, float fs)
{
	printf("arc %f,%f to %f,%f rx,ry=%f,%f xrot=%f fa=%f fs=%f\n",
	    x1, y1, x2, y2, rx, ry, xrot, fa, fs);

	if (x1 == x2 && y1 == y2)
		return;

	rx = fabs(rx);
	ry = fabs(ry);

	if (rx < EPSILON && ry < EPSILON) {
		_AddPointToCurrentContour (x2, y2, 0.);
		return;
	}

	// F6.5 Step 1: compute (x1prime,x2prime)
	float phi, cos_phi, sin_phi;
	phi = xrot * M_PI / 180.0f;
	sin_phi = sinf(phi);
	cos_phi = cosf(phi);

	float v1 = (x1 - x2) * 0.5f;
	float v2 = (y1 - y2) * 0.5f;

	float x1prime = cos_phi * v1 + sin_phi * v2;
	float y1prime = -sin_phi * v1 + cos_phi * v2;

	// F6.6 Step 3: ensure radii are large enough
	float rx2 = rx * rx;
	float ry2 = ry * ry;
	float x1p2 = x1prime * x1prime;
	float y1p2 = y1prime * y1prime;
	float gamma = x1p2 / rx2 + y1p2 / ry2;
	printf ("gamma = %f\n", gamma);
	if (gamma > 1.0f) {
		rx *= sqrtf(gamma);
		ry *= sqrtf(gamma);
		rx2 *= gamma;
		ry2 *= gamma;
	}

	// F6.5 Step 2: compute (cxprime, cyprime)
	float den = rx2 * y1p2 + ry2 * x1p2;
	float v3 = rx2 * ry2 / den - 1.0f;
	if (v3 < 0.0f) {
		_AddPointToCurrentContour (x2, y2, 0.);
		return;
	}

	float v4 = sqrtf(v3);
	if (fs == fa)
		v4 = -v4;

	float cxprime =  v4 * rx * y1prime / ry;
	float cyprime = -v4 * ry * x1prime / rx;

	// F6.5 Step 3: compute (cx, cy) from (cxprime, cyprime)
	float cx = cos_phi * cxprime - sin_phi * cyprime + (x1 + x2) * 0.5f;
	float cy = sin_phi * cxprime + cos_phi * cyprime + (y1 + y2) * 0.5f;

	// F6.5 Step 4: compute theta_1 and delta_theta
	v1 = (x1prime - cxprime) / rx;
	v2 = (y1prime - cyprime) / ry;
	v3 = (-x1prime - cxprime) / rx;
	v4 = (-y1prime - cyprime) / ry;

	// compute || (1,0) || || (v1,v2) ||
	float v5 = sqrtf(v1 * v1 + v2 * v2);
	if (v5 < EPSILON) {
		_AddPointToCurrentContour (x2, y2, 0.);
		return;
	}

	// compute (1,0) . (v1,v2) / (|| (1,0) || || (v1,v2) ||)
	v5 = v1 / v5;
	v5 = CLAMP(v5, -1.0f, 1.0f);
	float theta_1 = acosf(v5);
	// invert depending on sign(det((1,0),(v1,v2)))
	if (v2 < 0.0f)
		theta_1 = -theta_1;

	// compute || (v1,v2) || || (v3,v4) ||
	v5 = sqrtf((v1 * v1 + v2 * v2) * (v3 * v3 + v4 * v4)); 
	if (v5 < EPSILON) {
		_AddPointToCurrentContour (x2, y2, 0.);
		return;
	}

	// compute (v1,v2) . (v3,v4) / (|| (v1,v2) || || (v1,v2) ||)
	v5 = (v1 * v3 + v2 * v4) / v5; 
	v5 = CLAMP(v5, -1.0f, 1.0f);
	float delta_theta = acos(v5);
	// invert depending on sign(det((v1,v2),(v3,v4)))
	if (v1 * v4 - v3 * v2 < 0.0f)
		delta_theta = -delta_theta;

	if(fs == 1.0f && delta_theta < 0.0f)
		delta_theta += 2.0f * M_PI;
	if(fs == 0.0f && delta_theta > 0.0f)
		delta_theta -= 2.0f * M_PI;

	// draw segments every 5 degrees
	unsigned int nPts = (unsigned int) ceilf(fabs(360 / 5 * delta_theta / (2.0f * M_PI)));
	for (unsigned int i=1; i<=nPts; i++) {
		float theta = theta_1 + i * delta_theta / nPts;
		float xp = cx + rx * cosf(theta);
		float yp = cy + ry * sinf(theta);
		_AddPointToCurrentContour(xp, yp, 0.);
	}
}

static void _TreatCommand (char command, float *parameters, unsigned int nParameters)
{
/*
	printf ("%c (%d): \n", command, nParameters);
	for (int j=0; j<nParameters; j++)
		printf ("%f ", parameters[j]);
	printf ("\n");
*/		
	switch (command)
	{
	case 'M':
	{
		Xcurrent = parameters[0];
		Ycurrent = parameters[1];
		
		Xfirst = Xcurrent;
		Yfirst = Ycurrent;
		Zfirst = Zcurrent;

		_AddPointToCurrentContour (Xcurrent, Ycurrent, 0.);
		bPreviousValid = 0;
	}
	break;
	case 'm':
	{
		Xcurrent += parameters[0];
		Ycurrent += parameters[1];

		_AddPointToCurrentContour (Xcurrent, Ycurrent, 0.);
		bPreviousValid = 0;
	}
	break;
	case 'Z':
	case 'z':
	{
		_CloseCurrentContour ();
		bPreviousValid = 0;
	}
	break;
	case 'L':
	{
		for (unsigned int i=0; i<nParameters; i+=2)
		{
			Xcurrent = parameters[i+0];
			Ycurrent = parameters[i+1];
			
			_AddPointToCurrentContour (Xcurrent, Ycurrent, 0.);
		}
		bPreviousValid = 0;
	}
	break;
	case 'l':
	{
		for (unsigned int i=0; i<nParameters; i+=2)
		{
			Xcurrent = Xcurrent+parameters[i];
			Ycurrent = Ycurrent+parameters[i+1];
			
			_AddPointToCurrentContour (Xcurrent, Ycurrent, 0.);
		}
		bPreviousValid = 0;
	}
	break;
	case 'H':
	{
		Xcurrent = parameters[0];
		
		_AddPointToCurrentContour (Xcurrent, Ycurrent, 0.);
		bPreviousValid = 0;
	}
	break;
	case 'h':
	{
		Xcurrent = Xcurrent+parameters[0];
		
		_AddPointToCurrentContour (Xcurrent, Ycurrent, 0.);
		bPreviousValid = 0;
	}
	break;
	case 'V':
	{
		Ycurrent = parameters[0];
		
		_AddPointToCurrentContour (Xcurrent, Ycurrent, 0.);
		bPreviousValid = 0;
	}
	break;
	case 'v':
	{
		Ycurrent = Ycurrent+parameters[0];
		
		_AddPointToCurrentContour (Xcurrent, Ycurrent, 0.);
		bPreviousValid = 0;
	}
	break;
	case 'C':
	{
		CurveBezier *curve = new CurveBezier ();
		curve->addControlPoint (Xcurrent, Ycurrent, 0.);
		curve->addControlPoint (parameters[0], parameters[1], 0.);
		curve->addControlPoint (parameters[2], parameters[3], 0.);
		curve->addControlPoint (parameters[4], parameters[5], 0.);
		
		unsigned nPts = NPTS_CUBIC;
		vec3 *pts = NULL;
		unsigned int res = curve->computeInterpolation (nPts, &pts);
		if (res && pts)
		{
			for (unsigned int i=1; i<nPts; i++)
				_AddPointToCurrentContour (pts[i][0], pts[i][1], pts[i][2]);
			free (pts);
		}
		delete curve;

		Xprevious = parameters[2];
		Yprevious = parameters[3];
		Xcurrent = parameters[4];
		Ycurrent = parameters[5];
		bPreviousValid = 1;
	}
	break;
	case 'c':
	{
		CurveBezier *curve = new CurveBezier ();
		curve->addControlPoint (Xcurrent, Ycurrent, 0.);
		curve->addControlPoint (Xcurrent+parameters[0], Ycurrent+parameters[1], 0.);
		curve->addControlPoint (Xcurrent+parameters[2], Ycurrent+parameters[3], 0.);
		curve->addControlPoint (Xcurrent+parameters[4], Ycurrent+parameters[5], 0.);
		
		unsigned nPts = NPTS_CUBIC;
		vec3 *pts = NULL;
		unsigned int res = curve->computeInterpolation (nPts, &pts);
		if (res && pts)
		{
			for (unsigned int i=1; i<nPts; i++)
				_AddPointToCurrentContour (pts[i][0], pts[i][1], pts[i][2]);
			free (pts);
		}
		delete curve;

		Xprevious = Xcurrent+parameters[2];
		Yprevious = Ycurrent+parameters[3];
		Xcurrent = Xcurrent+parameters[4];
		Ycurrent = Ycurrent+parameters[5];
		bPreviousValid = 1;
	}
	break;
	case 'S':
	{
		CurveBezier *curve = new CurveBezier ();
		curve->addControlPoint (Xcurrent, Ycurrent, 0.);
		if (bPreviousValid)
			curve->addControlPoint (2 * Xcurrent - Xprevious, 2 * Ycurrent - Yprevious, 0.);
		else
			curve->addControlPoint (Xcurrent, Ycurrent, 0.);
		curve->addControlPoint (parameters[0], parameters[1], 0.);
		curve->addControlPoint (parameters[2], parameters[3], 0.);
		
		unsigned nPts = NPTS_CUBIC;
		vec3 *pts = NULL;
		unsigned int res = curve->computeInterpolation (nPts, &pts);
		if (res && pts)
		{
			for (unsigned int i=1; i<nPts; i++)
				_AddPointToCurrentContour (pts[i][0], pts[i][1], pts[i][2]);
			free (pts);
		}
		delete curve;

		Xprevious = parameters[0];
		Yprevious = parameters[1];
		Xcurrent = parameters[2];
		Ycurrent = parameters[3];
		bPreviousValid = 1;
	}
	break;
	case 's':
	{
		CurveBezier *curve = new CurveBezier ();
		curve->addControlPoint (Xcurrent, Ycurrent, 0.);
		if (bPreviousValid)
			curve->addControlPoint (2 * Xcurrent - Xprevious, 2 * Ycurrent - Yprevious, 0.);
		else
			curve->addControlPoint (Xcurrent, Ycurrent, 0.);
		curve->addControlPoint (Xcurrent+parameters[0], Ycurrent+parameters[1], 0.);
		curve->addControlPoint (Xcurrent+parameters[2], Ycurrent+parameters[3], 0.);
		
		unsigned nPts = NPTS_CUBIC;
		vec3 *pts = NULL;
		unsigned int res = curve->computeInterpolation (nPts, &pts);
		if (res && pts)
		{
			for (unsigned int i=1; i<nPts; i++)
				_AddPointToCurrentContour (pts[i][0], pts[i][1], pts[i][2]);
			free (pts);
		}
		delete curve;

		Xprevious = Xcurrent+parameters[0];
		Yprevious = Ycurrent+parameters[1];
		Xcurrent = Xcurrent+parameters[2];
		Ycurrent = Ycurrent+parameters[3];
		bPreviousValid = 1;
	}
	break;
	case 'Q':
	{
		CurveBezier *curve = new CurveBezier ();
		curve->addControlPoint (Xcurrent, Ycurrent, 0.);
		curve->addControlPoint (parameters[0], parameters[1], 0.);
		curve->addControlPoint (parameters[2], parameters[3], 0.);
		
		unsigned nPts = NPTS_QUADRIC;
		vec3 *pts;
		unsigned int res = curve->computeInterpolation (nPts+1, &pts);
		if (res)
		{
			for (unsigned int i=1; i<nPts; i++)
				_AddPointToCurrentContour (pts[i][0], pts[i][1], pts[i][2]);
		}
		free (pts);
		delete curve;

		Xprevious = parameters[0];
		Yprevious = parameters[1];
		Xcurrent = parameters[2];
		Ycurrent = parameters[3];
		bPreviousValid = 1;
	}
	break;
	case 'q':
	{
		CurveBezier *curve = new CurveBezier ();
		curve->addControlPoint (Xcurrent, Ycurrent, 0.);
		curve->addControlPoint (Xcurrent+parameters[0], Ycurrent+parameters[1], 0.);
		curve->addControlPoint (Xcurrent+parameters[2], Ycurrent+parameters[3], 0.);
		
		unsigned nPts = NPTS_QUADRIC;
		vec3 *pts;
		unsigned int res = curve->computeInterpolation (nPts+1, &pts);
		if (res)
		{
			for (unsigned int i=1; i<nPts; i++)
				_AddPointToCurrentContour (pts[i][0], pts[i][1], pts[i][2]);
		}
		free (pts);
		delete curve;
		
		Xprevious = Xcurrent+parameters[0];
		Yprevious = Ycurrent+parameters[1];
		Xcurrent = Xcurrent+parameters[2];
		Ycurrent = Ycurrent+parameters[3];
		bPreviousValid = 1;
	}
	break;
	case 'T':
	{
		if (!bPreviousValid)
		{
			Xprevious = Xcurrent;
			Yprevious = Ycurrent;
		}

		CurveBezier *curve = new CurveBezier ();
		curve->addControlPoint (Xcurrent, Ycurrent, 0.);
		if (bPreviousValid)
			curve->addControlPoint (2 * Xcurrent - Xprevious, 2 * Ycurrent - Yprevious, 0.);
		else
			curve->addControlPoint (Xcurrent, Ycurrent, 0.);
		curve->addControlPoint (parameters[0], parameters[1], 0.);
		
		unsigned nPts = (bPreviousValid)? NPTS_QUADRIC : 2;
		vec3 *pts;
		unsigned int res = curve->computeInterpolation (nPts, &pts);
		if (res)
		{
			for (unsigned int i=0; i<nPts; i++)
				_AddPointToCurrentContour (pts[i][0], pts[i][1], pts[i][2]);
		}
		free (pts);
		delete curve;

		Xprevious = 2 * Xcurrent - Xprevious;
		Yprevious = 2 * Ycurrent - Yprevious;

		Xcurrent = parameters[0];
		Ycurrent = parameters[1];
		bPreviousValid = 1;
	}
	break;
	case 't':
	{
		if (!bPreviousValid)
		{
			Xprevious = Xcurrent;
			Yprevious = Ycurrent;
		}

		CurveBezier *curve = new CurveBezier ();
		curve->addControlPoint (Xcurrent, Ycurrent, 0.);
		if (bPreviousValid)
			curve->addControlPoint (2 * Xcurrent - Xprevious, 2 * Ycurrent - Yprevious, 0.);
		else
			curve->addControlPoint (Xcurrent, Ycurrent, 0.);
		curve->addControlPoint (Xcurrent+parameters[0], Ycurrent+parameters[1], 0.);
		
		unsigned nPts = (bPreviousValid)? NPTS_QUADRIC : 2;
		vec3 *pts;
		unsigned int res = curve->computeInterpolation (nPts, &pts);
		if (res)
		{
			for (unsigned int i=0; i<nPts; i++)
				_AddPointToCurrentContour (pts[i][0], pts[i][1], pts[i][2]);
		}
		free (pts);
		delete curve;
		
		Xprevious = 2 * Xcurrent - Xprevious;
		Yprevious = 2 * Ycurrent - Yprevious;

		Xcurrent = Xcurrent+parameters[0];
		Ycurrent = Ycurrent+parameters[1];
		bPreviousValid = 1;
	}
	break;
	case 'A':
		_Arc(Xcurrent, Ycurrent, parameters[5], parameters[6],
		     parameters[0], parameters[1],
		     parameters[2], parameters[3], parameters[4]);
		Xcurrent = parameters[5];
		Ycurrent = parameters[6];
		bPreviousValid = 0;
		break;
	case 'a':
		_Arc(Xcurrent, Ycurrent, Xcurrent+parameters[5], Ycurrent+parameters[6],
		     parameters[0], parameters[1],
		     parameters[2], parameters[3], parameters[4]);
		Xcurrent = Xcurrent+parameters[5];
		Ycurrent = Ycurrent+parameters[6];
		bPreviousValid = 0;
		break;
	default:
		printf ("!!! unknown command: %c\n", command);
		break;
	}
}
//
///////////////////////

static unsigned int path_get_n_contours (char *path)
{
	unsigned int n = strlen (path);
	unsigned int nContours = 0;
	for (unsigned int i=0; i<n; i++)
	{
		if (path[i] == 'Z' || path[i] == 'z')
			nContours++;
	}
	return nContours;
}

void Polygon2::input_from_svg_path (char *filename)
{
	FILE *ptr = fopen (filename, "r");
	if (!ptr)
		printf ("couldn't open the contour file\n");

	char path[4096];
	fgets (path, 4096, ptr);
	for (int i=0; i<strlen(path); i++)
		if (path[i] == '\n')
			path[i] = '\0';
	printf ("\"%s\"\n", path);

	fclose(ptr);

	// convert a path (defined as in the svg specifications) to a polygon
	// ref : http://www.w3.org/TR/SVG/paths.html
	if (path == NULL || strlen (path) == 0)
		return;

	int i=0;

	// reset the polygon
	_ResetPolygon ();

	// create a new polygon
	_polygon = new Polygon2 ();
	_polygon->alloc_contours (path_get_n_contours (path));
	printf ("path_get_n_contours : %d\n", path_get_n_contours (path));
	
	// evaluate the number of vertices for each contour
	_bCountVertices = 1;
	i=0;
	do
	{
		if ( _isCommand (path[i]) )
		{
			char command = path[i];
			float *parameters = NULL;
			int nParameters;
			i = _ParseParameters (path, i+1, &parameters, &nParameters);
			//printf ("new command : %c %d\n", command, nParameters);
			_TreatCommand (command, parameters, nParameters);
			if (parameters)
				free (parameters);
		}
	} while (i != -1);
	_bCountVertices = 0;

	printf ("\n\n--------------------------------------------------------\n");
	// allocate memory for the contours
	for (unsigned int i=0; i<_polygon->m_nContours; i++)
	{
		printf ("contour %d : %d points\n", i, _polygon->get_n_points (i));
		_polygon->add_contour (i, _polygon->get_n_points (i), NULL);
	}
	
	// re initialize the number of vertices in each contour
	for (unsigned int i=0; i<_polygon->m_nContours; i++)
		_polygon->m_nPoints[i] = 0;
	_iCurrentContour = 0;
	_iCurrentPoint = 0;
	printf ("\n\n--------------------------------------------------------\n");

	// parse the path
	i=0;
	do
	{
		if ( _isCommand (path[i]) )
		{
			char command = path[i];
			float *parameters = NULL;
			int nParameters;
			i = _ParseParameters (path, i+1, &parameters, &nParameters);
			//printf ("new command : %c %d\n", command, nParameters);
			_TreatCommand (command, parameters, nParameters);
			if (parameters)
				free (parameters);
		}
	} while (i != -1);

	*this = *_polygon;

	// clean
	_ResetPolygon ();
}

void Polygon2::input (char *filename)
{
	if (strcmp (filename+(strlen(filename)-5), ".path") == 0)
	  return input_from_svg_path (filename);

	FILE *ptr = fopen (filename, "r");
	if (!ptr)
		printf ("couldn't open the contour file\n");
	
	int i, nPoints;
	fscanf (ptr, "# %d\n", &nPoints);
	alloc_contours (1);

	m_nPoints[0] = nPoints;
	m_pPoints[0] = (float*) malloc (2*nPoints*sizeof(float));
	for (i=0; i<nPoints; i++)
		fscanf (ptr, "%f %f\n", &m_pPoints[0][2*i], &m_pPoints[0][2*i+1]);
	fclose(ptr);
}

// dump
void Polygon2::dump (void)
{
	for (int i=0; i<m_nContours; i++)
	{
		printf ("contour %d / %d\n", i, m_nContours);
		int nPoints = m_nPoints[i];
		float *pPoints = m_pPoints[i];
		for (int j=0; j<nPoints; j++)
			printf (" %f %f\n", pPoints[2*j], pPoints[2*j+1]);
	}
}

// .dat : plot "data.dat" with filledcurve
// .pgm
void Polygon2::output (char *filename)
{
	if (strcmp (&filename[strlen(filename)-4], ".dat") == 0)
	{
		printf ("output in dat\n");
		FILE *ptr = fopen (filename, "w");
		for (int j=0; j<m_nContours; j++)
		{
			fprintf (ptr, "# %d\n", m_nPoints[j]);
			for (int i=0; i<m_nPoints[j]; i++)
				fprintf (ptr, "%f %f\n", m_pPoints[j][2*i], m_pPoints[j][2*i+1]);
		}
		fclose (ptr);
	}
	else if (strcmp (&filename[strlen(filename)-4], ".obj") == 0)
	{
		printf ("output in obj\n");
		FILE *ptr = fopen (filename, "w");
		for (int j=0; j<m_nContours; j++)
		{
			fprintf (ptr, "# %d\n", m_nPoints[j]);
			for (int i=0; i<m_nPoints[j]; i++)
				fprintf (ptr, "v %f %f %f\n", m_pPoints[j][2*i], m_pPoints[j][2*i+1], 0.);
		}
		fclose (ptr);
	}
	else if (strcmp (&filename[strlen(filename)-4], ".pgm") == 0)
	{
		float xmin, xmax, ymin, ymax;
		get_bbox (&xmin, &xmax, &ymin, &ymax);
		int w = 512;
		int h = w*(ymax-ymin)/(xmax-xmin);
		FILE *ptr = fopen (filename, "w");
		fprintf (ptr, "P2\n");
		fprintf (ptr, "%d %d\n", w, h);
		fprintf (ptr, "255\n");
		for (int j=0; j<h; j++)
			for (int i=0; i<w; i++)
			{
				float x = (float)i*(xmax-xmin)/(w-1) + xmin;
				float y = (float)j*(ymin-ymax)/(h-1) + ymax;
				fprintf (ptr, "%d\n", (is_point_inside (x, y) == 1)? 0 : 255);
			}
		fclose (ptr);
	}
	else if (strcmp (&filename[strlen(filename)-4], ".ppm") == 0)
	{
		float xmin, xmax, ymin, ymax;
		get_bbox (&xmin, &xmax, &ymin, &ymax);
		int w = 1+xmax-xmin;
		int h = 1+ymax-ymin;
		Img *img = new Img (w, h);

		for (int j=0; j<h; j++)
			for (int i=0; i<w; i++)
			{
				float x = xmin+(float)(i);
				float y = ymin+(float)(h-1-j);
				unsigned char level = (is_point_inside (x, y) == 1)? 0 : 255;
				img->set_pixel (i, j, level, level, level, 255);
			}

		unsigned int xstart, ystart, xend, yend;
		for (int j=0; j<m_nContours; j++)
		{
			for (int i=0; i<m_nPoints[j]; i++)
			{
				xstart = -xmin + m_pPoints[j][2*i];
				ystart = (-ymin + m_pPoints[j][2*i+1]);
				xend = -xmin + m_pPoints[j][2*((i+1)%m_nPoints[j])];
				yend = (-ymin + m_pPoints[j][2*((i+1)%m_nPoints[j])+1]);
				//printf ("%d %d %d %d\n", xstart, ystart, xend, yend);
				img->draw_line (xstart, h-1-ystart,
						xend, h-1-yend,
						255, 0, 0, 255);
			}
		}

		img->save (filename);
	}
}
