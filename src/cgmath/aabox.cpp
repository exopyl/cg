#include <stdio.h>
#include <assert.h>
#include <iostream>

#include "aabox.h"
#include "geometry.h"

void AABox::Dump (void)
{
	std::cout << parameters[0].x << " " << parameters[0].y << " " << parameters[0].z << std::endl;
	std::cout << parameters[1].x << " " << parameters[1].y << " " << parameters[1].z << std::endl;
}

void AABox::AddVertex (float x, float y, float z)
{
	if (parameters[0].x > x)
			parameters[0].x = x;
	if (parameters[0].y > y)
			parameters[0].y = y;
	if (parameters[0].z > z)
			parameters[0].z = z;

	if (parameters[1].x < x)
			parameters[1].x = x;
	if (parameters[1].y < y)
			parameters[1].y = y;
	if (parameters[1].z < z)
			parameters[1].z = z;
}

bool AABox::contains (float x, float y, float z) const
{
	return ((x >= parameters[0].x) && (x <= parameters[1].x) &&
			(y >= parameters[0].y) && (y <= parameters[1].y) &&
			(z >= parameters[0].z) && (z <= parameters[1].z));
}

/*
 * Ray-box intersection using IEEE numerical properties to ensure that the
 * test is both robust and efficient, as described in:
 *
 *      Amy Williams, Steve Barrus, R. Keith Morley, and Peter Shirley
 *      "An Efficient and Robust Ray-Box Intersection Algorithm"
 *      Journal of graphics tools, 10(1):49-54, 2005
 *
 */
bool AABox::intersection (const Ray &r, float t0, float t1) const
{
	float tmin, tmax, tymin, tymax, tzmin, tzmax;

	tmin = (parameters[r.sign[0]].x - r.origin.x) * r.inv_direction.x;
	tmax = (parameters[1-r.sign[0]].x - r.origin.x) * r.inv_direction.x;
	tymin = (parameters[r.sign[1]].y - r.origin.y) * r.inv_direction.y;
	tymax = (parameters[1-r.sign[1]].y - r.origin.y) * r.inv_direction.y;
	if ( (tmin > tymax) || (tymin > tmax) ) 
		return false;
	if (tymin > tmin)
		tmin = tymin;
	if (tymax < tmax)
		tmax = tymax;
	tzmin = (parameters[r.sign[2]].z - r.origin.z) * r.inv_direction.z;
	tzmax = (parameters[1-r.sign[2]].z - r.origin.z) * r.inv_direction.z;
	if ( (tmin > tzmax) || (tzmin > tmax) ) 
		return false;
	if (tzmin > tmin)
		tmin = tzmin;
	if (tzmax < tmax)
		tmax = tzmax;
	return ( (tmin < t1) && (tmax > t0) );
}

bool AABox::contains (const Triangle &tri) const
{
	return (contains(tri.m_v[0][0], tri.m_v[0][1], tri.m_v[0][2]) && 
			contains(tri.m_v[1][0], tri.m_v[1][1], tri.m_v[1][2]) &&
			contains(tri.m_v[2][0], tri.m_v[2][1], tri.m_v[2][2]));
}

//
// References :
// http://fileadmin.cs.lth.se/cs/Personal/Tomas_Akenine-Moller/pubs/tribox.pdf
// http://fileadmin.cs.lth.se/cs/Personal/Tomas_Akenine-Moller/code/
//
#define X 0
#define Y 1
#define Z 2

#define CROSS(dest,v1,v2) \
          dest[0]=v1[1]*v2[2]-v1[2]*v2[1]; \
          dest[1]=v1[2]*v2[0]-v1[0]*v2[2]; \
          dest[2]=v1[0]*v2[1]-v1[1]*v2[0]; 

#define DOT(v1,v2) (v1[0]*v2[0]+v1[1]*v2[1]+v1[2]*v2[2])

#define FINDMINMAX(x0,x1,x2,min,max) \
  min = max = x0;   \
  if(x1<min) min=x1;\
  if(x1>max) max=x1;\
  if(x2<min) min=x2;\
  if(x2>max) max=x2;

static int planeBoxOverlap(float normal[3], float vert[3], float maxbox[3])
{
  int q;
  float vmin[3],vmax[3],v;
  for(q=X;q<=Z;q++)
  {
    v=vert[q];
    if(normal[q]>0.0f)
    {
      vmin[q]=-maxbox[q] - v;
      vmax[q]= maxbox[q] - v;
    }
    else
    {
      vmin[q]= maxbox[q] - v;
      vmax[q]=-maxbox[q] - v;
    }
  }
  if(DOT(normal,vmin)>0.0f) return 0;
  if(DOT(normal,vmax)>=0.0f) return 1;
  
  return 0;
}


/*======================== X-tests ========================*/
#define AXISTEST_X01(a, b, fa, fb)			   \
	p0 = a*v0[Y] - b*v0[Z];			       	   \
	p2 = a*v2[Y] - b*v2[Z];			       	   \
        if(p0<p2) {min=p0; max=p2;} else {min=p2; max=p0;} \
	rad = fa * boxhalfsize[Y] + fb * boxhalfsize[Z];   \
	if(min>rad || max<-rad) return 0;

#define AXISTEST_X2(a, b, fa, fb)			   \
	p0 = a*v0[Y] - b*v0[Z];			           \
	p1 = a*v1[Y] - b*v1[Z];			       	   \
        if(p0<p1) {min=p0; max=p1;} else {min=p1; max=p0;} \
	rad = fa * boxhalfsize[Y] + fb * boxhalfsize[Z];   \
	if(min>rad || max<-rad) return 0;

/*======================== Y-tests ========================*/
#define AXISTEST_Y02(a, b, fa, fb)			   \
	p0 = -a*v0[X] + b*v0[Z];		      	   \
	p2 = -a*v2[X] + b*v2[Z];	       	       	   \
        if(p0<p2) {min=p0; max=p2;} else {min=p2; max=p0;} \
	rad = fa * boxhalfsize[X] + fb * boxhalfsize[Z];   \
	if(min>rad || max<-rad) return 0;

#define AXISTEST_Y1(a, b, fa, fb)			   \
	p0 = -a*v0[X] + b*v0[Z];		      	   \
	p1 = -a*v1[X] + b*v1[Z];	     	       	   \
        if(p0<p1) {min=p0; max=p1;} else {min=p1; max=p0;} \
	rad = fa * boxhalfsize[X] + fb * boxhalfsize[Z];   \
	if(min>rad || max<-rad) return 0;

/*======================== Z-tests ========================*/
#define AXISTEST_Z12(a, b, fa, fb)			   \
	p1 = a*v1[X] - b*v1[Y];			           \
	p2 = a*v2[X] - b*v2[Y];			       	   \
        if(p2<p1) {min=p2; max=p1;} else {min=p1; max=p2;} \
	rad = fa * boxhalfsize[X] + fb * boxhalfsize[Y];   \
	if(min>rad || max<-rad) return 0;

#define AXISTEST_Z0(a, b, fa, fb)			   \
	p0 = a*v0[X] - b*v0[Y];				   \
	p1 = a*v1[X] - b*v1[Y];			           \
        if(p0<p1) {min=p0; max=p1;} else {min=p1; max=p0;} \
	rad = fa * boxhalfsize[X] + fb * boxhalfsize[Y];   \
	if(min>rad || max<-rad) return 0;

bool AABox::intersection (Triangle &tri) const
{
	//float boxcenter[3],float boxhalfsize[3],float triverts[3][3]

  /*    use separating axis theorem to test overlap between triangle and box */
  /*    need to test for overlap in these directions: */
  /*    1) the {x,y,z}-directions (actually, since we use the AABB of the triangle */
  /*       we do not even need to test these) */
  /*    2) normal of the triangle */
  /*    3) crossproduct(edge from tri, {x,y,z}-directin) */
  /*       this gives 3x3=9 more tests */
   vec3 v0,v1,v2;
   float min,max,p0,p1,p2,rad,fex,fey,fez;
   vec3 normal,e0,e1,e2;

   // redefine the aabox
   vec3 boxcenter, boxhalfsize;
   vec3_init (boxcenter, .5*(parameters[0].x+parameters[1].x), .5*(parameters[0].y+parameters[1].y), .5*(parameters[0].z+parameters[1].z));
   vec3_init (boxhalfsize, .5*(parameters[1].x-parameters[0].x), .5*(parameters[1].y-parameters[0].y), .5*(parameters[1].z-parameters[0].z));

   /* This is the fastest branch on Sun */
   /* move everything so that the boxcenter is in (0,0,0) */
   vec3_subtraction (v0, tri.m_v[0], boxcenter);
   vec3_subtraction (v1, tri.m_v[1], boxcenter);
   vec3_subtraction (v2, tri.m_v[2], boxcenter);

   /* compute triangle edges */
   vec3_subtraction (e0,v1,v0);      /* tri edge 0 */
   vec3_subtraction (e1,v2,v1);      /* tri edge 1 */
   vec3_subtraction (e2,v0,v2);      /* tri edge 2 */

   /* Bullet 3:  */
   /*  test the 9 tests first (this was faster) */
   fex = fabsf(e0[X]);
   fey = fabsf(e0[Y]);
   fez = fabsf(e0[Z]);
   AXISTEST_X01(e0[Z], e0[Y], fez, fey);
   AXISTEST_Y02(e0[Z], e0[X], fez, fex);
   AXISTEST_Z12(e0[Y], e0[X], fey, fex);

   fex = fabsf(e1[X]);
   fey = fabsf(e1[Y]);
   fez = fabsf(e1[Z]);
   AXISTEST_X01(e1[Z], e1[Y], fez, fey);
   AXISTEST_Y02(e1[Z], e1[X], fez, fex);
   AXISTEST_Z0(e1[Y], e1[X], fey, fex);

   fex = fabsf(e2[X]);
   fey = fabsf(e2[Y]);
   fez = fabsf(e2[Z]);
   AXISTEST_X2(e2[Z], e2[Y], fez, fey);
   AXISTEST_Y1(e2[Z], e2[X], fez, fex);
   AXISTEST_Z12(e2[Y], e2[X], fey, fex);

   /* Bullet 1: */
   /*  first test overlap in the {x,y,z}-directions */
   /*  find min, max of the triangle each direction, and test for overlap in */
   /*  that direction -- this is equivalent to testing a minimal AABB around */
   /*  the triangle against the AABB */

   /* test in X-direction */
   FINDMINMAX(v0[X],v1[X],v2[X],min,max);
   if(min>boxhalfsize[X] || max<-boxhalfsize[X]) return false;

   /* test in Y-direction */
   FINDMINMAX(v0[Y],v1[Y],v2[Y],min,max);
   if(min>boxhalfsize[Y] || max<-boxhalfsize[Y]) return false;

   /* test in Z-direction */
   FINDMINMAX(v0[Z],v1[Z],v2[Z],min,max);
   if(min>boxhalfsize[Z] || max<-boxhalfsize[Z]) return false;

   /* Bullet 2: */
   /*  test if the box intersects the plane of the triangle */
   /*  compute plane equation of triangle: normal*x+d=0 */
   CROSS(normal,e0,e1);
   if(!planeBoxOverlap(normal,v0,boxhalfsize)) return false;

   return true;
}

