#include "background_manager.h"

#include <GL/gl.h>

BackgroundManager *BackgroundManager::m_pInstance = new BackgroundManager;

void BackgroundManager::select (BackgroundType eBg, char *path)
{
	m_eBgType = eBg;

	if (m_eBgType == BACKGROUND_SKYBOX)
	{
		if (m_pSkybox)
			delete m_pSkybox;

		m_pSkybox = new Skybox ();
		//m_pSkybox->load ("../data/skyboxes/sky01/");
		m_pSkybox->load ("../data/skyboxes/field.jpg");
	}
}

void BackgroundManager::display ()
{
	switch (m_eBgType)
	{
	case BACKGROUND_COLOR:
		DrawColor (m_color[0], m_color[1], m_color[2]);
		break;
	case BACKGROUND_GRADIENT:
		DrawGradient (m_gradient_color1[0], m_gradient_color1[1], m_gradient_color1[2],
						m_gradient_color2[0], m_gradient_color2[1], m_gradient_color2[2]);
		break;
	case BACKGROUND_SKYBOX:
		DrawSkybox ();
		break;
	default:
		break;
	}
}


BackgroundManager::BackgroundManager ()
{
	m_eBgType = BACKGROUND_COLOR;
	
	// color
	m_color[0] = 29.f/255.f;
	m_color[1] = 170.f/255.f;
	m_color[2] = 71.f/255.f;

	// gradient
	m_gradient_color1[0] = 1.;
	m_gradient_color1[1] = 0.;
	m_gradient_color1[2] = 0.;

	m_gradient_color2[0] = 0.;
	m_gradient_color2[1] = 0.;
	m_gradient_color2[2] = 1.;

	// skybox
	m_pSkybox = nullptr;
}

void BackgroundManager::DrawColor (float r, float g, float b)
{
	glClearColor(r, g, b, 0.);
}

void BackgroundManager::DrawGradient (float rtop, float gtop, float btop, float rbottom, float gbottom, float bbottom)
{
	glPushAttrib(GL_ALL_ATTRIB_BITS);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_LIGHTING);
	glBegin(GL_QUADS);
	//red color
	glColor3f(1.0,0.0,0.0);
	glVertex2f(-1.0,-1.0);
	glVertex2f(1.0,-1.0);
	//blue color
	glColor3f(0.0,0.0,1.0);
	glVertex2f(1.0, 1.0);
	glVertex2f(-1.0, 1.0);
	glEnd();
	glEnable(GL_DEPTH_TEST);

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);

	glPopAttrib();
}

void BackgroundManager::DrawSkybox (void)
{
	if (m_pSkybox)
		m_pSkybox->draw ();
}
