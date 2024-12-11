#ifndef __REGIONS_FACES_H__
#define __REGIONS_FACES_H__

#include "mesh_half_edge.h"
#include "geometric_primitives.h"

class Cregions_faces
{
  friend class Cmodel3d_geometric_primitive;
  
 public:
  Cregions_faces (Mesh_half_edge *mesh_half_edge);
  ~Cregions_faces ();

  void init_segmentation (float epsilon);
  void init (float *datas, int size);
  void clean_segmentation (float percentage);

  // getters
  int get_size (void) { return size; };

  int *get_selected_region (void);
  int *get_regions (void);
  Mesh_half_edge *get_mesh_half_edge (void);

  // update the colors in the model
  void refresh_colors (void);
  
  // selection
  void select_faces_by_id_region (int id);
  void select_face (int id);
  void deselect_face (int id);
  void select_faces (int id);
  void deselect_faces (int id);
  void deselect (void);

  // export methods
  void export_selected_region_cloud_points (char *filename);

  //
  // algorithms
  //

  // morphological operators
  void dilate_regions (void);
  void erode_regions (void);
  void opening_regions (void);
  void closing_regions (void);
  void delete_isolated_regions (void);

  void dilate_selected_region (void);
  void erode_selected_region (void);
  void opening_selected_region (void);
  void closing_selected_region (void);

  // smoothing
  void smoothing_laplacian (void);
  void smoothing_taubin (void);

  // fitting
  Plane*  plane_fitting  (void);

  //
  // colors
  //
  void set_color_regions         (float r, float g, float b);
  void set_color_selected_region (float r, float g, float b);
  void set_color_common_face     (float r, float g, float b);

 private:
  Mesh_half_edge *mesh_half_edge; // original model
  int  size;             // size of the following arrays (number of faces)
  float *data;          // for each element is associated a value
  int  *regions;         // precise if an element is included in a specific region
  int  *selected_region; // allow the user to select elements among the regions

  // colors
  float r_regions, g_regions, b_regions;
  float r_selected_region, g_selected_region, b_selected_region;
  float r_common_face, g_common_face, b_common_face;
};

#endif // __REGIOND_FACES_H__
