#pragma once

#include "../cgmath/cgmath.h"

#include "framerate.h"

// tools
extern void screenshot (int win_width, int win_height);

// utils
extern void repere_draw (void);
extern void repere_draw2 (void);
extern void draw_grid (float size = 1.f, int step = 10);

//extern void display_framerate (CFrameRate& framerate);

//
// drawing
//

// glut
extern void draw_cube (void);
extern void draw_sphere (float r = 2.);
extern void draw_teapot ();

// points
extern void draw_point (vec3 pt);
extern void draw_point (vec3 pt, float r, float g, float b);
extern int  draw_point (vec3 pt, int id);

// vector
extern void draw_vector (vec3 v, vec3 n, float r = 0.0, float g = 0.0, float b = 0.0);

// segment
extern void draw_segment (vec3 v1, vec3 v2, float r = 0.0, float g = 0.0, float b = 0.0);

// lines
extern void draw_line (vec3 begin, vec3 end, float r, float g, float b);
extern int  draw_line (vec3 begin, vec3 end, int id); /* with id on extremities */

// planes
extern void draw_plane (vec3 pt, vec3 normale);
extern int  draw_plane (vec3 pt, vec3 normale, int id);

// circles
#define N_SLICES 100
extern void draw_circle (float x, float y, float z, float radius);
extern void draw_arc (float x, float y, float z, float radius, float begin, float end);
extern int  draw_arc (float x, float y, float z, float radius, float begin, float end, int id); /* with id on extremities */

// sphere
extern void draw_sphere2 (int n);

