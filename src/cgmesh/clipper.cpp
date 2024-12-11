#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

#include "clipper.h"

Cmodel3d_half_edge_clipper::Cmodel3d_half_edge_clipper (Mesh_half_edge *_mesh)
{
  assert (_mesh);
  model = _mesh;
  
  n = Vector3d (0.0, 0.0, 1.0);
  d = 0.0;

  distances = (float*)malloc(model->m_nVertices*sizeof(float));
  assert (distances);
}

Cmodel3d_half_edge_clipper::~Cmodel3d_half_edge_clipper ()
{
  if (distances) free (distances);
}

void
Cmodel3d_half_edge_clipper::set_plane (Vector3d pt, Vector3d _n)
{
  _n.Normalize ();
  n = _n;
  d = - (n * pt);
}

void
Cmodel3d_half_edge_clipper::get_intersections (int *n_intersections, int **n_vertices, float ***intersections)
{
  int i, iwalk, nv = model->m_nVertices, nf = model->m_nFaces;
  Face **f = model->m_pFaces;
  float *v = model->m_pVertices;

  int n_intersections_max = 100;
  int n_vertices_max = 2048;

  int current_n_intersections = 0;
  int *current_n_vertices = (int*)malloc(n_intersections_max*sizeof(int));
  for (i=0; i<n_intersections_max; i++) current_n_vertices[i] = 0;
  float **current_intersections = (float**)malloc(n_intersections_max*sizeof(float*));

  /* compute the distances betwen the vertices and the plane */
  Vector3d v_walk;
  for (i=0; i<nv; i++)
    {
      v_walk.Set (v[3*i], v[3*i+1], v[3*i+2]);
      distances[i] = (v_walk * n) + d;
    }

  /* check if there is an intersection */
  int negative = 0;
  int positive = 0;
  for (i=0; i<nv; i++)
    {
      if (distances[i] < 0.00001) negative++;
      if (distances[i] > 0.00001) positive++;
      if (fabs(distances[i]) < 0.00001) distances[i] = 0.0;
    }
  if (negative == 0 || positive == 0)
    {
      *n_intersections = 0;
      n_vertices = NULL;
      intersections = NULL;
      return;
    }

  /* check all the edges to find the intersections */
  int *visited_faces = (int*)malloc(nf*sizeof(int));
  assert (visited_faces);
  for (i=0; i<nf; i++) visited_faces[i] = 0;

  Che_edge *e, *e_walk;
  int a,b,c;
  for (i=0; i<nf; i++)
    {
      if (visited_faces[i]) continue;
      visited_faces[i] = 1;

      /* is there an intersection between the plane and the current face ? */
      if (distances[f[i]->GetVertex(0)] * distances[f[i]->GetVertex(1)] > 0.0 &&
	  distances[f[i]->GetVertex(1)] * distances[f[i]->GetVertex(2)] > 0.0)
	continue; // no intersection

      iwalk = 0;
      current_intersections[current_n_intersections] = (float*)malloc(n_vertices_max*sizeof(float));

      /* look for the first edges of the intersection */
      e = model->m_edges_face[i];
      a = e->m_v_begin;
      b = e->m_v_end;
      c = e->m_he_next->m_v_end;

      if (distances[a] * distances[b] <= 0)
	{
	  if (distances[b] * distances[c] <= 0)
	    e_walk = e->m_he_next->m_pair;
	  else
	    e_walk = e->m_he_next->m_he_next->m_pair;
	}
      else
	{
	  e = e->m_he_next;
	  e_walk = e->m_he_next->m_pair;
	}
      if (!e_walk) continue;

      get_vertex_intersection (e->m_v_begin, e->m_v_end, v_walk);
      current_intersections[current_n_intersections][3*iwalk]   = v_walk.x;
      current_intersections[current_n_intersections][3*iwalk+1] = v_walk.y;
      current_intersections[current_n_intersections][3*iwalk+2] = v_walk.z;
      iwalk++;

      /* look for the complete intersection */
      do
	{
	  visited_faces[e_walk->m_face] = 1;

	  /* next vertex in the intersection */
	  get_vertex_intersection (e_walk->m_v_begin, e_walk->m_v_end, v_walk);
	  current_intersections[current_n_intersections][3*iwalk]   = v_walk.x;
	  current_intersections[current_n_intersections][3*iwalk+1] = v_walk.y;
	  current_intersections[current_n_intersections][3*iwalk+2] = v_walk.z;
	  iwalk++;

	  /* go to the next intersected edge */
	  a = e_walk->m_he_next->m_v_begin;
	  b = e_walk->m_he_next->m_v_end;
	  c = e_walk->m_he_next->m_he_next->m_v_end;
	  if (distances[a] * distances[b] <= 0)
	    e_walk = e_walk->m_he_next;
	  else
	    e_walk = e_walk->m_he_next->m_he_next;
	  assert (e_walk);
	  e_walk = e_walk->m_pair;
	} while (e_walk && e_walk != e);
      current_n_vertices[current_n_intersections] = iwalk;
      current_n_intersections++;
    }

  free (visited_faces);

  *n_intersections = current_n_intersections;
  *n_vertices = current_n_vertices;
  *intersections = current_intersections;
}

void
Cmodel3d_half_edge_clipper::get_vertex_intersection (int i, int j, Vector3d &inter)
{
  float *v = model->m_pVertices;
  float t = distances[i] / (distances[i] - distances[j]);
  inter.Set ((1.0 - t) * v[3*i]   + t * v[3*j],
	      (1.0 - t) * v[3*i+1] + t * v[3*j+1],
	      (1.0 - t) * v[3*i+2] + t * v[3*j+2]);
}

