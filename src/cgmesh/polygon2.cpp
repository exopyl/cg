#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../cgmath/cgmath.h"
#include "polygon2.h"

// memory allocation
int Polygon2::alloc_contours (int nContours)
{
	if (m_nPoints)
		free (m_nPoints);
	if (m_pPoints)
		free (m_pPoints);

	m_nContours = nContours;

	m_nPoints = (unsigned int*) malloc (m_nContours*sizeof(unsigned int));
	memset (m_nPoints, 0, m_nContours*sizeof(unsigned int));

	m_pPoints = (float**) malloc (m_nContours*sizeof(float*));
	memset (m_pPoints, 0, m_nContours*sizeof(float*));
	
	return 0;
}

// constructor
Polygon2::Polygon2 ()
{
	m_nContours = 0;
	m_nPoints = NULL;
	m_pPoints = NULL;
}

Polygon2::Polygon2 (const Polygon2 &pol)
{
	m_nContours = 0;
	m_nPoints = NULL;
	m_pPoints = NULL;
	printf ("--> %d\n", pol.m_nContours);
	alloc_contours (pol.m_nContours);
	for (int i=0; i<m_nContours; i++)
	{
		m_pPoints[i] = NULL;
		add_contour (i, pol.m_nPoints[i], pol.m_pPoints[i]);
	}
}

Polygon2 &Polygon2::operator=(const Polygon2 &pol)
{
	alloc_contours (pol.m_nContours);
	for (int i=0; i<m_nContours; i++)
	{
		m_pPoints[i] = NULL;
		add_contour (i, pol.m_nPoints[i], pol.m_pPoints[i]);
	}
	return *this;
} 

// destructor
Polygon2::~Polygon2 ()
{
	if (m_pPoints)
	{
		for (int i=0; i<m_nContours; i++)
			free (m_pPoints[i]);
		free (m_pPoints);
	}
	if (m_nPoints)
		free (m_nPoints);
}

// input
void Polygon2::input (float *pPoints, int nPoints)
{
	alloc_contours (1);

	m_nPoints[0] = nPoints;
	m_pPoints[0] = (float*) malloc (2*nPoints*sizeof(float));
	m_pPoints[0] = (float*) memcpy (m_pPoints[0], pPoints, 2*nPoints*sizeof(float));
}

void Polygon2::input (float *x, float *y, int nPoints)
{
	alloc_contours (1);

	m_nPoints[0] = nPoints;
	m_pPoints[0] = (float*) malloc (2*nPoints*sizeof(float));
	for (int i=0; i<nPoints; i++)
	{
		m_pPoints[0][2*i]   = x[i];
		m_pPoints[0][2*i+1] = y[i];
	}
}

void Polygon2::input (Polygon2 *p, int interpolation_type, int _n)
{
	int nContours = p->m_nContours;
	if (nContours != 1)
	{
		printf ("don't manage several contours in the polygon\n");
		return;
	}
	int   nPoints  = p->m_nPoints[0];
	float *pPoints = p->m_pPoints[0];
	m_nContours = nContours;
	m_pPoints = (float**) malloc (m_nContours*sizeof(float*));
	m_pPoints[0] = (float*)malloc(2*nPoints*sizeof(float));
	float l = p->length (interpolation_type);
	int i, j;
	
	// length between two consecutive points
	float lm = l / nPoints;

	// length in the current segment
	float ls = 0.0;
	
	// length remaining before the current segment point
	float lr = 0.0;
	
	switch (interpolation_type)
	{
	case INTERPOLATION_LINEAR:
	{
		m_pPoints[0][0] = pPoints[0];
		m_pPoints[0][1] = pPoints[1];
		
		i = 1;
		j = 0;
		lr = 0.0;
		for (j=0;j<nPoints;j++)
		{
			ls = sqrt ((pPoints[2*((j+1)%nPoints)]-pPoints[2*j])*(pPoints[2*((j+1)%nPoints)]-pPoints[2*j]) +
				   (pPoints[2*((j+1)%nPoints)+1]-pPoints[2*j+1])*(pPoints[2*((j+1)%nPoints)+1]-pPoints[2*j+1]));
			int n_pts = (int)((lr+ls) / lm);
			for (int k=1; k<=n_pts; k++)
			{
				m_pPoints[0][2*i]   = pPoints[2*j]+(k*lm-lr)*(pPoints[2*((j+1)%nPoints)]-pPoints[2*j])/ls;
				m_pPoints[0][2*i+1] = pPoints[2*j+1]+(k*lm-lr)*(pPoints[2*((j+1)%nPoints)+1]-pPoints[2*j+1])/ls;
				i++;
			}
			lr += ls - n_pts*lm;
		}
	}
	break;
	default:
		break;
	}
}

// edit
float* Polygon2::add_contour (unsigned int index, unsigned int nPoints, float *pPoints)
{
	if (index < m_nContours) // update an existing contour
	{
		if (m_pPoints)
			m_pPoints[index] = (float*) realloc (m_pPoints[index], 2*nPoints*sizeof(float));
		else
			m_pPoints[index] = (float*) malloc (2*nPoints*sizeof(float));

		if (pPoints)
			memcpy (m_pPoints[index], pPoints, 2*nPoints*sizeof(float));
		m_nPoints[index] = nPoints;
	}

	if (index >= m_nContours)
	{
		m_pPoints = (float**) realloc (m_pPoints, (index+1)*sizeof(float*));
		m_nPoints = (unsigned int*) realloc (m_nPoints, (index+1)*sizeof(unsigned int));
		m_nContours = (index+1);
		m_pPoints[index] = (float*) malloc (2*nPoints*sizeof(float));
		if (pPoints)
			memcpy (m_pPoints[index], pPoints, 2*nPoints*sizeof(float));
		m_nPoints[index] = nPoints;
	}

	return m_pPoints[index];
}

int Polygon2::add_polygon2d (Polygon2 *pol)
{
	if (!pol)
		return 0;

	m_nPoints = (unsigned int*) realloc (m_nPoints, (m_nContours+pol->m_nContours)*sizeof(unsigned int));
	m_pPoints = (float**) realloc (m_pPoints, (m_nContours+pol->m_nContours)*sizeof(float*));
	for (int i=0; i<pol->m_nContours; i++)
	{
		m_pPoints[m_nContours] = NULL;
		add_contour (m_nContours, pol->m_nPoints[i], pol->m_pPoints[i]);
	}

	return 0;
}

int Polygon2::set_point (unsigned int ci, unsigned int pi, float x, float y)
{
	if (ci >= m_nContours || pi >= m_nPoints[ci])
		return -1;

	m_pPoints[ci][2*pi]   = x;
	m_pPoints[ci][2*pi+1] = y;

	return 0;
}

int Polygon2::get_point (unsigned int ci, unsigned int pi, float *x, float *y)
{
	if (ci >= m_nContours || pi >= m_nPoints[ci])
		return -1;

	*x = m_pPoints[ci][2*pi];
	*y = m_pPoints[ci][2*pi+1];

	return 0;
}

float Polygon2::length (int interpolation_type)
{
	int i;
	float l = 0.0;
	
	switch (interpolation_type)
	{
	case INTERPOLATION_LINEAR:
		for (int j=0; j<m_nContours; j++)
		{
			int nPoints = m_nPoints[j];
			float *pPoints = m_pPoints[j];
			l = sqrt((pPoints[2*(nPoints-1)]-pPoints[0])*(pPoints[2*(nPoints-1)]-pPoints[0]) +
				 (pPoints[2*(nPoints-1)+1]-pPoints[1])*(pPoints[2*(nPoints-1)+1]-pPoints[1]));
			for (i=0; i<nPoints-1; i++)
				l += sqrt((pPoints[2*(i+1)]-pPoints[2*i])*(pPoints[2*(i+1)]-pPoints[2*i]) +
					  (pPoints[2*(i+1)+1]-pPoints[2*i+1])*(pPoints[2*(i+1)+1]-pPoints[2*i+1]));
		}
		break;
	default:
		break;
	}
	
	return l;
}

void Polygon2::get_bbox (float *xmin, float *xmax, float *ymin, float *ymax)
{
	*xmin = m_pPoints[0][0];
	*xmax = m_pPoints[0][0];
	*ymin = m_pPoints[0][1];
	*ymax = m_pPoints[0][1];
	for (int j=0; j<m_nContours; j++)
	{
		int nPoints = m_nPoints[j];
		float *pPoints = m_pPoints[j];
		for (int i=0; i<nPoints; i++)
		{
			if (*xmin > pPoints[2*i])   *xmin = pPoints[2*i];
			if (*xmax < pPoints[2*i])   *xmax = pPoints[2*i];
			if (*ymin > pPoints[2*i+1]) *ymin = pPoints[2*i+1];
			if (*ymax < pPoints[2*i+1]) *ymax = pPoints[2*i+1];
		}
	}
}

//
// http://local.wasp.uwa.edu.au/~pbourke/geometry/polyarea/
//
float
Polygon2::area (void)
{
	if (m_nContours != 1)
		return 0.;

	if (m_nPoints[0] <= 2)
		return 0.;
	
	float res=0.;
	for (int i=1; i<m_nPoints[0]-1; i++)
		res += area (0, i, i+1);
	return res;
}

float
Polygon2::area (int i1, int i2, int i3)
{
	vec2 u1, u2, u3;
	vec2_init (u1, m_pPoints[0][2*i1], m_pPoints[0][2*i1+1]);
	vec2_init (u2, m_pPoints[0][2*i2], m_pPoints[0][2*i2+1]);
	vec2_init (u3, m_pPoints[0][2*i3], m_pPoints[0][2*i3+1]);

	vec3 u1u2, u1u3, w;
	vec3_init (u1u2, u2[0]-u1[0], u2[1]-u1[1], 0.);
	vec3_init (u1u3, u3[0]-u1[0], u3[1]-u1[1], 0.);
	vec3_cross_product (w, u1u2, u1u3);
	float unsigned_area = 0.5*vec3_length (w);
	float sign = (w[2] > 0.0)? 1.0 : -1.0;
	
	return sign * unsigned_area;
}

void
Polygon2::smooth (void)
{
	int i;

	for (int j=0; j<m_nContours; j++)
	{
		int nPoints = m_nPoints[j];
		float *pPoints = m_pPoints[j];

		float *t = (float*)malloc(2*nPoints*sizeof(float));
		
		// smooth x
		t[0]   = (pPoints[2*(nPoints-1)]+pPoints[1])/2.0;
		t[2*(nPoints-1)] = (pPoints[2*(nPoints-2)]+pPoints[0])/2.0;
		for (i=1; i<nPoints-1; i++)
		t[2*i] = (pPoints[2*(i-1)]+pPoints[2*(i+1)])/2.0;
		
		// smooth y
		t[1]   = (pPoints[2*(nPoints-1)+1]+pPoints[3])/2.0;
		t[2*(nPoints-1)+1] = (pPoints[2*(nPoints-2)+1]+pPoints[1])/2.0;
		for (i=1; i<nPoints-1; i++)
			t[2*i+1] = (pPoints[2*(i-1)+1]+pPoints[2*(i+1)+1])/2.0;
		
		m_pPoints[j] = (float*) memcpy (m_pPoints[j], t, 2*nPoints*sizeof(float));
		
		free (t);
	}
}

void
Polygon2::flip_x (void)
{
	for (int j=0; j<m_nContours; j++)
		for (int i=0; i<m_nPoints[j]; i++)
			m_pPoints[j][2*i] *= -1.0;
}

void
Polygon2::apply_PCA (void)
{
	int i;
	float xc = 0.0, yc = 0.0;
	float xx, xy, yy;
	
	center (&xc, &xy);
	xx = xy = yy = 0.0;
	for (int j=0; j<m_nContours; j++)
		for (i=0; i<m_nPoints[j]; i++)
		{
			xx += (m_pPoints[j][2*i]-xc)*(m_pPoints[j][2*i]-xc);
			xy += (m_pPoints[j][2*i]-xc)*(m_pPoints[j][2*i+1]-yc);
			yy += (m_pPoints[j][2*i+1]-yc)*(m_pPoints[j][2*i+1]-yc);
		}
	
	mat2 m2;
	mat2_init (m2, xx, xy, xy, yy);
	vec2 ev1, ev2, evalues;
	mat2_solve_eigensystem (m2, ev1, ev2, evalues);
	float xmax = ev1[0];
	float ymax = ev1[1];
	
  /*
  Eigensystem *es = new Eigensystem (2,
				       xx, xy,
				       xy, yy);
  es->jacobi ();
  es->sort ();
  float xmax = es->get_eigenvector(0)[0];
  float ymax = es->get_eigenvector(0)[1];
  */

	vec3 ox, v;
	vec3_init (ox, 1.0, 0.0, 0.0);
	vec3_init (v, xmax, ymax, 0.0);
	vec3_normalize (v);
	float c = vec3_dot_product (ox, v);
	float alpha = acos (c);
	if (ymax < 0.0) alpha = 3.14159 - alpha;
	
	// rotate
	float xt, yt;
	for (int j=0; j<m_nContours; j++)
		for (i=0; i<m_nPoints[j]; i++)
		{
			xt = m_pPoints[j][2*i];
			yt = m_pPoints[j][2*i+1];
			m_pPoints[j][2*i] = xt*cos(-alpha) - yt*sin(-alpha);
			m_pPoints[j][2*i+1] = xt*sin(-alpha) + yt*cos(-alpha);
		}
}

void
Polygon2::translate (float tx, float ty)
{
	for (int j=0; j<m_nContours; j++)
		for (int i=0; i<m_nPoints[j]; i++)
		{
			m_pPoints[j][2*i]   += tx;
			m_pPoints[j][2*i+1] += ty;
		}
}


void
Polygon2::rotate (float angle)
{
	float alpha = angle*3.14159/180.0; /* conversion */
	for (int j=0; j<m_nContours; j++)
		for (int i=0; i<m_nPoints[j]; i++)
		{
			float px = m_pPoints[j][2*i];
			float py = m_pPoints[j][2*i+1];
			m_pPoints[j][2*i]   = px*cos(alpha) - py*sin(alpha);
			m_pPoints[j][2*i+1] = px*sin(alpha) + py*cos(alpha);
		}
}

void
Polygon2::centerize (void)
{
	int i;
	int nPoints = 0;
	float xc = 0.0;
	float yc = 0.0;
	for (int j=0; j<m_nContours; j++)
		for (i=0; i<m_nPoints[j]; i++)
		{
			xc += m_pPoints[j][2*i];
			yc += m_pPoints[j][2*i+1];
			nPoints++;
		}
	xc /= nPoints;
	yc /= nPoints;
	for (int j=0; j<m_nContours; j++)
		for (i=0; i<m_nPoints[j]; i++)
		{
			m_pPoints[j][2*i]   -= xc;
			m_pPoints[j][2*i+1] -= yc;
		}
}

void
Polygon2::center (float *xc, float *yc)
{
	int i;
	float xcc = 0.0;
	float ycc = 0.0;
	int nPoints = 0;
	for (int j=0; j<m_nContours; j++)
		for (i=0; i<m_nPoints[j]; i++)
		{
			xcc += m_pPoints[j][2*i];
			ycc += m_pPoints[j][2*i+1];
			nPoints++;
		}
	xcc /= nPoints;
	ycc /= nPoints;
	*xc = xcc;
	*yc = ycc;
}

// 1 is the point is inside the polygon
// 0 otherwise
// reference : http://local.wasp.uwa.edu.au/~pbourke/geometry/insidepoly/
int Polygon2::is_point_inside (float x, float y)
{
	int counter = 0;
	double xinters;
	float pt[2], p1[2], p2[2];
	
	pt[0] = x;
	pt[1] = y;
	for (unsigned int j=0; j<m_nContours; j++)
	{
		p1[0] = m_pPoints[j][0];
		p1[1] = m_pPoints[j][1];
		for (unsigned int i=1; i<=m_nPoints[j]; i++)
		{
			p2[0] = m_pPoints[j][2*(i % m_nPoints[j])];
			p2[1] = m_pPoints[j][2*(i % m_nPoints[j])+1];
			if (pt[1] > MIN(p1[1],p2[1]))
			{
				if (pt[1] <= MAX(p1[1],p2[1]))
				{
					if (pt[0] <= MAX(p1[0],p2[0]))
					{
						if (p1[1] != p2[1])
						{
							xinters = (pt[1]-p1[1])*(p2[0]-p1[0])/(p2[1]-p1[1])+p1[0];
							if (p1[0] == p2[0] || pt[0] <= xinters)
								counter++;
						}
					}
				}
			}
			p1[0] = p2[0];
			p1[1] = p2[1];
		}
	}
		
	if (counter % 2 == 0)
		return 0;
	else
		return 1;
}

// order of the points
void Polygon2::inverse_order (void)
{
	int i;
	float tmp;
	
	for (int j=0; j<m_nContours; j++)
		for (i=0; i<m_nPoints[j]/2; i++)
		{
			tmp = m_pPoints[j][2*i];
			m_pPoints[j][2*i] = m_pPoints[j][2*(m_nPoints[j]-1-i)];
			m_pPoints[j][2*(m_nPoints[j]-1-i)] = tmp;
			
			tmp = m_pPoints[j][2*i+1];
			m_pPoints[j][2*i+1] = m_pPoints[j][2*(m_nPoints[j]-1-i)+1];
			m_pPoints[j][2*(m_nPoints[j]-1-i)+1] = tmp;
		}
}

int Polygon2::is_trigonometric_order (void)
{
	return (area () >= 0.0)? 1 : 0;
}

static float cotangent (vec2 a, vec2 b, vec2 c)
{
	vec3 ba, bc, tmp;
	vec3_init (ba, a[0]-b[0], a[1]-b[1], 0.);
	vec3_init (bc, c[0]-b[0], c[1]-b[1], 0.);
	vec3_cross_product(tmp, bc, ba);
	return vec3_dot_product (bc, ba)/vec3_length (tmp);
}

int Polygon2::generalized_barycentric_coordinates (float pt[2], float *coords)
{
	if (m_nContours != 1)
		return -1;

	if (coords == NULL)
		coords = (float*)malloc(m_nPoints[0]*sizeof(float));

	float weightSum = 0.;
	int n = m_nPoints[0];
	for (int i=0; i<n; i++)
	{
		int iprev = (i-1+n)%n;
		int inext = (i+1)%n;
		float pti[2], ptprev[2], ptnext[2];
		pti[0] = m_pPoints[0][2*i];
		pti[1] = m_pPoints[0][2*i+1];
		ptprev[0] = m_pPoints[0][2*iprev];
		ptprev[1] = m_pPoints[0][2*iprev+1];
		ptnext[0] = m_pPoints[0][2*inext];
		ptnext[1] = m_pPoints[0][2*inext+1];
		vec2 tmp;
		vec2_subtraction (tmp, pt, pti);
		coords[i] = (cotangent(pt, pti, ptprev) + cotangent(pt, pti, ptnext)) / vec2_length2 (tmp);
		weightSum += coords[i];
	}

	// normalize the weights
	for (int i=0; i<n; i++)
		coords[i] /= weightSum;

	return 0;
}

void Polygon2::extrude (float fHeight, char *filename)
{
  int i;
  FILE *ptr = fopen (filename, "w");

  for (int j=0; j<m_nContours; j++)
  {
	  for (i=0; i<m_nPoints[j]; i++)
		  fprintf (ptr, "v %f %f %f\n", m_pPoints[j][2*i], m_pPoints[j][2*i+1], 0.);
	  for (i=0; i<m_nPoints[j]; i++)
		  fprintf (ptr, "v %f %f %f\n", m_pPoints[j][2*i], m_pPoints[j][2*i+1], fHeight);
  }

  int vOffset = 0;
  for (int j=0; j<m_nContours; j++)
  {
	  for (i=0; i<m_nPoints[j]-1; i++)
	  {
		  fprintf (ptr, "f %d %d %d %d\n",
			   1+vOffset+i, 1+vOffset+(i+1)%m_nPoints[j],
			   1+vOffset+m_nPoints[j]+(i+1)%m_nPoints[j], 1+vOffset+m_nPoints[j]+i);
		  //fprintf (ptr, "f %d %d %d\n", 1+i, 1+i+1, 1+(m_nPoints+i+1)%(2*m_nPoints));
		  //fprintf (ptr, "f %d %d %d\n", 1+i, 1+(m_nPoints+i+1)%(2*m_nPoints), 1+m_nPoints+i);
	  }
	  vOffset += 2*m_nPoints[j];
  }

  fclose (ptr);
}


void Polygon2::thicken (Polygon2* polygon, float ithickness, float othickness, int bOpen)
{
     if (polygon == NULL)
	  return;
     
     m_nContours = polygon->m_nContours;
     m_nPoints = (unsigned int*) malloc (m_nContours*sizeof(unsigned int));
     m_pPoints = (float**) malloc (m_nContours*sizeof(float*));

     for (unsigned int j=0; j<polygon->m_nContours; j++)
     {
	  m_nPoints[j] = 2*polygon->m_nPoints[j];
	  if (!bOpen)
	       m_nPoints[j] += 2;
	  unsigned int nvnew = m_nPoints[j];
	  m_pPoints[j] = (float*)malloc(2*m_nPoints[j]*sizeof(float));
	  for (unsigned int i=0; i<polygon->m_nPoints[j]; i++)
	  {
	       vec2 pt;
	       vec2 s1, s2;
	       if (bOpen && i == 0) // first vertex
	       {
		       vec2_init (s1,
				  polygon->m_pPoints[j][2] - polygon->m_pPoints[j][0],
				  polygon->m_pPoints[j][3] - polygon->m_pPoints[j][1]);
		       vec2_normalize (s1);
		       
		       // right side
		       set_point (j, i,
				  polygon->m_pPoints[j][0] + othickness*s1[1],
				  polygon->m_pPoints[j][1] - othickness*s1[0]);
		       
		       // left side
		       set_point (j, 2*polygon->m_nPoints[j]-1,
				  polygon->m_pPoints[j][0] - ithickness*s1[1],
				  polygon->m_pPoints[j][1] + ithickness*s1[0]);
	       }
	       else if (bOpen && i == polygon->m_nPoints[j]-1) // last vertex
	       {
		       vec2_init (s1,
				  polygon->m_pPoints[j][2*i] - polygon->m_pPoints[j][2*(i-1)],
				  polygon->m_pPoints[j][2*i+1] - polygon->m_pPoints[j][2*(i-1)+1]);
		       vec2_normalize (s1);
		       
		       // right side
		       set_point (j, i,
				  polygon->m_pPoints[j][2*i] + othickness*s1[1], 
				  polygon->m_pPoints[j][2*i+1] - othickness*s1[0]);
		       
		       // left side
		       set_point (j, 2*polygon->m_nPoints[j]-1-i,
				  polygon->m_pPoints[j][2*i] - ithickness*s1[1],
				  polygon->m_pPoints[j][2*i+1] + ithickness*s1[0]);
	       }
	       else
	       {
		       seg2 seg1, seg2;
		       vec2 pt2;
		       
		       vec2 prev, current, next;
		       polygon->get_point (j, (i+polygon->m_nPoints[j]-1)%polygon->m_nPoints[j], &prev[0], &prev[1]);
		       polygon->get_point (j, i, &current[0], &current[1]);
		       polygon->get_point (j, (i+1)%polygon->m_nPoints[j], &next[0], &next[1]);

		       vec2_subtraction (s1, current, prev);
		       vec2_normalize (s1);
		       vec2_subtraction (s2, next, current);
		       vec2_normalize (s2);

		       //
		       // outside side
		       //
		       
		       // segment 1
		       vec2_init (pt, othickness*s1[1], -othickness*s1[0]);
		       
		       seg1.vs[0] = polygon->m_pPoints[j][2*((i-1+polygon->m_nPoints[j])%polygon->m_nPoints[j])] + pt[0];
		       seg1.vs[1] = polygon->m_pPoints[j][2*((i-1+polygon->m_nPoints[j])%polygon->m_nPoints[j])+1] + pt[1];
		       seg1.ve[0] = polygon->m_pPoints[j][2*i] + pt[0];
		       seg1.ve[1] = polygon->m_pPoints[j][2*i+1] + pt[1];
		    
		       // segment 2
		       vec2_init (pt, othickness*s2[1], -othickness*s2[0]);
		       
		       seg2.vs[0] = polygon->m_pPoints[j][2*i] + pt[0];
		       seg2.vs[1] = polygon->m_pPoints[j][2*i+1] + pt[1];
		       seg2.ve[0] = polygon->m_pPoints[j][2*((i+1)%polygon->m_nPoints[j])] + pt[0];
		       seg2.ve[1] = polygon->m_pPoints[j][2*((i+1)%polygon->m_nPoints[j])+1] + pt[1];
		       
		       // get intersection
		       seg2_seg2_intersection (seg1, seg2, pt2);
		       set_point (j, i, pt2[0], pt2[1]);
		       
		       //
		       // interior side
		       //
		       
		       // segment 1
		       vec2_init (pt, -ithickness*s1[1], ithickness*s1[0]);
		       
		       seg1.vs[0] = polygon->m_pPoints[j][2*((i-1+polygon->m_nPoints[j])%polygon->m_nPoints[j])] + pt[0];
		       seg1.vs[1] = polygon->m_pPoints[j][2*((i-1+polygon->m_nPoints[j])%polygon->m_nPoints[j])+1] + pt[1];
		       seg1.ve[0] = polygon->m_pPoints[j][2*i] + pt[0];
		       seg1.ve[1] = polygon->m_pPoints[j][2*i+1] + pt[1];
		       
		       // segment 2
		       vec2_init (pt, -ithickness*s2[1], ithickness*s2[0]);
		       
		       seg2.vs[0] = polygon->m_pPoints[j][2*i] + pt[0];
		       seg2.vs[1] = polygon->m_pPoints[j][2*i+1] + pt[1];
		       seg2.ve[0] = polygon->m_pPoints[j][2*((i+1)%polygon->m_nPoints[j])] + pt[0];
		       seg2.ve[1] = polygon->m_pPoints[j][2*((i+1)%polygon->m_nPoints[j])+1] + pt[1];
		       
		       // get intersection
		       seg2_seg2_intersection (seg1, seg2, pt2);
		       set_point (j, (nvnew-1-i)%(nvnew), pt2[0], pt2[1]);
	       }
	  }
	  if (!bOpen)
	  {
		  float x, y;

		  get_point (j, 0, &x, &y);
		  set_point (j, polygon->m_nPoints[j], x, y);

		  get_point (j, nvnew-1, &x, &y);
		  set_point (j, polygon->m_nPoints[j]+1, x, y);
	  }
     }
}

void Polygon2::dilate (Polygon2* polygon, float d)
{
     if (polygon == NULL)
	  return;
     
     m_nContours = polygon->m_nContours;
     m_nPoints = (unsigned int*) malloc (m_nContours*sizeof(unsigned int));
     m_pPoints = (float**) malloc (m_nContours*sizeof(float*));

     for (unsigned int j=0; j<polygon->m_nContours; j++)
     {
	  m_nPoints[j] = polygon->m_nPoints[j];
	  m_pPoints[j] = (float*)malloc(2*m_nPoints[j]*sizeof(float));
	  for (unsigned int i=0; i<polygon->m_nPoints[j]; i++)
	  {
		  seg2 seg1, seg2;
		  
		  vec2 s1, s2;
		  vec2 prev, current, next, pt;
		  polygon->get_point (j, (i+polygon->m_nPoints[j]-1)%polygon->m_nPoints[j], &prev[0], &prev[1]);
		  polygon->get_point (j, i, &current[0], &current[1]);
		  polygon->get_point (j, (i+1)%polygon->m_nPoints[j], &next[0], &next[1]);
		  
		  vec2_subtraction (s1, current, prev);
		  vec2_normalize (s1);
		  vec2_subtraction (s2, next, current);
		  vec2_normalize (s2);
		  
		  // segment 1
		  vec2_init (pt, d*s1[1], -d*s1[0]);
		  
		  seg1.vs[0] = polygon->m_pPoints[j][2*((i-1+polygon->m_nPoints[j])%polygon->m_nPoints[j])] + pt[0];
		  seg1.vs[1] = polygon->m_pPoints[j][2*((i-1+polygon->m_nPoints[j])%polygon->m_nPoints[j])+1] + pt[1];
		  seg1.ve[0] = polygon->m_pPoints[j][2*i] + pt[0];
		  seg1.ve[1] = polygon->m_pPoints[j][2*i+1] + pt[1];
		  
		  // segment 2
		  vec2_init (pt, d*s2[1], -d*s2[0]);
		  
		  seg2.vs[0] = polygon->m_pPoints[j][2*i] + pt[0];
		  seg2.vs[1] = polygon->m_pPoints[j][2*i+1] + pt[1];
		  seg2.ve[0] = polygon->m_pPoints[j][2*((i+1)%polygon->m_nPoints[j])] + pt[0];
		  seg2.ve[1] = polygon->m_pPoints[j][2*((i+1)%polygon->m_nPoints[j])+1] + pt[1];
		  
		  // get intersection
		  seg2_seg2_intersection (seg1, seg2, pt);
		  set_point (j, i, pt[0], pt[1]);
	  }
     }
}

//
// moments
//
// References :
// - "On the Calculation of Moments of Polygons" http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.29.8765&rep=rep1&type=pdf
//
void Polygon2::moment_0 (float m[1])
{
	if (m_nContours != 1)
		return;

	m[0] = (float)m_nPoints[0];
}

// first order moment (centroid)
// m = { m10 , m01 }
void Polygon2::moment_1 (float m[2])
{
	if (m_nContours != 1)
		return;

	float m0[1];
	moment_0 (m0);

	m[0] = 0.;
	m[1] = 0.;
	for (int i=0; i<m_nPoints[0]; i++)
	{
		m[0] += m_pPoints[0][2*i];
		m[1] += m_pPoints[0][2*i+1];
	}
	m[0] /= m0[0];
	m[1] /= m0[0];
}

// second order moment
// m = { m20 , m11 , m02 }
void Polygon2::moment_2 (float m[3], bool centralized)
{
	if (m_nContours != 1)
		return;

	float m0[1];
	moment_0 (m0);

	m[0] = 0.0;
	m[1] = 0.0;
	m[2] = 0.0;
	for (int i=0; i<m_nPoints[0]; i++)
	{
		float x = m_pPoints[0][2*i];
		float y = m_pPoints[0][2*i+1];

		m[0] += x * x;
		m[1] += x * y;
		m[2] += y * y;
	}
	m[0] /= m0[0];
	m[1] /= m0[0];
	m[2] /= m0[0];

	if (centralized)
	{
		float m1[2];
		moment_1 (m1);
		
		m[0] -= m1[0]*m1[0];
		m[1] -= m1[0]*m1[1];
		m[2] -= m1[1]*m1[1];
	}
}

// m = { m30, m21, m12, m03 }
void Polygon2::moment_3 (float m[4])
{
	if (m_nContours != 1)
		return;

	m[0] = 0.0;
	m[1] = 0.0;
	m[2] = 0.0;
	m[3] = 0.0;
	for (int i=0; i<m_nPoints[0]; i++)
	{
		float x = m_pPoints[0][2*i];
		float y = m_pPoints[0][2*i+1];
		m[0] += x * x * x;
		m[1] += x * x * y;
		m[2] += x * y * y;
		m[3] += y * y * y;
	}
}
