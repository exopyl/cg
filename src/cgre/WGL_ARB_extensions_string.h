#ifndef WGL_ARB_EXTENSIONS_STRING_H 
#define WGL_ARB_EXTENSIONS_STRING_H 

#include <GL/glext.h>
#include <GL/wglext.h>

bool SetUpWGL_ARB_extensions_string(void); 
extern char* wglExtensionsStringARB;
 
extern PFNWGLGETEXTENSIONSSTRINGARBPROC wglGetExtensionsStringARB;

#endif // WGL_ARB_EXTENSIONS_STRING_H

