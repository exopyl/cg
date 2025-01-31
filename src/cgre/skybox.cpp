#include <stdio.h>
#include <string.h>

#include "skybox.h"
#include "../cgimg/cgimg.h"

#define GL_CLAMP_TO_EDGE 0x812F


///
/// Constructor
///
Skybox::Skybox ()
{
}

Skybox::~Skybox ()
{
	glDeleteTextures (6, texture);
}

void Skybox::load_directory(char *path)
{
	//
	// texture files
	//
	//char *path="../data/sky10/";
	char *frontfile  = "front.jpg";
	char *backfile   = "back.jpg";
	char *rightfile  = "right.jpg";
	char *leftfile   = "left.jpg";
	char *topfile    = "top.jpg";
	char *bottomfile = "bottom.jpg";

	char *front = (char*)malloc(256*sizeof(char));
	front = (char*)memcpy (front, path, strlen(path)+1);
	front = (char*)strcat (front, frontfile);

	char *back = (char*)malloc(256*sizeof(char));
	back = (char*)memcpy (back, path, strlen(path)+1);
	back = (char*)strcat (back, backfile);

	char *right = (char*)malloc(256*sizeof(char));
	right = (char*)memcpy (right, path, strlen(path)+1);
	right = (char*)strcat (right, rightfile);

	char *left = (char*)malloc(256*sizeof(char));
	left = (char*)memcpy (left, path, strlen(path)+1);
	left = (char*)strcat (left, leftfile);

	char *top = (char*)malloc(256*sizeof(char));
	top = (char*)memcpy (top, path, strlen(path)+1);
	top = (char*)strcat (top, topfile);

	char *bottom = (char*)malloc(256*sizeof(char));
	bottom = (char*)memcpy (bottom, path, strlen(path)+1);
	bottom = (char*)strcat (bottom, bottomfile);

	char *files[6];
	files[0] = front;
	files[1] = right;
	files[2] = back;
	files[3] = left;
	files[4] = top;
	files[5] = bottom;


	unsigned int width, height;
	unsigned char *data;

	Img *pImg = new Img ();
	for (int i=0; i<5; i++)
	{
		pImg->load (files[i]);
		data = pImg->m_pPixels;
		width = pImg->width ();
		height = pImg->height ();

		glGenTextures(1, &texture[i]);
		glBindTexture(GL_TEXTURE_2D, texture[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, 3, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
	delete pImg;

	free (front);
	free (back);
	free (right);
	free (left);
	free (top);
	free (bottom);
}

void Skybox::load_cross(char *path)
{
	Img *pImg = new Img ();
	int res = pImg->load (path);

	unsigned int width = pImg->width();
	unsigned int height = pImg->height();
	
	if (width / 4 != height / 3)
		return;

	unsigned int size = width / 4;

	Img *pTmp = new Img ();
	unsigned int xs[6] = {size, 2*size, 3*size, 0, size, size};
	unsigned int ys[6] = {size, size, size, size, 0, 2*size};
	for (int i=0; i<5; i++)
	{
		pTmp->crop(pImg, xs[i], ys[i], size, size);
		unsigned char *data = pTmp->m_pPixels;

		glGenTextures(1, &texture[i]);
		glBindTexture(GL_TEXTURE_2D, texture[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, 3, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
	delete pTmp;
	delete pImg;
}

void Skybox::load (char *path)
{
	if (path[strlen(path)-1] == '/')
		load_directory(path);
	else
		load_cross(path);
}

void Skybox::draw (void)
{
	glPushAttrib(GL_ENABLE_BIT|GL_POLYGON_BIT);
	glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
	
	glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
	glDisable(GL_LIGHTING);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	float size = 1.0;

	// Front Face
	glBindTexture(GL_TEXTURE_2D, texture[0]);
	glBegin(GL_QUADS);
	glTexCoord2f(0.0f, 0.0f); glVertex3f( size,  size,  size);
	glTexCoord2f(1.0f, 0.0f); glVertex3f( size, -size,  size);
	glTexCoord2f(1.0f, 1.0f); glVertex3f( size, -size, -size);
	glTexCoord2f(0.0f, 1.0f); glVertex3f( size,  size, -size);
	glEnd ();

	// Back Face
	glBindTexture(GL_TEXTURE_2D, texture[2]);
	glBegin(GL_QUADS);
	glTexCoord2f(0.0f, 0.0f); glVertex3f(-size, -size,  size);
	glTexCoord2f(1.0f, 0.0f); glVertex3f(-size,  size,  size);
	glTexCoord2f(1.0f, 1.0f); glVertex3f(-size,  size, -size);
	glTexCoord2f(0.0f, 1.0f); glVertex3f(-size, -size, -size);
	glEnd ();

	// Top Face
	glBindTexture(GL_TEXTURE_2D, texture[4]);
	glBegin(GL_QUADS);
	glTexCoord2f(0.0f, 0.0f); glVertex3f(-size,  size, size);
	glTexCoord2f(1.0f, 0.0f); glVertex3f(-size, -size, size);
	glTexCoord2f(1.0f, 1.0f); glVertex3f( size, -size, size);
	glTexCoord2f(0.0f, 1.0f); glVertex3f( size,  size, size);
	glEnd ();

	// Bottom Face
	//glBegin(GL_QUADS);
	//glTexCoord2f(1.0f, 1.0f); glVertex3f(-size, -size, -size);
	//glTexCoord2f(0.0f, 1.0f); glVertex3f( size, -size, -size);
	//glTexCoord2f(0.0f, 0.0f); glVertex3f( size, -size,  size);
	//glTexCoord2f(1.0f, 0.0f); glVertex3f(-size, -size,  size);
	//glEnd ();

	// Right Face
	glBindTexture(GL_TEXTURE_2D, texture[1]);
	glBegin(GL_QUADS);
	glTexCoord2f(0.0f, 0.0f); glVertex3f( size, -size,  size);
	glTexCoord2f(1.0f, 0.0f); glVertex3f(-size, -size,  size);
	glTexCoord2f(1.0f, 1.0f); glVertex3f(-size, -size, -size);
	glTexCoord2f(0.0f, 1.0f); glVertex3f( size, -size, -size);
	glEnd ();

	// Left Face
	glBindTexture(GL_TEXTURE_2D, texture[3]);
	glBegin(GL_QUADS);
	glTexCoord2f(0.0f, 0.0f); glVertex3f(-size, size,  size);
	glTexCoord2f(1.0f, 0.0f); glVertex3f( size, size,  size);
	glTexCoord2f(1.0f, 1.0f); glVertex3f( size, size, -size);
	glTexCoord2f(0.0f, 1.0f); glVertex3f(-size, size, -size);
	glEnd();


	glPopAttrib();

    glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glDisable (GL_TEXTURE_2D);
}
