#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "set_lines.h"
#include "octree.h"

Cset_lines::Cset_lines (Mesh_half_edge *_model)
{
  model = _model;
  n_extracted_lines = 0;
  extracted_lines = NULL;
  colors = (float*)malloc(3*model->m_nVertices*sizeof(float));
}

Cset_lines::~Cset_lines ()
{
  for (int i=0; i<n_extracted_lines; i++)
    delete extracted_lines[i];
  free (extracted_lines);
  free (colors);
}

void
Cset_lines::reinit (void)
{
  for (int i=0; i<n_extracted_lines; i++)
    delete extracted_lines[i];
  free (extracted_lines);
  extracted_lines = NULL;
  n_extracted_lines = 0;
}

void
Cset_lines::add_line (Vector3f _pluecker1, Vector3f _pluecker2, Vector3f _begin, Vector3f _end, int _n_vertices, int *_ivertices, Vector3f *_vertices, float _weight)
{
  n_extracted_lines++;

  extracted_lines = (Cextracted_line**)realloc(extracted_lines, n_extracted_lines*sizeof(Cextracted_line*));
  extracted_lines[n_extracted_lines-1] = new Cextracted_line (_pluecker1, _pluecker2, _begin, _end, _n_vertices, _ivertices, _vertices, _weight);
}

int              Cset_lines::get_n_extracted_lines (void) { return n_extracted_lines; }
Cextracted_line* Cset_lines::get_extracted_line (int index) { return extracted_lines[index]; }

void Cset_lines::apply_gaussian_noise (float variance)
{
	float *v = model->m_pVertices;
	int nv = model->m_nVertices;
  for (int i=0; i<nv; i++)
    {
      static long idum = -247;
      Vector3f perturb;
      perturb.Set (gasdev(&idum), gasdev(&idum), gasdev(&idum));
      v[3*i]   += variance*perturb.x;
      v[3*i+1] += variance*perturb.y;
      v[3*i+2] += variance*perturb.z;
    }

  model->ComputeNormals ();
}


/*************************/
/*** merge close lines ***/
/*************************/
void
Cset_lines::merge_close_lines (float cos_angle_max, float d_max)
{
  int i,j;
  
  Vector3f center1, center2;
  Vector3f direction1, direction2;
  for (i=0; i<n_extracted_lines-1; i++)
    {
      for (j=i+1; j<n_extracted_lines; j++)
	{
	  Cextracted_line *line1 = extracted_lines[i];
	  Cextracted_line *line2 = extracted_lines[j];

	  line1->get_center (center1);
	  line1->get_direction (direction1);
	  line2->get_center (center2);
	  line2->get_direction (direction2);

	  /* are the lines parallel ? */
	  if (fabs (direction1 * direction2) > cos_angle_max)
	    {
	      /* are the lines close enough ? */
	      Vector3f pt01, tmp;
	      pt01 = center1 - center2;
	      tmp = direction1 ^ pt01;
	      float distance1 = tmp.getLength();

	      tmp = direction2 ^ pt01;
	      float distance2 = tmp.getLength ();
	      //printf ("distance between line %d and line %d : %f\n", i, j, distance);
	      if (distance1 < d_max || distance2 < d_max) /* we merge the two lines */
		{
		  line1->merge (line2);

		  /* update the structure */
		  delete extracted_lines[j];
		  extracted_lines[j] = extracted_lines[--n_extracted_lines];

		  /* fit the vertices in the current line to have a better approximation */
		  /*
		  v3d *array = (v3d*)malloc(n_vertices[i]*sizeof(v3d));
		  for (int k=0; k<n_vertices[i]; k++)
		    v3d_init (array[k], vertices[i][3*k], vertices[i][3*k+1], vertices[i][3*k+2] );
		  Cline *line = new Cline ();
		  line->fit (array, n_vertices[i]);
		  line->get_point_begin (begin[i]);
		  line->get_point_end (end[i]);
		  line->get_direction (pluecker1[i]);
		  free (array);
		  */
		  /* test the next lines */
		  j--;
		}
	    }
	}
    }
}

//#define MEAN_SHIFT_KEEP_HISTORY
float
anisotropic_distance (Vector3f pos1, Vector3f dir1, Vector3f pos2)
{
  Vector3f tmp;
  float a, b, dist, dist2;
  tmp = pos2 - pos1;
  if (tmp.getLength () == 0.0) dist = 0.0;
  else
    {
	  /*
      a = fabs (v3d_dot_product (tmp, dir1));
      b = sqrt (v3d_dot_product (tmp, tmp) - a * a);
      dist = sqrt (0.0005*a*a + 1.0*b*b);
*/
	  /* infinite cylidner */
      a = fabs (tmp * dir1);
      dist2 = (tmp * tmp) - a * a;
  }
  return dist2;
}

void
Cset_lines::merge_close_lines_mean_shift (float hd, float hp,
					  Vector3f ***pos_histo, Vector3f ***dir_histo, int *n_ite)
{
  int i,j;
  float scaling = 1.0;

  /* create the history */
#ifdef MEAN_SHIFT_KEEP_HISTORY
  Vector3f **positions_history = (Vector3f**)malloc(50*sizeof(Vector3f*));
  Vector3f **directions_history = (Vector3f**)malloc(50*sizeof(Vector3f*));
  for (i=0; i<50; i++)
    {
      positions_history[i] = (Vector3f*)malloc(n_extracted_lines*sizeof(Vector3f));
      directions_history[i] = (Vector3f*)malloc(n_extracted_lines*sizeof(Vector3f));

      for (j=0; j<n_extracted_lines; j++)
	{
	  extracted_lines[j]->get_center (positions_history[i][j]);
	  extracted_lines[j]->get_direction (directions_history[i][j]);
	}
    }
#endif

  /* create the structures for the feature space */
  Vector3f *positions_init  = new Vector3f[n_extracted_lines];
  Vector3f *directions_init = new Vector3f[n_extracted_lines];
  Vector3f *positions       = new Vector3f[n_extracted_lines];
  Vector3f *directions      = new Vector3f[n_extracted_lines];
  
  /* init the feature space */
  for (i=0; i<n_extracted_lines; i++)
    {
      extracted_lines[i]->get_center (positions_init[i]);
      extracted_lines[i]->get_center (positions[i]);
      extracted_lines[i]->get_direction (directions_init[i]);
      extracted_lines[i]->get_direction (directions[i]);
    }

//#define USE_OCTREE
  /* create the octree */
#ifdef USE_OCTREE
  Octree *octree = new Octree (n_extracted_lines, positions_init, 10);
  int *ivertices = (int*)malloc(n_extracted_lines*sizeof(int));
#endif

  /* iterations */
  int convergence;
  int n_iterations = 0;
  do
    {
      n_iterations++;
      //printf ("iteration %d\n", n_iterations);

      convergence = 1;

      /* store the current distribution */
      if (0) // comparison with the previous positions
	{
	  for (j=0; j<n_extracted_lines; j++)
	    {
	      positions_init[j] = positions[j];
	      directions_init[j] = directions[j];
	    }
	}

      /* compute the new feature point */
      for (j=0; j<n_extracted_lines; j++)
	{
	  float sum_weights = 0.0;
	  Vector3f v_tmp, d_tmp;
	  v_tmp.Set (0.0, 0.0, 0.0);
	  d_tmp.Set (0.0, 0.0, 0.0);

#ifdef USE_OCTREE
	  int nivertices = 0;
	  int istart = 0;
	  octree->get_vertices (positions[j], directions[j], hp, &nivertices, ivertices, &istart);
	  for (int k=0; k<nivertices; k++)
	    {
	      //printf ("iteration %d : %d ( / %d) -> %d vertices found\n", n_iterations, j, n_extracted_lines, nivertices);
	      i = ivertices[k];
#else
	  for (i=0; i<n_extracted_lines; i++)
	    {
#endif /* USE_OCTREE */
	      /* compute the weight */

	      // anisotropic distance
	      float dist = MIN (anisotropic_distance (positions[j], directions[j], positions_init[i]),
				     anisotropic_distance (positions_init[i], directions_init[i], positions[j]));

		  float weight = (dist < hp && fabs(directions[j] * directions_init[i]) > hd)? 1.0 : 0.0;

		  //float tmp = v3d_dot_product (directions[j], directions_init[i]);
		  //float weight = (dist < hp)? exp (-dist*dist/(2*hp*hp)) * exp (-tmp*tmp/(2*hd*hd)) : 0.0;

	      v_tmp.x += weight * positions_init[i].x;
	      v_tmp.y += weight * positions_init[i].y;
	      v_tmp.z += weight * positions_init[i].z;

	      d_tmp.x += weight * directions_init[i].x;
	      d_tmp.y += weight * directions_init[i].y;
	      d_tmp.z += weight * directions_init[i].z;

	      sum_weights += weight;
	    }
	  Vector3f position_new, direction_new;
	  position_new.Set (v_tmp.x/sum_weights, v_tmp.y/sum_weights, v_tmp.z/sum_weights);

	  direction_new.Set (d_tmp.x/sum_weights, d_tmp.y/sum_weights, d_tmp.z/sum_weights);
	  direction_new.Normalize ();

	  /* test the convergence */
	  Vector3f tmp3, tmp4;
	  tmp3 = positions[j] - position_new;
	  tmp4 = directions[j] - direction_new;
	  if (tmp3.getLength () > hp/scaling || directions[j] * direction_new < hd/scaling)
	    convergence = 0;

	  positions[j] = position_new;
	  directions[j] = direction_new;
	  
#ifdef MEAN_SHIFT_KEEP_HISTORY
	  if (n_iterations < 50)
	    {
	      positions_history[n_iterations][j] = positions[j];
	      directions_history[n_iterations][j] = directions[j];
	    }
#endif
	    }
	  if (n_iterations > 19) convergence = 1;
	} while (!convergence);

#ifdef USE_OCTREE
      delete octree;
      free (ivertices);
#endif
      
    /* merging */
    if (1)
      for (i=0; i<n_extracted_lines-1; i++)
    {
      Vector3f v_current, d_current;
      v_current = positions[i];
      d_current = directions[i];
      for (j=i+1; j<n_extracted_lines; j++)
	{
	  Cextracted_line *line1 = extracted_lines[i];
	  Cextracted_line *line2 = extracted_lines[j];

	  Vector3f v_walk, d_walk;
	  v_walk = positions[j];
	  d_walk = directions[j];

	  Vector3f tmp3, tmp4;
	  tmp3 = v_current - v_walk;
	  tmp4 = d_current - d_walk;
  
	  // anisotropic distance
	  if (tmp3.getLength () < hp/scaling && d_current * d_walk > hd/scaling)
	    {
	      line1->merge (line2);
	      
	      /* update the structure */
	      delete extracted_lines[j];
	      n_extracted_lines--;
	      extracted_lines[j] = extracted_lines[n_extracted_lines];
	      positions[j] = positions[n_extracted_lines];
	      directions[j] = directions[n_extracted_lines];

	      j--; // test the next lines
	    }
	}
    }

#ifdef MEAN_SHIFT_KEEP_HISTORY
  *pos_histo = positions_history;
  *dir_histo = positions_history;
#endif
  *n_ite = (n_iterations > 50)? 50 : n_iterations;

  delete[] positions_init;
  delete[] positions;
  delete[] directions_init;
  delete[] directions;
}

#ifdef AAA
void
Cset_lines::merge_close_lines_pluecker (float cos_angle_max, float d_max)
{
  printf ("n_extracted_lines : %d\n", n_extracted_lines);
  int i,j,k;

  for (i=0; i<n_lines-1; i++)
    {
      for (j=i+1; j<n_lines; j++)
	{
	  /* are the lines parallel ? */
	  if (fabs (v3d_dot_product (pluecker1[i], pluecker1[j])) > cos_angle_max)
	    {
	      /* are the lines close enough ? */
	      v3d tmp;
	      v3d_cross_product (tmp, pluecker1[j], pluecker1[i]);
	      float distance = (v3d_dot_product(pluecker1[i], pluecker2[j]) +
				 v3d_dot_product(pluecker2[i], pluecker1[j])) / v3d_length (tmp);
	      distance = fabs (distance);
	      printf ("distance between line %d and line %d : %f\n", i, j, distance);
	      if (distance < d_max) /* we merge the two lines */
		{
		  /* copy the vertices from line2 to line1 */
		  vertices[i] = (v3d*)realloc(vertices[i], (n_vertices[i]+n_vertices[j])*sizeof(v3d));
		  for (k=0; k<n_vertices[j]; k++)
		    {
		      vertices[i][3*(n_vertices[i]+k)]   = vertices[j][3*k];
		      vertices[i][3*(n_vertices[i]+k)+1] = vertices[j][3*k+1];
		      vertices[i][3*(n_vertices[i]+k)+2] = vertices[j][3*k+2];
		    }

		  /* copy the ivertices from line2 to line1 */
		  ivertices[i] = (int*)realloc(ivertices[i], (n_vertices[i]+n_vertices[j])*sizeof(int));
		  for (k=0; k<n_vertices[j]; k++)
		    ivertices[i][n_vertices[i]+k] = ivertices[j][k];

		  n_vertices[i] += n_vertices[j];
		  
		  /* update the structure */
		  free (vertices[j]);
		  v3d_copy (begin[j], begin[n_lines-1]);
		  v3d_copy (end[j], end[n_lines-1]);
		  v3d_copy (pluecker1[j], pluecker1[n_lines-1]);
		  v3d_copy (pluecker2[j], pluecker2[n_lines-1]);
		  vertices[j] = vertices[n_lines-1];

		  n_lines--;

		  /* test the next lines */
		  j--;
		}
	    }
	}
    }
  printf ("after merging : %d lines\n", n_lines);
}
#endif

/* adjust begin and end to fit on the concerned vertices */
void
Cset_lines::compute_extremities (void)
{
  for (int i=0; i<n_extracted_lines; i++)
    extracted_lines[i]->compute_extremities ();
}

/* apply a least square fitting among the vertices to adjust the line */
void
Cset_lines::apply_least_square_fitting (void)
{
  for (int i=0; i<n_extracted_lines; i++)
    extracted_lines[i]->apply_least_square_fitting ();
}

/*****************************/
/*** delete isolated lines ***/
/*****************************/
void
Cset_lines::delete_isolated_lines (int n_max)
{
  for (int i=0; i<n_extracted_lines; i++)
    {
      if (extracted_lines[i]->get_n_vertices() <= n_max)
	{
	  delete extracted_lines[i];
	  extracted_lines[i] = extracted_lines[--n_extracted_lines];
	  i--;
	}
    }
}

/***********/
/* weights */
/***********/
/*
void
Cset_lines::compute_normalized_weights (Cextracted_line::weight_method id)
{
  int i;

  // compute the weights
  for (i=0; i<n_extracted_lines; i++)
    extracted_lines[i]->compute_weight (id);
  return;
  // get the maximal weight
  float max_weight = extracted_lines[0]->get_weight ();
  for (i=1; i<n_extracted_lines; i++)
    {
      if (max_weight < extracted_lines[i]->get_weight ())
	max_weight = extracted_lines[i]->get_weight ();
    }

  printf ("max_weight = %f\n", max_weight);
  if (max_weight == 0.0)
    {
      printf ("weight is nil\n");
      return;
    }

  for (int i=0; i<n_extracted_lines; i++)
    extracted_lines[i]->set_weight (extracted_lines[i]->get_weight () / max_weight);
}
*/

/**************/
/*** colors ***/
/**************/
void
Cset_lines::apply_random_colors (void)
{
  for (int i=0; i<n_extracted_lines; i++)
    {
      float r = (float)rand()/RAND_MAX;
      float g = (float)rand()/RAND_MAX;
      float b = (float)rand()/RAND_MAX;
      extracted_lines[i]->set_color (r, g, b);
    }
}

void
Cset_lines::compute_colors (void)
{
  int i, j, nv = model->m_nVertices;
  for (i=0; i<3*nv; i++) colors[i] = 0.6;
  for (i=0; i<n_extracted_lines; i++)
    {
      float r, g, b;
      extracted_lines[i]->get_color (&r, &g, &b);
      for (j=0; j<extracted_lines[i]->get_n_vertices (); j++)
	{
	  int index = extracted_lines[i]->get_ivertices()[j];
	  colors[3*index]   = r;
	  colors[3*index+1] = g;
	  colors[3*index+2] = b;
	}
    }
}

int
Cset_lines::compute_colors_for_selection (float _weight, int _minimum_n_vertices, float _length, float _density, float _mean_deviation)
{
  int i, j, nv = model->m_nVertices;
  for (i=0; i<3*nv; i++) colors[i] = 0.6;
  int new_n_lines = 0;
  for (i=0; i<n_extracted_lines; i++)
    {
      Cextracted_line *line = extracted_lines[i];
      float r, g, b;
      line->get_color (&r, &g, &b);
      if (line->get_weight () >= _weight &&
		  line->get_n_vertices () >= _minimum_n_vertices &&
		  line->get_length () >= _length &&
		  line->get_density () >= _density &&
		  line->get_mean_deviation () <= _mean_deviation)
	{
	  for (j=0; j<extracted_lines[i]->get_n_vertices (); j++)
	    {
	      int index = extracted_lines[i]->get_ivertices()[j];
	      colors[3*index]   = r;
	      colors[3*index+1] = g;
	      colors[3*index+2] = b;
	    }
	  new_n_lines++;
	}
    }
  return new_n_lines;
}

float*
Cset_lines::get_colors (void)
{
  return colors;
}

/********/
/* dump */
/********/
void
Cset_lines::dump (void)
{
  for (int i=0; i<n_extracted_lines; i++)
    {
      printf ("line %d / %d\n", i, n_extracted_lines);
      extracted_lines[i]->dump ();
      printf ("---------------\n");
    }
}
