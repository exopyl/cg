#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "gl_wrapper.h"

#include "offscreen_rendering_factory.h"

#include "../cgmath/cgmath.h"
#include "../cgmesh/cgmesh.h"

Coffscreen_rendering::Coffscreen_rendering (Mesh *_model)
{
  assert (_model);
  model = _model;
  buffer           = NULL;
  zbuffer          = NULL;
  floating_zbuffer = NULL;
  zoom = 1.0;

  /* temporaire */
  //buffer = (GLubyte*) malloc (3*200*200*sizeof(GLbyte));
  //assert (buffer);
  //zbuffer = (GLubyte*) malloc (3*30*30*sizeof(GLbyte));
  //assert (zbuffer);
}

Coffscreen_rendering::~Coffscreen_rendering ()
{
	/*
  if (buffer)           free (buffer);
  if (zbuffer)          free (zbuffer);
  if (floating_zbuffer) free (floating_zbuffer);
  */
}

void
Coffscreen_rendering::set_parameters_glulookat (float _x_eye, float _y_eye, float _z_eye,
						float _x_center, float _y_center, float _z_center,
						float _x_up, float _y_up, float _z_up)
{
  x_eye    = _x_eye;
  y_eye    = _y_eye;
  z_eye    = _z_eye;
  x_center = _x_center;
  y_center = _y_center;
  z_center = _z_center;
  x_up     = _x_up;
  y_up     = _y_up;
  z_up     = _z_up;
}

void
Coffscreen_rendering::alloc_memory_buffers (void)
{
  //if (buffer) free (buffer);
  buffer = (GLubyte*) malloc (4*width*height*sizeof(GLubyte));
  assert (buffer);

  //if (zbuffer) free (zbuffer);
  zbuffer = (GLubyte*) malloc (width*height*sizeof(GLubyte));
  assert (zbuffer);

  //if (floating_zbuffer) free (floating_zbuffer);
  floating_zbuffer = (GLfloat*) malloc (width*height*sizeof(GLfloat));
  assert (floating_zbuffer);
}

void
Coffscreen_rendering::set_parameters_glortho (float _left, float _right,
					      float _bottom, float _top,
					      float _near_plane, float _far_plane)
{
  left       = _left;
  right      = _right;
  bottom     = _bottom;
  top        = _top;
  near_plane = _near_plane;
  far_plane  = _far_plane;

  width  = (int)(zoom*right - zoom*left);//zoom*(int)(right - left);
  height = (int)(zoom*top - zoom*bottom);//zoom*(int)(top - bottom);
  width = 256;
  height = 256;
  printf ("w = %d\nh = %d\n", width, height);

  alloc_memory_buffers ();
}

void
Coffscreen_rendering::set_zoom (float _zoom)
{
  zoom   = _zoom;
  width  = (int)(zoom*right - zoom*left);//zoom*(int)(right - left);
  height = (int)(zoom*top - zoom*bottom);//zoom*(int)(top - bottom);

  alloc_memory_buffers ();
}

void
Coffscreen_rendering::draw_object (void)
{
  /* Draw the model */
  glClearColor(1.0, 1.0, 1.0, 0.0);

  glEnable(GL_LIGHTING);
  glEnable(GL_LIGHT0);
  glEnable(GL_DEPTH_TEST);
  //glClearDepth(1.0f);
  
  /* DrawElements */
  glEnableClientState (GL_NORMAL_ARRAY);
  //glEnableClientState (GL_COLOR_ARRAY);
  glEnableClientState (GL_VERTEX_ARRAY);

  glViewport (0, 0, width, height);

  glMatrixMode (GL_PROJECTION);
  glLoadIdentity ();
  glOrtho (left, right, bottom, top, near_plane, far_plane);

  glMatrixMode (GL_MODELVIEW);
  glLoadIdentity ();
  //printf ("%f %f %f %f %f %f %f %f %f\n", x_eye, y_eye, z_eye, x_center, y_center, z_center, x_up, y_up, z_up);
  gluLookAt (x_eye, y_eye, z_eye, x_center, y_center, z_center, x_up, y_up, z_up);

  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glPushMatrix();


	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glBegin(GL_TRIANGLES);
	for (unsigned int i=0; i<model->m_nFaces; i++)
	{
		Face *pFace = model->m_pFaces[i];

		unsigned int a = pFace->m_pVertices[0];
		unsigned int b = pFace->m_pVertices[1];
		unsigned int c = pFace->m_pVertices[2];
		glVertex3f (model->m_pVertices[3*a], model->m_pVertices[3*a+1], model->m_pVertices[3*a+2]);
		glVertex3f (model->m_pVertices[3*b], model->m_pVertices[3*b+1], model->m_pVertices[3*b+2]);
		glVertex3f (model->m_pVertices[3*c], model->m_pVertices[3*c+1], model->m_pVertices[3*c+2]);
	}
	glEnd();


/*
  glNormalPointer (GL_FLOAT, 0, model->m_pVertexNormals);
  //glColorPointer  (3, GL_FLOAT, 0, model->get_vertices_colors());
  glVertexPointer (3, GL_FLOAT, 0, model->m_pVertices);
  glDrawElements (GL_TRIANGLES, 3*model->get_n_faces(), GL_UNSIGNED_INT, model->get_faces());
*/

  glPopMatrix();
}

void
Coffscreen_rendering::fill_buffers (void)
{
  GLubyte *t_buffer = (GLubyte*) malloc (4*width*height*sizeof(GLubyte));
  GLubyte *t_zbuffer = (GLubyte*) malloc (width*height*sizeof(GLubyte));
  GLfloat *t_floating_zbuffer = (GLfloat*) malloc (width*height*sizeof(GLfloat));


  glReadPixels (0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, t_buffer);
  glReadPixels (0, 0, width, height, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, t_zbuffer);
  glReadPixels (0, 0, width, height, GL_DEPTH_COMPONENT, GL_FLOAT, t_floating_zbuffer);

  buffer           = (GLubyte*)memcpy (buffer, t_buffer, 4*width*height*sizeof(GLbyte));
  zbuffer          = (GLubyte*)memcpy (zbuffer, t_zbuffer, width*height*sizeof(GLbyte));
  floating_zbuffer = (GLfloat*)memcpy (floating_zbuffer, t_floating_zbuffer, width*height*sizeof(GLfloat));

  return;
  // buffer
  glReadPixels (0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, buffer);

  // zbuffer
  glReadPixels (0, 0, width, height, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, zbuffer);

  // floating zbuffer
  glReadPixels (0, 0, width, height, GL_DEPTH_COMPONENT, GL_FLOAT, floating_zbuffer);
}

#ifdef MESA
#include <GL/osmesa.h>

void
Coffscreen_rendering::rendering (char *output)
{
  printf ("offscreen MESA\n");

  /* create a RGBA-mode context */
  OSMesaContext ctx = OSMesaCreateContext (GL_RGBA, NULL);
  /* Allocate the image buffer */
  GLubyte *buffer = (GLubyte*)malloc (width*height*4*sizeof(GLubyte));
  /* Bind the buffer to the context and make it current */
  OSMesaMakeCurrent (ctx, buffer, GL_UNSIGNED_BYTE, width, height);
  
  /* let's draw the object */
  draw_object ();
  
  int i;
  FILE *ptr;
  ptr = fopen ("screenshot_mesa.ppm", "w");
  if (ptr == NULL) return;
  fprintf (ptr, "P6\n%d %d\n255\n", width, height);
  for (i=height-1; i>=0; i--)
    fwrite (&buffer[3*width*i], sizeof(GLubyte), 3*width, ptr);
  fclose (ptr);
  
  /* free the image buffer */
  free (buffer);
  /* destroy the context */
  OSMesaDestroyContext (ctx);
}
#endif /* MESA */

#ifdef linux
#include <GL/glx.h>
static int attributesList[] = {
  GLX_RGBA,
  //GLX_DOUBLEBUFFER,
  GLX_DEPTH_SIZE, 24,
  GLX_RED_SIZE, 5,
  GLX_GREEN_SIZE, 5,
  GLX_BLUE_SIZE, 5,
  None
};
/*
static int attributesList[] = {
  GLX_RGBA,
  GLX_RED_SIZE, 1,
  GLX_GREEN_SIZE, 1,
  GLX_BLUE_SIZE, 1,
  GLX_DEPTH_SIZE, 16, None};
*/
void
Coffscreen_rendering::rendering (char *output)
{
  printf ("offscreen GLX\n");

  Display *dpy;
  dpy = XOpenDisplay (0); /* open a X display connection */
  if (dpy == NULL)
    {
      printf ("couldn't open the display\n");
      return;
    }

  /* select a X visual */
  int screen = DefaultScreen (dpy);
  XVisualInfo *vis = glXChooseVisual (dpy, screen, attributesList);
  if (vis == NULL)
    {
      printf ("couldn't open the visual\n");
      return;
    }

  /* get a context */
  GLXContext context = glXCreateContext (dpy, vis, 0, GL_FALSE);
  if (context == NULL)
    {
      printf ("couldn't create a rendering context\n");
      return;
    }

  /* create a X pixmap specifying the depth of the X visual */
  Pixmap pixmap = XCreatePixmap (dpy, RootWindow (dpy, vis->screen), width, height, vis->depth);

  /* create the GLX pixmap */
  GLXPixmap glx_pixmap = glXCreateGLXPixmap (dpy, vis, pixmap); 

  /* bind an OpenGL rendering context to the GLX pixmap */
  glXMakeCurrent (dpy, glx_pixmap, context);

  draw_object ();  /* let's draw the object */
  fill_buffers (); /* and fill the buffers  */

  /* cleaning */
  glXDestroyGLXPixmap (dpy, glx_pixmap);
  XFreePixmap (dpy, pixmap);
  glXDestroyContext (dpy, context);
  XFree (vis);
  XCloseDisplay (dpy);
}
#endif /* linux */

#ifdef WIN32
#include <windows.h>
void
Coffscreen_rendering::rendering (char *output)
{
  printf ("offscreen WGL\n");
  printf ("%d %d\n", width, height);

  GLubyte *bits = (GLubyte*) malloc (4*width*height*sizeof(GLbyte));
  assert (bits);

  /* store the old device context */
  HDC m_hOldDC   = ::wglGetCurrentDC();
  HGLRC m_hOldRC = ::wglGetCurrentContext(); 
  
  /* create a new device context */
  HDC mHDC = CreateCompatibleDC (NULL);
  
  /* create a BITMAPINFOHEADER structure */
  BITMAPINFOHEADER *mBIH = new BITMAPINFOHEADER;
  int iSize = sizeof (BITMAPINFOHEADER);
  memset (mBIH, 0, iSize);
  mBIH->biSize        = iSize;
  mBIH->biWidth       = width;
  mBIH->biHeight      = height;
  mBIH->biPlanes      = 1;
  mBIH->biBitCount    = 32;
  mBIH->biCompression = BI_RGB;
  mBIH->biClrUsed     = 0;
  
  /* create the DIB section */
  HBITMAP mHBmp = CreateDIBSection (mHDC, (BITMAPINFO*)mBIH, DIB_RGB_COLORS, (void**)&bits, NULL, 0);
  if (mHBmp)
    HBITMAP nHBmpOld = (HBITMAP)::SelectObject (mHDC, mHBmp);
  
  /* pixel format */
  PIXELFORMATDESCRIPTOR pfd;
  memset (&pfd, 0, sizeof (pfd));
  pfd.nSize      = sizeof (pfd);
  pfd.nVersion   = 1;
  pfd.dwFlags    = PFD_DRAW_TO_BITMAP | PFD_SUPPORT_OPENGL | PFD_SUPPORT_GDI;
  pfd.iPixelType = PFD_TYPE_RGBA;
  pfd.cColorBits = 32;
  pfd.cDepthBits = 24;
  pfd.iLayerType = PFD_MAIN_PLANE;
  
  /* get the device context */
  int pixelformat = ChoosePixelFormat (mHDC, &pfd);
  SetPixelFormat (mHDC, pixelformat, &pfd);
  
  /* create GL context */
  HGLRC hGLRC = wglCreateContext (mHDC);
  
  /* make the context current */
  wglMakeCurrent (mHDC, hGLRC);


  /* let's draw the object */
  draw_object ();

  /* and fill the buffers */
  fill_buffers ();
/*
  GLubyte *t_buffer = (GLubyte*) malloc (4*width*height*sizeof(GLubyte));
  GLubyte *t_zbuffer = (GLubyte*) malloc (width*height*sizeof(GLubyte));
  float *t_floating_zbuffer = (float*) malloc (width*height*sizeof(float));


  glReadPixels (0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, t_buffer);
  glReadPixels (0, 0, width, height, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, t_zbuffer);
  glReadPixels (0, 0, width, height, GL_DEPTH_COMPONENT, GL_FLOAT, t_floating_zbuffer);

  buffer           = (GLubyte*)memcpy (buffer, t_buffer, 4*width*height*sizeof(GLbyte));
  zbuffer          = (GLubyte*)memcpy (zbuffer, t_zbuffer, width*height*sizeof(GLbyte));
  floating_zbuffer = (float*)memcpy (floating_zbuffer, t_floating_zbuffer, width*height*sizeof(float));
*/
  //save_render_floating_zbuffer (output);
  //save_render (output);
  //save_render_zbuffer (output);

return;
  /* cleaning */
  wglDeleteContext (hGLRC);
  DeleteObject (mHBmp);
  DeleteDC (mHDC);
  delete mBIH;
  
  /*restore the old context */
  wglMakeCurrent(m_hOldDC, m_hOldRC);

  free (bits);
}
#endif /* WIN32 */

/* get renders */
void
Coffscreen_rendering::get_render (unsigned char *render, int *_width, int *_height)
{
  *_width  = width;
  *_height = height;
  render  = (unsigned char*)malloc(3*width*height*sizeof(unsigned char));
  assert (render);
  memcpy (render, buffer, 3*width*height*sizeof(unsigned char));
}

unsigned char*
Coffscreen_rendering::get_render_zbuffer (unsigned char *render, int *_width, int *_height)
{
  *_width  = width;
  *_height = height;
  render  = (unsigned char*)malloc(width*height*sizeof(unsigned char));
  assert (render);
  memcpy (render, zbuffer, width*height*sizeof(unsigned char));
  return render;
}

float*
Coffscreen_rendering::get_render_floating_zbuffer (float *render, int *_width, int *_height)
{
  *_width  = width;
  *_height = height;
  render  = (float*)malloc(width*height*sizeof(float));
  assert (render);
  memcpy (render, zbuffer, width*height*sizeof(float));
  return render;
}

/* export functions */
void
Coffscreen_rendering::save_render (char *filename)
{
  FILE *ptr;
  ptr = fopen (filename, "wb");
  if (ptr == NULL) return;
//#ifdef linux
  fprintf (ptr, "P6\n%d %d\n255\n", width, height);
  for (int i=height-1; i>=0; i--)
    fwrite (&buffer[3*width*i], sizeof(unsigned char), 3*width, ptr);
//#endif
/*
#ifdef WIN32
  fprintf (ptr, "P3\n%d %d\n255\n", width, height);
  for (i=height-1; i>=0; i--)
    for (j=0; j<width; j++)
      fprintf (ptr, "%d %d %d\n",
	       buffer[4*(width*i+j)],
	       buffer[4*(width*i+j)+1],
	       buffer[4*(width*i+j)+2]);
#endif
*/
  fclose (ptr);
}

void
Coffscreen_rendering::save_render_zbuffer (char *filename)
{
  int i,j;
  FILE *ptr;
  ptr = fopen (filename, "w");
  if (ptr == NULL) return;
//#ifdef linux
  fprintf (ptr, "P5\n%d %d\n255\n", width, height);
  for (i=height-1; i>=0; i--)
    fwrite (&zbuffer[width*i], sizeof(unsigned char), width, ptr);
//#endif // linux
  /*
#ifdef WIN32
  fprintf (ptr, "P2\n%d %d\n255\n", width, height);
  for (i=height-1; i>=0; i--)
    for (j=0; j<width; j++)
      fprintf (ptr, "%d\n", zbuffer[width*i+j]);
#endif // WIN32
  */
  fclose (ptr);
}

void
Coffscreen_rendering::save_render_floating_zbuffer (char *filename)
{
  int i,j;
  FILE *ptr;
  ptr = fopen (filename, "w");
  if (ptr == NULL) return;

  /*
  fprintf (ptr, "%d %d\n", width, height);
  for (i=height-1; i>=0; i--)
  {
    for (j=0; j<width; j++)
      fprintf (ptr, "%f ", floating_zbuffer[width*i+j]);
	  //fprintf (ptr, "%f\n", (floating_zbuffer[width*i+j]-near_plane) / (far_plane - near_plane));
	printf ("\n");
  }
  */  

  for (i=2294; i>=1825; i--)
  {
    for (j=2786; j<3343; j++)
      fprintf (ptr, "%f ", floating_zbuffer[width*i+j]);
	  //fprintf (ptr, "%f\n", (floating_zbuffer[width*i+j]-near_plane) / (far_plane - near_plane));
	fprintf (ptr, "\n");
  }

  fclose (ptr);
}

//#include "console.h"
//extern Console *console;
void
Coffscreen_rendering::dump (void)
{
  printf ("OFFSCREEN_RENDERING dump:\n");
  printf ("width = %d\nheight = %d\n", width, height);
  printf ("zoom = %f\n", zoom);
  
  printf ("%f %f %f %f %f %f\n", left, right, bottom, top, near_plane, far_plane);
  printf ("%f %f %f %f %f %f %f %f %f\n",
	  x_eye, y_eye, z_eye,
	  x_center, y_center, z_center,
	  x_up, y_up, z_up);
  
  /* console */
  /*
  console->append (QString().sprintf ("OFFSCREEN_RENDERNG dump:"));
  console->append (QString().sprintf ("width = %d\nheight = %d", width, height));	
  console->append (QString().sprintf ("zoom = %d", zoom));
  console->append (QString().sprintf ("%f %f %f %f %f %f", left, right, bottom, top, near_plane, far_plane));
  console->append (QString().sprintf ("%f %f %f %f %f %f %f %f %f",
				      x_eye, y_eye, z_eye,
				      x_center, y_center, z_center,
				      x_up, y_up, z_up));
  */
}
