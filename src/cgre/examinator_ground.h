#ifndef __EXAMINATOR_GROUND_H__
#define __EXAMINATOR_GROUND_H__

#include "examinator.h"

class Cexaminator_ground : public Cexaminator
{
 public:
  Cexaminator_ground ();
  virtual ~Cexaminator_ground (){};
  
  virtual void key_pressed (unsigned char key);

  virtual void mouse_press (int button, int state, int x, int y);
  virtual void mouse_move  (int x, int y);
  
  virtual void set_camera (void);

 private:
  float x, y, z;
  float lx, ly, lz;
  float angle;
};

#endif // __EXAMINATOR_GROUND_H__
