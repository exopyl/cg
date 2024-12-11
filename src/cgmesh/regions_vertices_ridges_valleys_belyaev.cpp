#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

#include "regions_vertices.h"

/**
*
*/
void
Cregions_vertices::init_from_belyaev (void)
{
  int i, j;

  /**************************************/
  /* get the extremas of the curvatures */
  /**************************************/
  init_from_max_curvatures ();
  float kkmin, kkmax;
  get_extremas (&kkmin, &kkmax);

  /* initialization of regions */
  for (i=0; i<size; i++)
    {
      datas[i]   = 0;
      regions[i] = 0;
    }

  /*************/
  /* smoothing */
  /*************/

  /*****************************************/
  /* detection of ridges and ravines edges */
  /*****************************************/
	  /*
  Tensor **pDiffParam = mesh_half_edge->m_pDiffParam;
  if (!pDiffParam)
    return;
  float *v  = mesh_half_edge->m_pVertices;
  float *vn = mesh_half_edge->m_pVertexNormals;

  for (i=0; i<size; i++)
    {
      myreal kmax, kmin;  // principal curvatures
      Vector3d tmax, tmin;   // main directions
      Vector3d n1, n2; // auxiliary vectors

      Vector3d v_current (v[3*i], v[3*i+1], v[3*i+2]);
      Vector3d n_current (vn[3*i], vn[3*i+1], vn[3*i+2]);
      kmax = tensor[i]->GetKappaMax ();
      kmin = tensor[i]->GetKappaMin ();
      tensor[i]->GetDirectionMax (tmax);
      tensor[i]->GetDirectionMin (tmin);
      n1 = n_current ^ tmax;
      n2 = n_current ^ tmin;
      
      int n_adj_vertices = mesh_topology->n_vertices_adjacent_vertices[i];
      int kmax1_encountered = 0;
      int kmin1_encountered = 0;
      myreal kmax1, kmax2, kmin1, kmin2;
      for (j=0; j<n_adj_vertices; j++)
	{
	  int id1 = mesh_topology->vertices_adjacent_vertices[i][j];
	  int id2 = mesh_topology->vertices_adjacent_vertices[i][(j+1)%n_adj_vertices];
	  Vector3d v1 (v[3*id1], v[3*id1+1], v[3*id1+2]);
	  Vector3d v2 (v[3*id2], v[3*id2+1], v[3*id2+2]);
	  Vector3d t1 = v1 - v_current;
	  Vector3d t2 = v2 - v_current;

	  myreal in1, in2;
	  in1 = (n1 * t1);
	  in2 = (n1 * t2);
	  if (in1 * in2 <= 0.0)
	    {
	      // direction of the point between v1 and v2
	      Vector3d v_tmp ((in2*t1.x + in1*t2.x) / (in1 + in2),
			       (in2*t1.y + in1*t2.y) / (in1 + in2),
			       (in2*t1.z + in1*t2.z) / (in1 + in2));
	      
	      // linear interpolation to estimate the max curvature in v_tmp
	      myreal kmax_t1, kmax_t2;
	      kmax_t1 = tensor[id1]->get_kappa_max ();
	      kmax_t2 = tensor[id2]->get_kappa_max ();

	      myreal length1, length2;
	      Vector3d ttt = t1 - v_tmp;
	      length1 = ttt.getLength ();
	      ttt = t2 - v_tmp;
	      length2 = ttt.getLength ();

	      myreal kinterpolated = kmax_t1 + length1 * (kmax_t2 - kmax_t1) / (length1 + length2);

	      if (kmax1_encountered == 0)
		{
		  kmax1 = kinterpolated;
		  //printf ("1: %f\n", kmax1);
		  kmax1_encountered = 1;
		}
	      else
		{
		  kmax2 = kinterpolated;
		  //printf ("2: %f\n", kmax2);
		}
	    }

	  in1 = (n2 * t1);
	  in2 = (n2 * t2);
	  if (in1 * in2 <= 0.0)
	    {
	      // direction of the point between v1 and v2
	      Vector3d v_tmp ((in2*t1.x + in1*t2.x) / (in1 + in2),
			       (in2*t1.y + in1*t2.y) / (in1 + in2),
			       (in2*t1.z + in1*t2.z) / (in1 + in2));
	      
	      // linear interpolation to estimate the max curvature in v_tmp
	      myreal kmin_t1, kmin_t2;
	      kmin_t1 = tensor[id1]->get_kappa_min ();
	      kmin_t2 = tensor[id2]->get_kappa_min ();

	      myreal length1, length2;
	      Vector3d ttt = t1 - v_tmp;
	      length1 = ttt.getLength ();
	      ttt = t2 - v_tmp;
	      length2 = ttt.getLength ();
	      
	      myreal kinterpolated = kmin_t1 + length1 * (kmin_t2 - kmin_t1) / (length1 + length2);

	      if (kmin1_encountered == 0)
		{
		  kmin1 = kinterpolated;
		  //printf ("1: %f\n", kmin1);
		  kmin1_encountered = 1;
		}
	      else
		{
		  kmin2 = kinterpolated;
		  //printf ("2: %f\n", kmin2);
		}
	    }
	}

      // problem in the neighbourhood of the current vertex
      if (kmax1_encountered != 1 && kmin1_encountered != 1)
	continue;

      // is it a ridge
      if (kmax > kmax1 && kmax > kmax2)
	datas[i] = 1.0; // hack: this denotes a ravine

      // is it a ravine
      if (kmin < kmin1 && kmin < kmin2)
	datas[i] = 2.0; // hack: this denotes a ridge
    }

  //
  // decimation
  //

  //
  // cleaning
  //

  //
  // geodesic ridges and ravines
  //

  */
}

void
Cregions_vertices::get_directions_lines_from_belyaev (Vector3 **directions, int *n)
{
  int i, j=0, _n = 0;
  /*
  Vector3 dir;
  Vector3 *_directions;
  _directions = (Vector3*)malloc(size*sizeof(Vector3));
  
  Tensor **pDiffParam = mesh_half_edge->m_pDiffParam;
  for (i=0; i<size; i++)
    {
      if (datas[i] == 1.0) // ravine
			pDiffParam[i]->GetDirectionMin (dir);
      if (datas[i] == 2.0)
			pDiffParam[i]->GetDirectionMax (dir);
      _directions[j].Set (dir.x, dir.y, dir.z);
      
      j++;
      _n++;
    }
  *directions = _directions;
  *n = _n;
  */
}

/* select the n % vertices with higher values in the selection made by belyaev's method */
void
Cregions_vertices::belyaev_ridges_select_n (float percentage)
{
  int i, j;
/*
  Tensor **pDiffParam = mesh_half_edge->m_pDiffParam;
  float *curvatures_min = (float*)malloc(size*sizeof(float));
  assert (curvatures_min);
  for (j=0; j<size; j++)
    curvatures_min[j] = pDiffParam[j]->GetKappaMin ();
  int *indices = quicksort_indices (curvatures_min, size);
  int n = (int)((float)size*(100.0-percentage)/100.0);
  //printf ("%.1f\% -> %d\n", percentage, n);
  for (i=0; i<size-n; i++)
    {
      if (datas[indices[i]] == 2.0)
	regions[indices[i]] = 1;
      else
	regions[indices[i]] = 0;
    }
  for (i=size-n; i<size; i++)
    regions[indices[i]] = 0;
  free (indices);
  free (curvatures_min);
*/
}

/* select the n % vertices with lower values in the selection made by belyaev's method */
void
Cregions_vertices::belyaev_ravines_select_n (float percentage)
{
  int i, j;
/*
  Tensor **pDiffParam = mesh_half_edge->m_pDiffParam;
  float *curvatures_max = (float*)malloc(size*sizeof(float));
  assert (curvatures_max);
  for (j=0; j<size; j++)
    curvatures_max[j] = pDiffParam[j]->GetKappaMax ();
  int *indices = quicksort_indices (curvatures_max, size);
  int n = (int)((float)size*(percentage)/100.0);
  //printf ("%.1f\% -> %d\n", percentage, n);
  for (i=0; i<size-n; i++)
    regions[indices[i]] = 0;
  for (i=size-n; i<size; i++)
    {
      if (datas[indices[i]] == 1.0)
	regions[indices[i]] = 1;
      else
	regions[indices[i]] = 0;
    }
  free (indices);
  free (curvatures_max);

  int i, j;

  float *curvatures_max = (float*)malloc(size*sizeof(float));
  assert (curvatures_max);
  for (j=0; j<size; j++)
    curvatures_max[j] = model->tensor[j]->get_kappa_max ();
  int *indices = quicksort_indices (curvatures_max, size);
  int n = (int)((float)size*percentage/100.0);
  printf ("%.1f\% -> %d\n", percentage, n);
  for (i=0; i<size-n; i++)
    regions[indices[i]] = 0;
  for (i=size-n; i<n; i++)
    {
      if (datas[indices[i]] == 1.0)
	regions[indices[i]] = 1;
      else
	regions[indices[i]] = 0;
    }
  free (indices);
  free (curvatures_max);
*/
}
