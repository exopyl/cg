#include "gl_wrapper.h"

#include <math.h>

#include "examinator_fly.h"

Cfly::Cfly ()
{
  angle_a = angle_b = 0.0f;
  x = 0.0f;
  y = 0.0f;
  z = 20.0f;
  lx = 0.0f;
  ly = 0.0f;
  lz = -1.0f;
}

/* keyboard */
void
Cfly::key_pressed (unsigned char key)
{
  switch (key)
    {
    case 'a':
      x -= lx;
      y -= ly;
      z -= lz;
    break;
    case 'q':
      x += lx;
      y += ly;
      z += lz;
      break;
    case GLUT_KEY_UP:
      angle_a += 1.0f;
      break;
    case GLUT_KEY_DOWN:		
      angle_a -= 1.0f;
      break;
    case GLUT_KEY_RIGHT:
      angle_b += 1.0f;
      break;
    case GLUT_KEY_LEFT:		
      angle_b -= 1.0f;
      break;
    }
}

/* mouse */
void
Cfly::mouse_press (int button, int state, int x, int y)
{
}

void
Cfly::mouse_move (int x, int y)
{
}

void
Cfly::set_camera (void)
{
  float phi = ((-angle_a * 3.14159)/180);
  float theta = ((angle_b * 3.14159)/180);
  lx = -cos(phi)*sin(theta);
  ly = sin(phi);
  lz = cos(phi)*cos(theta);
  glRotatef (-angle_a, 1.0, 0.0, 0.0);
  glRotatef (angle_b, 0.0, 1.0, 0.0);
  gluLookAt (x, y, z, x, y, z-1, 0, 1, 0);
}
