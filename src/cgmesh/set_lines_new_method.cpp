#include <math.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "set_lines.h"

void
Cset_lines::extract_ridges_and_valleys (float kappa_epsilon)
{
  int i;

/*
  Cmodel3d_region_vertices *region_vertices;
  region_vertices = new Cmodel3d_region_vertices (hemodel);
  region_vertices->init_from_closest_to_zero_curvatures ();
  region_vertices->select_n_down (90.0);
  float epsilon = region_vertices->get_highest_value_from_selected_region ();
  delete region_vertices;

  region_vertices = new Cmodel3d_region_vertices (hemodel);
  region_vertices->init_from_highest_absolute_curvatures ();

  printf ("percentage : %f\n", percentage);
  if (percentage >= 0.0 && percentage <= 100.0)
  region_vertices->select_n_up (percentage);
  float kappa_threshold = region_vertices->get_lowest_value_from_selected_region ();
  delete region_vertices;

  printf ("curvatures : %f -> %f\n", epsilon, kappa_threshold);
*/

  int n_selected_vertices;
  int *iselected_vertices;
  //float *selected_vertices;
  Vector3f *directions;
  //printf ("kappa_epsilon = %f\n", kappa_epsilon);

  //model->extract_straight_ridges_ravines (kappa_epsilon, &n_selected_vertices, &iselected_vertices, &directions);

  //hemodel->extract_straight_ridges_ravines_belyaev2000 (kappa_epsilon, kappa_epsilon, &n_selected_vertices, &iselected_vertices, &directions);
  //hemodel->extract_straight_ridges_ravines_belyaev2000 (kappa_epsilon, kappa_threshold, &n_selected_vertices, &iselected_vertices, &directions);

  // get the vertices from the model
  float *v = model->m_pVertices;
  int   nv = model->m_nVertices;

#ifdef FUR
  float zmax, zmin;
  zmin = v[2];
  zmax = v[2];
for (i=1; i<n_vertices; i++)
{
	zmin = (v[3*i+2] < zmin)? v[3*i+2] : zmin;
	zmax = (v[3*i+2] > zmax)? v[3*i+2] : zmax;
}
printf ("zmin -> zmax = %f %f\n", zmin, zmax);

printf ("length diagonal bbox : %f\n", model->get_length_bounding_box_diagonal());
#endif

  /* create the lines */
	n_extracted_lines = 0;
	if (extracted_lines)
	{
		for (int i=0; i<n_extracted_lines; i++)
			delete extracted_lines[i];
		free (extracted_lines);
	}
	extracted_lines = NULL;

  extracted_lines = (Cextracted_line**)malloc(n_extracted_lines*sizeof(Cextracted_line*));
  for (i=0; i<n_selected_vertices; i++)
    {
      int index = iselected_vertices[i];
      Vector3f v_walk, pluecker1, pluecker2;

      pluecker1 = directions[i];
      if (pluecker1.x < 0.0) pluecker1 *= -1.0;

	  pluecker1.Normalize ();
      v_walk.Set (v[3*index], v[3*index+1], v[3*index+2]);
      pluecker2 = v_walk ^ pluecker1;

      int n_vertices_in_the_line = 1;
      int *ivertices_in_the_line = (int*)malloc(sizeof(int));
      Vector3f *vertices_in_the_line = (Vector3f*)malloc(sizeof(Vector3f));

      ivertices_in_the_line[0] = index;
      vertices_in_the_line[0].Set (v[3*index], v[3*index+1], v[3*index+2]);
      this->add_line (pluecker1, pluecker2,
							v_walk, v_walk, n_vertices_in_the_line,
							ivertices_in_the_line, vertices_in_the_line,
							model->GetTensor (index)->GetKappaMax() + model->GetTensor (index)->GetKappaMax());
    }
  free (iselected_vertices);
}



/*******************************/
/*** merge oriented vertices ***/
/*******************************/

/* distance between a line and a point */
static float
distance_line_point (Vector3f pos1, Vector3f dir1, Vector3f pos2)
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
      dist = sqrt (0.0005*a*a + 0.7*b*b);
	  */
	
	  /* infinite cylidner */

      a = fabs (tmp * dir1);
      dist2 = (tmp * tmp) - a * a;

  }
  return dist2;
}

void
Cset_lines::merge_oriented_vertices (float tolerance_angle, float tolerance_distance)
{
	int i,j;

	/* new extracted lines */
	int n_extracted_lines_new = n_extracted_lines;
    Cextracted_line **extracted_lines_new = (Cextracted_line**)malloc(n_extracted_lines*sizeof(Cextracted_line*));

	Vector3f v1, v2, dir;
	Vector3f pos1, pos2, dir1, dir2;


	for (i=0; i<n_extracted_lines; i++)
	{
		Vector3f pos, dir, cross;
		extracted_lines[i]->get_begin (pos);
		extracted_lines[i]->get_direction (dir);
		cross = v1 ^ dir;
		int n_vertices_in_the_line = 1;
		int *ivertices_in_the_line = (int*)malloc(n_vertices_in_the_line*sizeof(int));
		Vector3f *vertices_in_the_line = (Vector3f*)malloc(n_vertices_in_the_line*sizeof(Vector3f));
		ivertices_in_the_line[0] = extracted_lines[i]->ivertices[0];
		vertices_in_the_line[0] = pos;

		extracted_lines_new[i] = new Cextracted_line (dir, cross, pos, pos, n_vertices_in_the_line, ivertices_in_the_line, vertices_in_the_line);
	}

	// participation of the candidates
	float *n_candidates_for = (float*)malloc(n_extracted_lines_new*sizeof(float));
	n_candidates_for = (float*)memset ((void*)n_candidates_for, 0, n_extracted_lines_new*sizeof(float));
	int **candidates_for = (int**)malloc(n_extracted_lines_new*sizeof(int));
	for (i=0; i<n_extracted_lines_new; i++)
		candidates_for[i] = (int*)malloc(n_extracted_lines_new*sizeof(int));;

	/* evaluate each candidate line */
	for (i=0; i<n_extracted_lines_new; i++)
	{
		for (j=0; j<n_extracted_lines_new; j++)
		{
			printf ("%d %d\n", i, j);
			if (i == j) continue;
			extracted_lines_new[i]->get_direction (dir1);
			extracted_lines_new[i]->get_begin (pos1);
			extracted_lines_new[j]->get_direction (dir2);
			extracted_lines_new[j]->get_begin (pos2);
			if (distance_line_point (pos1, dir1, pos2) < tolerance_distance &&
				distance_line_point (pos2, dir2, pos1) < tolerance_distance &&
				dir1 * dir2 > tolerance_angle)
			{
				candidates_for[i][(int)n_candidates_for[i]] = j;
				n_candidates_for[i]++;
				assert (n_candidates_for[i] < n_extracted_lines_new);
			}
		}
	}
	
	// merge the lines
	int *sorted = quicksort_indices (n_candidates_for, n_extracted_lines_new);

	for (i=n_extracted_lines_new-1; i>=0; i--)
	{
		int index1 = sorted[i];
		for (j=0; j<(int)n_candidates_for[index1]; j++)
		{
			int index2 = candidates_for[index1][j];
			extracted_lines_new[index1]->merge (extracted_lines_new[index2]);
			n_candidates_for[index2] = 0; // we exclude the j-th line
			// ...and delete the participation of the j-th line in all the other lines
			for (int k=0; k<n_extracted_lines_new; k++)
				if  (k!=index1)
					for (int l=0; l<(int)n_candidates_for[k]; l++)
						if (candidates_for[k][l] == index2)
						{
							candidates_for[k][l] = candidates_for[k][(int)n_candidates_for[k]-1];
							n_candidates_for[k]--;
						}
		}
	}
	free (sorted);

    Cextracted_line **extracted_lines_new2 = (Cextracted_line**)malloc(n_extracted_lines_new*sizeof(Cextracted_line*));
	assert (extracted_lines_new2);
	for (i=0,j=0; i<n_extracted_lines_new; i++)
	{
		if (n_candidates_for[i] != 0)
		{
			extracted_lines_new2[j] = extracted_lines_new[i];
			j++;
		}
		else
			delete extracted_lines_new[i];
	}

	n_extracted_lines = j;
    extracted_lines = extracted_lines_new2;

	apply_least_square_fitting ();
    compute_extremities ();

	free (n_candidates_for);
	for (i=0; i<n_extracted_lines_new; i++)
		free (candidates_for[i]);
	free (candidates_for);
}

void
Cset_lines::merge_oriented_vertices2 (float tolerance_angle, float tolerance_distance)
{
	int i,j;

	/* new extracted lines */
	int n_extracted_lines_new = 0;
    Cextracted_line **extracted_lines_new = NULL;

	Vector3f v1, v2, dir;
	Vector3f pos1, pos2, dir1, dir2;


	for (i=0; i<n_extracted_lines-1; i++)
		for (j=i+1; j<n_extracted_lines; j++)
		{
			Vector3f pos1, dir1, pos21, pos22;
			extracted_lines[i]->get_begin (pos1);
			extracted_lines[i]->get_direction (dir1);
			extracted_lines[j]->get_begin (pos2);
			extracted_lines[j]->get_direction (dir2);
			if (distance_line_point (pos1, dir1, pos2) < tolerance_distance &&
				distance_line_point (pos2, dir2, pos1) < tolerance_distance &&
				dir1 * dir2 > tolerance_angle)
			{
				// create a new extracted line
				n_extracted_lines_new++;
				extracted_lines_new = (Cextracted_line**)realloc(extracted_lines_new, n_extracted_lines_new*sizeof(Cextracted_line*));

				dir = v2 - v1;
				dir.Normalize ();
				Vector3f cross = v1 ^ dir;
				int n_vertices_in_the_line = 2;
				int *ivertices_in_the_line = (int*)malloc(n_vertices_in_the_line*sizeof(int));
				Vector3f *vertices_in_the_line = (Vector3f*)malloc(n_vertices_in_the_line*sizeof(Vector3f));

				ivertices_in_the_line[0] = extracted_lines[i]->ivertices[0];
				ivertices_in_the_line[1] = extracted_lines[j]->ivertices[0];
				vertices_in_the_line[0] = v1;
				vertices_in_the_line[1] = v2;
				extracted_lines_new[n_extracted_lines_new-1] = new Cextracted_line (dir, cross, v1, v2, n_vertices_in_the_line, ivertices_in_the_line, vertices_in_the_line);
				Vector3f v1v2;
				v1v2 = v1 - v2;
				float length = v1v2.getLength();
				extracted_lines_new[n_extracted_lines_new-1]->set_weight (1.0);
			}
		}

	// participation of the candidates
	float *n_candidates_for = (float*)malloc(n_extracted_lines_new*sizeof(float));
	n_candidates_for = (float*)memset ((void*)n_candidates_for, 0, n_extracted_lines_new*sizeof(float));
	int **candidates_for = (int**)malloc(n_extracted_lines_new*sizeof(int));
	for (i=0; i<n_extracted_lines_new; i++)
		candidates_for[i] = (int*)malloc(n_extracted_lines_new*sizeof(int));;

	/* evaluate each candidate line */
	for (i=0; i<n_extracted_lines_new; i+=10)
	{
		for (j=0; j<n_extracted_lines_new; j+=10)
		{
			printf ("%d %d\n", i, j);
			if (i == j) continue;
			extracted_lines_new[i]->get_direction (dir1);
			extracted_lines_new[i]->get_begin (pos1);
			extracted_lines_new[j]->get_direction (dir2);
			extracted_lines_new[j]->get_begin (pos2);
			if (distance_line_point (pos1, dir1, pos2) < tolerance_distance &&
				distance_line_point (pos2, dir2, pos1) < tolerance_distance &&
				dir1 * dir2 > tolerance_angle)
			{
				candidates_for[i][(int)n_candidates_for[i]] = j;
				n_candidates_for[i]++;
				assert (n_candidates_for[i] < n_extracted_lines_new);
			}
		}
	}
	
	// merge the lines
	int *sorted = quicksort_indices (n_candidates_for, n_extracted_lines_new);

	for (i=n_extracted_lines_new-1; i>=0; i--)
	{
		int index1 = sorted[i];
		for (j=0; j<(int)n_candidates_for[index1]; j++)
		{
			int index2 = candidates_for[index1][j];
			extracted_lines_new[index1]->merge (extracted_lines_new[index2]);
			n_candidates_for[index2] = 0; // we exclude the j-th line
			// ...and delete the participation of the j-th line in all the other lines
			for (int k=0; k<n_extracted_lines_new; k++)
				if  (k!=index1)
					for (int l=0; l<(int)n_candidates_for[k]; l++)
						if (candidates_for[k][l] == index2)
						{
							candidates_for[k][l] = candidates_for[k][(int)n_candidates_for[k]-1];
							n_candidates_for[k]--;
						}
		}
	}
	free (sorted);

    Cextracted_line **extracted_lines_new2 = (Cextracted_line**)malloc(n_extracted_lines_new*sizeof(Cextracted_line*));
	assert (extracted_lines_new2);
	for (i=0,j=0; i<n_extracted_lines_new; i++)
	{
		if (n_candidates_for[i] != 0)
		{
			extracted_lines_new2[j] = extracted_lines_new[i];
			j++;
		}
		else
			delete extracted_lines_new[i];
	}

	n_extracted_lines = j;
    extracted_lines = extracted_lines_new2;

	apply_least_square_fitting ();
    compute_extremities ();

	free (n_candidates_for);
	for (i=0; i<n_extracted_lines_new; i++)
		free (candidates_for[i]);
	free (candidates_for);
}

