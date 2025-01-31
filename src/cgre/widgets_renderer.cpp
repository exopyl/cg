#include "gl_wrapper.h"

#include <stdlib.h>
#include <stdio.h>

#include "widgets_renderer.h"

void screenshot (int win_width, int win_height)
{
  GLubyte *buffer = (GLubyte*) malloc (3*win_width*win_height*sizeof(GLubyte));
  GLubyte *zbuffer = (GLubyte*) malloc (win_width*win_height*sizeof(GLubyte));
  GLfloat *floating_zbuffer = (GLfloat*) malloc (win_width*win_height*sizeof(GLfloat));
  GLubyte *stencil_buffer = (GLubyte*) malloc (3*win_width*win_height*sizeof(GLubyte));

  glReadPixels (0, 0, win_width, win_height, GL_BGR, GL_UNSIGNED_BYTE, buffer);
  glReadPixels (0, 0, win_width, win_height, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, zbuffer);
  glReadPixels (0, 0, win_width, win_height, GL_DEPTH_COMPONENT, GL_FLOAT, floating_zbuffer);
  glReadPixels (0, 0, win_width, win_height, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, stencil_buffer);

#ifdef WIN32
   FILE *ptrbuffer = fopen("screenshot_buffer.bmp", "wb");
   if (!ptrbuffer)
	   return;

   BITMAPFILEHEADER bitmapFileHeader;
   BITMAPINFOHEADER bitmapInfoHeader;
   bitmapFileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
   bitmapFileHeader.bfReserved1 = 0;
   bitmapFileHeader.bfReserved2 = 0;
   bitmapFileHeader.bfSize = sizeof(BITMAPFILEHEADER);
   bitmapFileHeader.bfType = 0x4D42;
   bitmapInfoHeader.biBitCount = 24;//8;
   bitmapInfoHeader.biClrImportant = 0;
   bitmapInfoHeader.biClrUsed = 0;
   bitmapInfoHeader.biCompression = BI_RGB;
   bitmapInfoHeader.biHeight = win_height;
   bitmapInfoHeader.biPlanes = 1;
   bitmapInfoHeader.biSize = sizeof(BITMAPINFOHEADER);
   bitmapInfoHeader.biSizeImage = win_width * win_height * 3;
   bitmapInfoHeader.biWidth = win_width;
   bitmapInfoHeader.biXPelsPerMeter = 0;
   bitmapInfoHeader.biYPelsPerMeter = 0;

   fwrite(&bitmapFileHeader, 1, sizeof(BITMAPFILEHEADER), ptrbuffer); // bitmap info header
   fwrite(&bitmapInfoHeader, 1, sizeof(BITMAPINFOHEADER), ptrbuffer);
   fwrite(buffer, 1, bitmapInfoHeader.biSizeImage, ptrbuffer); // image data
   fclose(ptrbuffer);
#else
  FILE *ptrbuffer = fopen ("_screenshot_buffer.ppm", "w");
  fprintf (ptrbuffer, "P3\n%d %d 255\n", win_width, win_height);
  
  FILE *ptrzbuffer = fopen ("_screenshot_zbuffer.pgm", "w");
  fprintf (ptrzbuffer, "P2\n%d %d 255\n", win_width, win_height);
  
  FILE *ptrsbuffer = fopen ("_screenshot_sbuffer.pgm", "w");
  fprintf (ptrsbuffer, "P2\n%d %d\n255\n", win_width, win_height);
  
  for (int j=win_height-1; j>=0; j--)
  //for (int j=0; j<win_height; j++)
	  for (int i=0; i<win_width; i++)
	  {
		  fprintf (ptrbuffer, "%d %d %d\n", buffer[4*(j*win_width+i)], buffer[4*(j*win_width+i)+1], buffer[4*(j*win_width+i)+2]);
		  fprintf (ptrzbuffer, "%d\n", zbuffer[(j*win_width+i)]);
		  fprintf (ptrsbuffer, "%d\n", stencil_buffer[(j*win_width+i)]);//, stencil_buffer[3*(j*win_width+i)+1], stencil_buffer[3*(j*win_width+i)+2]);
	  }
  fclose (ptrbuffer);
  fclose (ptrzbuffer);
  fclose (ptrsbuffer);
#endif

  free (buffer);
  free (zbuffer);
  free (floating_zbuffer);
  free (stencil_buffer);
}

void repere_draw (void)
{
  glPushAttrib (GL_ALL_ATTRIB_BITS);
  glDisable(GL_LIGHTING);
  glDisable(GL_TEXTURE_2D);
  glDisable(GL_COLOR_MATERIAL);

  glBegin (GL_LINES);
  glLineWidth (5.0f);
  
  glColor3f(1.0f, 0.0f, 0.0f);
  glVertex3f(-0.2f,  0.0f, 0.0f);
  glVertex3f( 0.2f,  0.0f, 0.0f);
  glVertex3f( 0.2f,  0.0f, 0.0f);
  glVertex3f( 0.15f,  0.04f, 0.0f);
  glVertex3f( 0.2f,  0.0f, 0.0f);
  glVertex3f( 0.15f, -0.04f, 0.0f);
  
  glColor3f(0.0f, 1.0f, 0.0f);
  glVertex3f( 0.0f,  0.2f, 0.0f);
  glVertex3f( 0.0f, -0.2f, 0.0f);			
  glVertex3f( 0.0f,  0.2f, 0.0f);
  glVertex3f( 0.04f,  0.15f, 0.0f);
  glVertex3f( 0.0f,  0.2f, 0.0f);
  glVertex3f( -0.04f,  0.15f, 0.0f);
  
  glColor3f(0.0f, 0.0f, 1.0f);
  glVertex3f( 0.0f,  0.0f,  0.2f);
  glVertex3f( 0.0f,  0.0f, -0.2f);
  glVertex3f( 0.0f,  0.0f, 0.2f);
  glVertex3f( 0.0f,  0.04f, 0.15f);
  glVertex3f( 0.0f, 0.0f, 0.2f);
  glVertex3f( 0.0f, -0.04f, 0.15f);
  
  glEnd ();
  glPopAttrib ();
}

void repere_draw2 (void)
{
  glPushAttrib (GL_ALL_ATTRIB_BITS);

  glDisable (GL_CULL_FACE);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

  float length_negative = -0.3;
  float length = 0.7f;
  float base = 0.08f;

  glDisable (GL_LIGHTING);
  glLineWidth (2.0f);

  // X axis
  glColor3f(1.0f, 0.0f, 0.0f);
  glBegin (GL_LINES);
  glVertex3f(length_negative,  0.0f, 0.0f);
  glVertex3f(length,  0.0f, 0.0f);
  glEnd ();
  glPushMatrix ();
  glTranslatef (length/2.0, 0.0f, 0.0f);
  glRotatef (90.0, 0.0, 1.0, 0.0);
  //glutSolidCone (base, length/2.0, 50, 1);
  glPopMatrix ();
  
  // Y axis
  glColor3f(0.0f, 1.0f, 0.0f);
  glBegin (GL_LINES);
  glVertex3f( 0.0f, length, 0.0f);
  glVertex3f( 0.0f, length_negative, 0.0f);			
  glEnd ();
  glPushMatrix ();
  glTranslatef (0.0f, length/2.0, 0.0f);
  glRotatef (-90.0, 1.0, 0.0, 0.0);
  //glutSolidCone (base, length/2.0, 50, 1);
  glPopMatrix ();
  
  // Z axis
  glColor3f(0.0f, 0.0f, 1.0f);
  glBegin (GL_LINES);
  glVertex3f( 0.0f,  0.0f, length);
  glVertex3f( 0.0f,  0.0f, length_negative);
  glEnd ();
  glPushMatrix ();
  glTranslatef (0.0f, 0.0f, length/2.0);
  //glutSolidCone (base, length/2.0, 50, 1);
  glPopMatrix ();
  
  glPopAttrib ();
}

void draw_grid (float size, int nStep)
{
  glPushAttrib (GL_ALL_ATTRIB_BITS);
  glColor3f (0.f, 0.f, 0.f);
  glBegin (GL_LINES);
  const float hSize = .5f * size;
  float x, y;
  for (int j=0; j<= nStep; j++)
    for (int i=0; i<= nStep; i++)
      {
		x = i * size / nStep;
		y = j * size / nStep;
		glVertex3f ((GLfloat)(x - hSize), -(GLfloat)(hSize), 0.f);
		glVertex3f ((GLfloat)(x - hSize), (GLfloat)(hSize), 0.f);
		glVertex3f (-(GLfloat)(hSize), (GLfloat)(y - hSize), 0.f);
		glVertex3f ((GLfloat)(hSize), (GLfloat)(y - hSize), 0.f);
      }
  glEnd ();
  glPopAttrib ();
}

#if 0
#include "console.h"
void display_framerate (CFrameRate& framerate)
{
	GLint viewport[4];
	glGetIntegerv (GL_VIEWPORT, viewport);
	int width = (viewport[2]-viewport[0]);
	int height = (viewport[3]-viewport[1]);
	Console::getInstance()->print(2, height - 15, "framerate : %d", framerate.CalculateFrameRate());
}
#endif

//
// Drawing
//


void draw_cube ()
{
	glBegin(GL_QUADS);
		glNormal3f (0., 1., 0.);
		glVertex3f( 1.0f, 1.0f,-1.0f);
		glVertex3f(-1.0f, 1.0f,-1.0f);
		glVertex3f(-1.0f, 1.0f, 1.0f);
		glVertex3f( 1.0f, 1.0f, 1.0f);

		glNormal3f (0., -1., 0.);
		glVertex3f( 1.0f,-1.0f, 1.0f);
		glVertex3f(-1.0f,-1.0f, 1.0f);
		glVertex3f(-1.0f,-1.0f,-1.0f);
		glVertex3f( 1.0f,-1.0f,-1.0f);

		glNormal3f (0., 0., 1.);
		glVertex3f( 1.0f, 1.0f, 1.0f);
		glVertex3f(-1.0f, 1.0f, 1.0f);
		glVertex3f(-1.0f,-1.0f, 1.0f);
		glVertex3f( 1.0f,-1.0f, 1.0f);

		glNormal3f (0., 0., -1.);
		glVertex3f( 1.0f,-1.0f,-1.0f);
		glVertex3f(-1.0f,-1.0f,-1.0f);
		glVertex3f(-1.0f, 1.0f,-1.0f);
		glVertex3f( 1.0f, 1.0f,-1.0f);

		glNormal3f (-1., 0., 0.);
		glVertex3f(-1.0f, 1.0f, 1.0f);
		glVertex3f(-1.0f, 1.0f,-1.0f);
		glVertex3f(-1.0f,-1.0f,-1.0f);
		glVertex3f(-1.0f,-1.0f, 1.0f);

		glNormal3f (1., 0., 0.);
		glVertex3f( 1.0f, 1.0f,-1.0f);
		glVertex3f( 1.0f, 1.0f, 1.0f);
		glVertex3f( 1.0f,-1.0f, 1.0f);
		glVertex3f( 1.0f,-1.0f,-1.0f);
	glEnd();
}

void draw_sphere (float r)
{
	//glutSolidSphere (r, 100, 100);
}

void draw_teapot ()
{
	//glFrontFace(GL_CW);
	//glutSolidTeapot (1.);
	//glFrontFace(GL_CCW);
}



/**************/
/*** points ***/
/**************/
void draw_point (vec3 pt)
{
  glPushAttrib (GL_ALL_ATTRIB_BITS);
  glDisable (GL_LIGHTING);
  glColor3f (0.0f, 0.0f, 1.0f);

  glPushMatrix ();
  glTranslatef ((float)pt[0], (float)pt[1], (float)pt[2]);
  //glutSolidCube (5.0f);
  glPopMatrix ();
  glPopAttrib ();
}

void draw_point (vec3 pt, float r, float g, float b)
{
  glPushAttrib (GL_ALL_ATTRIB_BITS);
  glDisable (GL_LIGHTING);
  glColor3f (r, g, b);

  glPushMatrix ();
  glTranslatef ((float)pt[0], (float)pt[1], (float)pt[2]);
  //glutSolidCube (5.0f);
  glPopMatrix ();
  glPopAttrib ();
}

int draw_point (vec3 pt, int id)
{
  glPushAttrib (GL_ALL_ATTRIB_BITS);
  glDisable (GL_LIGHTING);
  glColor3f (0.0f, 0.0f, 1.0f);

  glPushMatrix ();
  glLoadName (id++);
  glTranslatef ((float)pt[0], (float)pt[1], (float)pt[2]);
  //glutSolidCube (5.0f);
  glPopMatrix ();
  glPopAttrib ();

  return id;
}

/**************/
/*** vector ***/
/**************/
void draw_vector (vec3 v, vec3 n, float r, float g, float b)
{
  glPushAttrib (GL_ALL_ATTRIB_BITS);
  glColor3f (r, g, b);
  glBegin (GL_LINES);
  glVertex3f (v[0], v[1], v[2]);
  glVertex3f (v[0] + n[0], v[1] + n[1], v[2] + n[2]);
  glEnd ();
  glPopAttrib ();
}

/***************/
/*** segment ***/
/***************/
void draw_segment (vec3 v1, vec3 v2, float r, float g, float b)
{
  glPushAttrib(GL_ALL_ATTRIB_BITS);
  glDisable(GL_LIGHTING);
  glColor3f(0.0f, 0.0, 1.0f);

  /* the line */
  glLineWidth(2.0f);
  glBegin (GL_LINES);
  glVertex3f (v1[0], v1[1], v1[2]);
  glVertex3f (v2[0], v2[1], v2[2]);
  glEnd ();

  /* the cubes of the line */
  glPushMatrix();
  glTranslatef(v1[0], v1[1], v1[2]);
  //glutSolidCube(5.0f);
  glTranslatef(-v1[0]+v2[0], -v1[1]+v2[1], -v1[2]+v2[2]);
  //glutSolidCube(5.0f);
  glPopMatrix();
  glPopAttrib();
}

/*************/
/*** lines ***/
/*************/
void
draw_line (vec3 begin, vec3 end, float r, float g, float b)
{
  glPushAttrib (GL_ALL_ATTRIB_BITS);
  glDisable (GL_LIGHTING);
  glColor3f (r, g, b);

  /* the line */
  glLineWidth (2.0f);
  glBegin (GL_LINES);
  glVertex3f (begin[0], begin[1], begin[2]);
  glVertex3f (end[0], end[1], end[2]);
  glEnd ();

  /* the cubes of the line */
  glPushMatrix ();
  glTranslatef (begin[0], begin[1], begin[2]);
  //glutSolidCube (5.0f);
  glTranslatef (-begin[0]+end[0], -begin[1]+end[1], -begin[2]+end[2]);
  //glutSolidCube (5.0f);

  //glLineWidth (1.0f);
  glPopMatrix ();
  glPopAttrib ();
}

/* with identification on each extremity */
int
draw_line (vec3 begin, vec3 end, int id)
{
  glPushAttrib (GL_ALL_ATTRIB_BITS);
  
  glDisable (GL_LIGHTING);
  glColor3f (0.0f, 0.0, 1.0f);

  /* the line */
  glLineWidth (2.0f);
  glBegin (GL_LINES);
  glVertex3f (begin[0], begin[1], begin[2]);
  glVertex3f (end[0], end[1], end[2]);
  glEnd ();

  /* the cubes of the line */
  glPushMatrix ();
  glTranslatef (begin[0], begin[1], begin[2]);
  glLoadName (id++);
  //glutSolidCube (5.0f);
  glTranslatef (-begin[0]+end[0], -begin[1]+end[1], -begin[2]+end[2]);
  glLoadName (id++);
  //glutSolidCube (5.0f);

  glLineWidth (1.0f);
  glPopMatrix ();
  glPopAttrib ();

  return id;
}

/**************/
/*** planes ***/
/**************/
void draw_plane (vec3 pt, vec3 normale)
{
  glPushAttrib (GL_ALL_ATTRIB_BITS);
  glDisable (GL_LIGHTING);
  glColor3f (0.0f, 0.0f, 1.0f);

  glPushMatrix ();
  glTranslatef (pt[0], pt[1], pt[2]);
  //glutSolidCube (5.0f);
  glLineWidth (3.0);
  glBegin (GL_LINES);
  glVertex3f (0.0f, 0.0f, 0.0f);
  glVertex3f (40.0*normale[0], 40.0*normale[1], 40.0*normale[2]);
  glEnd ();
  glLineWidth (1.0);
  glPopMatrix ();

  glPopAttrib ();
}

int
draw_plane (vec3 pt, vec3 normale, int id)
{
  glPushAttrib (GL_ALL_ATTRIB_BITS);
  glDisable (GL_LIGHTING);
  glColor3f (0.0f, 0.0f, 1.0f);

  glPushMatrix ();
  glTranslatef (pt[0], pt[1], pt[2]);
  glLoadName (id++);
  //glutSolidCube (5.0f);
  glLineWidth (3.0);
  glBegin (GL_LINES);
  glVertex3f (0.0f, 0.0f, 0.0f);
  glVertex3f (40.0*normale[0], 40.0*normale[1], 40.0*normale[2]);
  glEnd ();
  glLineWidth (1.0);
  glPopMatrix ();
  glPopAttrib ();

  return id;
}

/***************/
/*** circles ***/
/***************/
void
draw_circle (float x, float y, float z, float r)
{
  float i, step = (float)(2.0*3.14159/N_SLICES);

  glPushAttrib (GL_ALL_ATTRIB_BITS);
  glDisable (GL_LIGHTING);
  glColor3f (1.0f, 0.0f, 0.0f);

  glPushMatrix ();
  glTranslatef (x, y, z);
  glBegin (GL_LINE_LOOP);
  for (i=0.0; i<=2.0*3.14159; i+=step)
    glVertex2d (r*sin(i), r*cos(i));
  glEnd ();
  glPopMatrix ();

  glPopAttrib ();
}

void
draw_arc (float x, float y, float z, float r, float begin, float end) /* begin and end in radians */
{
  float i, step = (float)(2.0*3.14159/N_SLICES);
  step = (float)0.01;

  glPushAttrib (GL_ALL_ATTRIB_BITS);

  glDisable (GL_LIGHTING);
  glColor3f (0.0f, 0.0f, 1.0f);

  glPushMatrix ();
  glTranslatef (x, y, z);

  glPushMatrix ();
  glTranslatef (r*cos(begin), r*sin(begin), 0.0);
  //glutSolidCube (5.0f);
  glTranslatef (-r*cos(begin)+r*cos(end), -r*sin(begin)+r*sin(end), 0.0);
  //glutSolidCube (5.0f);
  glPopMatrix ();

  glLineWidth (2.0f);
  glBegin (GL_LINE_STRIP);
  for (i=begin; i<=end; i+=step)
    glVertex3f (r*cos(i), r*sin(i), 0.0);
  glEnd ();
  glLineWidth (1.0f);
  glPopMatrix ();

  glPopAttrib ();
}

int
draw_arc (float x, float y, float z, float r, float begin, float end, int id)
{
  glPushAttrib (GL_ALL_ATTRIB_BITS);

  glDisable (GL_LIGHTING);
  glColor3f (0.0f, 0.0f, 1.0f);

  glPushMatrix ();
  glTranslatef (x, y, z);

  glPushMatrix ();
  glTranslatef (r*cos(begin), r*sin(begin), 0.0);
  glLoadName (id++);
  //glutSolidCube (5.0f);
  glTranslatef (-r*cos(begin)+r*cos(end), -r*sin(begin)+r*sin(end), 0.0);
  glLoadName (id++);
  //glutSolidCube (5.0f);
  glPopMatrix ();

  //glLineWidth (2.0f);
  glBegin (GL_LINE_STRIP);
  float i, step = (float)(2.0*3.14159/N_SLICES);
  for (i=begin; i<=end; i+=step)
    glVertex3f (r*cos(i), r*sin(i), 0.0);
  glEnd ();
  //glLineWidth (1.0f);
  glPopMatrix ();

  glPopAttrib ();

  return id;
}

/*** sphere ***/
typedef float point[4];
/* initial tetrahedron */
point initial_tetrahedron[]={{0.0, 0.0, 1.0}, {0.0, 0.942809, -0.33333},
			     {-0.816497, -0.471405, -0.333333}, {0.816497, -0.471405, -0.333333}};
point initial_octahedron[]={{0.0, 0.0, 1.0},
			    {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {-1.0, 0.0, 0.0}, {0.0, -1.0, 0.0},
			    {0.0, 0.0, -1.0}};

static void
normal(point p)
{
  /* normalize a vector */
  float d =0.0;
  int i;
  for(i=0; i<3; i++) d+=p[i]*p[i];
  d=sqrt(d);
  if(d>0.0) for(i=0; i<3; i++) p[i]/=d;
}

static void
triangle (point a, point b, point c)
/* display one triangle using a line loop for wire frame, a single
normal for constant shading, or three normals for interpolative shading */
{
  glBegin(GL_POLYGON);
  glNormal3fv(a);
  glVertex3fv(a);
  glNormal3fv(c);
  glVertex3fv(c);
  glNormal3fv(b);
  glVertex3fv(b);
  glEnd();
}

static void
divide_triangle (point a, point b, point c, int n)
{
  /* triangle subdivision using vertex numbers
     righthand rule applied to create outward pointing faces */
  
  point v1, v2, v3;
  int j;
  if(n>0)
    {
      for(j=0; j<3; j++) v1[j]=a[j]+b[j];
      normal(v1);
      for(j=0; j<3; j++) v2[j]=a[j]+c[j];
      normal(v2);
      for(j=0; j<3; j++) v3[j]=b[j]+c[j];
      normal(v3);
      divide_triangle(a, v1, v2, n-1);
      divide_triangle(c, v2, v3, n-1);
      divide_triangle(b, v3, v1, n-1);
      divide_triangle(v1, v3, v2, n-1);
    }
  else(triangle(a,b,c)); /* draw triangle at end of recursion */
}

void
draw_sphere2 (int n)
{
  /* Apply triangle subdivision to faces of tetrahedron */
  /*
  divide_triangle (initial_tetrahedron[0], initial_tetrahedron[1], initial_tetrahedron[2], n);
  divide_triangle (initial_tetrahedron[3], initial_tetrahedron[2], initial_tetrahedron[1], n);
  divide_triangle (initial_tetrahedron[0], initial_tetrahedron[3], initial_tetrahedron[1], n);
  divide_triangle (initial_tetrahedron[0], initial_tetrahedron[2], initial_tetrahedron[3], n);
  */

  divide_triangle (initial_octahedron[0], initial_octahedron[1], initial_octahedron[2], n);
  divide_triangle (initial_octahedron[0], initial_octahedron[2], initial_octahedron[3], n);
  divide_triangle (initial_octahedron[0], initial_octahedron[3], initial_octahedron[4], n);
  divide_triangle (initial_octahedron[0], initial_octahedron[4], initial_octahedron[1], n);

  divide_triangle (initial_octahedron[5], initial_octahedron[2], initial_octahedron[1], n);
  divide_triangle (initial_octahedron[5], initial_octahedron[3], initial_octahedron[2], n);
  divide_triangle (initial_octahedron[5], initial_octahedron[4], initial_octahedron[3], n);
  divide_triangle (initial_octahedron[5], initial_octahedron[1], initial_octahedron[4], n);
}
