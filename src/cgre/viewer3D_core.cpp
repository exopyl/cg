#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#include "gl_wrapper.h"

#include "../cgmesh/cgmesh.h"

#include "viewer3D_core.h"
#include "widgets_renderer.h"
#include "examinator_trackball.h"
#include "console.h"
#include "material_renderer.h"
#include "light_renderer.h"
#include "mesh_renderer.h"
#include "shaders_manager.h"
#include "background_manager.h"

Ctrackball *_g_examinator = NULL;

bool Viewer3DInitializeGL (void);
void Viewer3DResizeGLScene (unsigned int width, unsigned int height);
bool Viewer3DDrawGLScene (void);
void Viewer3DDrawGLScene_callback (void);

void Viewer3DIdleFunc (void);

static bool (*Viewer3DDrawGeneral)(void) = NULL;
static bool (*Viewer3DDrawCustom)(void) = NULL;
static bool (*Viewer3DDestroyCustom)(void) = NULL;
static bool (*Viewer3DKeyPressedCustom)(int key, int x, int y) = NULL;

static bool (*Viewer3DMouseFuncCustom)(int button, int state, int x, int y) = NULL;

void set_Viewer3DDrawGeneral (bool (*pFunction)(void))
{
	Viewer3DDrawGeneral = pFunction;
}

void set_Viewer3DDrawCustom (bool (*pFunction)(void))
{
	Viewer3DDrawCustom = pFunction;
}

void set_Viewer3DDestroyCustom (bool (*pFunction)(void))
{
	Viewer3DDestroyCustom = pFunction;
}

void set_Viewer3DKeyPressedCustom (bool (*pFunction)(int key, int x, int y))
{
	Viewer3DKeyPressedCustom = pFunction;
}

void set_Viewer3DMouseFuncCustom (bool (*pFunction)(int button, int state, int x, int y))
{
	Viewer3DMouseFuncCustom = pFunction;
}


// display
static int windowId;
int Viewer3DGetWindowId (void) { return windowId; }

static int win_width = 0;
static int win_height = 0;
int  Viewer3DGetWindowWidth (void) { return win_width; }
int  Viewer3DGetWindowHeight (void) { return win_height; }

// rendering
static int light = 1;
static int wireframe = 0;
static int _fill = 1;
static int pointcloud = 0;
static int pointsize  = 1;
static int repere = 0;
static int grid = 0;
static int smooth = 1;
static int texture = 1;

//
//
//
bool Viewer3DInitialize (int argc, char *argv[])
{
	win_width = 800;
	win_height = 600;

	glutInit(&argc, argv);  
	
	glutInitDisplayMode (GLUT_RGBA | GLUT_DOUBLE | (GLUT_ALPHA & 0) | GLUT_DEPTH);  
	glutInitWindowSize (win_width, win_height);
	glutInitWindowPosition (0, 0);
	windowId = glutCreateWindow ("");
	
	Viewer3DInitializeGL ();

	glutDisplayFunc (&Viewer3DDrawGLScene_callback);
	//glutFullScreen();
	glutReshapeFunc(&Viewer3DResizeGLScene);
	
	//
	// Callbacks
	//
	glutKeyboardFunc(&Viewer3DKeyPressed);
	glutSpecialFunc (&Viewer3DSpecialKeyPressed);
	glutMotionFunc (&Viewer3DMotionFunc);
	glutMouseFunc (&Viewer3DMouseFunc);
	glutJoystickFunc (&Viewer3DJoystickFunc, 300);
	glutForceJoystickFunc ();
	//glutIdleFunc (&Viewer3DIdleFunc);

	return true;
}

void Viewer3DLoop (void)
{
	glutMainLoop();  
}

void Viewer3DResizeGLScene (GLsizei width, GLsizei height)
{
	if (height == 0)
		height = 1;

	glViewport(0,0,width,height);						// Reset The Current Viewport

	glMatrixMode(GL_PROJECTION);						// Select The Projection Matrix
	glLoadIdentity();									// Reset The Projection Matrix
	
	//gluPerspective(45.0f, (GLdouble)width/(GLdouble)height, 3.f, 4.0f);  // select the viewing volume
	gluPerspective(45.0f, (GLdouble)width/(GLdouble)height, 0.001f, 100.f);  // select the viewing volume
	
	glMatrixMode(GL_MODELVIEW);							// Select The Modelview Matrix
	glLoadIdentity();									// Reset The Modelview Matrix
	
	win_width  = width;
	win_height = height;
	if (_g_examinator) _g_examinator->set_dimensions (win_width, win_height);
}    

bool Viewer3DInitializeGL (void)
{
	_g_examinator = new Ctrackball ();
	_g_examinator->set_zoom (-10.0);
	_g_examinator->set_dimensions (win_width, win_height);
	
	Console::getInstance()->set_size_screen (win_width, win_height);

	glShadeModel(GL_SMOOTH);							// Enable Smooth Shading
	glClearColor(29./255., 170./255., 71./255., 0.f);	// set background

	glClearDepth (1.0f);								// specify the back of the buffer as clear depth
	glDepthFunc (GL_LESS);
	
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);	// Really Nice Perspective Calculations

	//glEnable (GL_CULL_FACE); // Polygons to be discarded
	//glCullFace (GL_BACK);

	glClearStencil(0);

/*
	glEnable( GL_LINE_SMOOTH );
	glHint(GL_LINE_SMOOTH_HINT,GL_DONT_CARE);
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	glEnable( GL_BLEND );
*/

	// Ensure correct display of polygons
	//glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);

	// Textures
#ifndef WIN32
	//	MaterialRenderer::getInstance()->Initialize ();
#endif

	// Lights
	LightsManager::getInstance()->Initialize ();
	Light *pLight = new Light (); // create a light by default
	pLight->SetEnable (true);
	pLight->SetDisplay (true);
	unsigned int idLight = LightsManager::getInstance()->AddLight (pLight);
	LightsManager::getInstance()->EnableLight (idLight, true);

  // Transparence
  glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glMatrixMode(GL_MODELVIEW);

  return true;
 }

//
// Draw the scene
//
bool Viewer3DDrawGLScene (void)
{
	if (Viewer3DDrawGeneral)
		return Viewer3DDrawGeneral();

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	if (0) // lights don't move (mode "lampe frontale")
		LightRenderer::getInstance()->EnableLighting (true);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();


	// background
	if (BackgroundManager::getInstance()->get_type () == BackgroundManager::BACKGROUND_SKYBOX)
	{
		_g_examinator->set_camera_rotate ();
		BackgroundManager::getInstance()->display ();
		glLoadIdentity();
	}
	else
	{
		BackgroundManager::getInstance()->display ();
	}

	_g_examinator->set_camera ();


	if (1) // lights move with all the scene
	{
		LightRenderer::getInstance()->EnableLighting (true);
	}

	// repere
	if (repere) repere_draw ();

	// wireframe of fill
	if (wireframe)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	else
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	// texture
/*
	if (texture)
		glEnable(GL_TEXTURE_2D);
	else
		glDisable(GL_TEXTURE_2D);
*/

	// customized draw function
	if (Viewer3DDrawCustom)
	{
		Viewer3DDrawCustom ();
	}
	else
	{
		MaterialRenderer::SetMaterial (MaterialColorExt::RUBY);
		draw_teapot ();
	}
	
	glPopMatrix ();

#ifndef WIN32
	glutSwapBuffers();
#endif

	return true;
}

void Viewer3DDrawGLScene_callback (void)
{
	Viewer3DDrawGLScene ();
}

void Viewer3DIdleFunc (void)
{
	glutPostRedisplay();
}

///////////////////////
//
// Picking
//
int pickObject (int x, int y)
{
  GLuint buffer[512];
  GLint hits;
  GLint viewport[4];
  
  memset (buffer, 0, 512*sizeof(GLuint));
  // Get the size of the current viewport
  // [0] : x
  // [1] : y
  // [2] : length
  // [3] : width
  glGetIntegerv (GL_VIEWPORT, viewport);
  
  // Allocate the selection buffer and enter selection mode
  glSelectBuffer (512, buffer);
  glRenderMode (GL_SELECT);

  // Clear the names on the name stack and push on an empty name
  glInitNames ();
  glPushName (0);
  
  // Switch to projection mode and save our current projection
  glMatrixMode (GL_PROJECTION);
  glPushMatrix ();
  glLoadIdentity ();

  // Create matrix that will zoom up to a small portion of the screen
  gluPickMatrix ((GLdouble)x, (GLdouble)(viewport[3]-y), 1.0f, 1.0f, viewport);
  
  // Apply the perspective matrix
  GLdouble aspect_ratio = (viewport[2]-viewport[0])/(viewport[3]-viewport[1]);//(GLdouble)win_width / (GLdouble)win_height;
  gluPerspective(45.0f, aspect_ratio, 0.001f, 5000.0f);

  // Go back to modelview and draw the obejcts again
  // this time loading names on the stack
  glMatrixMode (GL_MODELVIEW);
  Viewer3DDrawGLScene ();
  
  // Force OpenGL to finish up before continuing
  glFlush ();
  
  // Restore the projection matrix
  glMatrixMode (GL_PROJECTION);
  glPopMatrix ();
  
  // Go back to modelview for normal rendering
  glMatrixMode (GL_MODELVIEW);
  
  // Collect the number of hits from the render and parse the selection
  hits = glRenderMode (GL_RENDER);
  
  if (hits > 0)
    {
      int choose = buffer[3];
      int depth = buffer[1];
      int loop;
      for (loop=1; loop<hits; loop++)
	{
		if (buffer[loop*4+1] < GLuint(depth))
		{
			choose = buffer[loop*4+3];
			depth = buffer[loop*4+1];
		}
	}
      return choose;
    }
  return -1;
}
//
///////////////////////

/********** Controls ***********/
void Viewer3DMotionFunc (int x, int y)
{
	if (_g_examinator)
		_g_examinator->mouse_move (x, y);
#ifndef WIN32
	glutPostRedisplay ();
#endif
}

void Viewer3DMouseFunc (int button, int state, int x, int y)
{
	if (_g_examinator)
		_g_examinator->mouse_press (button, state, x, y);

	// customized key pressed function
	if (Viewer3DMouseFuncCustom)
		Viewer3DMouseFuncCustom (button, state, x, y);

#ifndef WIN32
	glutPostRedisplay ();
#endif
}

void Viewer3DJoystickFunc (unsigned int buttonMask,int x, int y, int z)
{
	printf ("joystick\n");
	if(buttonMask & GLUT_JOYSTICK_BUTTON_A) {
		printf("button A is pressed ");
	}
	if(buttonMask & GLUT_JOYSTICK_BUTTON_B) {
		printf("button B is pressed ");
	}
	if(buttonMask & GLUT_JOYSTICK_BUTTON_C) {
		printf("button C is pressed ");
	}
	if(buttonMask & GLUT_JOYSTICK_BUTTON_D) {
		printf("button D is pressed ");
	}
}

void Viewer3DKeyPressed (unsigned char key, int x, int y)
{
  switch (key)
    {
	case 'a':
	case 'A':
		screenshot (win_width, win_height);
		break;

    case 'l':
    case 'L':
	    light = !light;
	    break;
    case 'f':
    case 'F':
	    _fill = !_fill;
	    break;
    case 'w':
    case 'W':
	    wireframe = !wireframe;
	    break;
    case 'p':
    case 'P':
	    pointcloud = !pointcloud;
	    break;
    case 'r':
    case 'R':
	    repere = !repere;
	    break;
    case 's':
    case 'S':
	    smooth = !smooth;
	    break;
    case 't':
	    texture = !texture;
	    break;
    case PLUS:
	    pointsize += 1.;
	    break;
    case MINUS:
	    pointsize -= 1.;
	    if (pointsize < 0.)
		    pointsize = 0.;
	    break;
    case SPACE:
	    break;
    case 'q':
	    break;
    case ESCAPE:
		if (Viewer3DDestroyCustom)
		    Viewer3DDestroyCustom ();
	    glutDestroyWindow(windowId);
	    exit(EXIT_SUCCESS);            
      break;
    default:
      ;
      // case not reached
    }

  // customized key pressed function
  if (Viewer3DKeyPressedCustom)
	  Viewer3DKeyPressedCustom (key, x, y);

#ifndef WIN32
  glutPostRedisplay ();
#endif
}

void Viewer3DSpecialKeyPressed (int key, int x, int y)
{
  switch (key)
    {
    case GLUT_KEY_LEFT:
    {
    }
    break;
    case GLUT_KEY_RIGHT:
    {
    }
    break;
    case GLUT_KEY_UP:
    {
    }
    break;
    case GLUT_KEY_DOWN:
    {
	}
    break;
    default:
	    break;
    }
#ifndef WIN32
  glutPostRedisplay ();
#endif
}
/************* Controls (End) *************/
