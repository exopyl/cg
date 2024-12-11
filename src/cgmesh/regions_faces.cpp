#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "regions_faces.h"
#include "regions_vertices.h"
#include "garray.h"

//class Cregions_vertices;

/**
* Constructor. Initializes with a Cmodel3d_half_edge.
*/
Cregions_faces::Cregions_faces (Mesh_half_edge *_mesh_half_edge)
{
	assert (_mesh_half_edge);
	mesh_half_edge  = _mesh_half_edge;
	size            = mesh_half_edge->m_nFaces;
	data            = new float[size];
	regions         = new int[size];
	selected_region = new int[size];
}

Cregions_faces::~Cregions_faces ()
{
	delete data;
	delete regions;
	delete selected_region;
}

/**
* Get the selected region.
*/
int*
Cregions_faces::get_selected_region (void)
{
	return selected_region;
}

/**
* Get the regions.
*/
int*
Cregions_faces::get_regions (void)
{
	return regions;
}

/**
* Get the 3D model as a Cmodel3d_half_edge.
*/
Mesh_half_edge*
Cregions_faces::get_mesh_half_edge (void)
{
	return mesh_half_edge;
}

/**
* Initializes the array data with the values contained in _data.
* The size is only used to check the comptability between the arrays.
* If the sizes are different, nothing is done.
*/
void
Cregions_faces::init (float *_data, int _size)
{
	if (size != _size) return;
	for (int i=0; i<size; i++)
		data[i] = _data[i];
}

/**
* Exports the indices of the selected faces into a file.
*/
void
Cregions_faces::export_selected_region_cloud_points (char *filename)
{
	int i,j;
	int *selected_vertices;
	int n_selected_vertices;
	
	FILE *ptr = fopen (filename,"wt");
	assert (ptr);
	
	int n_vertices;
	float *vertices = NULL;
	Face **faces;
	float *normales;
	
	if (mesh_half_edge)
    {
		n_vertices = mesh_half_edge->m_nVertices;
		vertices   = mesh_half_edge->m_pVertices;
		faces      = mesh_half_edge->m_pFaces;
		normales   = mesh_half_edge->m_pVertexNormals;
	}
	if (!vertices) return;
	
	selected_vertices = new int[n_vertices];
	assert (selected_vertices);
	n_selected_vertices = 0;
	
	for (i=0; i<size; i++)
    {
		if (selected_region[i] == 1)
		{
			int a = faces[i]->GetVertex(0);
			int b = faces[i]->GetVertex(1);
			int c = faces[i]->GetVertex(2);
			for (j=0; j<n_selected_vertices; j++)
			{
				if (selected_vertices[j] == a)
					break;
			}
			if (j == n_selected_vertices)
			{
				selected_vertices[n_selected_vertices] = a;
				n_selected_vertices++;
			}
			for (j=0; j<n_selected_vertices; j++)
			{
				if (selected_vertices[j] == b)
					break;
			}
			if (j == n_selected_vertices)
			{
				selected_vertices[n_selected_vertices] = b;
				n_selected_vertices++;
			}
			for (j=0; j<n_selected_vertices; j++)
			{
				if (selected_vertices[j] == c)
					break;
			}
			if (j == n_selected_vertices)
			{
				selected_vertices[n_selected_vertices] = c;
				n_selected_vertices++;
			}
		}
    }
	
	fprintf (ptr, "%d\n", n_selected_vertices);
	for (i=0; i<n_selected_vertices; i++)
		fprintf (ptr, "%f %f %f %f %f %f\n",
		vertices[3*selected_vertices[i]],
		vertices[3*selected_vertices[i]+1],
		vertices[3*selected_vertices[i]+2],
		normales[3*selected_vertices[i]],
		normales[3*selected_vertices[i]+1],
		normales[3*selected_vertices[i]+2]);
	fclose (ptr);
}

/**
* Create a segmentation of the 3D model.
*/
void
Cregions_faces::init_segmentation (float epsilon)
{
	int i, j;
	int count;
	Cgarray *neighborhood;
	int current_region;
	int n_faces = size;
	
	// determine for each face the label of the region containing it
	assert (data && regions && selected_region);
	current_region = 0;
	for (i=0; i<n_faces; i++) regions[i] = -1;
	
	if (mesh_half_edge)
	{
		FILE* ptr = fopen ("output.txt", "w");
		
		float *fn = mesh_half_edge->m_pFaceNormals;
		for (i=0; i<n_faces; i++)
		{
			if (regions[i] == -1)
			{
				regions[i] = current_region;
				count = 1;
				Vector3d n (fn[3*i], fn[3*i+1], fn[3*i+2]);
				fprintf (ptr, "face %d / %d : %f %f %f\n", i, n_faces, n.x, n.y, n.z);
				neighborhood = new Cgarray ();
				
				Che_edge *e = mesh_half_edge->m_edges_face[i];
				if (!e) continue;
				Che_edge *e_walk = e;
				do
				{
					if (e_walk->m_pair) neighborhood->add ((void*)e_walk->m_pair->m_face);
					else			    break;
					e_walk = e_walk->m_he_next;
				} while (e_walk && e_walk != e);
				
				for (j=0; j<neighborhood->get_size (); j++)
				{
					int index_walk = (int)(neighborhood->get_data (j));
					if (regions[index_walk] == -1)
					{
						Vector3d ntmp (fn[3*index_walk], fn[3*index_walk+1], fn[3*index_walk+2]);
						if (n * ntmp > epsilon)
						{
							regions[index_walk] = current_region;
							n.Set ((n.x*count+ntmp.x)/(count+1),
								(n.y*count+ntmp.y)/(count+1),
								(n.z*count+ntmp.z)/(count+1));
							
							Che_edge *e = mesh_half_edge->m_edges_face[index_walk];
							if (!e) continue;
							Che_edge *e_walk = e;
							do
							{
								if (e_walk->m_pair) neighborhood->add ((void*)e_walk->m_pair->m_face);
								else				break;
								e_walk = e_walk->m_he_next;
							} while (e_walk && e_walk != e);
							count++;
						}
					}
				}
				current_region++;
				delete neighborhood;
			}
		}
		fclose (ptr);
	}
}

void
Cregions_faces::clean_segmentation (float percentage)
{
	int i;
	float total_area = mesh_half_edge->GetArea ();
	float *areas_by_region;
	int n_regions = 0;
	int n_faces = mesh_half_edge->m_nFaces;
	
	for (i=0; i<n_faces; i++)
		if (n_regions < regions[i])
			n_regions = regions[i];
		n_regions++;
		
		areas_by_region = (float*)malloc(n_regions*sizeof(float));
		assert (areas_by_region);
		areas_by_region = (float*)memset((void*)areas_by_region, 0, n_regions*sizeof(float));
		float *areas = mesh_half_edge->GetAreas ();
		assert (areas);
		
		// determine the area of each region
		for (i=0; i<n_faces; i++)
			areas_by_region[regions[i]] += areas[i];
		/*
		int *nfaf = mesh_topology->get_n_faces_adjacent_faces ();
		int **faf = mesh_topology->get_faces_adjacent_faces ();
		for (i=0; i<n_regions; i++)
		{
			if (areas_by_region[i] < percentage*total_area)
			{
				int i_region = -1; // look for the neighbour
				for (j=0; j<n_faces; j++)
				{
					if (regions[j] == i)
					{
						for (k=0; k<nfaf[j]; k++)
						{
							if ((regions[faf[j][k]] != i)
								&&
								(i_region == -1 || areas_by_region[i_region] < areas_by_region[faf[j][k]]))
								i_region = regions[faf[j][k]];
						}
					}
				}
				if (i_region != -1)
				{
					for (j=0; j<n_faces; j++) // merge regions
						if (regions[j] == i)
							regions[j] = i_region;
						areas_by_region[i_region] += areas_by_region[i];
						areas_by_region[i] = 0;
				}
			}
		}
		*/
		
		free (areas_by_region);
		free (areas);
}

void
Cregions_faces::refresh_colors (void)
{
	int i, j, nv, nf;
	float *vc = NULL;
	Face **f = NULL;
	
	// get the array for the colors
	if (mesh_half_edge)
	{
		nv = mesh_half_edge->m_nVertices;
		nf = mesh_half_edge->m_nFaces;
		f  = mesh_half_edge->m_pFaces;
		vc = mesh_half_edge->m_pVertexColors;
	}
	assert (vc && f);
	if (!vc || !f) return;
	
	if (mesh_half_edge)
	{
		// paint the boundaries
		for (i=0; i<nv; i++)
		{
			int is_boundary = 0;
			int region_walk = regions[mesh_half_edge->m_edges_vertex[i]->m_face];
			Che_edge *e = mesh_half_edge->m_edges_vertex[i];
			if (!e) continue;
			Che_edge *e_walk = e;
			do
			{
				if (regions[e_walk->m_face] != region_walk) is_boundary = 1;			  
				
				e_walk = e_walk->m_he_next->m_he_next->m_pair;
			} while (e_walk && e_walk != e);
			
			if (is_boundary)
			{
				vc[3*i]   = 1.0f;
				vc[3*i+1] = 0.0f;
				vc[3*i+2] = 0.0f;
			}
			else
			{
				vc[3*i]   = 0.5f;
				vc[3*i+1] = 0.5f;
				vc[3*i+2] = 0.5f;
			}
			
		}
		// paint the selected region
		for (i=0; i<nf; i++)
		{
			if (selected_region[i] == 1)
			{
				/* first vertex */
				vc[3*f[i]->GetVertex(0)]     = 1.0;
				vc[3*f[i]->GetVertex(0)+1]   = 1.0;
				vc[3*f[i]->GetVertex(0)+2]   = 0.0;
				
				/* second vertex */
				vc[3*f[i]->GetVertex(1)]   = 1.0;
				vc[3*f[i]->GetVertex(1)+1] = 1.0;
				vc[3*f[i]->GetVertex(1)+2] = 0.0;
				
				/* third vertex */
				vc[3*f[i]->GetVertex(2)]   = 1.0;
				vc[3*f[i]->GetVertex(2)+1] = 1.0;
				vc[3*f[i]->GetVertex(2)+2] = 0.0;
			}
		}
		
	}
}

/*************/
/* selection */
/*************/
void
Cregions_faces::select_faces_by_id_region (int id)
{
	for (int i=0; i<size; i++)
		if (regions[i] == id)
			selected_region[i] = 1;
}

void
Cregions_faces::select_face (int id)
{
	if (id >= 0 && id < size)
		selected_region[id] = 1;
}

void
Cregions_faces::deselect_face (int id)
{
	if (id >= 0 && id < size)
		selected_region[id] = 0;
}

/**
* Selects the region containing the face id.
*/
void
Cregions_faces::select_faces (int id)
{
	for (int i=0; i<size; i++)
		if (regions[i] == regions[id])
			selected_region[i] = 1;
}

/**
* Deselects the region containing the face id.
*/
void
Cregions_faces::deselect_faces (int id)
{
	for (int i=0; i<size; i++)
		if (regions[i] == regions[id])
			selected_region[i] = 0;
}

/**
* Deselects all the selected regions.
*/
void
Cregions_faces::deselect (void)
{
	for (int i=0; i<size; i++)
		selected_region[i]=0;
}

/*****************************/
/*** Morphologic operators ***/
/*****************************/

/**
* Dilate the regions.
*/
void
Cregions_faces::dilate_regions (void)
{
	int *res = new int[size];
	int i;

	if (mesh_half_edge)
	{
		for (i=0; i<size; i++)
		{
			res[i] = regions[i];
			
			// look for the neighbours
			if (regions[i] == 0)
			{
				Che_edge *e = mesh_half_edge->m_edges_face[i];
				if (!e) continue;
				Che_edge *e_walk = e;
				do
				{
					if (regions[e_walk->m_pair->m_face] == 1)
						res[i] = 1;
					
					e_walk = e_walk->m_he_next;
				} while (e_walk && e_walk != e);
			}
		}
	}
	
	delete regions;
	regions = res;
}

/**
* Erode the regions.
*/
void
Cregions_faces::erode_regions (void)
{
	int *res = new int[size];
	int i;
	
	if (mesh_half_edge)
	{
		for (i=0; i<size; i++)
		{
			res[i] = regions[i];
			
			// look for the neighbours
			if (regions[i] == 1)
			{
				Che_edge *e = mesh_half_edge->m_edges_face[i];
				if (!e) continue;
				Che_edge *e_walk = e;
				do
				{
					if (regions[e_walk->m_pair->m_face] == 0)
						res[i] = 0;
					
					e_walk = e_walk->m_he_next;
				} while (e_walk && e_walk != e);
			}
		}
	}
	
	delete regions;
	regions = res;
}

/**
* Opening on the regions.
*/
void
Cregions_faces::opening_regions (void)
{
	erode_regions ();
	dilate_regions ();
}

/**
* Closing on the regions.
*/
void
Cregions_faces::closing_regions (void)
{
	dilate_regions ();
	erode_regions ();
}

/**
* Delete isolated regions.
*/
void
Cregions_faces::delete_isolated_regions (void)
{
	int i,j;
	/*
	for (i=0; i<size; i++)
    {
		if (regions[i] == 1)
		{
			for (j=0; j<mesh_topology->n_faces_adjacent_faces[i]; j++)
				if (regions[mesh_topology->faces_adjacent_faces[i][j]] == 1)
					break;
				if (j == mesh_topology->n_faces_adjacent_faces[i])
					regions[i] = 0;
		}
    }
	*/
}

/**
* Dilate the selected region.
*/
void
Cregions_faces::dilate_selected_region (void)
{
	int *res = new int[size];
	int i;
	

	if (mesh_half_edge)
	{
		for (i=0; i<size; i++)
		{
			res[i] = selected_region[i];
			
			// look for the neighbours
			if (selected_region[i] == 0)
			{
				Che_edge *e = mesh_half_edge->m_edges_face[i];
				if (!e) continue;
				Che_edge *e_walk = e;
				do
				{
					if (selected_region[e_walk->m_pair->m_face] == 1)
						res[i] = 1;
					
					e_walk = e_walk->m_he_next;
				} while (e_walk && e_walk != e);
			}
		}
	}
	
	delete selected_region;
	selected_region = res;
}

/**
* Erode the selected region.
*/
void
Cregions_faces::erode_selected_region (void)
{
	int *res = new int[size];
	int i;
	
	if (mesh_half_edge)
	{
		for (i=0; i<size; i++)
		{
			res[i] = selected_region[i];
			
			// look for the neighbours
			if (selected_region[i] == 1)
			{
				Che_edge *e = mesh_half_edge->m_edges_face[i];
				if (!e) continue;
				Che_edge *e_walk = e;
				do
				{
					if (selected_region[e_walk->m_pair->m_face] == 0)
						res[i] = 0;
					
					e_walk = e_walk->m_he_next;
				} while (e_walk && e_walk != e);
			}
		}
	}
	
	delete selected_region;
	selected_region = res;
}

void
Cregions_faces::opening_selected_region (void)
{
	erode_selected_region ();
	dilate_selected_region ();
}

void
Cregions_faces::closing_selected_region (void)
{
	dilate_selected_region ();
	erode_selected_region ();
}

/*****************/
/*** smoothing ***/
/*****************/
void
Cregions_faces::smoothing_laplacian (void)
{
	/*
	if (mesh_topology)
	{
		int i;
		int *faces = mesh_topology->get_faces ();
		Cregions_vertices *regions_vertices = new Cregions_vertices (mesh_topology);
		if (!regions_vertices)
			return;
		for (i=0; i<size; i++)
			if (selected_region[i] == 1)
			{
				regions_vertices->select_vertex (faces[3*i]);
				regions_vertices->select_vertex (faces[3*i+1]);
				regions_vertices->select_vertex (faces[3*i+2]);
			}
			regions_vertices->smoothing_laplacian_selected_region ();
	}
	*/
}

void
Cregions_faces::smoothing_taubin (void)
{
	/*
	if (mesh_topology)
	{
		int i;
		int *faces = mesh_topology->get_faces ();
		Cregions_vertices *region_vertices = new Cregions_vertices (mesh_topology);
		if (!region_vertices)
			return;
		for (i=0; i<size; i++)
			if (selected_region[i] == 1)
			{
				region_vertices->select_vertex (faces[3*i]);
				region_vertices->select_vertex (faces[3*i+1]);
				region_vertices->select_vertex (faces[3*i+2]);
			}
			region_vertices->smoothing_taubin_selected_region ();
	}
	*/
}

/***************/
/*** fitting ***/
/***************/

/**
* Fits a plane through the selected faces.
*/
Plane* Cregions_faces::plane_fitting  (void)
{
	Vector3f center (0.0, 0.0, 0.0);
	Vector3f normale (0.0, 0.0, 0.0);
	int i, n_selected_faces = 0;
	float *v;
	unsigned int *f;
	float *fn;
	
	if (mesh_half_edge)
	{
		v  = mesh_half_edge->m_pVertices;
		f  = mesh_half_edge->GetTriangles ();
		fn = mesh_half_edge->m_pFaceNormals;
	}
	
	for (i=0; i<size; i++)
    {
		if (selected_region[i] == 1)
		{
			
			Vector3f v1 (v[3*f[3*i]], v[3*f[3*i]+1], v[3*f[3*i]+2]);
			Vector3f v2 (v[3*f[3*i+1]], v[3*f[3*i+1]+1], v[3*f[3*i+1]+2]);
			Vector3f v3 (v[3*f[3*i+2]], v[3*f[3*i+2]+1], v[3*f[3*i+2]+2]);
			Vector3f barycenter ((v1.x + v2.x + v3.x) / 3.0,
				(v1.y + v2.y + v3.y) / 3.0,
				(v1.z + v2.z + v3.z) / 3.0);
			
			center = center + barycenter;
			
			Vector3f n (fn[3*i], fn[3*i+1], fn[3*i+2]);
			normale = normale + n;
			
			n_selected_faces++;
		}
	}
	
	center.x /= n_selected_faces;
	center.y /= n_selected_faces;
	center.z /= n_selected_faces;
	
	normale.x /= n_selected_faces;
	normale.y /= n_selected_faces;
	normale.z /= n_selected_faces;
	
	Plane *plane = new Plane (center, normale);
	return plane;
}

/**********/
/* colors */
/**********/
void
Cregions_faces::set_color_regions (float r, float g, float b)
{
	r_regions = r;
	g_regions = g;
	b_regions = b;
}

void
Cregions_faces::set_color_selected_region (float r, float g, float b)
{
	r_selected_region = r;
	g_selected_region = g;
	b_selected_region = b;
}

void
Cregions_faces::set_color_common_face (float r, float g, float b)
{
	r_common_face = r;
	g_common_face = g;
	b_common_face = b;
}
