#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <memory>
#include <vector>

#include "slicer.h"
#include "clipper.h"

Cmodel3d_half_edge_sliced::Cmodel3d_half_edge_sliced (Mesh_half_edge *_mesh, int dir=ALONGOZ, float _step_slice=1.0)
{
	assert (_mesh);
	model      = _mesh;
	n_slices   = 0;
	slices     = nullptr;
	n_contours = nullptr;
	step_slice = _step_slice;
	direction  = dir;
	
	scan_model (direction);
}

Cmodel3d_half_edge_sliced::~Cmodel3d_half_edge_sliced ()
{
}

void
Cmodel3d_half_edge_sliced::scan_model_along_Oz (void)
{
	float *vertices = model->m_pMesh->m_pVertices.data();
	int n_vertices  = model->m_pMesh->m_nVertices;
	int i,j,k;

	/* look for the extremities in the Oz direction */
	float z_min = vertices[2];
	float z_max = vertices[2];
	for (i=1; i<n_vertices; i++)
    {
		if (vertices[3*i+2] > z_max)
			z_max = vertices[3*i+2];
		if (vertices[3*i+2] < z_min)
			z_min = vertices[3*i+2];
    }
	zmin = z_min;
	//printf ("z_min : %f\nz_max : %f\n", z_min, z_max);
	
	/* alloc memory */
	n_slices = (int)((z_max-z_min)/step_slice);
	//printf ("number of slices : %d\n", n_slices);
	slices = (Polygon2***)malloc(n_slices*sizeof(Polygon2**));
	assert (slices);
	n_contours = (int*)malloc(n_slices*sizeof(int));
	assert (n_contours);
	
	/* scan the model */
	// Le clipper n'etait jamais detruit : une fuite par appel, et la classe est
	// instanciee a chaque construction du slicer (cpp:S3584, slicer.cpp:99).
	std::unique_ptr<Cmodel3d_half_edge_clipper> clipper
		(new Cmodel3d_half_edge_clipper (model));
	float z_walk= z_min;
	k=0;
	while (z_walk < z_max) /* for each slice */
    {
		/* results */ 
		float **intersections;
		int n_intersections;
		int *n_vertices;
		Vector3d pt (0.0, 0.0, z_walk);
		Vector3d n (0.0, 0.0, 1.0);
		clipper->set_plane (pt, n);
		clipper->get_intersections (&n_intersections, &n_vertices, &intersections);
		
		if (n_intersections)
		{
			// L'allocation etait faite AVANT ce test, puis perdue par le
			// `slices[k] = nullptr` de la branche else : une fuite par tranche vide.
			slices[k] = (Polygon2**)malloc(n_intersections*sizeof(Polygon2*));
			assert (slices[k]);
			n_contours[k] = n_intersections;
			for (i=0; i<n_intersections; i++) /* for each contour in the current slice */
			{
				std::vector<float> xx(n_vertices[i]);
				std::vector<float> yy(n_vertices[i]);
				for (j=0; j<n_vertices[i]; j++)
				{
					xx[j] = intersections[i][3*j];
					yy[j] = intersections[i][3*j+1];
				}
				slices[k][i] = new Polygon2 ();
				slices[k][i]->input (xx.data(), yy.data(), n_vertices[i]);
			}
		}
		else
		{
			n_contours[k] = 0;
			slices[k] = nullptr;
		}
		
		z_walk += step_slice; /* step */
		k++;
    }
}

void
Cmodel3d_half_edge_sliced::scan_model_along_Ox (void)
{
	float *vertices = model->m_pMesh->m_pVertices.data();
	int n_vertices  = model->m_pMesh->m_nVertices;
	int i,j,k;

	/* look for the extremities in the Oz direction */
	float x_min = vertices[0];
	float x_max = vertices[0];
	for (i=1; i<n_vertices; i++)
    {
		if (vertices[3*i] > x_max)
			x_max = vertices[3*i];
		if (vertices[3*i] < x_min)
			x_min = vertices[3*i];
    }
	xmin = x_min;
	printf ("x_min : %f\nx_max : %f\n", x_min, x_max);
	
	/* alloc memory */
	n_slices = (int)((x_max-x_min)/step_slice);
	printf ("number of slices : %d\n", n_slices);
	slices = (Polygon2***)malloc(n_slices*sizeof(Polygon2**));
	assert (slices);
	n_contours = (int*)malloc(n_slices*sizeof(int));
	assert (n_contours);
	
	/* scan the model */
	// Meme fuite que dans la variante Oz (cpp:S3584, slicer.cpp:172).
	std::unique_ptr<Cmodel3d_half_edge_clipper> clipper
		(new Cmodel3d_half_edge_clipper (model));
	float x_walk= x_min;
	k=0;
	while (x_walk < x_max) /* for each slice */
    {
		/* results */ 
		int n_intersections;
		int *n_vertices;
		float **intersections;
		Vector3d pt (x_walk, 0.0, 0.0);
		Vector3d n (1.0, 0.0, 0.0);
		clipper->set_plane (pt, n);
		clipper->get_intersections (&n_intersections, &n_vertices, &intersections);
		
		if (n_intersections)
		{
			// L'allocation etait faite AVANT ce test, puis perdue par le
			// `slices[k] = nullptr` de la branche else : une fuite par tranche vide.
			slices[k] = (Polygon2**)malloc(n_intersections*sizeof(Polygon2*));
			assert (slices[k]);
			n_contours[k] = n_intersections;
			for (i=0; i<n_intersections; i++) /* for each contour in the current slice */
			{
				std::vector<float> yy(n_vertices[i]);
				std::vector<float> zz(n_vertices[i]);
				for (j=0; j<n_vertices[i]; j++)
				{
					yy[j] = intersections[i][3*j+1];
					zz[j] = intersections[i][3*j+2];
				}
				slices[k][i] = new Polygon2 ();
				slices[k][i]->input (yy.data(), zz.data(), n_vertices[i]);
			}
		}
		else
		{
			n_contours[k] = 0;
			slices[k] = nullptr;
		}
		
		x_walk += step_slice; /* step */
		k++;
    }
}

void
Cmodel3d_half_edge_sliced::scan_model (int dir)
{
	switch (dir)
    {
    case ALONGOX:
		scan_model_along_Ox ();
		break;
    case ALONGOZ:
		scan_model_along_Oz ();
		break;
    default:
		break;
    }
}

void
Cmodel3d_half_edge_sliced::get_areas (float **areas, int *size)
{
	int i,j;
	float *a = (float*)malloc(n_slices*sizeof(float));
	assert (a);
	
	/* get the areas */
	for (i=0; i<n_slices; i++)
    {
		switch (n_contours[i])
		{
		case 0:
			a[i] = 0.0;
			break;
		case 1:
			a[i]  = fabs (slices[i][0]->area ());
			break;
		default:
			{
				int imax = 0;
				for (j=1; j<n_contours[i]; j++)
					if (fabs (slices[i][imax]->area ()) < fabs (slices[i][j]->area ()))
						imax = j;
					a[i]  = fabs (slices[i][imax]->area ());
			}
		}
    }
	*areas = a;
	*size  = n_slices;
}

void
Cmodel3d_half_edge_sliced::get_slice (int index, Polygon2 ***slice, int *nc)
{
	if (index<n_slices && index>=0)
    {
		*nc = n_contours[index];
		*slice = slices[index];
    }
	else
    {
		*nc = 0;
		*slice = nullptr;
    }
}

void
Cmodel3d_half_edge_sliced::look_at_symmetry (void)
{
	int i,j;

	/* interval for the histogram */
	float t = 50.0;

	if (n_slices <= 0)
		return;

	// Les trois tableaux etaient des malloc jamais liberes : cpp:S3584 aux
	// lignes 281 (iareas), 302 (histo) et 321 (areas). iareas etait meme
	// dimensionne en sizeof(float) pour un int* -- juste par coincidence de taille.
	//
	// Le remplissage a zero corrige au passage une lecture de memoire non
	// initialisee : le `case 0` ci-dessous n'affecte QUE iareas[i], laissant
	// areas[i] indetermine, valeur ensuite comparee puis divisee pour indexer
	// l'histogramme.
	std::vector<float> areas  (n_slices, 0.0f);
	std::vector<int>   iareas (n_slices, -1);

	/* get the areas */
	for (i=0; i<n_slices; i++)
	{
		switch (n_contours[i])
		{
		case 0:
			iareas[i] = -1;
			break;
		case 1:
			iareas[i] = 0;
			areas[i]  = fabs (slices[i][0]->area ());
			break;
		default:
			{
				int imax = 0;
				for (j=1; j<n_contours[i]; j++)
					if (fabs (slices[i][imax]->area ()) < fabs (slices[i][j]->area ()))
						imax = j;
				iareas[i] = imax;
				areas[i]  = fabs (slices[i][imax]->area ());
			}
		}
		//printf ("area[%d][%d] : %f\n", i, iareas[i], areas[i]);
	}

	/* find the area appearing the most often */

	/* find the higher area */
	float area_max = areas[0];
	for (i=0; i<n_slices; i++)
		if (areas[i] > area_max) area_max = areas[i];

	/* alloc memory for histogram */
	// n_histo valait 0 des que area_max < t, et l'increment ci-dessous ecrivait
	// alors hors du tableau.
	int n_histo = (int)(area_max / t);
	if (n_histo <= 0)
		return;
	std::vector<int> histo (n_histo, 0);

	/* fill the histogram */
	for (i=0; i<n_slices; i++)
	{
		// areas[i] peut valoir area_max, donc l'indice atteignait n_histo :
		// une case au-dela du tableau. On le ramene dans le dernier casier.
		int bin = (int)(areas[i]/t);
		if (bin < 0)        bin = 0;
		if (bin >= n_histo) bin = n_histo - 1;
		histo[bin]++;
	}

	/* find the maximal value in the histogram */
	int imax = 0;
	for (i=1; i<n_histo; i++)
		if (histo[i] > histo[imax]) imax = i;
	float mean_area = imax*t;
	//printf ("%f\n", mean_area);

	/* list of the polygons whose the area is near of area_mean  */
	for (i=0; i<n_slices; i++)
	{
		if (fabs(areas[i]-mean_area) < t/2.0)
		{
			if (n_contours[i] <= 0 || slices[i] == nullptr)
				continue;
			//printf ("-> %d\n", i);
			float xc, yc;
			float theta;
			// Etait un `new Polygon2` jamais detruit, donc une fuite par tranche
			// retenue. Un objet local suffit : il ne sort pas de la boucle.
			Polygon2 pol;
			pol.input (slices[i][0], INTERPOLATION_LINEAR, 200);
			pol.centerize ();
			pol.search_symmetry_zabrodsky (&xc, &yc, &theta);
			//printf ("symmetry = %f\n", theta);
			//printf ("%f*x\n", sin(theta)/cos(theta));
		}
	}
}

void
Cmodel3d_half_edge_sliced::dump (char *prefix)
{
	int i,j;
	printf ("n_slices : %d\n", n_slices);
	for (i=0; i<n_slices; i++)
    {
		printf ("slice %d\n", i);
		
		/* output the polygons of intersection */
		char filename[256];
		sprintf (filename, "%s_%03d.txt", prefix, i);
		if (n_contours[i] > 0)
			slices[i][0]->output (filename);
		
		for (j=0; j<n_contours[i]; j++)
		{
			Polygon2 *contour = slices[i][j];
			assert (contour);
			float l = contour->length (INTERPOLATION_LINEAR);
			printf (" -> length of contour %d : %f\n", j, l);
		}
    }
}
