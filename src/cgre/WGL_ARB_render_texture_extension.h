#ifndef WGL_ARB_RENDER_TEXTURE_EXTENSION_H 
#define WGL_ARB_RENDER_TEXTURE_EXTENSION_H 
 
bool SetUpWGL_ARB_render_texture(const char * wglExtensions); 
extern bool WGL_ARB_render_texture_supported; 
 
extern PFNWGLBINDTEXIMAGEARBPROC     wglBindTexImageARB;
extern PFNWGLRELEASETEXIMAGEARBPROC  wglReleaseTexImageARB;
extern PFNWGLSETPBUFFERATTRIBARBPROC wglSetPbufferAttribARB;

#endif // WGL_ARB_RENDER_TEXTURE_EXTENSION_H

