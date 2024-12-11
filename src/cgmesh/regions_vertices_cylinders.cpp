#include <assert.h>
#include <malloc.h>
#include <stdio.h>
#include <math.h>

#include "regions_vertices.h"

#if 0
#define __DEBUG__ 0

#define R 5.0
#define Z 100.0

/*******************/
/* compute volumes */
/*******************/
static
float _get_volume (float a, float b, float c, float theta)
{
  return -R*R*R * ( a*sin(theta) - b*cos(theta) + b )/( 3*c ) + Z*R*R*theta /2;
}

void
Cregions_vertices::init_from_cylinders (void)
{
	/*
  int i, j;
  int *indices = NULL;
  int nv = size;
  float *v  = mesh_half_edge->get_vertices ();
  float *vn = mesh_half_edge->get_vertices_normales ();
  int   *f  = mesh_half_edge->get_faces ();
  float *fn = mesh_half_edge->get_faces_normales ();
  
  myreal *volumes = (myreal*)malloc(nv*sizeof(myreal));

  for (j=0; j<nv; j++)
    {
      int n_adjacent_faces = mesh_topology->n_vertices_adjacent_faces[j];
      Vector3d tmp1, tmp2, I, J;
      myreal dotproduct;
      myreal theta;
      myreal volume;

      // initialization
      volume = 0.0f;

      Vector3d v_current (v[3*j], v[3*j+1], v[3*j+2]);
      Vector3d n_current (vn[3*j], vn[3*j+1], vn[3*j+2]);

      for (i=0; i<n_adjacent_faces; i++)
	{
	  // looking for the adjacent vertices V1 and V2 to V with V V1 and V2 is a face
	  int adjacent_face_walk = mesh_topology->vertices_adjacent_faces[j][i];
	  int index_neighbour1=-1;
	  int index_neighbour2=-1;
	  if (f[3*adjacent_face_walk] == j)
	    {
	      index_neighbour1 = f[3*adjacent_face_walk+1];
	      index_neighbour2 = f[3*adjacent_face_walk+2];
	    }
	  if (f[3*adjacent_face_walk+1] == j)
	    {
	      index_neighbour1 = f[3*adjacent_face_walk];
	      index_neighbour2 = f[3*adjacent_face_walk+2];
	    }
	  if (f[3*adjacent_face_walk+2] == j)
	    {
	      index_neighbour1 = f[3*adjacent_face_walk];
	      index_neighbour2 = f[3*adjacent_face_walk+1];
	    }
	  //printf ("   neighbours : %d %d\n" ,index_neighbour1, index_neighbour2);
	  
	  Vector3d v1 (v[3*index_neighbour1], v[3*index_neighbour1+1], v[3*index_neighbour1+2]);
	  Vector3d v2 (v[3*index_neighbour2], v[3*index_neighbour2+1], v[3*index_neighbour2+2]);
	  
	  // on determine l'angle entre VV1 et VV2
	  
	  // determination of I
	  tmp1 = v1 - v_current;
	  dotproduct = n_current * tmp1;
	  tmp2.Set (dotproduct*n_current.x, dotproduct*n_current.y, dotproduct*n_current.z);
	  I = tmp1 - tmp2;
	  I.Normalize ();
	  
	  // then J
	  tmp1 = v2 - v_current;
	  dotproduct = n_current * tmp1;
	  tmp2.Set (dotproduct*n_current.x, dotproduct*n_current.y, dotproduct*n_current.z);
	  J = tmp1 - tmp2;
	  J.Normalize ();
	  
	  // et enfin l'angle theta entre les deux vecteurs I et J
	  theta = acos (I*J);
	  //printf ("   theta = %f\n", theta);

	  // determination de la matrice de transformation
	  tmp1 = n_current ^ I;
	  //printf ("j' : %f %f %f\n", tmp1[0], tmp1[1], tmp1[2]);
	  Matrix3 m1 (I.x, tmp1.x, n_current.x,
		       I.y, tmp1.y, n_current.y,
		       I.z, tmp1.z, n_current.z);
	  m1.SetInverse ();
	  
	  // matrice contenant les composantes de la normale a la facette
	  Vector3 m2 (fn[3*adjacent_face_walk], fn[3*adjacent_face_walk+1], fn[3*adjacent_face_walk+2]);
	  
	  // on effectue la transformation
	  Vector3 res = m1 * m2;
	  //m1.mult_vector3d (m2, res);
	  tmp1.Set (res.x, res.y, res.z);
	  tmp1.Normalize ();
	  
	  // determination du volume
	  // et mise à jour
	  //printf ("%lf\n", _get_volume (tmp1[0], tmp1[1], tmp1[2], theta));
	  volume += _get_volume (tmp1.x, tmp1.y, tmp1.z, theta);
	}
      volumes[j] = volume;
    }

  for (i=0; i<nv; i++)
    datas[i] = volumes[i];
	*/
}

#endif

