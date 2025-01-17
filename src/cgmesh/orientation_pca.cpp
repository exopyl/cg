#include <stdlib.h>

#include "orientation_pca.h"

Cmesh_orientation_pca::Cmesh_orientation_pca (Mesh_half_edge *model)
  : Cmesh_orientation(model)
{
}

void
Cmesh_orientation_pca::compute_orientation (int id)
{
  switch (id)
    {
    case 0:
      compute_pca ();
      break;
    case 1:
      compute_pca_weighted_vertices ();
      break;
    case 2:
      compute_pca_barycenter ();
      break;
    case 3:
      compute_pca_continuous ();
      break;
    default:
      printf ("unknown method (%d)\n", id);
      break;
    }

  if (mrot[0] < 0.0)
    {
      mrot[0] *= -1.0;
      mrot[1] *= -1.0;
      mrot[2] *= -1.0;
    }

  if (mrot[4] < 0.0)
    {
      mrot[3] *= -1.0;
      mrot[4] *= -1.0;
      mrot[5] *= -1.0;
    }

  if (mrot[8] < 0.0)
    {
      mrot[6] *= -1.0;
      mrot[7] *= -1.0;
      mrot[8] *= -1.0;
    }
}

void
Cmesh_orientation_pca::compute_pca (void)
{
  int i;

  // get data
  int nv;
  float *v;
  if (mesh)
  {
	  nv = mesh->m_nVertices;
	  v = mesh->m_pVertices;
  }
  if (model3d_half_edge)
  {
	  nv = model3d_half_edge->m_nVertices;
	  v = model3d_half_edge->m_pVertices;
  }
  
  // center
  center[0] = center[1] = center[2] = 0.0;
  for (i=0; i<nv; i++)
    {
      center[0] += v[3*i];
      center[1] += v[3*i+1];
      center[2] += v[3*i+2];
    }
  center[0] /= nv;
  center[1] /= nv;
  center[2] /= nv;

  // principal axes
  double xx, yy, zz, xy, xz, yz;
  float x, y, z;
  xx = yy = zz = xy = xz = yz = 0.0;
  for (i=0; i<nv; i++)
    {
      x = v[3*i]   - center[0];
      y = v[3*i+1] - center[1];
      z = v[3*i+2] - center[2];
      xx += x*x;
      yy += y*y;
      zz += z*z;
      xy += x*y;
      xz += x*z;
      yz += y*z;
    }

  Matrix3 m3 (	xx, xy, xz,
				xy, yy, yz,
				xz, yz, zz	);
  Vector3 ev1, ev2, ev3, evalues;
  m3.SolveEigensystem (ev1, ev2, ev3, evalues);

  mrot[0] = ev1.x;
  mrot[1] = ev1.y;
  mrot[2] = ev1.z;

  mrot[3] = ev2.x;
  mrot[4] = ev2.y;
  mrot[5] = ev2.z;

  mrot[6] = ev3.x;
  mrot[7] = ev3.y;
  mrot[8] = ev3.z;
}

void
Cmesh_orientation_pca::compute_pca_weighted_vertices (void)
{
#ifdef AAA
  int   nv = model3d_half_edge->get_n_vertices ();
  float *v = model3d_half_edge->get_vertices ();
  int   nf = model3d_half_edge->get_n_faces ();
  int   *f = model3d_half_edge->get_faces ();
  float *a = (float*) malloc (nf*sizeof(float));
  float atot;
  int   **adj_f  = model3d_half_edge->get_faces_adjacent_faces ();
  int   *adj_n_f = model3d_half_edge->get_n_faces_adjacent_faces ();
  int i,j;
  
  /* compute the areas of the triangles */
  CVector3d v1, v2, v3;
  atot = 0.0;
  for (i=0; i<nf; i++)
    {
      /* get the vertices of the current face */
      v1.init (v[3*f[3*i]] - center[0], v[3*f[3*i]+1] - center[1], v[3*f[3*i]+2] - center[2]);
      v2.init (v[3*f[3*i+1]] - center[0], v[3*f[3*i+1]+1] - center[1], v[3*f[3*i+1]+2] - center[2]);
      v3.init (v[3*f[3*i+2]] - center[0], v[3*f[3*i+2]+1] - center[1], v[3*f[3*i+2]+2] - center[2]);
      
      /* store in a */
      a[i] = area_triangle (v1, v2, v3);
      atot += a[i];
    }

  /* center */
  center[0] = center[1] = center[2] = 0.0;
  for (i=0; i<nv; i++)
    {
      center[0] += v[3*i];
      center[1] += v[3*i+1];
      center[2] += v[3*i+2];
    }
  center[0] /= nv;
  center[1] /= nv;
  center[2] /= nv;

  /* principal axes */
  double xx, yy, zz, xy, xz, yz;
  float x, y, z;
  float w;
  xx = yy = zz = xy = xz = yz = 0.0;
  for (i=0; i<nv; i++)
    {
      /* compute the weight */
      w = 0.0;
      for (j=0; j<adj_n_f[i]; j++)
	  w += a[adj_f[i][j]];
      w /= (3*atot);

      x = v[3*i]   - center[0];
      y = v[3*i+1] - center[1];
      z = v[3*i+2] - center[2];
      x *= w;
      y *= w;
      z *= w;
      xx += x*x;
      yy += y*y;
      zz += z*z;
      xy += x*y;
      xz += x*z;
      yz += y*z;
    }

  free (a);
  
  Ceigensystem *es = new Ceigensystem (3,
				       xx, xy, xz,
				       xy, yy, yz,
				       xz, yz, zz);
  es->jacobi ();
  es->sort ();
  //es->dump_solution ();

  mrot[0] = es->get_eigenvector(0)[0];
  mrot[1] = es->get_eigenvector(0)[1];
  mrot[2] = es->get_eigenvector(0)[2];

  mrot[3] = es->get_eigenvector(1)[0];
  mrot[4] = es->get_eigenvector(1)[1];
  mrot[5] = es->get_eigenvector(1)[2];

  mrot[6] = es->get_eigenvector(2)[0];
  mrot[7] = es->get_eigenvector(2)[1];
  mrot[8] = es->get_eigenvector(2)[2];

  delete es;
#endif

  
  int i;

  // get the data
  int nv, nf;
  float *v;
  Face **f;
  if (mesh)
  {
	  return;
  }
  if (model3d_half_edge)
  {
	  nv = model3d_half_edge->m_nVertices;
	  v  = model3d_half_edge->m_pVertices;
	  nf = model3d_half_edge->m_nFaces;
	  f  = model3d_half_edge->m_pFaces;
  }

  // compute the areas of the triangles
  float *a = new float[nf];
  float atot = 0.0;
  vec3 v1, v2, v3;
  for (i=0; i<nf; i++)
    {
      // get the vertices of the current face
	    vec3_init (v1, v[3*f[i]->GetVertex(0)], v[3*f[i]->GetVertex(0)+1], v[3*f[i]->GetVertex(0)+2]);
	    vec3_init (v2, v[3*f[i]->GetVertex(1)], v[3*f[i]->GetVertex(1)+1], v[3*f[i]->GetVertex(1)+2]);
	    vec3_init (v3, v[3*f[i]->GetVertex(2)], v[3*f[i]->GetVertex(2)+1], v[3*f[i]->GetVertex(2)+2]);
      
	    // store the current area in a
	  a[i] = vec3_triangle_area (v1, v2, v3);
	  atot += a[i];
    }

  // compute the weights
  float *w = new float[nv];
  for (i=0; i<nv; i++)
	{
	  if (!model3d_half_edge->m_topology_ok[i])
	  {
		  w[i] = 0.0;
		  continue;
	  }

	  w[i] = 0.0;
	  Che_edge *e = model3d_half_edge->m_edges_vertex[i];
	  if (!e) continue;
	  Che_edge *e_walk = e;
	  do
	    {
          w[i] += a[e_walk->m_face];
	      
	      e_walk = e_walk->m_he_next->m_he_next->m_pair;
	    } while (e_walk && e_walk != e);

		w[i] /= (3*atot);
	}

  // center the model
  center[0] = center[1] = center[2] = 0.0;
  for (i=0; i<nv; i++)
    {
      center[0] += w[i]*v[3*i];
      center[1] += w[i]*v[3*i+1];
      center[2] += w[i]*v[3*i+2];
    }
  center[0] /= nv;
  center[1] /= nv;
  center[2] /= nv;

  // principal axes
  double xx, yy, zz, xy, xz, yz;
  float x, y, z;
  xx = yy = zz = xy = xz = yz = 0.0;
  for (i=0; i<nv; i++)
    {
      x = v[3*i]   - center[0];
      y = v[3*i+1] - center[1];
      z = v[3*i+2] - center[2];
      x *= w[i];
      y *= w[i];
      z *= w[i];
      xx += x*x;
      yy += y*y;
      zz += z*z;
      xy += x*y;
      xz += x*z;
      yz += y*z;
    }
  
  Matrix3 m3 (	xx, xy, xz,
				xy, yy, yz,
				xz, yz, zz	);
  Vector3 ev1, ev2, ev3, evalues;
  m3.SolveEigensystem (ev1, ev2, ev3, evalues);

  mrot[0] = ev1.x;
  mrot[1] = ev1.y;
  mrot[2] = ev1.z;

  mrot[3] = ev2.x;
  mrot[4] = ev2.y;
  mrot[5] = ev2.z;

  mrot[6] = ev3.x;
  mrot[7] = ev3.y;
  mrot[8] = ev3.z;

  delete[] a;
  delete[] w;

}

void
Cmesh_orientation_pca::compute_pca_barycenter (void)
{
  int i;

  // get the data
  int nv, nf;
  float *v;
  Face **f;
  if (mesh)
  {
	  nv = mesh->m_nVertices;
	  v  = mesh->m_pVertices;
	  nf = mesh->m_nFaces;
	  f  = mesh->m_pFaces;
  }
  if (model3d_half_edge)
  {
	  nv = model3d_half_edge->m_nVertices;
	  v  = model3d_half_edge->m_pVertices;
	  nf = model3d_half_edge->m_nFaces;
	  f  = model3d_half_edge->m_pFaces;
  }
  
  // compute the weights for the triangles
  float *w = new float[nf];
  float atot = 0.0;
  vec3 v1, v2, v3, vb;
  for (i=0; i<nf; i++)
    {
      // get the vertices of the current face
	    vec3_init (v1, v[3*f[i]->GetVertex(0)],   v[3*f[i]->GetVertex(0)+1],   v[3*f[i]->GetVertex(0)+2]);
	    vec3_init (v2, v[3*f[i]->GetVertex(1)], v[3*f[i]->GetVertex(1)+1], v[3*f[i]->GetVertex(1)+2]);
	    vec3_init (v3, v[3*f[i]->GetVertex(2)], v[3*f[i]->GetVertex(2)+1], v[3*f[i]->GetVertex(2)+2]);
      
      // store the current area in a
	  w[i] = vec3_triangle_area (v1, v2, v3);
	  atot += w[i];
    }
  for (i=0; i<nf; i++)
	w[i] /= atot;

  // center
  center[0] = center[1] = center[2] = 0.0;
  for (i=0; i<nf; i++)
    {
      // get the vertices of the current face
	    vec3_init (v1, v[3*f[i]->GetVertex(0)],   v[3*f[i]->GetVertex(0)+1],   v[3*f[i]->GetVertex(0)+2]);
	    vec3_init (v2, v[3*f[i]->GetVertex(1)], v[3*f[i]->GetVertex(1)+1], v[3*f[i]->GetVertex(1)+2]);
	    vec3_init (v3, v[3*f[i]->GetVertex(2)], v[3*f[i]->GetVertex(2)+1], v[3*f[i]->GetVertex(2)+2]);

      // compute the barycenter
	    vec3_barycenter (vb, v1, v2, v3);

	  // update the center
      center[0] += w[i]*vb[0];
      center[1] += w[i]*vb[1];
      center[2] += w[i]*vb[2];
    }
  center[0] /= nf;
  center[1] /= nf;
  center[2] /= nf;

  // principal axes
  double xx, yy, zz, xy, xz, yz;
  float x, y, z;
  //float s;
  xx = yy = zz = xy = xz = yz = 0.0;
  for (i=0; i<nf; i++)
    {
      /* get the vertices of the current face */
      //v1.Set (v[3*f[3*i]] - center[0], v[3*f[3*i]+1] - center[1], v[3*f[3*i]+2] - center[2]);
      //v2.Set (v[3*f[3*i+1]] - center[0], v[3*f[3*i+1]+1] - center[1], v[3*f[3*i+1]+2] - center[2]);
      //v3.Set (v[3*f[3*i+2]] - center[0], v[3*f[3*i+2]+1] - center[1], v[3*f[3*i+2]+2] - center[2]);
	    vec3_init (v1, v[3*f[i]->GetVertex(0)], v[3*f[i]->GetVertex(0)+1], v[3*f[i]->GetVertex(0)+2]);
	    vec3_init (v2, v[3*f[i]->GetVertex(1)], v[3*f[i]->GetVertex(1)+1], v[3*f[i]->GetVertex(1)+2]);
	    vec3_init (v3, v[3*f[i]->GetVertex(2)], v[3*f[i]->GetVertex(2)+1], v[3*f[i]->GetVertex(2)+2]);

      /* compute the barycenter */
	    vec3_barycenter (vb, v1, v2, v3);
     
      x = w[i]*vb[0] - center[0];
      y = w[i]*vb[1] - center[1];
      z = w[i]*vb[2] - center[2];

      xx += x*x;
      yy += y*y;
      zz += z*z;
      xy += x*y;
      xz += x*z;
      yz += y*z;
    }

  Matrix3 m3 (	xx, xy, xz,
				xy, yy, yz,
				xz, yz, zz	);
  Vector3 ev1, ev2, ev3, evalues;
  m3.SolveEigensystem (ev1, ev2, ev3, evalues);

  mrot[0] = ev1.x;
  mrot[1] = ev1.y;
  mrot[2] = ev1.z;

  mrot[3] = ev2.x;
  mrot[4] = ev2.y;
  mrot[5] = ev2.z;

  mrot[6] = ev3.x;
  mrot[7] = ev3.y;
  mrot[8] = ev3.z;

  delete[] w;
}

void
Cmesh_orientation_pca::compute_pca_continuous (void)
{
  // get the data
  int nv, nf;
  float *v;
  Face **f;
  if (mesh)
  {
	  nv = mesh->m_nVertices;
	  v  = mesh->m_pVertices;
	  nf = mesh->m_nFaces;
	  f  = mesh->m_pFaces;
  }
  if (model3d_half_edge)
  {
	  nv = model3d_half_edge->m_nVertices;
	  v  = model3d_half_edge->m_pVertices;
	  nf = model3d_half_edge->m_nFaces;
	  f  = model3d_half_edge->m_pFaces;
  }
  int i;
  
  // center
  center[0] = center[1] = center[2] = 0.0;
  for (i=0; i<nv; i++)
    {
      center[0] += v[3*i];
      center[1] += v[3*i+1];
      center[2] += v[3*i+2];
    }
  center[0] /= nv;
  center[1] /= nv;
  center[2] /= nv;

  // principal axes
  double xx, yy, zz, xy, xz, yz;
  vec3 v1, v2, v3, w1, w2;
  float a;
  xx = yy = zz = xy = xz = yz = 0.0;
  for (i=0; i<nf; i++)
    {
      // get the vertices of the current face
	    vec3_init (v1, v[3*f[i]->GetVertex(0)] - center[0], v[3*f[i]->GetVertex(0)+1] - center[1], v[3*f[i]->GetVertex(0)+2] - center[2]);
	    vec3_init (v2, v[3*f[i]->GetVertex(1)] - center[0], v[3*f[i]->GetVertex(1)+1] - center[1], v[3*f[i]->GetVertex(1)+2] - center[2]);
	    vec3_init (v3, v[3*f[i]->GetVertex(2)] - center[0], v[3*f[i]->GetVertex(2)+1] - center[1], v[3*f[i]->GetVertex(2)+2] - center[2]);

      // get the basis
	    vec3_subtraction (w1, v2, v1);
	    vec3_subtraction (w2, v3, v1);

      // compute the element of the matrix
	  a = 48 * vec3_triangle_area (v1, v2, v3);
      a = 1/a;
      xx += a * (4*w2[0]*w2[0] + 6*w1[0]*w2[0] + 4*w1[0]*w1[0]);
      yy += a * (4*w2[1]*w2[1] + 6*w1[1]*w2[1] + 4*w1[1]*w1[1]);
      zz += a * (4*w2[2]*w2[2] + 6*w1[2]*w2[2] + 4*w1[2]*w1[2]);
      xy += a * (4*w2[0]*w2[1] + 3*(w1[0]*w2[1]+w2[0]*w1[1]) + 4*w1[0]*w1[1]);
      xz += a * (4*w2[0]*w2[2] + 3*(w1[0]*w2[2]+w2[0]*w1[2]) + 4*w1[0]*w1[2]);
      yz += a * (4*w2[1]*w2[2] + 3*(w1[1]*w2[2]+w2[1]*w1[2]) + 4*w1[1]*w1[2]);
    }

  Matrix3 m3 (	xx, xy, xz,
				xy, yy, yz,
				xz, yz, zz	);
  Vector3 ev1, ev2, ev3, evalues;
  m3.SolveEigensystem (ev1, ev2, ev3, evalues);

  mrot[0] = ev1[0];
  mrot[1] = ev1[1];
  mrot[2] = ev1[2];

  mrot[3] = ev2[0];
  mrot[4] = ev2[1];
  mrot[5] = ev2[2];

  mrot[6] = ev3[0];
  mrot[7] = ev3[1];
  mrot[8] = ev3[2];
}

