#ifdef WIN32
#include <windows.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/wglext.h>
#include "WGL_ARB_pixel_format_extension.h"   

bool WGL_ARB_pixel_format_supported=false;   

bool SetUpWGL_ARB_pixel_format(const char * wglExtensions)   
{   
   
    //Check for support   
    char * extensionString=(char *)wglExtensions;   
    char * extensionName="WGL_ARB_pixel_format";   
   
    char * endOfString;                                 //store pointer to end of string   
    unsigned int distanceToSpace;                       //distance to next space   
   
    endOfString=extensionString+strlen(extensionString);   
   
    //loop through string   
    while(extensionString<endOfString)   
    {   
        //find distance to next space   
        distanceToSpace=strcspn(extensionString, " ");   
   
        //see if we have found extensionName   
        if((strlen(extensionName)==distanceToSpace) &&   
            (strncmp(extensionName, extensionString, distanceToSpace)==0))   
        {   
            WGL_ARB_pixel_format_supported=true;   
        }   
   
        //if not, move on   
        extensionString+=distanceToSpace+1;   
    }   
       
   
    if(!WGL_ARB_pixel_format_supported)   
    {   
        //errorLog.OutputError("WGL_ARB_pixel_format unsupported!");   
        return false;   
    }   
   
    //errorLog.OutputSuccess("WGL_ARB_pixel_format supported!");   
   
    //get function pointers   
    wglChoosePixelFormatARB = (PFNWGLCHOOSEPIXELFORMATARBPROC)wglGetProcAddress("wglChoosePixelFormatARB"); 
       
    return true;   
}
   
//function pointers   
PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB = NULL;
#endif // WIN32