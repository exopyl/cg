#ifndef __SHAPE_DISTRIBUTION_OSADA_H__
#define __SHAPE_DISTRIBUTION_OSADA_H__

#include "mesh_half_edge.h"

//
// Reference :
// "Matching 3D Models with Shape Distributions"
// Robert Osada, Thomas Funkhouser, Bernard Chazelle, David Dobkin
// Proceedings of the International Conference on Shape Modeling & Applications, 2001
//
//
// Choix des paramètres:
// A3 :
// n selected points -> 3*factorielle(n)/(6*factorielle(n-3)) samples
// D1 :
// n selected points -> n samples
// D2 :
// n selected points -> n*(n-1))/2 samples
// D3 :
// n selected points -> factorielle(n)/(6*factorielle(n-3)) samples
// D4 :
// n selected points -> factorielle(n)/(24*factorielle(n-4)) samples
//

class Cshape_distribution_osada
{
 public:
  Cshape_distribution_osada (int nv, float *v, int nf, unsigned int *f);
  ~Cshape_distribution_osada ();

  enum shape_function_type {A3, D1, D2, D3, D4};

  void init_random (int seed) { srand (seed); };

  void compute_distribution (shape_function_type type, int n_points, int n_bins);
  void evaluate_distribution (shape_function_type type, int n_data, int n_bins);
  float *get_histogram (void) { return histogram; };
  void normalize_distribution (void);
  void export_distribution (char *filename);

 private:
  void compute_distribution_a3 (int _n_bins);
  void compute_distribution_d1 (int _n_bins);
  void compute_distribution_d2 (int _n_bins);
  void compute_distribution_d3 (int _n_bins);
  void compute_distribution_d4 (int _n_bins);

  void evaluate_distribution_a3 (int n_data, int _n_bins);
  void evaluate_distribution_d1 (int n_data, int _n_bins);
  void evaluate_distribution_d2 (int n_data, int _n_bins);
  void evaluate_distribution_d3 (int n_data, int _n_bins);
  void evaluate_distribution_d4 (int n_data, int _n_bins);

  int  find_area_position (float random_area, int istart, int iend);
  void compute_cumulative_areas (void);
  void select_points (int _n_points);
 public:
  int select_random_point (vec3 &point); // return the id of the face containing the point

 private:
  int nv, nf;
  float *v;
  unsigned int *f;
  
  float *cumulative_areas;
  int n_cumulative_areas;
  float total_area;

  float *selected_points;
  int n_selected_points;

  float *histogram;
  int n_bins;
};

#endif /* __SHAPE_DISTRIBUTION_OSADA_H__ */
