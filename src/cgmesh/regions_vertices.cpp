#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "regions_vertices.h"
#include "garray.h"
//#include "fitting.h"

/**
* Constructor. Create an object managing vertices from a Cmodel3d_half_edge.
*/
Cregions_vertices::Cregions_vertices (Mesh_half_edge *_mesh_half_edge)
{
  assert (_mesh_half_edge);
  mesh_half_edge  = _mesh_half_edge;
  size            = mesh_half_edge->m_nVertices;
  datas           = new float[size];
  regions         = new int[size];
  selected_region = new int[size];

  // colors
  r_regions         = 0.6;
  g_regions         = 0.6;
  b_regions         = 0.6;
  r_selected_region = 1.0;
  g_selected_region = 1.0;
  b_selected_region = 1.0;
  r_common_vertex   = 0.5;
  g_common_vertex   = 0.5;
  b_common_vertex   = 0.5;
}

/**
* Destructor.
*/
Cregions_vertices::~Cregions_vertices ()
{
  delete datas;
  delete regions;
  delete selected_region;
}

/**
* Copy the values from _datas to data in the object.
*
* The parameter _size is used only to check if the arrays have the same size.
* If the sizes are different, nothing is done.
*/
void
Cregions_vertices::set_datas (float *_datas, int _size)
{
  if (size !=  _size) return;
  for (int i=0; i<size; i++)
    datas[i] = _datas[i];
}

/**
* Get the data.
*/
float*
Cregions_vertices::get_datas ()
{
  return datas;
}

/**
* Get the maximal principal directions defined on the vertices selected. 
*/
void
Cregions_vertices::get_principal_direction_max_from_selected_regions (Vector3d **directions, int *n)
{
	/*
  Vector3d tmp;
  int i,j,_n = 0;
  for (i=0; i<size; i++)
    if (regions[i] == 1)
      _n++;
  *n = _n;
  Vector3d *_directions;
  _directions = (Vector3d*)malloc((_n)*sizeof(Vector3d));
  for (i=0, j=0; i<size; i++)
    if (regions[i] == 1)
      {
		mesh_topology->tensor[i]->get_direction_max (tmp);
		_directions[j].Set (tmp.x, tmp.y, tmp.z);
		j++;
      }
  *directions = _directions;
  */
}

/**
*  Get the minimal principal directions defined on the vertices selected. 
*/
void
Cregions_vertices::get_principal_direction_min_from_selected_regions (Vector3d **directions, int *n)
{
	/*
  Vector3d tmp;
  int i,j,_n = 0;
  for (i=0; i<size; i++)
    if (regions[i] == 1)
      _n++;
  *n = _n;
  Vector3d *_directions;
  _directions = (Vector3d*)malloc((_n)*sizeof(Vector3d));
  for (i=0, j=0; i<size; i++)
    if (regions[i] == 1)
      {
	mesh_topology->tensor[i]->get_direction_min (tmp);
	_directions[j].Set (tmp.x, tmp.y, tmp.z);
	j++;
      }
  *directions = _directions;
  */
}

/**
* Get the maximal curvatures defined on the vertices selected.
*/
void
Cregions_vertices::get_curvature_max_from_selected_regions (float **curvatures, int *n)
{
	/*
  int i,j,_n = 0;
  for (i=0; i<size; i++)
    if (regions[i] == 1)
      _n++;
  *n = _n;
  float *_curvatures;
  _curvatures = (float*)malloc((_n)*sizeof(float));
  for (i=0, j=0; i<size; i++)
    if (regions[i] == 1)
      {
	_curvatures[j] = mesh_topology->tensor[i]->get_kappa_max ();
	j++;
      }
  *curvatures = _curvatures;
  */
}

/**
* Get the minimal curvatures defined on the vertices selected.
*/
void
Cregions_vertices::get_curvature_min_from_selected_regions (float **curvatures, int *n)
{
	/*
  int i,j,_n = 0;
  for (i=0; i<size; i++)
    if (regions[i] == 1)
      _n++;
  *n = _n;
  float *_curvatures;
  _curvatures = (float*)malloc((_n)*sizeof(float));
  for (i=0,j=0; i<size; i++)
    if (regions[i] == 1)
      {
	_curvatures[j] = mesh_topology->tensor[i]->get_kappa_min ();
	j++;
      }
  *curvatures = _curvatures;
  */
}

/**
* Exports the indices of the selected faces into a file.
*/
void
Cregions_vertices::export_selected_region_cloud_points (char *filename)
{
  int i,n_selected_vertices;
  FILE *ptr = fopen (filename,"wt");
  assert (ptr);

  float *v = NULL;
  float *vn = NULL;

  // get the mesh
  if (mesh_half_edge)
    {
      v  = mesh_half_edge->m_pVertices;
      vn = mesh_half_edge->m_pVertexNormals;
   }
  if (!v || !vn) return;
 
  n_selected_vertices = 0;
  for (i=0; i<size; i++)
	  if (selected_region[i] == 1)
		  n_selected_vertices++;
  fprintf (ptr, "%d\n", n_selected_vertices);

  for (i=0; i<size; i++)
	  if (selected_region[i] == 1)
	    fprintf (ptr, "%f %f %f %f %f %f\n", v[3*i], v[3*i+1], v[3*i+2], vn[3*i], vn[3*i+1], vn[3*i+2]);
  fclose (ptr);
}

/**
* Initializes the regions with the boundary of the 3D model
*/
void
Cregions_vertices::init_from_boundary (void)
{
	/*
  int *n_vertices_adjacent_vertices = mesh_topology->get_n_vertices_adjacent_vertices ();
  int *n_vertices_adjacent_faces    = mesh_topology->get_n_vertices_adjacent_faces ();
  for (int i=0; i<size; i++)
    regions[i] = (n_vertices_adjacent_faces[i] == n_vertices_adjacent_vertices[i])? 0 : 1;
	*/
}

/**
* Initializes the data with the values of the maximal curvatures.
*/
void
Cregions_vertices::init_from_max_curvatures (void)
{
	/*
  if (mesh_topology->tensor == NULL) return;
  for (int i=0; i<size; i++)
    datas[i] = mesh_topology->tensor[i]->get_kappa_max ();
	*/
}

/**
* Initializes the data with the values of the minimal curvatures.
*/
void
Cregions_vertices::init_from_min_curvatures (void)
{
	/*
  if (mesh_topology->tensor == NULL) return;
  for (int i=0; i<size; i++)
    datas[i] = mesh_topology->tensor[i]->get_kappa_min ();
	*/
}

/**
*
*/
void
Cregions_vertices::init_from_closest_to_zero_curvatures (void)
{
  /*
  if (mesh_half_edge)
    {
      Ctensor **tensor = mesh_half_edge->m_tensor;
      if (tensor == NULL) return;
      for (int i=0; i<size; i++)
	{
	  if (tensor[i])
	    {
	      datas[i] = (fabs (tensor[i]->get_kappa_min ()) < fabs (tensor[i]->get_kappa_max ()))?
		fabs (tensor[i]->get_kappa_min ()) : fabs (tensor[i]->get_kappa_max ());
	    }
	  else
	    {
	      datas[i] = 1000.0;
	    }
	}
    }
	*/
}

/**
*
*/
void
Cregions_vertices::init_from_highest_absolute_curvatures (void)
{
  /*
  if (mesh_half_edge)
  {
    Ctensor **tensor = mesh_half_edge->m_tensor;
    if (tensor == NULL) return;
    for (int i=0; i<size; i++)
      {
	if (tensor[i])
	  {
	    datas[i] = (fabs (tensor[i]->get_kappa_min ()) > fabs (tensor[i]->get_kappa_max ()))?
	      fabs (tensor[i]->get_kappa_min ()) : fabs (tensor[i]->get_kappa_max ());
	  }
	else
	  {
	    datas[i] = -1.0;
	  }
      }
  }
  */
}
  
/*** init features ***/
void
Cregions_vertices::get_directions_lines (Vector3 **directions, int *n, float threshold, float epsilon)
{
	/*
  int i,j=0,_n = 0;

  Vector3d dir;
  Vector3d *_directions;
  _directions = (Vector3d*)malloc(size*sizeof(Vector3d));

  float kappa0, kappa1;
  Vector3d dir0;
  Ctensor **tensor = mesh_topology->tensor;
  for (i=0; i<size; i++)
    {
      if (fabs (tensor[i]->get_kappa_min ()) < fabs (tensor[i]->get_kappa_max ()))
	{
	  kappa0 = fabs (tensor[i]->get_kappa_min ());
	  kappa1 = fabs (tensor[i]->get_kappa_max ());
	  tensor[i]->get_direction_min (dir0);
	}
      else
	{
	  kappa0 = fabs (tensor[i]->get_kappa_max ());
	  kappa1 = fabs (tensor[i]->get_kappa_min ());
	  tensor[i]->get_direction_max (dir0);
	}

      if (kappa0 < epsilon && kappa1 > threshold)
	{
	  _directions[j].Set (dir0.x, dir0.y, dir0.z);
	  tensor[i]->get_direction_min (dir0);
	  //v3d_dump (dir0);
	  //model->tensor[i]->get_direction_max (dir0);
	  //v3d_dump (dir0);
	  //printf ("---------------- > "); v3d_dump (_directions[j]);
	  j++;
	  _n++;
	}
    }
  *directions = _directions;
  *n = _n;
  */
}

void
Cregions_vertices::init_lines (float threshold, float epsilon)
{
	/*
  Ctensor **tensor = mesh_topology->tensor;
  for (int i=0; i<size; i++)
    {
      if ((fabs (tensor[i]->get_kappa_min ()) > threshold &&
	   fabs (tensor[i]->get_kappa_max ()) < epsilon)
	  ||
	  (fabs (tensor[i]->get_kappa_max ()) > threshold &&
	   fabs (tensor[i]->get_kappa_min ()) < epsilon))
	regions[i] = 1;
      else
	regions[i] = 0;
    }
	*/
}

/**
*
*/
void
Cregions_vertices::init_lines_ridges (float threshold, float epsilon)
{
	/*
  Ctensor **tensor = mesh_topology->tensor;
  for (int i=0; i<size; i++)
    {
      if (fabs (tensor[i]->get_kappa_min ()) > threshold &&
	  fabs (tensor[i]->get_kappa_max ()) < epsilon)
	regions[i] = 1;
      else
	regions[i] = 0;
    }
	*/
}

/**
*
*/
void
Cregions_vertices::init_lines_ravines (float threshold, float epsilon)
{
	/*
  Ctensor **tensor = mesh_topology->tensor;
  for (int i=0; i<size; i++)
    {
      if (fabs (tensor[i]->get_kappa_max ()) > threshold &&
	  fabs (tensor[i]->get_kappa_min ()) < epsilon)
	regions[i] = 1;
      else
	regions[i] = 0;
    }
	*/
}

/**
*
*/
void
Cregions_vertices::init_circles (float radius, float threshold, float epsilon)
{
	/*
  Ctensor **tensor = mesh_topology->tensor;
  for (int i=0; i<size; i++)
    {
      regions[i] = 0;
      if (fabs (tensor[i]->get_kappa_max ()) - 1.0/radius < epsilon &&
	  fabs (tensor[i]->get_kappa_min ()) - threshold < epsilon)
	regions[i] = 1;
      if (fabs (tensor[i]->get_kappa_min ()) - 1.0/radius < epsilon &&
	  fabs (tensor[i]->get_kappa_max ()) - threshold < epsilon)
	regions[i] = 1;
    }
	*/
}

/**
*
*/
void
Cregions_vertices::init_planes (float epsilon)
{
	/*
  Ctensor **tensor = mesh_topology->tensor;
  for (int i=0; i<size; i++)
    {
      if (fabs (tensor[i]->get_kappa_min ()) < epsilon &&
	  fabs (tensor[i]->get_kappa_max ()) < epsilon)
	regions[i] = 1;
      else
	regions[i] = 0;
    }
	*/
}

/**
*
*/
void
Cregions_vertices::init_spheres (float threshold, float epsilon)
{
	/*
  Ctensor **tensor = mesh_topology->tensor;
  for (int i=0; i<size; i++)
    {
      if (fabs (tensor[i]->get_kappa_min () - tensor[i]->get_kappa_max ()) < epsilon)
	regions[i] = 1;
      else
	regions[i] = 0;
    }
	*/
}

/**
*
*/
void
Cregions_vertices::init_cylinders (float threshold, float epsilon)
{
	/*
  Ctensor **tensor = mesh_topology->tensor;
  for (int i=0; i<size; i++)
    {
      if (fabs (tensor[i]->get_kappa_min ()) > threshold &&
	  fabs (tensor[i]->get_kappa_max ()) < epsilon)
	regions[i] = 1;
      else
	regions[i] = 0;
    }
	*/
}

/**
*
*/
void
Cregions_vertices::refresh_colors (void)
{
  float *vc = mesh_half_edge->m_pVertexColors;
  for (int i=0; i<size; i++)
    {
      if (regions[i] == 1)
	{
	  vc[3*i]   = r_regions;
	  vc[3*i+1] = g_regions;
	  vc[3*i+2] = b_regions;
	}
      else
	{
	  vc[3*i]   = r_common_vertex;
	  vc[3*i+1] = g_common_vertex;
	  vc[3*i+2] = b_common_vertex;
	}
      if (selected_region[i] == 1)
	{
	  vc[3*i]   = r_selected_region;
	  vc[3*i+1] = g_selected_region;
	  vc[3*i+2] = b_selected_region;
	}
    }
}

// get extrema of data
void
Cregions_vertices::get_extremas (float *_min, float *_max)
{
  float min, max;
  min = datas[0];
  max = datas[0];
  for (int i=1; i<size; i++)
    {
      if (datas[i] > max)
	max = datas[i];
      if (datas[i] < min)
	min = datas[i];
    }
  *_min = min;
  *_max = max;
}

//
// smoothing data
//
void
Cregions_vertices::smoothing_data (void)
{
}

//
// morphological operators
//

/*** regions ***/
void
Cregions_vertices::dilate_regions (void)
{
	/*
  int *res = new int[size];
  
  for (int i=0; i<size; i++)
    {
      res[i] = regions[i];
      if (regions[i] == 0)
	{
	  for (int j=0; j<mesh_topology->n_vertices_adjacent_vertices[i]; j++)
	    if (regions[mesh_topology->vertices_adjacent_vertices[i][j]] == 1)
	      res[i] = 1;
	}
    }
  delete regions;
  regions = res;
  */
}

void
Cregions_vertices::erode_regions (void)
{
	/*
  int *res = new int[size];

  for (int i=0; i<size; i++)
    {
      res[i] = regions[i];
      if (regions[i] == 1)
	{
	  for (int j=0; j<mesh_topology->n_vertices_adjacent_vertices[i]; j++)
	    if (regions[mesh_topology->vertices_adjacent_vertices[i][j]] == 0)
	      res[i] = 0;
	}
    }
  delete regions;
  regions = res;
  */
}

void
Cregions_vertices::opening_regions (void)
{
  erode_regions ();
  dilate_regions ();
}

void
Cregions_vertices::closing_regions (void)
{
  dilate_regions ();
  erode_regions ();
}

void
Cregions_vertices::delete_isolated_regions (int n)
{
	/*
  int i,j,k,id_walk;
  Cgarray *candidates;

  for (i=0; i<size; i++)
    {
      if (regions[i] == 1)
	{
	  candidates = new Cgarray ();
	  assert (candidates);

	  // construct the region containing regions[i]
	  regions[i] = 2; // temporary
	  candidates->add((void*)i);
	  for (j=0; j<candidates->get_size (); j++)
	    {
	      id_walk = (int)candidates->get_data (j);
	      for (k=0; k<mesh_topology->n_vertices_adjacent_vertices[id_walk]; k++)
		{
		  if (regions[mesh_topology->vertices_adjacent_vertices[id_walk][k]] == 1)
		    {
		      regions[mesh_topology->vertices_adjacent_vertices[id_walk][k]] = 2;
		      candidates->add ((void*)mesh_topology->vertices_adjacent_vertices[id_walk][k]);
		    }
		}
	    }
	  // delete the too small regions
	  if (candidates->get_size () <= n)
	    {
	      for (j=0; j<candidates->get_size (); j++)
		{
		  id_walk = (int)candidates->get_data (j);
		  regions[id_walk] = 0;
		}
	    }
	  delete candidates;
	}
    }
  for (i=0; i<size; i++)
    regions[i] = (regions[i] == 2)? 1 : 0;
	*/
}

/*** selected region ***/
void
Cregions_vertices::dilate_selected_region (void)
{
	/*
  int *res = new int[size];
  
  for (int i=0; i<size; i++)
    {
      res[i] = selected_region[i];
      if (selected_region[i] == 0)
	{
	  for (int j=0; j<mesh_topology->n_vertices_adjacent_vertices[i]; j++)
	    if (selected_region[mesh_topology->vertices_adjacent_vertices[i][j]] == 1)
	      res[i] = 1;
	}
    }
  delete selected_region;
  selected_region = res;
  */
}

void
Cregions_vertices::erode_selected_region (void)
{
	/*
  int *res = new int[size];

  for (int i=0; i<size; i++)
    {
      res[i] = selected_region[i];
      if (selected_region[i] == 1)
	{
	  for (int j=0; j<mesh_topology->n_vertices_adjacent_vertices[i]; j++)
	    if (selected_region[mesh_topology->vertices_adjacent_vertices[i][j]] == 0)
	      res[i] = 0;
	}
    }
  delete selected_region;
  selected_region = res;
  */
}

void
Cregions_vertices::opening_selected_region (void)
{
  erode_selected_region ();
  dilate_selected_region ();
}

void
Cregions_vertices::closing_selected_region (void)
{
  dilate_selected_region ();
  erode_selected_region ();
}

/*******************/
/* skeletonization */
/*******************/
void
Cregions_vertices::skeletonize_selected_region (void)
{
  int i,j;
  int *res     = new int[size];
  int *disks   = new int[size];
  int *centers = new int[size];
/*
  do {
    modified = 0;
    int n_adj_vertices;

    // compute the union of all disks and the union of centers
    for (i=0; i<size; i++)
      disks[i] = centers[i] = 0;
    for (i=0; i<size; i++)
      {
	if (selected_region[i] == 1)
	  {
	    int check = 1;
	    n_adj_vertices = mesh_topology->n_vertices_adjacent_vertices[i];
	    for (int j=0; j<n_adj_vertices; j++)
	      if (selected_region[mesh_topology->vertices_adjacent_vertices[i][j]] == 0)
		check = 0;
	    if (check)
	      {
		centers[i] = 1;
		disks[i] = 1;
		for (int j=0; j<n_adj_vertices; j++)
		  disks[mesh_topology->vertices_adjacent_vertices[i][j]] = 1;
	      }
	  }	
      }

    for (i=0; i<size; i++)
      {
	res[i] = selected_region[i];
	if (selected_region[i] == 1)
	  {
	    n_adj_vertices = mesh_topology->n_vertices_adjacent_vertices[i];

	    // the current vertex is it a complex vertex
	    int complexity = 0;
	    for (j=0; j<n_adj_vertices; j++)
	      {
		if (selected_region[mesh_topology->vertices_adjacent_vertices[i][j]] !=
		    selected_region[mesh_topology->vertices_adjacent_vertices[i][(j+1)%n_adj_vertices]])
		  complexity++;
	      }

	    // we finally unselect the current vertex
	    if (disks[i] && !(complexity >= 4 || centers[i]))
	      {
		res[i] = 0;
		// upadte the temporary arrays
		disks[i] = 0;
		centers[i] = 0;
		for (j=0; j<n_adj_vertices; j++)
		  disks[mesh_topology->vertices_adjacent_vertices[i][j]] = 0;
		
		modified = 1; // update the "modified" token
	      }
	  }
      }

    // we update the selected region array
    for (i=0; i<size; i++)
      selected_region[i] = res[i];
  } while (modified);
  */
}

/********************************************/
/* pruning (to use after the skeletization) */
/********************************************/
void
Cregions_vertices::pruning_selected_region (void)
{
  int n_adj_vertices;
  int modified;
/*
  int rank = 2;
  do {
    modified = 0;
    for (int i=0; i<size; i++)
      {
	if (selected_region[i] == 1)
	  {
	    n_adj_vertices = mesh_topology->n_vertices_adjacent_vertices[i];
	    // the current vertex is it a complex vertex
	    int complexity = 0;
	    for (int j=0; j<n_adj_vertices; j++)
	      {
		if (selected_region[mesh_topology->vertices_adjacent_vertices[i][j]] !=
		    selected_region[mesh_topology->vertices_adjacent_vertices[i][(j+1)%n_adj_vertices]])
		  complexity++;
	      }
	    if (complexity < 4)
	      {
		selected_region[i] = 0;
		modified = 1;
	      }
	  }
      }
  } while (rank--);
  */
}

/*************/
/* smoothing */
/*************/
void
Cregions_vertices::smoothing_laplacian_regions (void)
{
  //mesh_topology->smoothing_laplacian (regions);
}

void
Cregions_vertices::smoothing_laplacian_inverse_regions (void)
{
  //mesh_topology->smoothing_laplacian_inverse (regions);
}

void
Cregions_vertices::smoothing_taubin_regions (void)
{
  float lambda = 0.7;
  float mu = -0.7527;
  //mesh_topology->smoothing_taubin (lambda, mu, regions);
}

void
Cregions_vertices::smoothing_taubin_regions (float lambda, float mu)
{
  //mesh_topology->smoothing_taubin (lambda, mu, regions);
}

void
Cregions_vertices::smoothing_taubin_inverse_regions (void)
{
  float lambda = -1.2;
  float mu = 0.7;
  //mesh_topology->smoothing_taubin (lambda, mu, regions);
}

void
Cregions_vertices::smoothing_laplacian_selected_region (void)
{
  //mesh_topology->smoothing_laplacian (selected_region);
}

void
Cregions_vertices::smoothing_laplacian_inverse_selected_region (void)
{
  //mesh_topology->smoothing_laplacian_inverse (selected_region);
}

void
Cregions_vertices::smoothing_taubin_selected_region (void)
{
  float lambda = 0.7;
  float mu = -0.7527;
  //mesh_topology->smoothing_taubin (lambda, mu, selected_region);
}

void
Cregions_vertices::smoothing_taubin_inverse_selected_region (void)
{
  float lambda = -1.2;
  float mu = 0.7;
  //mesh_topology->smoothing_taubin (lambda, mu, selected_region);
}

void
Cregions_vertices::smoothing_taubin_selected_region (float lambda, float mu)
{
  //mesh_topology->smoothing_taubin (lambda, mu, selected_region);
}
  
/***********/
/* fitting */
/***********/
#if 0
/* fit with a line the selected vertices */
Line*
Cregions_vertices::line_fitting (void)
{
  int i, j = 0, n;

  // check the number of vertices
  for (i=0, n=0; i<size; i++)
    if (selected_region[i] == 1)
      n++;

  // fill the array
  Vector3d *array = new Vector3d[n];
  for (i=0; i<size; i++)
    if (selected_region[i] == 1)
      {
	array[j].x = mesh_half_edge->get_vertices()[3*i];
	array[j].y = mesh_half_edge->get_vertices()[3*i+1];
	array[j].z = mesh_half_edge->get_vertices()[3*i+2];
	j++;
      }
      
  Line *line = new Line();
  line->fit (array, n);
  delete array;
  return line;
}

Circle*
Cregions_vertices::circle_fitting (void)
{
  int i, j = 0, n;

  // check the number of vertices
  for (i=0, n=0; i<size; i++)
    if (selected_region[i] == 1)
      n++;

  // fill the array
  Vector3d *array = new Vector3d[n];
  for (i=0; i<size; i++)
    if (selected_region[i] == 1)
      {
	array[j].x = mesh_half_edge->get_vertices()[3*i];
	array[j].y = mesh_half_edge->get_vertices()[3*i+1];
	array[j].z = mesh_half_edge->get_vertices()[3*i+2];
	j++;
      }

  Circle *circle = new Circle ();
  //circle->fit (array, n);
  delete array;
  return circle;
}

Plane*
Cregions_vertices::plane_fitting (void)
{
  int i;
  Vector3f normale (0.0, 0.0, 0.0), center (0.0, 0.0, 0.0), walk;
 /* 
  for (i=0; i<size; i++)
    if (selected_region[i] == 1)
      {
	walk.Set (mesh_topology->get_vertices_normales()[3*i],
		   mesh_topology->get_vertices_normales()[3*i+1],
		   mesh_topology->get_vertices_normales()[3*i+2]);
	normale = normale + walk;
	
	walk.Set (mesh_topology->get_vertices()[3*i],
		   mesh_topology->get_vertices()[3*i+1],
		   mesh_topology->get_vertices()[3*i+2]);
	center = center + walk;
      }
  normale.x /= size;
  normale.y /= size;
  normale.z /= size;
  
  center.x /= size;
  center.y /= size;
  center.z /= size;
  */
  Plane *plane = new Plane (center, normale);
  return plane;
}
#endif

/*************/
/* selection */
/*************/
void
Cregions_vertices::select_all_regions (void)
{
  for (int i=0; i<size; i++)
    selected_region[i] = (regions[i])? 1 : 0;
}

/* select the n % vertices with higher values in datas */
void
Cregions_vertices::select_n_up (float percentage)
{
  int *indices = quicksort_indices (datas, size);
  int i, n = (int)((float)size*percentage/100.0);
  //printf ("%.1f\% -> %d\n", percentage, n);
  for (i=0; i<size-n; i++)
    regions[indices[i]] = 0;
  for (i=size-n; i<size; i++)
    regions[indices[i]] = 1;

  /* colors */
  /*
  int n_before, n_after;
  int range = 9;
  n_before = (int)((float)size*(percentage-range)/100.0);
  n_after = (int)((float)size*(percentage+range)/100.0);
  printf ("%d -> %d\n", n_before, n_after);
  if (n_before < 0) n_before = 0;
  if (n_after > size) n_after = size;
  for (i=0; i<n_before; i++)
    {
      model->vertices_colors[3*indices[i]]   = 0.5;
      model->vertices_colors[3*indices[i]+1] = 0.5;
      model->vertices_colors[3*indices[i]+2] = 0.5;
    }
  for (i=n_before; i<n_after; i++)
    {
      model->vertices_colors[3*indices[i]]   = 0.25;//(float)(i-n_before)/(2.0*(float)(n_after-n_before));
      model->vertices_colors[3*indices[i]+1] = 0.25;//(float)(i-n_before)/(2.0*(float)(n_after-n_before));
      model->vertices_colors[3*indices[i]+2] = 0.25;//(float)(i-n_before)/(2.0*(float)(n_after-n_before));
    }
  for (i=n_after; i<size; i++)
    {
      model->vertices_colors[3*indices[i]]   = 0.0;
      model->vertices_colors[3*indices[i]+1] = 0.0;
      model->vertices_colors[3*indices[i]+2] = 0.0;
    }
  */
  free (indices);
}

/* select the n % vertices with lower values in datas */
void
Cregions_vertices::select_n_down (float percentage)
{
  int *indices = quicksort_indices (datas, size);
  int i, n = (int)((float)size*percentage/100.0);
  //printf ("%.1f\% -> %d\n", percentage, n);
  for (i=0; i<n; i++)
    regions[indices[i]] = 1;
  for (i=n; i<size; i++)
    regions[indices[i]] = 0;

  free (indices);
}

void
Cregions_vertices::select_threshold_up (float threshold)
{
  for (int i=0; i<size; i++)
    regions[i] = (datas[i] > threshold)? 1 : 0;
}

void
Cregions_vertices::select_threshold_down (float threshold)
{
  for (int i=0; i<size; i++)
    regions[i] = (datas[i] < threshold)? 1 : 0;
}

void
Cregions_vertices::select_datas (float value)
{
  for (int i=0; i<size; i++)
    regions[i] = (datas[i] == value)? 1 : 0;
}

float
Cregions_vertices::get_lowest_value_from_selected_region (void)
{
  int i;
  float res;
  for (i=0; i<size; i++)
    if (regions[i] == 1) res = datas[i];
  for (; i<size; i++)
    if (regions[i] == 1 && res > datas[i]) res = datas[i];
  return res;
}

float
Cregions_vertices::get_highest_value_from_selected_region (void)
{
  int i;
  float res;
  for (i=0; i<size; i++)
    if (regions[i] == 1) res = datas[i];
  for (; i<size; i++)
    if (regions[i] == 1 && res < datas[i]) res = datas[i];
  return res;
}
  
/*************/
/* selection */
/*************/
void
Cregions_vertices::select_region (int id)
{
  for (int i=0; i<size; i++)
    selected_region[i] = (regions[i] == id)? 1 : 0;
}

void
Cregions_vertices::select_vertex (int id)
{
  if (id >= 0 && id < size)
    selected_region[id] = 1;
}

void
Cregions_vertices::deselect_vertex (int id)
{
  if (id >= 0 && id < size)
    selected_region[id] = 0;
}

void
Cregions_vertices::select_vertices (int id)
{
	/*
  Cgarray *candidates;
  candidates = new Cgarray ();
  assert (candidates);

  candidates->add((void*)id);
  for (int i=0; i<candidates->get_size (); i++)
    {
      int id_walk = (int)candidates->get_data (i);
      if (selected_region[id_walk] == 1)
	continue;
      if (regions[id_walk] == 1)
	{
	  selected_region[id_walk] = 1;
	  for (int j=0; j<mesh_topology->n_vertices_adjacent_vertices[id_walk]; j++)
	    candidates->add ((void*)mesh_topology->vertices_adjacent_vertices[id_walk][j]);
	}
    }
	*/
}

void
Cregions_vertices::deselect_vertices (int id)
{
	/*
  Cgarray *candidates;
  candidates = new Cgarray ();
  assert (candidates);

  candidates->add((void*)id);
  for (int i=0; i<candidates->get_size (); i++)
    {
      int id_walk = (int)candidates->get_data (i);
      if (selected_region[id_walk] == 0)
	continue;
      if (regions[id_walk] == 1)
	{
	  selected_region[id_walk] = 0;
	  for (int j=0; j<mesh_topology->n_vertices_adjacent_vertices[id_walk]; j++)
	    candidates->add ((void*)mesh_topology->vertices_adjacent_vertices[id_walk][j]);
	}
    }
	*/
}

void
Cregions_vertices::deselect (void)
{
  for (int i=0; i<size; i++)
    selected_region[i]=0;
}

/**********/
/* colors */
/**********/
void
Cregions_vertices::set_color_regions (float r, float g, float b)
{
  r_regions = r;
  g_regions = g;
  b_regions = b;
}

void
Cregions_vertices::set_color_selected_region (float r, float g, float b)
{
  r_selected_region = r;
  g_selected_region = g;
  b_selected_region = b;
}

void
Cregions_vertices::set_color_common_vertex (float r, float g, float b)
{
  r_common_vertex = r;
  g_common_vertex = g;
  b_common_vertex = b;
}

/********/
/* edit */
/********/
/*****************/
/* edit vertices */
/*****************/
void
Cregions_vertices::delete_selected_region (void)
{
	/*
  int i,j;
  float *vertices = mesh_topology->get_vertices ();
  int *faces      = mesh_topology->get_faces ();
  int n_vertices  = mesh_topology->get_n_vertices ();
  int n_faces     = mesh_topology->get_n_faces ();
  int **vertices_adjacent_faces  = mesh_topology->get_vertices_adjacent_faces ();
  int *n_vertices_adjacent_faces = mesh_topology->get_n_vertices_adjacent_faces ();
  for (i=0; i<size; i++)
    {
      if (selected_region[i] == 1)
	{
	  // delete all faces containing the vertex selected_region[i]
	  for (j=0; j<n_vertices_adjacent_faces[i]; j++)
	    {
	      ;//for (k=0
	    }
	}
    }
	*/
}
