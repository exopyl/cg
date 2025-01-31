#include <math.h>
#include <stdio.h>

#include "examinator_ground.h"

Cexaminator_ground::Cexaminator_ground ()
{
	angle = 0.0f;
	x = 0.0f;
	y = 0.0f;
	z = 2.0f;
	lx = 0.0f;
	ly = 1.0f;
	lz = 0.0f;
}

void Cexaminator_ground::key_pressed (unsigned char key)
{
  switch (key)
    {
    case 'o':
      x -= lx;
      y -= ly;
      z -= lz;
    break;
    case 'l':
      x += lx;
      y += ly;
      z += lz;
      break;
    case 'k':
      angle -= 2.0f;
      lx = sin (angle*3.14159/180.0);
      ly = cos (angle*3.14159/180.0);
      break;
    case 'm':
      angle += 2.0f;
      lx = sin (angle*3.14159/180.0);
      ly = cos (angle*3.14159/180.0);
      break;
    }

  // clamp angle
  if (angle >= 360.0)
    angle -= 360.0;
  if (angle <= 360.0)
    angle += 360.0;
}

void Cexaminator_ground::mouse_press (int button, int state, int x, int y)
{
}

void Cexaminator_ground::mouse_move (int x, int y)
{
}

void Cexaminator_ground::set_camera (void)
{
  glRotatef (angle, 0.0, 1.0, 0.0);
  gluLookAt (x, y, z, x, y-1, z, 0, 0, 1);
}
