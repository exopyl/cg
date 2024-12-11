#include <math.h>

#include "regions_vertices.h"

/*
 * accessibility :
 * detect ridges (regions[i] = 1)
 * and valleys (regions[i] = 2)
 */

/*
 * pseudo code presented in
 * "Unwrapping and visualizing cuneiform tablets"
 * S. E. Anderson M. Levoy
 * Stanford university
 *
 * inspired by
 * G.S.P. Miller
 * "Efficient algorithms for local and global accessibility shading"
 * computer graphics (Proc. siggraph 94)
 * ACM Press, New York, 1994, pp. 319-326
 */

void
Cregions_vertices::init_from_accessibility (int type)
{
  float *v  = mesh_half_edge->m_pVertices;
  float *vn = mesh_half_edge->m_pVertexNormals;
  float epsilon = 2.0;
  float offset  = 0.5;
  int i, j;
  
  /* initialize the regions to 0 */
  for (i=0; i<size; i++)
    {
      datas[i]   = 0;
      regions[i] = 0;
    }
  
  for (i=0; i<size; i++)
    {
      Vector3d v_walk (v[3*i], v[3*i+1], v[3*i+2]);
      Vector3d n_walk (vn[3*i], vn[3*i+1], vn[3*i+2]);
      
      switch (type)
	{
	case ACCESSIBILITY_RIDGES:
	  n_walk.x *= -1;
	  n_walk.y *= -1;
	  n_walk.z *= -1;
	  break;
	case ACCESSIBILITY_RAVINES:
	  break;
	default:
	  break;
	}

	  float r = 0.0;
      float d;

	  /*
      int *adj_vertices = mesh_topology->vertices_adjacent_vertices[i];
      int n_adj_vertices = mesh_topology->n_vertices_adjacent_vertices[i];

	 do
	{
	  r += epsilon;
	  Vector3d c (n_walk.x * (r + offset), n_walk.y* (r + offset), n_walk.z * (r + offset));
	  c = c + v_walk;
      
	  // find the closest point to c, excluding v_walk
	  //
	  // in the article, the comparison is done with all the vertices of the mesh.
	  //
	  d = 1000000.0;
	  if (0) // there, we just compare with the neighbours of the current vertex 
	    {
	      for (j=0; j<n_adj_vertices; j++)
		{
		  int id_tmp = adj_vertices[j];
		  Vector3d v_tmp (v[3*id_tmp], v[3*id_tmp+1], v[3*id_tmp+2]);
		  Vector3d ttt = c - v_tmp;
		  float d_tmp = ttt.getLength ();
		  if (d_tmp < d)
		    d = d_tmp;
		}
	    }
	  else // there, we compare with all the vertices of the mesh except v_walk
	    {
	      for (j=0; j<size; j++)
		{
		  if (i == j)
		    continue;
		  Vector3d v_tmp (v[3*j], v[3*j+1], v[3*j+2]);
		  Vector3d ttt = c - v_tmp;
		  float d_tmp = ttt.getLength ();
		  if (d_tmp < d)
		    d = d_tmp;
		}
	    }
	} while (d > r && d < 50.0);
	*/
      
      r -= epsilon;
      datas[i] = r;
      printf ("%d -> %f\n", i, datas[i]);
    }
}

void
Cregions_vertices::init_from_accessibility2 (void)
{
	/*
  float *v  = mesh_half_edge->v;
  float *vn = mesh_half_edge->vn;
  
  for (int i=0; i<size; i++)
    {
      Vector3d v_walk (v[3*i], v[3*i+1], v[3*i+2]);
      Vector3d n_walk (vn[3*i], vn[3*i+1], vn[3*i+2]);
      
      int *adj_vertices = mesh_topology->vertices_adjacent_vertices[i];
      int n_adj_vertices = mesh_topology->n_vertices_adjacent_vertices[i];
      
      Vector3d sum (0.0, 0.0, 0.0);
      for (int j=0; j<n_adj_vertices; j++)
	{
	  int id_tmp = adj_vertices[j];
	  Vector3d n_tmp (vn[3*id_tmp],
			   vn[3*id_tmp+1],
			   vn[3*id_tmp+2]);
	  sum = sum + n_tmp;
	}
      sum.x /= n_adj_vertices;
      sum.y /= n_adj_vertices;
      sum.z /= n_adj_vertices;
      float curv = 1 - (n_walk * sum);
      datas[i] = 1 - sqrt ((2-curv)*curv);
      
      regions[i] = (datas[i] > 0.7)? 0 : 1;
    }
	*/
}
