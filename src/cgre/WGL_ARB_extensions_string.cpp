#ifdef WIN32
#include <windows.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/wglext.h>
#include "WGL_ARB_extensions_string.h"   

char* wglExtensionsStringARB = NULL;   

bool SetUpWGL_ARB_extensions_string(void)   
{
    //get function pointers   
  	PFNWGLGETEXTENSIONSSTRINGARBPROC wglGetExtensionsStringARB = (PFNWGLGETEXTENSIONSSTRINGARBPROC)wglGetProcAddress("wglGetExtensionsStringARB");
	
	if( wglGetExtensionsStringARB )
		wglExtensionsStringARB = (char*)wglGetExtensionsStringARB( wglGetCurrentDC() );
	else
		return false;

	return true;   
}   
   
//function pointers   
PFNWGLGETEXTENSIONSSTRINGARBPROC wglGetExtensionsStringARB = NULL;
#endif // WIN32