#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "geometric_primitives.h"

VectorizedElement::VectorizedElement (unsigned int *pElements, unsigned int nElements, VectorizedElementType type)
{
	m_pElements = pElements;
	m_nElements = nElements;
	m_type = type;
}

VectorizedPoint::VectorizedPoint (unsigned int *pElements, unsigned int nElements, Vector3f pos)
	: VectorizedElement (pElements, nElements, GEOMETRIC_PRIMITIVE_POINT)
{
	m_position = pos;
}

VectorizedLine::VectorizedLine (unsigned int *pElements, unsigned int nElements, Vector3f start, Vector3f end)
	: VectorizedElement (pElements, nElements, GEOMETRIC_PRIMITIVE_LINE)
{
	m_start = start;
	m_end = end;
}

VectorizedPlane::VectorizedPlane (unsigned int *pElements, unsigned int nElements, Plane *pPlane)
	: VectorizedElement (pElements, nElements, GEOMETRIC_PRIMITIVE_PLANE)
{
	m_pPlane = pPlane;
}


Cgeometric_primitives::Cgeometric_primitives (Mesh_half_edge *_model, char *filename)
{
	FILE *ptr = fopen (filename, "r");
	if (ptr == NULL) return;

	ngp = 0;
	gp = NULL;

	model = _model;

	int igp, ngp;
    char *buffer = (char*)malloc(512*sizeof(char));
	char prefix[5];

	fscanf (ptr, "%d", &ngp);
	printf ("%d geometric primitives\n", ngp);

	for (igp=0; igp<ngp; igp++)
	//while (1)
	{
printf ("igp = %d / %d\n", igp, ngp);
		buffer[0]='\0';
      buffer = fgets (buffer, 512, ptr);
      buffer = fgets (buffer, 512, ptr); // don't ask me why there are the same line twice
      //if (feof(ptr)) break;
      sscanf (buffer, "%s", prefix);
	  printf ("%s\n", buffer);

      if (!strcmp(prefix, "s")) // read a simple point as a geometric primitive
	{
	  Vector3f *pt = (Vector3f*)malloc(sizeof(Vector3f));
	  float x, y, z;

	  // read the informations about the point
	  sscanf (buffer, "%s %f %f %f", prefix, &x, &y, &z);
	
	  // create the geometric primitive
	  VectorizedPoint *geometric_primitive =
	    new VectorizedPoint (NULL,
					      0,
					      VectorizedElement::GEOMETRIC_PRIMITIVE_POINT);
	  geometric_primitive->m_position.Set (x, y, z);

	  // add the geometric primitive
	  this->add_geometric_primitive ((VectorizedElement*)geometric_primitive);
	
	  prefix[0] = '\0';
	}
      if (!strcmp(prefix, "l")) // read a line as a geometric primitive
	{
		/*
	  Cline *line;
	  Vector3f orig, dir;
	  float x_orig, y_orig, z_orig;
	  float x_dir, y_dir, z_dir;
	  float begin, end;
	  int n_vertices_concerned=0;
	  int *vertices_concerned;

	  // read the informations about the line
	  sscanf (buffer, "%s %f %f %f %f %f %f %f %f", prefix,
		  &x_orig, &y_orig, &z_orig, &x_dir, &y_dir, &z_dir,
		  &begin, &end);

	  orig.Set (x_orig, y_orig, z_orig);
	  dir.Set (x_dir, y_dir, z_dir);

	  // create the line
	  line = new Cline (orig, dir, begin, end);

	  // read the number of vertices concerned
	  fscanf (ptr, "%d", &n_vertices_concerned);
	
	  // read the indices of the vertices concerned
	  vertices_concerned = (int*)malloc(n_vertices_concerned*sizeof(int));
	  assert (vertices_concerned);
	  for (int i=0; i<n_vertices_concerned; i++)
	    fscanf (ptr, "%d", &vertices_concerned[i]);

	  // create the geometric primitive
	  VectorizedElement *geometric_primitive =
	    new VectorizedElement (vertices_concerned,
					      n_vertices_concerned,
					      VectorizedElement::GEOMETRIC_PRIMITIVE_LINE,
					      (void*)line);

	  // add the geometric primitive
	  this->add_geometric_primitive (geometric_primitive);

	  prefix[0] = '\0';
	  */
	}
     
	if (!strcmp(prefix, "c")) // read a circle as a geometric primitive
	{
		/*
	  Ccircle *circle;
	  float center_x, center_y, center_z, radius, angle_begin, angle_end;
	  int n_vertices_concerned=0;
	  int *vertices_concerned;

	  // read the informations about the circle
	  sscanf (buffer, "%s %f %f %f %f %f %f", prefix,
		  &center_x, &center_y, &center_z, &radius, &angle_begin, &angle_end);
	
	  // create the circle
	  circle = new Ccircle (center_x, center_y, center_z, radius, angle_begin, angle_end);

	  // read the number of vertices concerned
	  fscanf (ptr, "%d", &n_vertices_concerned);

	  // read the indices of the vertices concerned
	  vertices_concerned = (int*)malloc(n_vertices_concerned*sizeof(int));
	  assert (vertices_concerned);
	  for (int i=0; i<n_vertices_concerned; i++)
	    fscanf (ptr, "%d", &vertices_concerned[i]);

	  // create the geometric primitive
	  VectorizedElement *geometric_primitive =
	    new VectorizedElement (vertices_concerned,
					      n_vertices_concerned,
					      VectorizedElement::GEOMETRIC_PRIMITIVE_CIRCLE,
					      (void*)circle);

	  // add the geometric primitive
	  this->add_geometric_primitive (geometric_primitive);

	  prefix[0] = '\0';
	  */
	}
      if (!strcmp(prefix, "p")) // read a plane as a geometric primitive
	{
	  Plane *plane;
	  Vector3f pt, normale;
	  float pt_x, pt_y, pt_z, normale_x, normale_y, normale_z;
	  unsigned int n_faces_concerned=0;
	  unsigned int *faces_concerned;
	  float area, total_area;

	  // read the informations about the circle
	  sscanf (buffer, "%s %f %f %f %f %f %f %f %f", prefix,
		  &area, &total_area, &pt_x, &pt_y, &pt_z, &normale_x, &normale_y, &normale_z);
	
	  // create the plane
	  pt.Set (pt_x, pt_y, pt_z);
	  normale.Set (normale_x, normale_y, normale_z);
	  plane = new Plane (pt, normale);

	  // read the number of faces concerned
	  fscanf (ptr, "%d", &n_faces_concerned);

	  // read the indices of the faces concerned
	  faces_concerned = (unsigned int*)malloc(n_faces_concerned*sizeof(unsigned int));
	  assert (faces_concerned);
	  for (int i=0; i<n_faces_concerned; i++)
	    fscanf (ptr, "%d", &faces_concerned[i]);

	  // create hte geometric primitive
	  VectorizedPlane *geometric_primitive =
	    new VectorizedPlane (faces_concerned,
					      n_faces_concerned,
					      plane);

	  // add the geometric primitive
	  this->add_geometric_primitive ((VectorizedElement*)geometric_primitive);

	  prefix[0] = '\0';
	}
    }
  fclose (ptr);
  free (buffer);
}

void
Cgeometric_primitives::add_geometric_primitive (VectorizedElement *geometric_primitive)
{
  gp = (VectorizedElement**)realloc(gp, ((ngp+1)*sizeof(VectorizedElement*)));
  assert (gp);
  gp[ngp] = geometric_primitive;
  ngp++;
}


void
Cgeometric_primitives::refresh_vertices_colors (void)
{
	for (int i=0; i<ngp; i++)
	{
		printf ("primitive %d / %d\n", i+1, ngp);
		VectorizedElement *p = gp[i];
		VectorizedElement::VectorizedElementType type = p->get_type ();
		switch (p->get_type ())
		{
		case VectorizedElement::GEOMETRIC_PRIMITIVE_POINT:
			printf ("Point\n");
			break;
		case VectorizedElement::GEOMETRIC_PRIMITIVE_LINE:
			printf ("Point\n");
			break;
		case VectorizedElement::GEOMETRIC_PRIMITIVE_PLANE:
				/*			printf ("Plane\n");
			{
		
				int i,j,k,l;
				float *vc = model->get_vertices_colors ();
				int *f = model->get_faces ();
				int nf = model->get_n_faces ();

				int nfselected = p->get_n_vertices ();
				int *fselected = p->get_vertices ();
				int *selected_region = (int*)malloc(nf*sizeof(int));
				assert (selected_region);

				for (i=0; i<nf; i++)         selected_region[i] = 0;
				for (i=0; i<nfselected; i++) selected_region[fselected[i]] = 1;

				// paint the interior
				for (i=0; i<nfselected; i++)
				{
					int index = fselected[i];
					// first vertex
					vc[3*f[3*index]]     = 1.0;
					vc[3*f[3*index]+1]   = 1.0;
					vc[3*f[3*index]+2]   = 0.0;
	  
					// second vertex
					vc[3*f[3*index+1]]   = 1.0;
					vc[3*f[3*index+1]+1] = 1.0;
					vc[3*f[3*index+1]+2] = 0.0;
	  
					// third vertex
					vc[3*f[3*index+2]]   = 1.0;
					vc[3*f[3*index+2]+1] = 1.0;
					vc[3*f[3*index+2]+2] = 0.0;
				}
				  // paint the boundary

				int *n_faces_adjacent_faces = model->get_n_faces_adjacent_faces ();
				int **faces_adjacent_faces = model->get_faces_adjacent_faces ();
		  for (i=0; i<nf; i++)
		  {
			  if (selected_region[i])
			  {
			    for (k=1; k<n_faces_adjacent_faces[i]; k++)
				if (faces_adjacent_faces[i][k] < nf && selected_region[i] != selected_region[faces_adjacent_faces[i][k]])
				  {
				  // first vertex
				  vc[3*f[3*i]]     = 16.0/255.0;
				  vc[3*f[3*i]+1]   = 180.0/255.0;
				  vc[3*f[3*i]+2]   = 0.0;
	  
				  // second vertex
				  vc[3*f[3*i+1]]   = 16.0/255.0;
				  vc[3*f[3*i+1]+1] = 180.0/255.0;
				  vc[3*f[3*i+1]+2] = 0.0;
	  
				  // third vertex
				  vc[3*f[3*i+2]]   = 16.0/255.0;
				  vc[3*f[3*i+2]+1] = 180.0/255.0;
				  vc[3*f[3*i+2]+2] = 0.0;
				  }
			  }
		   }

				  free (selected_region);
				}
				*/
			break;
		case VectorizedElement::GEOMETRIC_PRIMITIVE_CIRCLE:
			printf ("Circle\n");
			break;
		default:
			printf ("unknown type\n");
			break;
		}
	}
}



int Cgeometric_primitives::get_n_geometric_primitives (void)
{
  return ngp;
}

VectorizedElement* Cgeometric_primitives::get_geometric_primitive (int index)
{
  return (index>=0 && index<ngp)? gp[index] : NULL;
}

void Cgeometric_primitives::dump (void)
{
	for (int i=0; i<ngp; i++)
	{
		printf ("primitive %d / %d\n", i+1, ngp);
		VectorizedElement *p = gp[i];
		VectorizedElement::VectorizedElementType type = p->get_type ();

		switch (p->get_type ())
		{
		case VectorizedElement::GEOMETRIC_PRIMITIVE_POINT:
			printf ("Point\n");
			break;
		case VectorizedElement::GEOMETRIC_PRIMITIVE_LINE:
			printf ("Line\n");
			{
				/*
				Cline *line = (Cline*)p->get_geometric_primitive ();
				Vector3f orig, dir;
				line->get_origin (orig);
				line->get_direction (dir);
				orig.dump();
				dir.dump();

				float begin = line->get_begin ();
				float end   = line->get_end ();
				printf ("%f -> %f\n\n", begin, end);
				*/
			}
			break;
		case VectorizedElement::GEOMETRIC_PRIMITIVE_PLANE:
			printf ("Plane\n");
			break;
		case VectorizedElement::GEOMETRIC_PRIMITIVE_CIRCLE:
			printf ("Circle\n");
			break;
		default:
			printf ("unknown type\n");
			break;
		}
	}
}
