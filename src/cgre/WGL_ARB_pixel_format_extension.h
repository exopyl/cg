#ifndef WGL_ARB_PIXEL_FORMAT_EXTENSION_H 
#define WGL_ARB_PIXEL_FORMAT_EXTENSION_H 
 
bool SetUpWGL_ARB_pixel_format(const char * wglExtensions); 
extern bool WGL_ARB_pixel_format_supported; 
 
extern PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB;

#endif // WGL_ARB_PIXEL_FORMAT_EXTENSION_H

