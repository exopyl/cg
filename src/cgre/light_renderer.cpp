#include "gl_wrapper.h"

#include <string.h>
#include <math.h>

#include "light_renderer.h"
#include "../cgmesh/lights_manager.h"
#include "widgets_renderer.h"

LightRenderer *LightRenderer::m_pInstance = new LightRenderer;

LightRenderer::LightRenderer()
{
}

LightRenderer::~LightRenderer()
{
}

static unsigned int _id2gl[8] = {GL_LIGHT0, GL_LIGHT1, GL_LIGHT2, GL_LIGHT3, GL_LIGHT4, GL_LIGHT5, GL_LIGHT6, GL_LIGHT7}; // convert internal ids into id for OpenGL

void LightRenderer::EnableLighting (bool bFlag)
{
	LightsManager *pLightsManager = LightsManager::getInstance();
	//pLightsManager->EnableLighting (true);

	if (bFlag == true)	glEnable	(	GL_LIGHTING	);
	else				glDisable	(	GL_LIGHTING	);

	for (int i=0; i<pLightsManager->m_nLights; i++)
	{
		Light *pLight = pLightsManager->m_pLights[i];
		if (pLight->GetEnable ())
		{
			EnableLight (i);

			if (pLight->GetDisplay ())
				DisplayLight (pLight);		
		}
		else
			glDisable (_id2gl[i]);
	}
}

void LightRenderer::EnableLight (unsigned int id)
{
	LightsManager *pLightsManager = LightsManager::getInstance();

	Light *light = pLightsManager->m_pLights[id];
	glEnable (_id2gl[id]);

	GLfloat shininess[]	= { 100.0f }; 
	GLfloat ambient[4]	= { light->Ambient[0], light->Ambient[1], light->Ambient[2], light->Ambient[3]	};
	GLfloat diffuse[4]	= { light->Diffuse[0], light->Diffuse[1], light->Diffuse[2], light->Diffuse[3]	};
	GLfloat specular[4]	= { light->Specular[0], light->Specular[1], light->Specular[2], light->Specular[3]	};

	glLightfv ( _id2gl[id], GL_AMBIENT,	 ambient  );
	glLightfv ( _id2gl[id], GL_DIFFUSE,  diffuse  );
	glLightfv ( _id2gl[id], GL_SPECULAR, specular );

	switch (light->Type)
	{
	case CG_LIGHT_TYPE_POINT: //POINT
		{
			GLfloat position[]		= { light->Position[0],
										light->Position[1],
										light->Position[2],
										1.0							};
			glLightfv ( _id2gl[id], GL_POSITION, position );
		}
		break;
	case CG_LIGHT_TYPE_SPOT: //SPOT
		{
			GLfloat position[]		= { light->Position[0],
										light->Position[1],
										light->Position[2],
										1.0							};
			GLfloat direction[]		= { light->Direction[0],
										light->Direction[1],
										light->Direction[2]	};

			glLightfv ( _id2gl[id], GL_POSITION, position);
			glLightfv ( _id2gl[id], GL_SPOT_DIRECTION, direction);
			glLightf  ( _id2gl[id], GL_SPOT_EXPONENT, 0.0);
			glLightf  ( _id2gl[id], GL_SPOT_CUTOFF, 45.0);//light->Range  );
		}
		break;
	case CG_LIGHT_TYPE_DIRECTIONAL: //DIRECTIONAL
		{
			GLfloat position[]		= { light->Position[0],
										light->Position[1],
										light->Position[2],
										0.0							};
			glLightfv ( _id2gl[id], GL_POSITION, position );
		}
		break;
	default:
		break;
	}
}

void LightRenderer::DisplayLight (Light *pLight)
{
	GLfloat position[] = { pLight->Position[0], pLight->Position[1], pLight->Position[2], 1. };

 	glPushAttrib (GL_ALL_ATTRIB_BITS);
	glDisable (GL_LIGHTING);
	glColor3f (pLight->Diffuse[0], pLight->Diffuse[1], pLight->Diffuse[2]);
	switch (pLight->Type)
	{
	case CG_LIGHT_TYPE_POINT:
		{
			glPushMatrix ();
			glTranslatef (position[0], position[1], position[2]);
			draw_sphere (.2f);
			glPopMatrix ();
		}
		break;
	case CG_LIGHT_TYPE_SPOT:
	case CG_LIGHT_TYPE_DIRECTIONAL:
		{
			GLfloat direction[]	= { pLight->Direction[0], pLight->Direction[1], pLight->Direction[2] };
			glPushMatrix ();
			glBegin (GL_LINES);
			glVertex3f (position[0], position[1], position[2]);
			glVertex3f (position[0]+5.0*direction[0], position[1]+5.0*direction[1], position[2]+5.0*direction[2]);
			glEnd ();
			glTranslatef (position[0], position[1], position[2]);
			if (pLight->Type == CG_LIGHT_TYPE_SPOT)
			{
				////
				/// could be factorized
				/// begin...

				// normalize the direction
				GLfloat l = sqrt (	direction[0]*direction[0] + 
									direction[1]*direction[1] +
									direction[2]*direction[2]	);
				GLfloat d[3] = {direction[0]/l, direction[1]/l, direction[2]/l};

				// get the axis
				GLfloat axis[3] = {d[1], -d[0], 0.};
				l = sqrt (	axis[0]*axis[0] + 
							axis[1]*axis[1] +
							axis[2]*axis[2]	);
				axis[0] /= l;
				axis[1] /= l;
				axis[2] /= l;

				// get the angle
				GLfloat angle = acos (-d[2]);

				// get the transform matrix
				GLfloat transform[4][4] = {	{1., 0., 0., 0.},
											{0., 1., 0., 0.},
											{0., 0., 1., 0.},
											{0., 0., 0., 1.}	};
				GLfloat rcos = cos(angle);
				GLfloat rsin = sin(angle);
				GLfloat u = axis[0];
				GLfloat v = axis[1];
				GLfloat w = axis[2];
				/*
				transform[0][0] =      rcos + u*u*(1-rcos);
				transform[1][0] =  w * rsin + v*u*(1-rcos);
				transform[2][0] = -v * rsin + w*u*(1-rcos);
				transform[0][1] = -w * rsin + u*v*(1-rcos);
				transform[1][1] =      rcos + v*v*(1-rcos);
				transform[2][1] =  u * rsin + w*v*(1-rcos);
				transform[0][2] =  v * rsin + u*w*(1-rcos);
				transform[1][2] = -u * rsin + v*w*(1-rcos);
				transform[2][2] =      rcos + w*w*(1-rcos);
				*/
				transform[0][0] =      rcos + u*u*(1-rcos);
				transform[0][1] =  w * rsin + v*u*(1-rcos);
				transform[0][2] = -v * rsin + w*u*(1-rcos);
				transform[1][0] = -w * rsin + u*v*(1-rcos);
				transform[1][1] =      rcos + v*v*(1-rcos);
				transform[1][2] =  u * rsin + w*v*(1-rcos);
				transform[2][0] =  v * rsin + u*w*(1-rcos);
				transform[2][1] = -u * rsin + v*w*(1-rcos);
				transform[2][2] =      rcos + w*w*(1-rcos);
				//// ...end

				// apply the transform matrix
				glMultMatrixf ((GLfloat *)transform);

				// draw a cone
				glTranslatef (0.0, 0.0, -2.0);
				glDisable (GL_CULL_FACE);
				glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
				//glutSolidCone (1.0, 2.0, 20, 20); // TODO
			}
			else
			{
				draw_sphere (.2f);
			}
			glPopMatrix ();
		}
		break;
	default:
		break;
	}
	glEnable (GL_LIGHTING);
	glPopAttrib ();
}
