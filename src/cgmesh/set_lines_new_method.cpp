#include <math.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include <vector>

#include "set_lines.h"
#include "set_lines_internal.h"

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
  float *v = model->m_pMesh->m_pVertices.data();
  int   nv = model->m_pMesh->m_nVertices;

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
	extracted_lines = nullptr;

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
							model->m_pMesh->GetTensor (index)->GetKappaMax() + model->m_pMesh->GetTensor (index)->GetKappaMax());
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

namespace {

// Le bloc « participation des candidats puis fusion » etait ecrit DEUX fois dans
// ce fichier, sur 75 lignes, a UN detail pres : le pas de parcours du double
// balayage -- 1 dans merge_oriented_vertices, 10 dans merge_oriented_vertices2.
// D'ou ce parametre `stride`.
//
// Deux allocations y posaient probleme :
//
//   - `candidates_for` etait un int** alloue avec n*sizeof(int) au lieu de
//     n*sizeof(int*). En 64 bits sizeof(int)==4 et sizeof(int*)==8 : le tableau
//     faisait la MOITIE de la taille necessaire, et la boucle d'initialisation
//     ecrivait hors du tas des la seconde moitie des indices
//     (cpp:S3519, set_lines_new_method.cpp:163). vector<vector<int>> supprime le
//     calcul de taille, donc la possibilite de le rater.
//   - `n_candidates_for` etait malloc + memset, libere a la main.
//
// `lines` est CONSOMME : les lignes retenues sont rendues a l'appelant, les
// autres detruites -- exactement le partage que faisait le code d'origine.
std::vector<Cextracted_line*>
merge_candidate_lines (std::vector<Cextracted_line*>& lines, int stride,
		       float tolerance_angle, float tolerance_distance)
{
	const int n = (int)lines.size();
	if (n <= 0)
		return std::vector<Cextracted_line*>();

	std::vector<float> n_candidates_for (n, 0.0f);
	std::vector<std::vector<int> > candidates_for (n, std::vector<int>(n, 0));

	Vector3f pos1, pos2, dir1, dir2;

	/* evaluate each candidate line */
	for (int i=0; i<n; i+=stride)
	{
		for (int j=0; j<n; j+=stride)
		{
			printf ("%d %d\n", i, j);
			if (i == j) continue;
			lines[i]->get_direction (dir1);
			lines[i]->get_begin (pos1);
			lines[j]->get_direction (dir2);
			lines[j]->get_begin (pos2);
			if (distance_line_point (pos1, dir1, pos2) < tolerance_distance &&
				distance_line_point (pos2, dir2, pos1) < tolerance_distance &&
				dir1 * dir2 > tolerance_angle)
			{
				// Borne prouvee : chaque j ajoute au plus une entree et i==j est
				// saute, donc au plus n-1 entrees pour n cases.
				candidates_for[i][(int)n_candidates_for[i]] = j;
				n_candidates_for[i]++;
				assert (n_candidates_for[i] < n);
			}
		}
	}

	// merge the lines
	int *sorted = quicksort_indices (n_candidates_for.data(), n);
	assert (sorted);
	for (int i=n-1; i>=0; i--)
	{
		int index1 = sorted[i];
		for (int j=0; j<(int)n_candidates_for[index1]; j++)
		{
			int index2 = candidates_for[index1][j];
			lines[index1]->merge (lines[index2]);
			n_candidates_for[index2] = 0; // we exclude the j-th line
			// ...and delete the participation of the j-th line in all the other lines
			for (int k=0; k<n; k++)
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

	std::vector<Cextracted_line*> kept;
	kept.reserve (n);
	for (int i=0; i<n; i++)
	{
		if (n_candidates_for[i] != 0)
			kept.push_back (lines[i]);
		else
			delete lines[i];
	}
	return kept;
}

} // namespace

void
Cset_lines::merge_oriented_vertices (float tolerance_angle, float tolerance_distance)
{
	int i;

	/* new extracted lines */
	// Etait un malloc jamais libere : le TABLEAU fuyait a chaque appel, ses
	// elements etant soit transferes soit detruits plus bas
	// (cpp:S3584, set_lines_new_method.cpp:224).
	std::vector<Cextracted_line*> lines (n_extracted_lines, nullptr);

	Vector3f v1;   // (0,0,0) par le constructeur par defaut, comme avant

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

		lines[i] = new Cextracted_line (dir, cross, pos, pos, n_vertices_in_the_line, ivertices_in_the_line, vertices_in_the_line);
	}

	const std::vector<Cextracted_line*> kept =
		merge_candidate_lines (lines, /*stride=*/1, tolerance_angle, tolerance_distance);

	n_extracted_lines = (int)kept.size();
	extracted_lines  = set_lines_detail::to_raw_array (kept);

	apply_least_square_fitting ();
	compute_extremities ();
}

void
Cset_lines::merge_oriented_vertices2 (float tolerance_angle, float tolerance_distance)
{
	int i,j;

	/* new extracted lines */
	// Etait un realloc incremental jamais libere ; push_back rend la croissance
	// explicite (cpp:S3584, set_lines_new_method.cpp:350).
	std::vector<Cextracted_line*> lines;

	Vector3f v1, v2, dir;
	Vector3f pos2, dir2;

	for (i=0; i<n_extracted_lines-1; i++)
		for (j=i+1; j<n_extracted_lines; j++)
		{
			Vector3f pos1, dir1;
			extracted_lines[i]->get_begin (pos1);
			extracted_lines[i]->get_direction (dir1);
			extracted_lines[j]->get_begin (pos2);
			extracted_lines[j]->get_direction (dir2);
			if (distance_line_point (pos1, dir1, pos2) < tolerance_distance &&
				distance_line_point (pos2, dir2, pos1) < tolerance_distance &&
				dir1 * dir2 > tolerance_angle)
			{
				// create a new extracted line
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
				lines.push_back (new Cextracted_line (dir, cross, v1, v2, n_vertices_in_the_line, ivertices_in_the_line, vertices_in_the_line));
				lines.back()->set_weight (1.0);
			}
		}

	const std::vector<Cextracted_line*> kept =
		merge_candidate_lines (lines, /*stride=*/10, tolerance_angle, tolerance_distance);

	n_extracted_lines = (int)kept.size();
	extracted_lines  = set_lines_detail::to_raw_array (kept);

	apply_least_square_fitting ();
	compute_extremities ();
}

