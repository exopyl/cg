#pragma once

/*
#ifdef WIN32
#include <windows.h>
#endif

#include <GL/glut.h>
*/
#define ESCAPE 27
#define SPACE  32
#define PLUS   43
#define MINUS  45

#include "../cgmesh/cgmesh.h"

//
// global functions to initialize the renderer
//
extern bool Viewer3DInitialize (int argc, char *argv[]); // for glut => can be exported in a separate file
extern bool Viewer3DInitializeGL (void);
extern void Viewer3DResizeGLScene (GLsizei width, GLsizei height);
extern bool Viewer3DDrawGLScene (void);

extern void Viewer3DLoop (void);
extern int  Viewer3DGetWindowId (void);
extern int  Viewer3DGetWindowWidth (void);
extern int  Viewer3DGetWindowHeight (void);

//
// global functions to manage the events (mouse & keyboard)
//
extern void Viewer3DMotionFunc (int x, int y);
extern void Viewer3DMouseFunc (int button, int state, int x, int y);
extern void Viewer3DJoystickFunc (unsigned int buttonMask,int x, int y, int z);
extern void Viewer3DKeyPressed (unsigned char key, int x, int y);
extern void Viewer3DSpecialKeyPressed (int key, int x, int y);

//
// global functions to customize the renderer
//
extern void set_Viewer3DDrawGeneral (bool (*pFunction)(void));
extern void set_Viewer3DDrawCustom (bool (*pFunction)(void));
extern void set_Viewer3DDestroyCustom (bool (*pFunction)(void));
extern void set_Viewer3DKeyPressedCustom (bool (*pFunction)(int key, int x, int y));
extern void set_Viewer3DMouseFuncCustom (bool (*pFunction)(int button, int state, int x, int y));

// picking function
extern int pickObject (int x, int y);
