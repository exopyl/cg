#pragma once

#include <stdlib.h>
#include "gl_wrapper.h"


/**
*
*/
class Projector
{
public:
	Projector ();
	~Projector ();


	void SetLinearFilter (GLboolean status) { m_linearFilter = status; };
	void SetShowProjection (GLboolean status) { m_showProjection = status; };

	void loadSpotlightTexture(void);
	void SetPicture (char *filename);


	void PreDisplay (void);
	void PostDisplay (void);

private:
	void loadTextureProjection(GLfloat m[16]);
	void drawTextureProjection(void);


public:
	GLboolean m_linearFilter;
	GLboolean m_showProjection;

	GLfloat m_objectXform[4][4];
	GLfloat m_textureXform[4][4];

	float m_eye[3];
	float m_lookAt[3];
	float m_up[3];

	float m_angle1;
	float m_angle2;


	float xmin, xmax;
	float ymin, ymax;
	float nnear;
	float ffar;
	float distance;

};
