#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../cgmath/cgmath.h"
#include "polygon2.h"

// memory allocation
int Polygon2::alloc_contours (int nContours)
{
	// n empty contours (each contour is a vector<Vector2f> of size 0)
	m_contours.assign (nContours, {});
	return 0;
}

// input
void Polygon2::input (float *pPoints, int nPoints)
{
	m_contours.assign (1, {});
	m_contours[0].resize (nPoints);
	if (nPoints > 0)
		memcpy ((float*)m_contours[0].data(), pPoints, 2*nPoints*sizeof(float));
}

void Polygon2::input (float *x, float *y, int nPoints)
{
	m_contours.assign (1, {});
	m_contours[0].resize (nPoints);
	for (int i=0; i<nPoints; i++)
	{
		m_contours[0][i].x = x[i];
		m_contours[0][i].y = y[i];
	}
}

void Polygon2::input (Polygon2 *p, int interpolation_type, int _n)
{
	int nContours = (int)p->m_contours.size();
	if (nContours != 1)
	{
		printf ("don't manage several contours in the polygon\n");
		return;
	}
	// take a local copy of the source contour so we stay correct even if
	// &p == this (resizing our own storage below would otherwise dangle).
	std::vector<Vector2f> src = p->m_contours[0];
	int nPoints = (int)src.size();
	if (_n <= 0) _n = nPoints;                 // 0 => keep the source resolution
	const float l = p->length (interpolation_type);

	m_contours.assign (1, {});
	m_contours[0].resize (_n);                 // exactly _n output points

	switch (interpolation_type)
	{
	case INTERPOLATION_LINEAR:
	{
		// Resample the closed contour at _n points equally spaced by arc length
		// (s_k = k * l / _n). This now HONOURS _n — previously the step used the
		// source point count, so _n was ignored and callers (matching_arkin,
		// slicer) read past the resampled array.
		const float step = (_n > 0) ? l / _n : 0.f;
		int out = 0;
		float acc = 0.f;        // arc length at the start of the current segment
		float target = 0.f;     // arc length of the next sample to place
		for (int j = 0; j < nPoints && out < _n; j++)
		{
			const int jn = (j + 1) % nPoints;
			const float ax = src[j].x,  ay = src[j].y;
			const float bx = src[jn].x, by = src[jn].y;
			const float seg = sqrt ((bx-ax)*(bx-ax) + (by-ay)*(by-ay));
			while (out < _n && target < acc + seg)
			{
				const float u = (seg > 1e-12f) ? (target - acc) / seg : 0.f;
				m_contours[0][out].x = ax + u * (bx - ax);
				m_contours[0][out].y = ay + u * (by - ay);
				out++;
				target += step;
			}
			acc += seg;
		}
		// float-rounding safety net: fill any leftover slot with the last sample
		while (out < _n)
		{
			m_contours[0][out].x = (out > 0) ? m_contours[0][out-1].x : src[0].x;
			m_contours[0][out].y = (out > 0) ? m_contours[0][out-1].y : src[0].y;
			out++;
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
	if (index >= m_contours.size())
		m_contours.resize (index+1);

	m_contours[index].resize (nPoints);
	if (pPoints)
		memcpy ((float*)m_contours[index].data(), pPoints, 2*nPoints*sizeof(float));

	return (float*)m_contours[index].data();
}

int Polygon2::add_polygon2d (Polygon2 *pol)
{
	if (!pol)
		return 0;

	for (size_t i=0; i<pol->m_contours.size(); i++)
		m_contours.push_back (pol->m_contours[i]);

	return 0;
}

int Polygon2::set_point (unsigned int ci, unsigned int pi, float x, float y)
{
	if (ci >= m_contours.size() || pi >= m_contours[ci].size())
		return -1;

	m_contours[ci][pi].x = x;
	m_contours[ci][pi].y = y;

	return 0;
}

int Polygon2::get_point (unsigned int ci, unsigned int pi, float *x, float *y)
{
	if (ci >= m_contours.size() || pi >= m_contours[ci].size())
		return -1;

	*x = m_contours[ci][pi].x;
	*y = m_contours[ci][pi].y;

	return 0;
}

float Polygon2::length (int interpolation_type)
{
	int i;
	float l = 0.0;
	
	switch (interpolation_type)
	{
	case INTERPOLATION_LINEAR:
		for (int j=0; j<(int)m_contours.size(); j++)
		{
			int nPoints = (int)m_contours[j].size();
			float *pPoints = (float*)m_contours[j].data();
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
	*xmin = m_contours[0][0].x;
	*xmax = m_contours[0][0].x;
	*ymin = m_contours[0][0].y;
	*ymax = m_contours[0][0].y;
	for (int j=0; j<(int)m_contours.size(); j++)
	{
		int nPoints = (int)m_contours[j].size();
		float *pPoints = (float*)m_contours[j].data();
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
	if (m_contours.size() != 1)
		return 0.;

	if (m_contours[0].size() <= 2)
		return 0.;

	float res=0.;
	for (int i=1; i<(int)m_contours[0].size()-1; i++)
		res += area (0, i, i+1);
	return res;
}

float
Polygon2::area (int i1, int i2, int i3)
{
	Vector2f u1, u2, u3;
	u1.Set (m_contours[0][i1].x, m_contours[0][i1].y);
	u2.Set (m_contours[0][i2].x, m_contours[0][i2].y);
	u3.Set (m_contours[0][i3].x, m_contours[0][i3].y);

	Vector3f u1u2, u1u3, w;
	u1u2.Set (u2[0]-u1[0], u2[1]-u1[1], 0.);
	u1u3.Set (u3[0]-u1[0], u3[1]-u1[1], 0.);
	w = (u1u2).CrossProduct (u1u3);
	float unsigned_area = 0.5*(w).getLength ();
	float sign = (w[2] > 0.0)? 1.0 : -1.0;
	
	return sign * unsigned_area;
}

void
Polygon2::smooth (void)
{
	int i;

	for (int j=0; j<(int)m_contours.size(); j++)
	{
		int nPoints = (int)m_contours[j].size();
		float *pPoints = (float*)m_contours[j].data();

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

		memcpy (pPoints, t, 2*nPoints*sizeof(float));

		free (t);
	}
}

void
Polygon2::flip_x (void)
{
	for (int j=0; j<(int)m_contours.size(); j++)
		for (int i=0; i<(int)m_contours[j].size(); i++)
			m_contours[j][i].x *= -1.0;
}

void
Polygon2::apply_PCA (void)
{
	int i;
	float xc = 0.0, yc = 0.0;
	float xx, xy, yy;
	
	center (&xc, &xy);
	xx = xy = yy = 0.0;
	for (int j=0; j<(int)m_contours.size(); j++)
		for (i=0; i<(int)m_contours[j].size(); i++)
		{
			xx += (m_contours[j][i].x-xc)*(m_contours[j][i].x-xc);
			xy += (m_contours[j][i].x-xc)*(m_contours[j][i].y-yc);
			yy += (m_contours[j][i].y-yc)*(m_contours[j][i].y-yc);
		}
	
	Matrix2f m2 (xx, xy, xy, yy);
	Vector2f ev1, ev2, evalues;
	m2.SolveEigensystem (ev1, ev2, evalues);
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

	Vector3f ox, v;
	ox.Set (1.0, 0.0, 0.0);
	v.Set (xmax, ymax, 0.0);
	(v).Normalize ();
	float c = (ox).DotProduct (v);
	float alpha = acos (c);
	if (ymax < 0.0) alpha = 3.14159 - alpha;
	
	// rotate
	float xt, yt;
	for (int j=0; j<(int)m_contours.size(); j++)
		for (i=0; i<(int)m_contours[j].size(); i++)
		{
			xt = m_contours[j][i].x;
			yt = m_contours[j][i].y;
			m_contours[j][i].x = xt*cos(-alpha) - yt*sin(-alpha);
			m_contours[j][i].y = xt*sin(-alpha) + yt*cos(-alpha);
		}
}

void
Polygon2::translate (float tx, float ty)
{
	for (int j=0; j<(int)m_contours.size(); j++)
		for (int i=0; i<(int)m_contours[j].size(); i++)
		{
			m_contours[j][i].x += tx;
			m_contours[j][i].y += ty;
		}
}


void
Polygon2::rotate (float angle)
{
	float alpha = angle*3.14159/180.0; /* conversion */
	for (int j=0; j<(int)m_contours.size(); j++)
		for (int i=0; i<(int)m_contours[j].size(); i++)
		{
			float px = m_contours[j][i].x;
			float py = m_contours[j][i].y;
			m_contours[j][i].x = px*cos(alpha) - py*sin(alpha);
			m_contours[j][i].y = px*sin(alpha) + py*cos(alpha);
		}
}

void
Polygon2::centerize (void)
{
	int i;
	int nPoints = 0;
	float xc = 0.0;
	float yc = 0.0;
	for (int j=0; j<(int)m_contours.size(); j++)
		for (i=0; i<(int)m_contours[j].size(); i++)
		{
			xc += m_contours[j][i].x;
			yc += m_contours[j][i].y;
			nPoints++;
		}
	xc /= nPoints;
	yc /= nPoints;
	for (int j=0; j<(int)m_contours.size(); j++)
		for (i=0; i<(int)m_contours[j].size(); i++)
		{
			m_contours[j][i].x -= xc;
			m_contours[j][i].y -= yc;
		}
}

void
Polygon2::center (float *xc, float *yc)
{
	int i;
	float xcc = 0.0;
	float ycc = 0.0;
	int nPoints = 0;
	for (int j=0; j<(int)m_contours.size(); j++)
		for (i=0; i<(int)m_contours[j].size(); i++)
		{
			xcc += m_contours[j][i].x;
			ycc += m_contours[j][i].y;
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
	for (unsigned int j=0; j<m_contours.size(); j++)
	{
		float *pPoints = (float*)m_contours[j].data();
		unsigned int nPointsJ = (unsigned int)m_contours[j].size();
		p1[0] = pPoints[0];
		p1[1] = pPoints[1];
		for (unsigned int i=1; i<=nPointsJ; i++)
		{
			p2[0] = pPoints[2*(i % nPointsJ)];
			p2[1] = pPoints[2*(i % nPointsJ)+1];
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

	for (int j=0; j<(int)m_contours.size(); j++)
	{
		float *pPoints = (float*)m_contours[j].data();
		int nP = (int)m_contours[j].size();
		for (i=0; i<nP/2; i++)
		{
			tmp = pPoints[2*i];
			pPoints[2*i] = pPoints[2*(nP-1-i)];
			pPoints[2*(nP-1-i)] = tmp;

			tmp = pPoints[2*i+1];
			pPoints[2*i+1] = pPoints[2*(nP-1-i)+1];
			pPoints[2*(nP-1-i)+1] = tmp;
		}
	}
}

int Polygon2::is_trigonometric_order (void)
{
	return (area () >= 0.0)? 1 : 0;
}

static float cotangent (const float *a, const float *b, const float *c)
{
	Vector3f ba, bc, tmp;
	ba.Set (a[0]-b[0], a[1]-b[1], 0.);
	bc.Set (c[0]-b[0], c[1]-b[1], 0.);
	tmp = bc.CrossProduct (ba);
	return (bc).DotProduct (ba)/(tmp).getLength ();
}

int Polygon2::generalized_barycentric_coordinates (float pt[2], float *coords)
{
	if (m_contours.size() != 1)
		return -1;

	if (coords == nullptr)   // caller must supply the output buffer (get_n_points(0) floats) — no hidden allocation to leak
		return -1;

	float weightSum = 0.;
	int n = (int)m_contours[0].size();
	float *pPoints = (float*)m_contours[0].data();
	for (int i=0; i<n; i++)
	{
		int iprev = (i-1+n)%n;
		int inext = (i+1)%n;
		float pti[2], ptprev[2], ptnext[2];
		pti[0] = pPoints[2*i];
		pti[1] = pPoints[2*i+1];
		ptprev[0] = pPoints[2*iprev];
		ptprev[1] = pPoints[2*iprev+1];
		ptnext[0] = pPoints[2*inext];
		ptnext[1] = pPoints[2*inext+1];
		Vector2f tmp (pt[0]-pti[0], pt[1]-pti[1]);
		coords[i] = (cotangent(pt, pti, ptprev) + cotangent(pt, pti, ptnext)) / tmp.getLength2 ();
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

  for (int j=0; j<(int)m_contours.size(); j++)
  {
	  int nP = (int)m_contours[j].size();
	  float *pPoints = (float*)m_contours[j].data();
	  for (i=0; i<nP; i++)
		  fprintf (ptr, "v %f %f %f\n", pPoints[2*i], pPoints[2*i+1], 0.);
	  for (i=0; i<nP; i++)
		  fprintf (ptr, "v %f %f %f\n", pPoints[2*i], pPoints[2*i+1], fHeight);
  }

  int vOffset = 0;
  for (int j=0; j<(int)m_contours.size(); j++)
  {
	  int nP = (int)m_contours[j].size();
	  for (i=0; i<nP-1; i++)
	  {
		  fprintf (ptr, "f %d %d %d %d\n",
			   1+vOffset+i, 1+vOffset+(i+1)%nP,
			   1+vOffset+nP+(i+1)%nP, 1+vOffset+nP+i);
	  }
	  vOffset += 2*nP;
  }

  fclose (ptr);
}


void Polygon2::thicken (Polygon2* polygon, float ithickness, float othickness, int bOpen)
{
     if (polygon == nullptr)
	  return;

     m_contours.assign (polygon->m_contours.size(), {});

     for (unsigned int j=0; j<polygon->m_contours.size(); j++)
     {
	  float        *src  = (float*)polygon->m_contours[j].data();
	  unsigned int  srcN = (unsigned int)polygon->m_contours[j].size();
	  unsigned int newN = 2*srcN;
	  if (!bOpen)
	       newN += 2;
	  unsigned int nvnew = newN;
	  m_contours[j].resize (newN);
	  for (unsigned int i=0; i<srcN; i++)
	  {
	       Vector2f pt;
	       Vector2f s1, s2;
	       if (bOpen && i == 0) // first vertex
	       {
		       s1.Set (
				  src[2] - src[0],
				  src[3] - src[1]);
		       (s1).Normalize ();
		       
		       // right side
		       set_point (j, i,
				  src[0] + othickness*s1[1],
				  src[1] - othickness*s1[0]);
		       
		       // left side
		       set_point (j, 2*srcN-1,
				  src[0] - ithickness*s1[1],
				  src[1] + ithickness*s1[0]);
	       }
	       else if (bOpen && i == srcN-1) // last vertex
	       {
		       s1.Set (
				  src[2*i] - src[2*(i-1)],
				  src[2*i+1] - src[2*(i-1)+1]);
		       (s1).Normalize ();
		       
		       // right side
		       set_point (j, i,
				  src[2*i] + othickness*s1[1], 
				  src[2*i+1] - othickness*s1[0]);
		       
		       // left side
		       set_point (j, 2*srcN-1-i,
				  src[2*i] - ithickness*s1[1],
				  src[2*i+1] + ithickness*s1[0]);
	       }
	       else
	       {
		       seg2 seg1, seg2;
		       Vector2f pt2;
		       
		       Vector2f prev, current, next;
		       polygon->get_point (j, (i+srcN-1)%srcN, &prev[0], &prev[1]);
		       polygon->get_point (j, i, &current[0], &current[1]);
		       polygon->get_point (j, (i+1)%srcN, &next[0], &next[1]);

		       s1 = current - prev;
		       (s1).Normalize ();
		       s2 = next - current;
		       (s2).Normalize ();

		       //
		       // outside side
		       //
		       
		       // segment 1
		       pt.Set (othickness*s1[1], -othickness*s1[0]);
		       
		       seg1.vs[0] = src[2*((i-1+srcN)%srcN)] + pt[0];
		       seg1.vs[1] = src[2*((i-1+srcN)%srcN)+1] + pt[1];
		       seg1.ve[0] = src[2*i] + pt[0];
		       seg1.ve[1] = src[2*i+1] + pt[1];
		    
		       // segment 2
		       pt.Set (othickness*s2[1], -othickness*s2[0]);
		       
		       seg2.vs[0] = src[2*i] + pt[0];
		       seg2.vs[1] = src[2*i+1] + pt[1];
		       seg2.ve[0] = src[2*((i+1)%srcN)] + pt[0];
		       seg2.ve[1] = src[2*((i+1)%srcN)+1] + pt[1];
		       
		       // get intersection
		       seg2_seg2_intersection (seg1, seg2, pt2);
		       set_point (j, i, pt2[0], pt2[1]);
		       
		       //
		       // interior side
		       //
		       
		       // segment 1
		       pt.Set (-ithickness*s1[1], ithickness*s1[0]);
		       
		       seg1.vs[0] = src[2*((i-1+srcN)%srcN)] + pt[0];
		       seg1.vs[1] = src[2*((i-1+srcN)%srcN)+1] + pt[1];
		       seg1.ve[0] = src[2*i] + pt[0];
		       seg1.ve[1] = src[2*i+1] + pt[1];
		       
		       // segment 2
		       pt.Set (-ithickness*s2[1], ithickness*s2[0]);
		       
		       seg2.vs[0] = src[2*i] + pt[0];
		       seg2.vs[1] = src[2*i+1] + pt[1];
		       seg2.ve[0] = src[2*((i+1)%srcN)] + pt[0];
		       seg2.ve[1] = src[2*((i+1)%srcN)+1] + pt[1];
		       
		       // get intersection
		       seg2_seg2_intersection (seg1, seg2, pt2);
		       set_point (j, (nvnew-1-i)%(nvnew), pt2[0], pt2[1]);
	       }
	  }
	  if (!bOpen)
	  {
		  float x, y;

		  get_point (j, 0, &x, &y);
		  set_point (j, srcN, x, y);

		  get_point (j, nvnew-1, &x, &y);
		  set_point (j, srcN+1, x, y);
	  }
     }
}

void Polygon2::dilate (Polygon2* polygon, float d)
{
     if (polygon == nullptr)
	  return;

     m_contours.assign (polygon->m_contours.size(), {});

     for (unsigned int j=0; j<polygon->m_contours.size(); j++)
     {
	  float        *src  = (float*)polygon->m_contours[j].data();
	  unsigned int  srcN = (unsigned int)polygon->m_contours[j].size();
	  m_contours[j].resize (srcN);
	  for (unsigned int i=0; i<srcN; i++)
	  {
		  seg2 seg1, seg2;
		  
		  Vector2f s1, s2;
		  Vector2f prev, current, next, pt;
		  polygon->get_point (j, (i+srcN-1)%srcN, &prev[0], &prev[1]);
		  polygon->get_point (j, i, &current[0], &current[1]);
		  polygon->get_point (j, (i+1)%srcN, &next[0], &next[1]);
		  
		  s1 = current - prev;
		  (s1).Normalize ();
		  s2 = next - current;
		  (s2).Normalize ();
		  
		  // segment 1
		  pt.Set (d*s1[1], -d*s1[0]);
		  
		  seg1.vs[0] = src[2*((i-1+srcN)%srcN)] + pt[0];
		  seg1.vs[1] = src[2*((i-1+srcN)%srcN)+1] + pt[1];
		  seg1.ve[0] = src[2*i] + pt[0];
		  seg1.ve[1] = src[2*i+1] + pt[1];
		  
		  // segment 2
		  pt.Set (d*s2[1], -d*s2[0]);
		  
		  seg2.vs[0] = src[2*i] + pt[0];
		  seg2.vs[1] = src[2*i+1] + pt[1];
		  seg2.ve[0] = src[2*((i+1)%srcN)] + pt[0];
		  seg2.ve[1] = src[2*((i+1)%srcN)+1] + pt[1];
		  
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
	if (m_contours.size() != 1)
		return;

	m[0] = (float)m_contours[0].size();
}

// first order moment (centroid)
// m = { m10 , m01 }
void Polygon2::moment_1 (float m[2])
{
	if (m_contours.size() != 1)
		return;

	float m0[1];
	moment_0 (m0);

	m[0] = 0.;
	m[1] = 0.;
	int n = (int)m_contours[0].size();
	float *pPoints = (float*)m_contours[0].data();
	for (int i=0; i<n; i++)
	{
		m[0] += pPoints[2*i];
		m[1] += pPoints[2*i+1];
	}
	m[0] /= m0[0];
	m[1] /= m0[0];
}

// second order moment
// m = { m20 , m11 , m02 }
void Polygon2::moment_2 (float m[3], bool centralized)
{
	if (m_contours.size() != 1)
		return;

	float m0[1];
	moment_0 (m0);

	m[0] = 0.0;
	m[1] = 0.0;
	m[2] = 0.0;
	int n = (int)m_contours[0].size();
	float *pPoints = (float*)m_contours[0].data();
	for (int i=0; i<n; i++)
	{
		float x = pPoints[2*i];
		float y = pPoints[2*i+1];

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
	if (m_contours.size() != 1)
		return;

	m[0] = 0.0;
	m[1] = 0.0;
	m[2] = 0.0;
	m[3] = 0.0;
	int n = (int)m_contours[0].size();
	float *pPoints = (float*)m_contours[0].data();
	for (int i=0; i<n; i++)
	{
		float x = pPoints[2*i];
		float y = pPoints[2*i+1];
		m[0] += x * x * x;
		m[1] += x * x * y;
		m[2] += x * y * y;
		m[3] += y * y * y;
	}
}
