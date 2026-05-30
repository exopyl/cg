#pragma once

#include "gl_wrapper.h"

class Ctrackball
{
public:
	Ctrackball ();
	virtual ~Ctrackball () {};
	
	// change dimensions of the GL window
	virtual void set_dimensions (GLint width, GLint height)
		{
			gl_window_width = width;
			gl_window_height = height;
		};

	// mouse
	virtual void mouse_press (int button, int state, int x, int y);
	virtual void mouse_move  (int x, int y);
	virtual void key_pressed  (unsigned char key);
	
	virtual void set_camera (void);
	virtual void set_camera_translate (void);
	virtual void set_camera_rotate (void);
	virtual void set_inverse_camera (void);

	// misc
	virtual void get_vector (GLfloat axis[3]);
	
	void set_zoom (float zoom);
	void set_zoom_precision (float fZoomPrecision);

	// Camera framing helper. The camera always orbits the world origin; the
	// model is shown wherever its coordinates place it.
	//
	// set_scene_radius lets the camera derive its near/far planes from the
	// *current* zoom each frame, so the model never crosses the clip planes
	// while zooming. A radius <= 0 (the default) keeps the fixed .01/10 planes.
	void set_scene_radius (float radius);
	void get_matrix (GLfloat m[4][4]);
	void get_matrix (GLfloat *m);
	int  get_inverse_matrix (GLfloat *m);
	
	void getCameraPosition	(float *x, float *y, float *z);
	void getCameraDirection	(float *x, float *y, float *z);
	
private:
	void tbPointToVector (int x, int y, float v[3]);
	
public:
	GLint gl_window_width;
	GLint gl_window_height;
	
	GLfloat   tb_lastposition[3];
	GLfloat   tb_angle;
	GLfloat   tb_axis[3];
	GLfloat   tb_transform[4][4];
	
	int       tb_state;
	int       tb_button;
	GLfloat   zoom;
	float     m_fZoomPrecision;
	GLfloat   xtrans, ytrans;
	int       lastX, lastY;

	// Perspective clip planes (fallback when no scene radius is set) and
	// scene radius for dynamic clip planes.
	float     m_zNear, m_zFar;
	float     m_sceneRadius;
};
