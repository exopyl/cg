#ifndef __SET_LINES_H__
#define __SET_LINES_H__

#include "mesh_half_edge.h"
#include "extracted_line.h"

class Cset_lines
{
 public:
  Cset_lines (Mesh_half_edge *model);
  ~Cset_lines ();

  void reinit (void);

  void  add_line (Vector3 _pluecker1, Vector3 _pluecker2, Vector3 _begin, Vector3 _end, int _n_vertices, int *_ivertices, Vector3 *_vertices, float _weight = 1.0);
  int   get_n_extracted_lines (void);
  Cextracted_line* get_extracted_line (int index);

  /************/
  /*** misc ***/
  /************/
  void apply_gaussian_noise (float variance);

  /****************/
  /*** Cantzler ***/
  /****************/
  void cantzler_extract_edges (float threshold); // in radians
  void cantzler_merge_edges (int n_candidates, float tolerance);


  /******************/
  /*** New method ***/
  /******************/
  void extract_ridges_and_valleys (float kappa_epsilon);

  /*******************************/
  /*** merge oriented vertices ***/
  /*******************************/
  void merge_oriented_vertices (float tolerance_angle, float tolerance_distance);
  void merge_oriented_vertices2 (float tolerance_angle, float tolerance_distance);


  /* merge close lines */
  void merge_close_lines            (float cos_angle_max, float d_max);
  void merge_close_lines_mean_shift (float hd, float hp, Vector3 ***pos_histo, Vector3 ***dir_histo, int *n_ite);
  void merge_close_lines_pluecker   (float cos_angle_max, float d_max);

  /* adjust begin and end to fit on the concerned vertices */
  void compute_extremities (void);

  /* apply a least square fitting among the vertices to adjust the line */
  void apply_least_square_fitting (void);

  /* delete isolated lines */
  void delete_isolated_lines (int n_max);
  
  /* compute normalized weight for each lines */
  //void compute_normalized_weights (Cextracted_line::weight_method id);

  /* visu */
  void apply_random_colors (void);
  void compute_colors (void);
  int  compute_colors_for_selection (float _weight, int _minimum_n_vertices, float _length, float _density, float _mean_deviation);
  float* get_colors (void);
  void dump (void);

 private:
  Mesh_half_edge *model;
  int n_extracted_lines;
  Cextracted_line **extracted_lines;
  float *colors;
};

#endif /* __SET_LINES_H__ */
