#include <stdlib.h>
#include <assert.h>
#include <math.h>

#include "regions_vertices.h"

class Cregion
{
public:
  Cregion (float _curvature, int _label = 0)
  {
    curvature = _curvature;
    label     = _label;
    previous  = NULL;
    next      = NULL;
  };
  
  float    curvature;
  Cregion *previous;
  Cregion *next;   /* descent */
  int     label;
};


void
Cregions_vertices::init_from_whitaker (void)
{
	/*
  int i,j,k,l,m;
  int label_walk;
  int last_label_for_a_minimum;
  int   nv                     = mesh_half_edge->get_n_vertices ();
  float *v                     = mesh_half_edge->get_vertices ();
  int   *vertices_n_adjacent_faces    = mesh_topology->get_n_vertices_adjacent_faces ();
  int   **vertices_adjacent_faces     = mesh_topology->get_vertices_adjacent_faces ();
  float *fn                           = mesh_topology->get_faces_normales ();
  int   *vertices_n_adjacent_vertices = mesh_topology->get_n_vertices_adjacent_vertices ();
  int   **vertices_adjacent_vertices  = mesh_topology->get_vertices_adjacent_vertices ();
  assert (v && vertices_n_adjacent_faces && vertices_adjacent_faces &&
	  fn && vertices_n_adjacent_vertices && vertices_adjacent_vertices);

  Cregion **cregions;
  cregions = (Cregion**)malloc(nv*sizeof(Cregion*));
  assert (cregions);

  // compute curvature k for each vertex by computing covariance matrix
  float C[9]={0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
  float bar[3]; // contents xbar, ybar and zbar
  int index;
  for (k=0; k<nv; k++)
    {
      //printf ("vertex %d\n", k);
      // compute xbar, ybar and zbar
      for (l=0; l<vertices_n_adjacent_faces[k]; l++)
	{
	  index = vertices_adjacent_faces[k][l];
	  for (m=0; m<3; m++)
	    bar[m] += fn[3*index+m];
	}
      for (m=0; m<3; m++)
	bar[m] /= vertices_n_adjacent_faces[k];

      // fill the variance matrix
      for (j=0; j<3; j++)
	for (i=j; i<3; i++)
	  {
	    if (i == j)
	      {
		for (l=0; l<vertices_n_adjacent_faces[k]; l++)
		  {
		    index = vertices_adjacent_faces[k][l];
		    C[3*j+i] +=
		      (fn[3*index+i] - bar[i]) *
		      (fn[3*index+i] - bar[i]);
		  }
		C[3*j+i] /= vertices_n_adjacent_faces[k];
		C[3*j+i] = sqrt (C[3*j+i]);
	      }
	    else
	      {
		for (l=0; l<vertices_n_adjacent_faces[k]; l++)
		  {
		    index = vertices_adjacent_faces[k][l];
		    C[3*j+i] +=
		      fabs(fn[3*index+i] - bar[i]) *
		      fabs(fn[3*index+j] - bar[j]);
		  }
		C[3*j+i] /= vertices_n_adjacent_faces[k];
		C[3*j+i] = sqrt (C[3*j+i]);
	      }
	  }
      C[3] = C[1];
      C[6] = C[2];
      C[7] = C[5];
      float curvature =
	C[0]*(C[4]*C[8]-C[7]*C[5]) -
	C[1]*(C[3]*C[8]-C[6]*C[5]) +
	C[2]*(C[3]*C[7]-C[6]*C[4]);
      //printf ("%f\n", curvature);
      cregions[k] = new Cregion (curvature, 0);
    }

  // find local minima and flat area
  label_walk = 1;
  for (k=0; k<nv; k++)
    {
      // local minima
      for (i=0; i<vertices_n_adjacent_vertices[k]; i++)
	{
	  index = vertices_adjacent_vertices[k][i];
	  if (cregions[index]->curvature < cregions[k]->curvature)
	    break;
	}
      if (i == vertices_n_adjacent_vertices[k]) // local minimum
	cregions[k]->label = label_walk++;
    }
  last_label_for_a_minimum = label_walk;
  //printf ("%d\n", label_walk);

  // descent

  // pre compute structure
  for (k=0; k<nv; k++)
    {
      if (vertices_n_adjacent_vertices[k] == 0)
	continue;
      else
	{
	  int index_adj_curvature_walk = k;
	  for (i=0; i<vertices_n_adjacent_vertices[k]; i++)
	    {
	      index = vertices_adjacent_vertices[k][i];
	      if (cregions[index]->curvature < cregions[index_adj_curvature_walk]->curvature)
		index_adj_curvature_walk = index;
	    }
	  cregions[k]->next = cregions[index_adj_curvature_walk];
	}
    }

  // not labelized vertices
  int modified;
  int label_current;
  do {
    modified = 0;
    for (k=0; k<nv; k++)
      {
	if (cregions[k]->label == 0)
	  {
	    Cregion *region_walk = cregions[k];
	    while (region_walk->label == 0)
	    {
	      region_walk->next->previous = region_walk;
	      region_walk = region_walk->next;
	    }
	    label_current = region_walk->label;
	    while (region_walk != NULL)
	      {
		region_walk->label = label_current;
		region_walk = region_walk->previous;
	      }
	    modified = 1;
	  }
      }
  } while (modified == 1);

  // fill the array regions
  for (i=0; i<nv; i++)
    regions[i] = cregions[i]->label;
  //for (i=0; i<n_vertices; i++)
  //printf ("%d\n", regions[i]);

  // color vertices
  float *colors = mesh_topology->get_vertices_colors ();
  for (i=0; i<nv; i++)
    {
      colors[3*i]   = (float)regions[i]/(float)label_walk;
      colors[3*i+1] = (float)regions[i]/(float)(label_walk*regions[i]);
      colors[3*i+2] = (float)regions[i]/(float)(label_walk*regions[i]);
      //printf ("%f %f %f\n", colors[3*i], colors[3*i+1], colors[3*i+2]);
    }
	*/
}
