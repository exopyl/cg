#include <math.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

#include "examinator_walk.h"

Cexaminator_walk::Cexaminator_walk ()
{
   x = 0.0;  y = 0.0;  z = 0.0;
  vx = 0.0; vy = 1.0; vz = 0.0;
}

void Cexaminator_walk::key_pressed (unsigned char key)
{
	switch (key)
	{
	case GLUT_KEY_UP:
		x += vx * 1.0;
		y += vy * 1.0;
		z += 0.0;
		break;
	case GLUT_KEY_DOWN:
		x -= vx * 1.0;
		y -= vy * 1.0;
		z -= 0.0;
		break;
	case GLUT_KEY_LEFT:
		{
		float alpha = acos (vx);
		if (vy < 0.0) alpha *= -1.0;
		alpha += 0.02;
		vx = cos (alpha);
		vy = sin (alpha);
		}
		break;
	case GLUT_KEY_RIGHT:
		{
		float alpha = acos (vx);
		if (vy < 0.0) alpha *= -1.0;
		alpha -= 0.02;
		vx = cos (alpha);
		vy = sin (alpha);
		}
		break;
	default:
		;
	}
}

void Cexaminator_walk::mouse_press (int button, int state, int newX, int newY)
{
  if (state == GLUT_DOWN)
    {
      switch (button)
	{
	case GLUT_LEFT_BUTTON:
	  tb_state  = GLUT_DOWN;
	  tb_button = GLUT_LEFT_BUTTON;
	  lastX = newX; lastY = newY;
	  break;
	case GLUT_RIGHT_BUTTON:
	  tb_state  = GLUT_DOWN;
	  tb_button = GLUT_RIGHT_BUTTON;
	  lastX = newX; lastY = newY;
	  break;
	case GLUT_MIDDLE_BUTTON:			
	  tb_state  = GLUT_DOWN;
	  tb_button = GLUT_MIDDLE_BUTTON;
	  lastX = newX; lastY = newY;
	  break;
	}
    }
  else
    tb_state = GLUT_UP;
}

void Cexaminator_walk::mouse_move (int newX, int newY)
{
  if (tb_state == GLUT_DOWN)
    {
      switch (tb_button)
	{
	case  GLUT_LEFT_BUTTON:
		{
		x += vx * (lastY - newY) / 10.0;
		y += vy * (lastY - newY) / 10.0;
		z += 0.0;

		float alpha = acos (vx);
		if (vy < 0.0) alpha *= -1.0;
		alpha += (lastX - newX)/500.0;
		vx = cos (alpha);
		vy = sin (alpha);

		lastX = newX; lastY = newY;
		}
	  break;
	case GLUT_MIDDLE_BUTTON:
		x += -vy * (lastX - newX) / 10.0;
		y +=  vx * (lastX - newX) / 10.0;
		lastX = newX; lastY = newY;
	  break;
	case GLUT_RIGHT_BUTTON:
		z += (lastY - newY) / 10.0;
	    lastX = newX; lastY = newY;
	  break;
	}
    }
}

void Cexaminator_walk::set_camera (void)
{
	gluLookAt(x, y, z,          // position of the camera
			  x+vx, y+vy, z+vz, // look at the point
			  0.0, 0.0, 1.0);   // up vector
}

void Cexaminator_walk::get_vector (float v[3])
{
}

void Cexaminator_walk::set_zoom (float _zoom)
{
}

void Cexaminator_walk::get_matrix (GLfloat m[4][4])
{
}
