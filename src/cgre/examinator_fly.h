#ifndef __EXAMINATOR_FLY_H__
#define __EXAMINATOR_FLY_H__

#include "examinator.h"

class Cfly : public Cexaminator
{
 public:
  Cfly ();
  virtual ~Cfly (){};
  
  virtual void key_pressed (unsigned char key);

  virtual void mouse_press (int button, int state, int x, int y);
  virtual void mouse_move  (int x, int y);
  
  virtual void set_camera (void);

 private:
  float x, y, z;
  float lx, ly, lz;
  float angle_a, angle_b;
};

#endif // __EXAMINATOR_FLY_H__
