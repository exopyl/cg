#include "examinator_glulookat.h"

Cexaminator_glulookat::Cexaminator_glulookat (float _x_eye, float _y_eye, float _z_eye,
					      float _x_center, float _y_center, float _z_center,
					      float _x_up, float _y_up, float _z_up)
{
  x_eye = _x_eye;
  y_eye = _y_eye;
  z_eye = _z_eye;
  x_center = _x_center;
  y_center = _y_center;
  z_center = _z_center;
  x_up = _x_up;
  y_up = _y_up;
  z_up = _z_up;
}

void
Cexaminator_glulookat::set_camera ()
{
  gluLookAt (x_eye, y_eye, z_eye, x_center, y_center, z_center, x_up, y_up, z_up);
};

