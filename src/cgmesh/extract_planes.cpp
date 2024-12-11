#include <stdlib.h>
#include <assert.h>

#include "extract_planes.h"
#include "regions_faces.h"

Cextract_planes::Cextract_planes (Mesh_half_edge *_model)
{
	assert (_model);
	model = _model;
	n_planes = 0;
	planes = NULL;
}

void
Cextract_planes::add_plane (VectorizedPlane *plane)
{
  planes = (VectorizedPlane**)realloc(planes, ((n_planes+1)*sizeof(VectorizedPlane*)));
  assert (planes);
  planes[n_planes] = plane;
  n_planes++;
}

void Cextract_planes::compute (float threshold, float percentage)
{
  int i,j,k;
  float *v = model->m_pVertices;
  int nv = model->m_nVertices;
  //float *vc = model->get_vertices_colors ();
  unsigned int *f = model->GetTriangles ();
  int nf = model->m_nFaces;

  Cregions_faces *model3d_regions = new Cregions_faces (model);
  model3d_regions->init_segmentation (threshold);

  float *areas = model->GetAreas ();
  total_area = (float)model->GetArea ();

  int *regions = model3d_regions->get_regions ();
  int imax = regions[0];
  for (i=0; i<nf; i++)
	  if (imax < regions[i]) imax = regions[i];
  printf ("imax = %d\n", imax);

  for (int iregion=0; iregion<imax; iregion++)
  {
	  float area = 0.0;
	  for (j=0; j<nf; j++)
		  if (regions[j] == iregion) area += areas[j];
	  if (area > percentage*total_area)
	  {
		  printf ("%f\n", area = area);
		  model3d_regions->select_faces_by_id_region (iregion);
          Plane *plane = model3d_regions->plane_fitting ();
		  VectorizedPlane *vPlane = new VectorizedPlane(NULL, 0, plane);
		  vPlane->set_area (area);

		  int *selected_region = model3d_regions->get_selected_region ();

		  // identify the faces selected
		  int *ftmp = NULL, nftmp = 0;
		  for (i=0; i<nf; i++)
			  if (selected_region[i] == 1)
				  nftmp++;

		  ftmp = (int*)malloc(nftmp*sizeof(int));
		  int iwalk = 0;
		  for (i=0; i<nf; i++)
			  if (selected_region[i] == 1)
				  ftmp[iwalk++] = i;

		  /*
		  Plane *gpplane = new Cgeometric_primitive (ftmp,
				nftmp,
				GEOMETRIC_PRIMITIVE_PLANE,
				(void*)plane);
		  add_plane (gpplane);
		  */
		  model3d_regions->deselect ();
	  }
  }
  printf ("total_area = %f\n", total_area);

}

void Cextract_planes::Dump (char *filename)
{
	FILE *ptr = fopen (filename, "w");
	fprintf (ptr, "%d\n", n_planes);
	for (int i=0; i<n_planes; i++)
	{
		Vector3f pt, n;
		VectorizedPlane *vplane = planes[i];
		Plane *plane = (Plane*)vplane;
		plane->position (pt);
		plane->get_normale (n);
		fprintf (ptr, "p %f %f %f %f %f %f %f %f\n", vplane->get_area (), total_area, pt.x, pt.y, pt.z, n.x, n.y, n.z);

		unsigned int nf = vplane->get_n_elements ();
		unsigned int *f = vplane->get_elements ();

		fprintf (ptr, "%d\n", nf);
		for (int j=0; j<nf; j++)
			fprintf (ptr, "%d\n", f[j]);

	}
	fclose (ptr);
}
