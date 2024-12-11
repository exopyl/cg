#ifndef __EXTRACTED_LINE_H__
#define __EXTRACTED_LINE_H__

#include "../cgmath/cgmath.h"

class Cextracted_line
{
	friend class Cset_lines;
 public:
  enum weight_method
  {
    WEIGHT_METHOD_         = 0
  };

public:
  Cextracted_line (Vector3f _pluecker1, Vector3f _pluecker2, Vector3f _begin, Vector3f _end, int _n_vertices, int *_ivertices, Vector3f *_vertices, float weight = 0.0);
  ~Cextracted_line ();
  
  /* merging */
  void merge (Cextracted_line *line);

  /* adjust begin and end to fit on the concerned vertices */
  void compute_extremities (void);

  /* apply a least square fitting to adjust the line */
  void apply_least_square_fitting (void);

  /* setters */
  void add_vertex (int _ivertex, Vector3f _vertex);

  /* getters */
  int  get_n_vertices (void);
  int* get_ivertices  (void);
  Vector3f* get_vertices   (void);
  void get_begin (Vector3f _begin);
  void get_end   (Vector3f _end);
  void get_center (Vector3f _center);
  void get_direction (Vector3f d);

  float get_length (void)         { return length; };
  float get_density (void)        { return density; };
  float get_mean_deviation (void) { return mean_deviation; };
  float get_mean_curvature (void) { return mean_curvature; }; // problem to compute it correctly

  /* misc */
  int is_index_in_line (int index);

  /* weight */
  float get_weight (void);
  void   set_weight (float _weight);
  void   compute_weight (weight_method id);
  void   normalize_weight (void);

  /* color */
  void set_color (float r, float g, float b);
  void get_color (float *r, float *g, float *b);

  // dump
  void dump (void);

 private:
  void compute_density (void);
  void compute_length (void);
  void compute_mean_deviation (void);
  void compute_mean_curvature (void);
  void update_parameters (void);

private:
  Vector3f pluecker1, pluecker2;
  Vector3f begin, end;
  int n_vertices;
  int *ivertices;
  Vector3f *vertices;
  float density, length, mean_deviation;
  float mean_curvature;
  float weight;

  // visu
  float r, g, b;
};

#endif // __EXTRACTED_LINE_H__
