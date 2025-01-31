#pragma once

#include "gl_wrapper.h"

class Cexaminator
{
public:
  virtual void set_dimensions (GLint width, GLint height)
    {
      gl_window_width = width;
      gl_window_height = height;
    };
  
  virtual void key_pressed (unsigned char key) {};
  
  virtual void mouse_press (int button, int state, int x, int y) {};
  virtual void mouse_move  (int x, int y) {};
  
  virtual void set_camera  (void) {};
  
  virtual void get_vector (GLfloat axis[3]) {};

 protected:
  GLint gl_window_width;
  GLint gl_window_height;
};
