#ifndef __REGIONS_VERTICES_H__
#define __REGIONS_VERTICES_H__

#include "mesh_half_edge.h"

#define ACCESSIBILITY_RIDGES  1
#define ACCESSIBILITY_RAVINES 2

class Cregions_vertices
{
  friend class Cmodel3d_geometric_primitive;

 public:
  Cregions_vertices (Mesh_half_edge *mesh_half_edge);
  ~Cregions_vertices ();

  int get_size (void) { return size; };

  void set_datas (float *datas, int size);
  float* get_datas ();

  int *get_selected_region (void) { return selected_region; };

  void get_principal_direction_max_from_selected_regions (Vector3d **directions, int *n);
  void get_principal_direction_min_from_selected_regions (Vector3d **directions, int *n);
  void get_curvature_max_from_selected_regions (float **curvatures, int *n);
  void get_curvature_min_from_selected_regions (float **curvatures, int *n);

  /* export methods */
  void export_selected_region_cloud_points (char *filename);

  // init the regions to the boundary of the 3d model
  void init_from_boundary (void);

  /* edit */
  void delete_selected_region (void);

  /**************************/
  /*** ridges and ravines ***/
  /**************************/

  // threshold of the curvatures
  void init_from_max_curvatures (void);
  void init_from_min_curvatures (void);
  void init_from_closest_to_zero_curvatures (void);
  void init_from_highest_absolute_curvatures (void);

  // method of cylinders
  void init_from_cylinders (void);

  // accessibility
  void init_from_accessibility (int type);
  void init_from_accessibility2 (void);

  // belyaev
  void init_from_belyaev (void);
  void get_directions_lines_from_belyaev (Vector3 **directions, int *n);
  void belyaev_ridges_select_n (float percentage);
  void belyaev_ravines_select_n (float percentage);

  void get_directions_lines (Vector3 **directions, int *n, float threshold, float epsilon);

  void init_lines         (float threshold, float epsilon);
  void init_lines_ridges  (float threshold, float epsilon);
  void init_lines_ravines (float threshold, float epsilon);
  void init_circles       (float radius, float threshold, float epsilon);
  void init_planes        (float epsilon);
  void init_spheres       (float threshold, float epsilon);
  void init_cylinders     (float threshold, float epsilon);

  //
  // segmentation
  //
  
  // segmentation whitaker
  void init_from_whitaker (void);

  //
  // misc
  //

  // update the colors in the model
  void refresh_colors (void);

  // get extrema of datas
  void get_extremas (float *min, float *max);

  //
  // selection
  //
  void select_n_up   (float percentage); // select the n vertices with higher values in datas
  void select_n_down (float percentage); // select the n vertices with lower values in datas
  void select_threshold_up   (float threshold);
  void select_threshold_down (float threshold);
  void select_datas          (float value);
  float get_lowest_value_from_selected_region (void);
  float get_highest_value_from_selected_region (void);

  void select_all_regions (void);
  void select_region (int id);
  void select_vertex (int id);
  void deselect_vertex (int id);
  void select_vertices (int id);
  void deselect_vertices (int id);
  void deselect (void);

  //
  // algorithms
  //

  //
  // smoothing data
  //
  void smoothing_data (void);

  //
  // morphological operators
  //
  void dilate_regions (void);
  void erode_regions (void);
  void opening_regions (void);
  void closing_regions (void);
  void delete_isolated_regions (int n = 1); // delete connex regions containing less than n vertices

  void dilate_selected_region (void);
  void erode_selected_region (void);
  void opening_selected_region (void);
  void closing_selected_region (void);

  //
  // skeletonization
  //
  void skeletonize_selected_region (void);
  void pruning_selected_region (void);

  //
  // smoothing
  //
  void smoothing_laplacian_regions (void);
  void smoothing_laplacian_inverse_regions (void);
  void smoothing_taubin_regions (void);
  void smoothing_taubin_regions (float lambda, float mu);
  void smoothing_taubin_inverse_regions (void);

  void smoothing_laplacian_selected_region (void);
  void smoothing_laplacian_inverse_selected_region (void);
  void smoothing_taubin_selected_region (void);
  void smoothing_taubin_selected_region (float lambda, float mu);
  void smoothing_taubin_inverse_selected_region (void);

  //
  // fitting
  //
/*
  Line*   line_fitting   (void);
  Circle* circle_fitting (void);
  Plane*  plane_fitting  (void);
*/

  //
  // colors
  //
  void set_color_regions (float r, float g, float b);
  void set_color_selected_region (float r, float g, float b);
  void set_color_common_vertex (float r, float g, float b);

 protected:
  Mesh_half_edge *mesh_half_edge; // original model
  int  size;                // size of the following arrays (number of vertices)
  float *datas;            // for each element is associated a value
  int  *regions;            // 0 or 1 : precise if an element is included in a specific region
  int  *selected_region;    // 0 or 1 : allow the user to select elements among the regions

  // colors
  float r_regions, g_regions, b_regions;
  float r_selected_region, g_selected_region, b_selected_region;
  float r_common_vertex, g_common_vertex, b_common_vertex;
};

#endif // __REGIONS_VERTICES_H__
