#include <math.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "set_lines.h"

#include "mesh_half_edge.h"

void Cset_lines::cantzler_extract_edges (float threshold)
{
	float *fn = model->m_pFaceNormals;
	float *v = model->m_pVertices;
	int nv = model->m_nVertices;
	int ne = 3*model->m_nFaces;
	Che_edge **edges = model->m_edges;

	// visited half edges
	Cedges_visited *ev = new Cedges_visited (nv);
	for (int i=0; i<ne; i++)
	{
		Vector3f n1, n2;
		int f1, f2;
		if (!edges[i]->m_pair || ev->is_edge_visited (edges[i]->m_v_begin, edges[i]->m_v_end) > 0)
			continue;
		ev->add_edge (edges[i]->m_v_begin, edges[i]->m_v_end,i);

		f1 = edges[i]->m_face;
		f2 = edges[i]->m_pair->m_face;
		n1.Set (fn[3*f1], fn[3*f1+1], fn[3*f1+2]);
		n2.Set (fn[3*f2], fn[3*f2+1], fn[3*f2+2]);
		float dot = n1 * n2;
		if (dot>=1.0) dot = 1.0;
		if (dot<=-1.0) dot = -1.0;
		float alpha = (float)acos(dot);
		if (alpha > threshold)
		{
			Vector3f v1, v2, dir, pluecker1, pluecker2;
			int iv1, iv2;
			iv1 = edges[i]->m_v_begin;
			iv2 = edges[i]->m_v_end;
			v1.Set (v[3*iv1], v[3*iv1+1], v[3*iv1+2]);
			v2.Set (v[3*iv2], v[3*iv2+1], v[3*iv2+2]);
			dir = v1 - v2;
			dir.Normalize ();
			pluecker1 = dir;
			pluecker2 = v1 ^ pluecker1;

			int n_vertices_in_the_line = 2;
			int *ivertices_in_the_line = (int*)malloc(n_vertices_in_the_line*sizeof(int));
			Vector3f *vertices_in_the_line = (Vector3f*)malloc(n_vertices_in_the_line*sizeof(Vector3f));

			ivertices_in_the_line[0] = iv1;
			ivertices_in_the_line[1] = iv2;
			vertices_in_the_line[0] = v1;
			vertices_in_the_line[1] = v2;
			add_line (pluecker1, pluecker2,
						v1, v2, n_vertices_in_the_line,
						ivertices_in_the_line, vertices_in_the_line,
						2.0);
		}
	}
	delete ev;
}

/* distance between a line and a point */
static float
distance_line_point (Vector3f pos1, Vector3f dir1, Vector3f pos2)
{
  Vector3f tmp;
  float a, dist, dist2;
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
Cset_lines::cantzler_merge_edges (int n_candidates, float tolerance)
{
	int i,j;

	/* new extracted lines */
	//int n_extracted_lines_new2;
    Cextracted_line **extracted_lines_new = (Cextracted_line**)malloc(n_candidates*sizeof(Cextracted_line*));
	assert (extracted_lines_new);

	Vector3f *v1, *v2, *dir;
	v1  = (Vector3f*)malloc(n_candidates*sizeof(Vector3f));
	v2  = (Vector3f*)malloc(n_candidates*sizeof(Vector3f));
	dir = (Vector3f*)malloc(n_candidates*sizeof(Vector3f));
	assert (v1 && v2 && dir);

	// half candidates are edges
	for (i=0; i<n_candidates/2; i++)
	{
		int index = n_extracted_lines*rand()/(RAND_MAX+1.0);
		extracted_lines[index]->get_begin (v1[i]);
		extracted_lines[index]->get_end (v2[i]);
		extracted_lines[index]->get_direction (dir[i]);

		// create a new extracted line
		Vector3f cross = v1[i] ^ dir[i];
		int n_vertices_in_the_line = 2;
		int *ivertices_in_the_line = (int*)malloc(n_vertices_in_the_line*sizeof(int));
		Vector3f *vertices_in_the_line = (Vector3f*)malloc(n_vertices_in_the_line*sizeof(Vector3f));

		ivertices_in_the_line[0] = extracted_lines[index]->ivertices[0];
		ivertices_in_the_line[1] = extracted_lines[index]->ivertices[1];
		vertices_in_the_line[0] = v1[i];
		vertices_in_the_line[1] = v2[i];
		extracted_lines_new[i] = new Cextracted_line (dir[i], cross, v1[i], v2[i], n_vertices_in_the_line, ivertices_in_the_line, vertices_in_the_line);
		Vector3f v1v2 = v1[i] - v2[i];
		float length = v1v2.getLength ();
		extracted_lines_new[i]->set_weight (length);
	}

	// half candidates are vertices belonging to edges randomly chosen
	for (i=n_candidates/2; i<n_candidates; i++)
	{
		int index11, index12, index21, index22;

		// first vertex
		index11 = n_extracted_lines*rand()/(RAND_MAX+1.0);
		index12 = 1+(int)(2.0*rand()/(RAND_MAX+1.0));
		assert (index12 == 1 || index12 == 2);
		if (index12 == 1) extracted_lines[index11]->get_begin (v1[i]);
		if (index12 == 2) extracted_lines[index11]->get_end (v1[i]);

		// second vertex
		index21 = n_extracted_lines*rand()/(RAND_MAX+1.0);
		index22 = 1+(int)(2.0*rand()/(RAND_MAX+1.0));
		assert (index22 == 1 || index22 == 2);
		if (index22 == 1) extracted_lines[index21]->get_begin (v2[i]);
		if (index22 == 2) extracted_lines[index21]->get_end (v2[i]);

		// direction
		dir[i] = v2[i] - v1[i];
		dir[i].Normalize ();

		// create a new extracted line
		Vector3f cross = v1[i] ^ dir[i];
		int n_vertices_in_the_line = 2;
		int *ivertices_in_the_line = (int*)malloc(n_vertices_in_the_line*sizeof(int));
		assert (ivertices_in_the_line);
		Vector3f *vertices_in_the_line = (Vector3f*)malloc(n_vertices_in_the_line*sizeof(Vector3f));
		assert (vertices_in_the_line);

		ivertices_in_the_line[0] = extracted_lines[index11]->ivertices[index12-1];
		ivertices_in_the_line[1] = extracted_lines[index12]->ivertices[index22-1];
		vertices_in_the_line[0] = v1[i];
		vertices_in_the_line[1] = v2[i];
		extracted_lines_new[i] = new Cextracted_line (dir[i], cross, v1[i], v2[i], n_vertices_in_the_line, ivertices_in_the_line, vertices_in_the_line);
		extracted_lines_new[i]->set_weight (0.0);
	}


	// participation of the candidates
	float *n_candidates_for = (float*)malloc(n_candidates*sizeof(float));
	n_candidates_for = (float*)memset ((void*)n_candidates_for, 0, n_candidates*sizeof(float));
	int **candidates_for = (int**)malloc(n_candidates*sizeof(int));
	for (i=0; i<n_candidates; i++)
		candidates_for[i] = (int*)malloc(1000*sizeof(int));;

	/* evaluate each candidate line */
	for (i=0; i<n_candidates; i++)
	{
		for (j=0; j<n_candidates; j++)
		{
			if (i == j) continue;
			Vector3f pos1, dir1, pos21, pos22;
			extracted_lines_new[i]->get_begin (pos1);
			extracted_lines_new[i]->get_direction (dir1);
			extracted_lines_new[j]->get_begin (pos21);
			extracted_lines_new[j]->get_end (pos22);
			if (distance_line_point (pos1, dir1, pos21) < tolerance &&
				distance_line_point (pos1, dir1, pos22) < tolerance)
			{
				candidates_for[i][(int)n_candidates_for[i]] = j;
				n_candidates_for[i]++;
				assert (n_candidates_for[i] < 1000);
			}
		}
	}
	
	// merge the lines
	int *sorted = quicksort_indices (n_candidates_for, n_candidates);
	for (i=n_candidates-1; i>=0; i--)
	{
		int index1 = sorted[i];
		for (j=0; j<(int)n_candidates_for[index1]; j++)
		{
			int index2 = candidates_for[index1][j];
			extracted_lines_new[index1]->merge (extracted_lines_new[index2]);
			n_candidates_for[index2] = 0; // we exclude the j-th line
			// ...and delete the participation of the j-th line in all the other lines
			for (int k=0; k<(int)n_candidates; k++)
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

    Cextracted_line **extracted_lines_new2 = (Cextracted_line**)malloc(n_candidates*sizeof(Cextracted_line*));
	assert (extracted_lines_new2);
	for (i=0,j=0; i<n_candidates; i++)
	{
		if (n_candidates_for[i] != 0)
		{
			extracted_lines_new2[j] = extracted_lines_new[i];
			j++;
		}
		else
			delete extracted_lines_new[i];
	}
	n_candidates = j;

	n_extracted_lines = n_candidates;
    extracted_lines = extracted_lines_new2;

	apply_least_square_fitting ();
    compute_extremities ();

	free (n_candidates_for);
	for (i=0; i<n_candidates; i++)
		free (candidates_for[i]);
	free (candidates_for);
    free (v1);
	free (v2);
	free (dir);
}

