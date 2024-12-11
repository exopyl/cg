#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "extracted_line.h"
#include "geometric_primitives.h"

/* static functions */
// compute the closest point from the line (return the relative position)
static float coordinate_closest_point (Vector3f pt, Vector3f origin, Vector3f direction)
{
  Vector3f tmp = pt - origin;
  return tmp * direction; // direction is already normalized
}

Cextracted_line::Cextracted_line (Vector3f _pluecker1, Vector3f _pluecker2, Vector3f _begin, Vector3f _end, int _n_vertices, int *_ivertices, Vector3f *_vertices, float _weight)
{
  pluecker1 = _pluecker1;
  pluecker2 = _pluecker2;
  begin = _begin;
  end = _end;

  n_vertices = _n_vertices;

  ivertices = (int*)malloc(n_vertices*sizeof(int));
  assert (ivertices);
  ivertices = (int*)memcpy (ivertices, _ivertices, n_vertices*sizeof(int));
  //for (int i=0; i<n_vertices; i++) ivertices[i] = _ivertices[i];

  vertices = (Vector3f*)malloc(n_vertices*sizeof(Vector3f));
  assert (vertices);
  vertices = (Vector3f*)memcpy (vertices, _vertices, n_vertices*sizeof(Vector3f));
  //for (int i=0; i<n_vertices; i++) v3d_copy (vertices[i], _vertices[i]);
  update_parameters ();
  weight = _weight;
}

Cextracted_line::~Cextracted_line ()
{
  if (ivertices) free (ivertices);
  if (vertices)  free (vertices);
}

/* setters */
void
Cextracted_line::add_vertex (int _ivertex, Vector3f _vertex)
{
  n_vertices++; 
  ivertices = (int*)realloc((void*)ivertices, n_vertices*sizeof(int));
  vertices = (Vector3f*)realloc((void*)vertices, n_vertices*sizeof(Vector3f));

  ivertices[n_vertices-1] = _ivertex;
  vertices[n_vertices-1] = _vertex;
}
  
/* getters */
int  Cextracted_line::get_n_vertices (void)      { return n_vertices; }
int* Cextracted_line::get_ivertices  (void)      { return ivertices; }
Vector3f* Cextracted_line::get_vertices   (void)      { return vertices; }
void Cextracted_line::get_direction (Vector3f d)      { d = pluecker1; }
void Cextracted_line::get_begin     (Vector3f _begin) { _begin = begin; }
void Cextracted_line::get_end       (Vector3f _end)   { _end = end; }
void Cextracted_line::get_center (Vector3f _center)
{ _center.Set ((begin.x + end.x) / 2.0, (begin.y + end.y) / 2.0, (begin.z + end.z) / 2.0); }

/* misc */
int
Cextracted_line::is_index_in_line (int index)
{
	for (int i=0; i<n_vertices; i++)
		if (ivertices[i] == index)
			return 1;
	return 0;
}

/* weight */
float Cextracted_line::get_weight (void) { return weight; };
void Cextracted_line::set_weight (float _weight) { weight = _weight; };

/* color */
void Cextracted_line::set_color (float _r, float _g, float _b)    { r = _r; g = _g; b = _b; }
void Cextracted_line::get_color (float *_r, float *_g, float *_b) { *_r= r; *_g= g; *_b= b; }

/* merging */
void
Cextracted_line::merge (Cextracted_line *line)
{
  int k;
  /* copy the vertices from line2 to line1 */
  vertices = (Vector3f*)realloc(vertices, (n_vertices+line->n_vertices)*sizeof(Vector3f));
  for (k=0; k<line->n_vertices; k++)
    vertices[n_vertices+k] = line->vertices[k];
  
  /* copy the ivertices from line2 to line1 */
  ivertices = (int*)realloc(ivertices, (n_vertices+line->n_vertices)*sizeof(int));
  for (k=0; k<line->n_vertices; k++)
    ivertices[n_vertices+k] = line->ivertices[k];
  
  n_vertices += line->n_vertices;

  weight += line->weight;

  update_parameters ();
}

// apply a least square fitting to adjust the line
void Cextracted_line::apply_least_square_fitting (void)
{
  int i;

  if (n_vertices == 1) return;

  // init array with vertices
  Vector3f *array = new Vector3f[n_vertices];
  for (i=0; i<n_vertices; i++)
    array[i] = vertices[i];

  // create the line
  Line *line = new Line ();
  line->fit (array, n_vertices);

  // get results
  line->get_point (begin);
  line->get_direction (pluecker1);
  compute_extremities ();

  delete line;
  delete array;
}

/* update parameters */
/* adjust begin and end to fit among the implied vertices */
void Cextracted_line::compute_extremities (void)
{
  Vector3f origin, direction, pt_walk;

  // the line
  origin = begin;
  direction = pluecker1;
  
  // compute the positions of the extremities
  float begin2, end2, pos_walk;
  
  pt_walk = vertices[0];
  begin2 = coordinate_closest_point (pt_walk, origin, direction);
  end2   = coordinate_closest_point (pt_walk, origin, direction);
  for (int j=1; j<n_vertices; j++)
    {
      pt_walk = vertices[j];
      pos_walk = coordinate_closest_point (pt_walk, origin, direction);
      if (pos_walk < begin2) begin2 = pos_walk;
      if (pos_walk > end2)   end2   = pos_walk;
    }
  
  begin.Set (origin.x + begin2 * direction.x,
	    origin.y + begin2 * direction.y,
	    origin.z + begin2 * direction.z);
  end.Set (origin.x + end2 * direction.x,
	    origin.y + end2 * direction.y,
	    origin.z + end2 * direction.z);
}

void 
Cextracted_line::compute_length (void)
{
  Vector3f tmp = end - begin;
  length = tmp.getLength ();
}

void
Cextracted_line::compute_density (void)
{
  density = (n_vertices == 1)? 0.0 : n_vertices / length;
}

void
Cextracted_line::compute_mean_deviation (void)
{
  mean_deviation = 0.0;
  for (int i=0; i<n_vertices; i++)
    {
      Vector3f v_walk, v_closest, tmp;
      v_walk = vertices[i];
      float d = coordinate_closest_point (v_walk, begin, pluecker1);
      v_closest.Set (begin.x + d * pluecker1.x, begin.y + d * pluecker1.y, begin.z + d * pluecker1.z);
      tmp = v_walk - v_closest;
      mean_deviation += tmp.getLength ();
    }
  mean_deviation /= n_vertices;
  //printf (" %f -> ", mean_deviation);
}

void
Cextracted_line::compute_mean_curvature (void)
{
}

void
Cextracted_line::update_parameters (void)
{
  apply_least_square_fitting ();
  compute_extremities ();
  compute_density ();
  compute_length ();
  compute_mean_deviation ();
  compute_mean_curvature ();
}

void
Cextracted_line::normalize_weight (void)
{
	weight /= n_vertices;
}

/* dump */
void
Cextracted_line::dump (void)
{
  //v3d_dump (pluecker1);
  //v3d_dump (begin);
  //v3d_dump (end);
  printf ("%d vertices\n", n_vertices);
  return;
  for (int i=0; i<n_vertices; i++)
    {
      printf ("vertex %d : ", ivertices[i]);
      cout << vertices[i] << endl;
    }
}
