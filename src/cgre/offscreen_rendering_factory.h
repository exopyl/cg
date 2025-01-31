#pragma once

#include "gl_wrapper.h"

#include "offscreen_rendering_factory.h"

class Mesh;
class Coffscreen_rendering
{
 public:
  Coffscreen_rendering (Mesh *model);
  ~Coffscreen_rendering ();

  void set_parameters_glulookat (float x_eye, float y_eye, float z_eye,
								 float x_center, float y_center, float z_center,
								 float x_up, float y_up, float z_up);
  void set_parameters_glortho (float left, float right,
							   float bottom, float top,
							   float near_plane, float far_plane);

  void set_zoom (float zoom);

  void rendering (char *output);

  // get renders
  void           get_render                  (unsigned char *render, int *width, int *height);
  unsigned char* get_render_zbuffer          (unsigned char *buffer, int *width, int *height);
  float*         get_render_floating_zbuffer (float *render, int *width, int *height);

  void save_render         (char *filename);
  void save_render_zbuffer (char *filename);
  void save_render_floating_zbuffer (char *filename);

  void dump (void);
 
 private:
  void alloc_memory_buffers (void);
  void draw_object (void);
  void fill_buffers (void);

 private:
  Mesh *model;
  int width, height;
  float zoom;

  /* projection en perspective cavaliere */
  float left, right;
  float bottom, top;
  float near_plane, far_plane;

  /* gluLookAt */
  float x_eye, y_eye, z_eye;
  float x_center, y_center, z_center;
  float x_up, y_up, z_up;

  /* buffers */
  GLubyte *buffer;
  GLubyte *zbuffer;
  GLfloat *floating_zbuffer;
};
