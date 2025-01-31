#include "projectorsManager.h"

#include <stdlib.h>
#include "gl_wrapper.h"

ProjectorsManager::ProjectorsManager ()
{
}

ProjectorsManager::~ProjectorsManager ()
{
}

void ProjectorsManager::init ()
{
	GLfloat eyePlaneS[] = {1.0, 0.0, 0.0, 0.0};
	GLfloat eyePlaneT[] = {0.0, 1.0, 0.0, 0.0};
	GLfloat eyePlaneR[] = {0.0, 0.0, 1.0, 0.0};
	GLfloat eyePlaneQ[] = {0.0, 0.0, 0.0, 1.0};

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_EYE_LINEAR);
	glTexGenfv(GL_S, GL_EYE_PLANE, eyePlaneS);

	glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_EYE_LINEAR);
	glTexGenfv(GL_T, GL_EYE_PLANE, eyePlaneT);

	glTexGeni(GL_R, GL_TEXTURE_GEN_MODE, GL_EYE_LINEAR);
	glTexGenfv(GL_R, GL_EYE_PLANE, eyePlaneR);

	glTexGeni(GL_Q, GL_TEXTURE_GEN_MODE, GL_EYE_LINEAR);
	glTexGenfv(GL_Q, GL_EYE_PLANE, eyePlaneQ);
}
