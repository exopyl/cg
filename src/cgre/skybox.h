#pragma once

#ifdef WIN32
#include <windows.h>
#endif
#include <GL/gl.h>

class Skybox
{
public:
	Skybox ();
	~Skybox ();

	void load (char *path);
	void draw (void);

private:
	void load_directory(char *path);
	void load_cross(char *path);
	GLuint texture[6];


	/*
	void load_bmp (char *path);
	void load_jpeg (char *path);

	void draw_bmp (void);
	void draw_jpeg (void);
	*/
};
