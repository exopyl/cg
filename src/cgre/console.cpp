#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "console.h"

#ifdef linux
#include <GL/glx.h>

static int base;

void
font_init (void)
{
  Display *dpy;
  XFontStruct *fontInfo;
  base = glGenLists (96);
  dpy = XOpenDisplay (NULL);
  fontInfo = XLoadQueryFont (dpy, "-*-helvetica-bold-r-normal-*-*-120-*-*-*-*-iso8859-1");
  if (fontInfo == NULL)
    {
      fontInfo = XLoadQueryFont (dpy, "fixed");
      if (fontInfo == NULL)
	printf ("no X font available\n");
    }
  glXUseXFont (fontInfo->fid, 32, 96, base);
  XFreeFont (dpy, fontInfo);
  XCloseDisplay (dpy);
}

void
font_destroy (void)
{
  glDeleteLists (base, 96);
}
#endif // linux

#ifdef WIN32
#include <windows.h>
#include <gl/gl.h>
#include <gl/glu.h>
/*

static int base;
HDC hDC = NULL;

void
font_init (void)
{
  hDC = ::wglGetCurrentDC();
  HFONT	font;
  HFONT	oldfont;
  
  base = glGenLists(96);
  
  font = CreateFont(-24,
		    0,
		    0,
		    0,
		    FW_BOLD,
		    FALSE,
		    FALSE,
		    FALSE,
		    ANSI_CHARSET,
		    OUT_TT_PRECIS,
		    CLIP_DEFAULT_PRECIS,
		    ANTIALIASED_QUALITY,
		    FF_DONTCARE|DEFAULT_PITCH,
		    (const unsigned short*)"Courier New");
  oldfont = (HFONT)SelectObject(hDC, font);
  wglUseFontBitmaps(hDC, 32, 96, base);
  SelectObject(hDC, oldfont);
  DeleteObject(font);
}

void
font_destroy (void)
{
  glDeleteLists(base, 96);
}
*/
#endif // WIN32


#include <GL/glut.h>

Console* Console::getInstance (void)
{
	return m_pInstance;
}

Console::Console ()
{
  font = (char*)GLUT_BITMAP_8_BY_13;
  r = 1.0f;
  g = 0.0f;
  b = 0.0f;
}

Console::~Console ()
{
}

void Console::set_size_screen (unsigned int _width, unsigned int _height)
{
  width  = _width;
  height = _height;
}

void Console::set_font (void *_font)
{
  font = (char*)_font;
}

void Console::set_color (float _r, float _g, float _b)
{
  r = _r;
  g = _g;
  b = _b;
}

void Console::print (int x, int y, char *fmt, ...)
{
  if (fmt == NULL)
    return;
  
	// construct the final text to print
	char text[256];
	va_list ap;
	va_start(ap, fmt);
	vsprintf(text, fmt, ap);
	va_end(ap);
  
	// push ortho
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	gluOrtho2D(0,width,0,height);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
		
	glDisable(GL_LIGHTING);
	glColor3f(r, g, b);

	// print the text
	glRasterPos2i (x, y);
	int length = (int)strlen(text);
	//for (int i=0; i<length; i++)
	//	glutBitmapCharacter (font, text[i]);

	// pop ortho
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
}

Console *Console::m_pInstance = new Console();
