#include <math.h>
#include <stdio.h>

#include "examinator_trackball.h"

Ctrackball::Ctrackball ()
{
	gl_window_width = 0;
	gl_window_height = 0;
	zoom = -5.f;
	xtrans = 0.f;
	ytrans = 0.f;

  for (int i=0; i<3; i++)
  {
	tb_lastposition[i] = 0.;
	tb_axis[i] = 0.;
  }
  tb_angle =  0.f;
  for (int i=0; i<4; i++)
    for (int j=0; j<4; j++)
	    tb_transform[i][j] = (i == j)? 1.0 : 0.0;

	tb_state = RELEASED;
	tb_button = 0;
	m_fZoomPrecision = 1.0;
	xtrans = 0.;
	ytrans = 0.;
	lastX = 0;
	lastY = 0;
}

void
Ctrackball::tbPointToVector(int x, int y, float v[3])
{
  float d, a;

  // project x, y onto a hemi-sphere centered within width, height
  v[0] = (2.0 * x - gl_window_width) / gl_window_width;
  v[1] = (gl_window_height - 2.0 * y) / gl_window_height;
  d = sqrt(v[0] * v[0] + v[1] * v[1]);
  v[2] = cos((3.14159265 / 2.0) * ((d < 1.0) ? d : 1.0));
  a = 1.0 / sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  v[0] *= a;
  v[1] *= a;
  v[2] *= a;
}

// mouse callbacks
void
Ctrackball::mouse_press (int button, int state, int x, int y)
{
  if (state == PRESSED)
    {
      switch (button)
		{
			case LEFT_BUTTON:
				tb_state  = PRESSED;
				tb_button = LEFT_BUTTON;
				tbPointToVector(x, y, tb_lastposition);
				break;

			case RIGHT_BUTTON:
				tb_state  = PRESSED;
				tb_button = RIGHT_BUTTON;
				lastX = x; lastY = y;
				break;

			case MIDDLE_BUTTON:			
				tb_state  = PRESSED;
				tb_button = MIDDLE_BUTTON;
				lastX = x; lastY = y;
				break;
		}
    }
  else
    tb_state = RELEASED;
}

void
Ctrackball::mouse_move (int x, int y)
{
  GLfloat current_position[3], dx, dy, dz;
  if (tb_state == PRESSED)
    {
      switch (tb_button)
	{
	case  LEFT_BUTTON:
	  tbPointToVector(x, y, current_position);
	  
	  // calculate the angle to rotate by (directly proportional
	  // to the length of the mouse movement)
	  dx = (current_position[0] - tb_lastposition[0]);
	  dy = (current_position[1] - tb_lastposition[1]);
	  dz = (current_position[2] - tb_lastposition[2]);
	  tb_angle = 180.0 * sqrt(dx * dx + dy * dy + dz * dz);
	  
	  // calculate the axis of rotation (cross product)
	  tb_axis[0] = tb_lastposition[1] * current_position[2] -
	    tb_lastposition[2] * current_position[1];
	  tb_axis[1] = tb_lastposition[2] * current_position[0] -
	    tb_lastposition[0] * current_position[2];
	  tb_axis[2] = tb_lastposition[0] * current_position[1] -
	    tb_lastposition[1] * current_position[0];
	  
	  // reset for next time
	  tb_lastposition[0] = current_position[0];
	  tb_lastposition[1] = current_position[1];
	  tb_lastposition[2] = current_position[2];
	  
	  glPushMatrix();
	  glLoadIdentity();
	  glRotatef(tb_angle, tb_axis[0], tb_axis[1], tb_axis[2]);
	  glMultMatrixf((GLfloat *)tb_transform);
	  glGetFloatv(GL_MODELVIEW_MATRIX, (GLfloat *)tb_transform);
	  glPopMatrix();
	  break;

	case MIDDLE_BUTTON:
	  xtrans += 1.*(x-lastX)/(double)gl_window_width*1;
	  ytrans += -1.*(y-lastY)/(double)gl_window_height*1;
	  lastX = x; lastY = y;
	  break;

	case RIGHT_BUTTON:
	  zoom += (m_fZoomPrecision)*(y-lastY)/(double)gl_window_height*5;
	  lastX = x; lastY = y;
	  break;
	}
    }
}

void Ctrackball::key_pressed  (unsigned char key)
{

}

void Ctrackball::set_camera (void)
{
	// adjust perspective
	float fovyInDegrees = 45.f;
	float aspectRatio = (float)gl_window_width / (float)gl_window_height;
	float zNear = .01f;
	float zFar = 10.f;

	float ymax = zNear * tanf(fovyInDegrees * 3.14159 / 360.0);
	// ymin = -ymax;
	// xmin = -ymax * aspectRatio;
	float xmax = ymax * aspectRatio;

	// frustrum
	float matrix[16];
	//glhFrustumf2(matrix, -xmax, xmax, -ymax, ymax, zNear, zFar);
	float left = -xmax;
	float right = xmax;
	float bottom = -ymax;
	float top = ymax;
	float temp, temp2, temp3, temp4;
	temp = 2.f * zNear;
	temp2 = right - left;
	temp3 = top - bottom;
	temp4 = zFar - zNear;
	matrix[0] = temp / temp2;
	matrix[1] = 0.f;
	matrix[2] = 0.f;
	matrix[3] = 0.f;
	matrix[4] = 0.f;
	matrix[5] = temp / temp3;
	matrix[6] = 0.f;
	matrix[7] = 0.f;
	matrix[8] = (right + left) / temp2;
	matrix[9] = (top + bottom) / temp3;
	matrix[10] = (-zFar - zNear) / temp4;
	matrix[11] = -1.f;
	matrix[12] = 0.f;
	matrix[13] = 0.f;
	matrix[14] = (-temp * zFar) / temp4;
	matrix[15] = 0.f;

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glMultMatrixf((GLfloat*)matrix);

	// modelview
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glTranslatef(xtrans, ytrans, zoom);
	glMultMatrixf((GLfloat*)tb_transform);
}

void
Ctrackball::set_camera_translate (void)
{
  glTranslatef (xtrans, ytrans, zoom);
  //glMultMatrixf ((GLfloat *)tb_transform);
}

void
Ctrackball::set_camera_rotate (void)
{
  //glTranslatef (xtrans, ytrans, zoom);
  glMultMatrixf ((GLfloat *)tb_transform);
}

void
Ctrackball::set_inverse_camera (void)
{
  //glTranslatef (0.0, 0.0, -zoom);
  float m[16];
  get_inverse_matrix (m);
  glMultMatrixf ((GLfloat *)m);
}

// misc
void
Ctrackball::get_vector (float v[3])
{
  v[0] = tb_transform[0][2];
  v[1] = tb_transform[1][2];
  v[2] = tb_transform[2][2];
}

void
Ctrackball::set_zoom (float _zoom)
{
  zoom = _zoom;
}

void
Ctrackball::set_zoom_precision (float fZoomPrecision)
{
	m_fZoomPrecision = fZoomPrecision;
}

void
Ctrackball::get_matrix (GLfloat m[4][4])
{
	for (int i=0; i<4; i++)
		for (int j=0; j<4; j++)
			m[i][j] = tb_transform[i][j];
}

void
Ctrackball::get_matrix (GLfloat *m)
{
	for (int i=0; i<4; i++)
		for (int j=0; j<4; j++)
			m[4*i+j] = tb_transform[i][j];
}

int
Ctrackball::get_inverse_matrix (GLfloat *m)
{
  int i,j;
  float tmp[12]; // temp array for pairs
  float src[16]; // array of transpose source matrix
  float dst[16];
  float det;     // determinant
  
  float tmp2[16];
	for (int i=0; i<4; i++)
		for (int j=0; j<4; j++)
			tmp2[4*i+j] = tb_transform[i][j];

  // transpose matrix
  for (i = 0; i < 4; i++)
  {
    src[i]      = tmp2[i*4];
    src[i + 4]  = tmp2[i*4 + 1];
    src[i + 8]  = tmp2[i*4 + 2];
    src[i + 12] = tmp2[i*4 + 3];
  }
  // calculate pairs for first 8 elements (cofactors)
  tmp[0]  = src[10] * src[15];
  tmp[1]  = src[11] * src[14];
  tmp[2]  = src[9]  * src[15];
  tmp[3]  = src[11] * src[13];
  tmp[4]  = src[9]  * src[14];
  tmp[5]  = src[10] * src[13];
  tmp[6]  = src[8]  * src[15];
  tmp[7]  = src[11] * src[12];
  tmp[8]  = src[8]  * src[14];
  tmp[9]  = src[10] * src[12];
  tmp[10] = src[8]  * src[13];
  tmp[11] = src[9]  * src[12];
  // calculate first 8 elements (cofactors)
  dst[0] =  tmp[0]*src[5] + tmp[3]*src[6] + tmp[4]*src[7];
  dst[0] -= tmp[1]*src[5] + tmp[2]*src[6] + tmp[5]*src[7];
  dst[1] =  tmp[1]*src[4] + tmp[6]*src[6] + tmp[9]*src[7];
  dst[1] -= tmp[0]*src[4] + tmp[7]*src[6] + tmp[8]*src[7];
  dst[2] =  tmp[2]*src[4] + tmp[7]*src[5] + tmp[10]*src[7];
  dst[2] -= tmp[3]*src[4] + tmp[6]*src[5] + tmp[11]*src[7];
  dst[3] =  tmp[5]*src[4] + tmp[8]*src[5] + tmp[11]*src[6];
  dst[3] -= tmp[4]*src[4] + tmp[9]*src[5] + tmp[10]*src[6];
  dst[4] =  tmp[1]*src[1] + tmp[2]*src[2] + tmp[5]*src[3];
  dst[4] -= tmp[0]*src[1] + tmp[3]*src[2] + tmp[4]*src[3];
  dst[5] =  tmp[0]*src[0] + tmp[7]*src[2] + tmp[8]*src[3];
  dst[5] -= tmp[1]*src[0] + tmp[6]*src[2] + tmp[9]*src[3];
  dst[6] =  tmp[3]*src[0] + tmp[6]*src[1] + tmp[11]*src[3];
  dst[6] -= tmp[2]*src[0] + tmp[7]*src[1] + tmp[10]*src[3];
  dst[7] =  tmp[4]*src[0] + tmp[9]*src[1] + tmp[10]*src[2];
  dst[7] -= tmp[5]*src[0] + tmp[8]*src[1] + tmp[11]*src[2];
  // calculate pairs for second 8 elements (cofactors)
  tmp[0] = src[2]*src[7];
  tmp[1] = src[3]*src[6];
  tmp[2] = src[1]*src[7];
  tmp[3] = src[3]*src[5];
  tmp[4] = src[1]*src[6];
  tmp[5] = src[2]*src[5];
  tmp[6] = src[0]*src[7];
  tmp[7] = src[3]*src[4];
  tmp[8] = src[0]*src[6];
  tmp[9] = src[2]*src[4];
  tmp[10] = src[0]*src[5];
  tmp[11] = src[1]*src[4];
  // calculate second 8 elements (cofactors)
  dst[8] = tmp[0]*src[13] + tmp[3]*src[14] + tmp[4]*src[15];
  dst[8] -= tmp[1]*src[13] + tmp[2]*src[14] + tmp[5]*src[15];
  dst[9] = tmp[1]*src[12] + tmp[6]*src[14] + tmp[9]*src[15];
  dst[9] -= tmp[0]*src[12] + tmp[7]*src[14] + tmp[8]*src[15];
  dst[10] = tmp[2]*src[12] + tmp[7]*src[13] + tmp[10]*src[15];
  dst[10]-= tmp[3]*src[12] + tmp[6]*src[13] + tmp[11]*src[15];
  dst[11] = tmp[5]*src[12] + tmp[8]*src[13] + tmp[11]*src[14];
  dst[11]-= tmp[4]*src[12] + tmp[9]*src[13] + tmp[10]*src[14];
  dst[12] = tmp[2]*src[10] + tmp[5]*src[11] + tmp[1]*src[9];
  dst[12]-= tmp[4]*src[11] + tmp[0]*src[9] + tmp[3]*src[10];
  dst[13] = tmp[8]*src[11] + tmp[0]*src[8] + tmp[7]*src[10];
  dst[13]-= tmp[6]*src[10] + tmp[9]*src[11] + tmp[1]*src[8];
  dst[14] = tmp[6]*src[9] + tmp[11]*src[11] + tmp[3]*src[8];
  dst[14]-= tmp[10]*src[11] + tmp[2]*src[8] + tmp[7]*src[9];
  dst[15] = tmp[10]*src[10] + tmp[4]*src[8] + tmp[9]*src[9];
  dst[15]-= tmp[8]*src[9] + tmp[11]*src[10] + tmp[5]*src[8];
  // calculate determinant
  det=src[0]*dst[0]+src[1]*dst[1]+src[2]*dst[2]+src[3]*dst[3];
  if (det == 0.0)
  {
	  return 0;
  }
  // calculate matrix inverse
  det = 1/det;
  for (j = 0; j < 16; j++)
    m[j] = dst[j] * det;

  return 1;
}

void Ctrackball::getCameraPosition (float *x, float *y, float *z)
{
	glPushMatrix ();
	glLoadIdentity ();
	glTranslatef (0.0, 0.0, zoom);
	glMultMatrixf ((GLfloat *)tb_transform);
	glPopMatrix ();
}

void Ctrackball::getCameraDirection (float *x, float *y, float *z)
{
}


