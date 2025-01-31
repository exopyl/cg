#ifndef __EXAMINATOR_WALK_H__
#define __EXAMINATOR_WALK_H__

#include "gl_wrapper.h"

class Cexaminator_walk
{
 public:
  Cexaminator_walk ();
  virtual ~Cexaminator_walk () {};
  
  // change dimensions of the GL window
  void set_dimensions (GLint width, GLint height)
    {
      gl_window_width = width;
      gl_window_height = height;
    };

  void key_pressed (unsigned char key);

  void mouse_press (int button, int state, int x, int y);
  void mouse_move  (int x, int y);
  
  void set_camera (void);

  // misc
  void set_position  (GLfloat xpos, GLfloat ypos, GLfloat zpos) { x = xpos; y = ypos; z = zpos;       };
  void get_position  (GLfloat *xpos, GLfloat *ypos, GLfloat *zpos) { *xpos = x; *ypos = y; *zpos = z; };
  void set_direction (GLfloat xdir, GLfloat ydir, GLfloat zdir) { vx = xdir; vy = ydir; vz = zdir;    };
  void get_vector (GLfloat axis[3]);
  void set_zoom (float zoom);
  void get_matrix (GLfloat m[4][4]);

 public:
  GLint gl_window_width;
  GLint gl_window_height;

  GLfloat x, y, z; // position
  GLfloat vx, vy, vz; // view orientation

  int       tb_state;
  int       tb_button;
  int       lastX, lastY;
};

#endif // __EXAMINATOR_WALK_H__
