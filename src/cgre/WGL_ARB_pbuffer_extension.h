#ifndef WGL_ARB_PBUFFER_EXTENSION_H 
#define WGL_ARB_PBUFFER_EXTENSION_H 
 
bool SetUpWGL_ARB_pbuffer(const char * wglExtensions); 
extern bool WGL_ARB_pbuffer_supported; 
 
extern PFNWGLCREATEPBUFFERARBPROC			wglCreatePbufferARB; 
extern PFNWGLGETPBUFFERDCARBPROC			wglGetPbufferDCARB; 
extern PFNWGLRELEASEPBUFFERDCARBPROC		wglReleasePbufferDCARB; 
extern PFNWGLDESTROYPBUFFERARBPROC			wglDestroyPbufferARB; 
extern PFNWGLQUERYPBUFFERARBPROC			wglQueryPbufferARB; 

#endif // WGL_ARB_PBUFFER_EXTENSION_H

