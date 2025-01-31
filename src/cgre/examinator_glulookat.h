#ifndef __EXAMINATOR_GLULOOKAT_H__
#define __EXAMINATOR_GLULOOKAT_H__

#include "examinator.h"

class Cexaminator_glulookat : public Cexaminator
{
 public:
  Cexaminator_glulookat (float x_eye, float y_eye, float z_eye,
			 float x_center, float y_center, float z_center,
			 float x_up, float y_up, float z_up);
  virtual ~Cexaminator_glulookat (){};
  
  virtual void key_pressed (unsigned char key, int x, int y) {};

  virtual void mouse_press (int button, int state, int x, int y) {};
  virtual void mouse_move  (int x, int y) {};
  
  virtual void set_camera (void);

 private:
  float x_eye, y_eye, z_eye;
  float x_center, y_center, z_center;
  float x_up, y_up, z_up;
};

#endif // __EXAMINATOR_GLULOOKAT_H__
