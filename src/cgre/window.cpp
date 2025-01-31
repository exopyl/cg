#include <stdio.h>
#include <stdlib.h>

#include "gl_wrapper.h"

#include "window.h"

Window::Window ()
{
	// create the frame buffer object
	m_fbo = new FrameBufferObject (256, 256);
	if (m_fbo && !m_fbo->isOK ())
	{
		delete m_fbo;
		m_fbo = NULL;
		return;
	}

	m_examinator = new Ctrackball ();
	m_examinator->set_dimensions (256, 256);


	m_selected = false;
}

Window::~Window ()
{
}

void Window::pre (void)
{
	if (m_fbo)
	{
		m_fbo->activate ();

		glClearColor(1.0, 1.0, 1.0, 0.0f); // background
/*
		glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glLoadIdentity();
		m_examinator->set_camera ();
		glTranslatef (0.0, 0.0, -3.5);

		glPushAttrib (GL_ALL_ATTRIB_BITS);
		//glFrontFace (GL_CW);
		glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
		glDepthFunc (GL_LESS);
		glutSolidTeapot (1.0f);
		glPopAttrib ();
*/
		m_fbo->desactivate ();
	}
}

void Window::display (void)
{
	if (m_fbo)
	{
		glLoadName (m_id);
		glEnable (GL_TEXTURE_2D);
		m_fbo->bindTexture ();
		float t = 1.0; 
		glDisable (GL_LIGHTING);
		glBegin (GL_QUADS);
		glTexCoord2f (1.0f, 0.0f); glVertex3f ( t, -t, 0.0); 
		glTexCoord2f (1.0f, 1.0f); glVertex3f ( t,  t, 0.0); 
		glTexCoord2f (0.0f, 1.0f); glVertex3f (-t,  t, 0.0); 
		glTexCoord2f (0.0f, 0.0f); glVertex3f (-t, -t, 0.0);
		glEnd ();
		if (m_selected)
		{
			glPushAttrib (GL_ALL_ATTRIB_BITS);
			glPolygonMode (GL_FRONT_AND_BACK, GL_LINE);
			glEnable (GL_POLYGON_OFFSET_FILL);
			glPolygonOffset (1.0, 1.0);
			glLineWidth (4.0);
			glColor3f (1.0, 0.0, 0.0);
			glBegin (GL_LINE_LOOP);
			glVertex3f ( t, -t, 0.0); 
			glVertex3f ( t,  t, 0.0); 
			glVertex3f (-t,  t, 0.0); 
			glVertex3f (-t, -t, 0.0);
			glEnd ();
			glPopAttrib ();
		}
		glEnable (GL_LIGHTING);
		glDisable (GL_TEXTURE_2D);
	}
}
