#pragma once

extern int inside_mandelbrot (int iteration_max, float x0, float y0, unsigned char c[3]);
extern int draw_mandelbrot (int iteration_max, float xmin, float xmax, float ymin, float ymax, float step);

extern int inside_mandelbulb (int max_iteration, float x0, float y0, float z0);
extern int draw_mandelbulb (float xmin, float xmax, float ymin, float ymax, float zmin, float zmax, float step);
