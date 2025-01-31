#include	"Win32_GL_utils/GL_PBuffer.hxx"
#include	"utils/getconfig.hxx"
#include	"utils/2d3dAssert.hxx"
#include	<inttypes.h>

// WGL_ARB_extensions_string
PFNWGLGETEXTENSIONSSTRINGARBPROC wglGetExtensionsStringARB = NULL;

// WGL_ARB_pbuffer
PFNWGLCREATEPBUFFERARBPROC    wglCreatePbufferARB = NULL;
PFNWGLGETPBUFFERDCARBPROC     wglGetPbufferDCARB = NULL;
PFNWGLRELEASEPBUFFERDCARBPROC wglReleasePbufferDCARB = NULL;
PFNWGLDESTROYPBUFFERARBPROC   wglDestroyPbufferARB = NULL;
PFNWGLQUERYPBUFFERARBPROC     wglQueryPbufferARB = NULL;

// WGL_ARB_pixel_format
PFNWGLGETPIXELFORMATATTRIBIVARBPROC wglGetPixelFormatAttribivARB = NULL;
PFNWGLGETPIXELFORMATATTRIBFVARBPROC wglGetPixelFormatAttribfvARB = NULL;
PFNWGLCHOOSEPIXELFORMATARBPROC      wglChoosePixelFormatARB = NULL;

// WGL_ARB_render_texture
PFNWGLBINDTEXIMAGEARBPROC     wglBindTexImageARB = NULL;
PFNWGLRELEASETEXIMAGEARBPROC  wglReleaseTexImageARB = NULL;
PFNWGLSETPBUFFERATTRIBARBPROC wglSetPbufferAttribARB = NULL;

GL_IBuffer::~GL_IBuffer()
{}

GL_PBuffer::GL_PBuffer()
{
	m_pbuffer.hPBuffer = NULL;
	m_pbuffer.nWidth = 0;
	m_pbuffer.nHeight = 0;
	m_pbuffer.hRC = NULL;
	m_pbuffer.hDC = NULL;

	m_glRC = NULL;

}

GL_PBuffer::~GL_PBuffer()
{
	Release();
}


static void ShowPixelFormatCapabilities(HDC g_hDC)
{
	{
		int iattributes[] = {
			WGL_NUMBER_PIXEL_FORMATS_ARB,
			WGL_DRAW_TO_WINDOW_ARB,
			WGL_DRAW_TO_BITMAP_ARB,
			WGL_ACCELERATION_ARB,
			WGL_NEED_PALETTE_ARB,
			WGL_NEED_SYSTEM_PALETTE_ARB,
			WGL_SWAP_LAYER_BUFFERS_ARB,
			WGL_SWAP_METHOD_ARB,
			WGL_NUMBER_OVERLAYS_ARB,
			WGL_NUMBER_UNDERLAYS_ARB,
			WGL_TRANSPARENT_ARB,
			WGL_TRANSPARENT_RED_VALUE_ARB,
			WGL_TRANSPARENT_GREEN_VALUE_ARB,
			WGL_TRANSPARENT_BLUE_VALUE_ARB,
			WGL_TRANSPARENT_ALPHA_VALUE_ARB,
			WGL_TRANSPARENT_INDEX_VALUE_ARB,
			WGL_SHARE_DEPTH_ARB,
			WGL_SHARE_STENCIL_ARB,
			WGL_SHARE_ACCUM_ARB,
			WGL_SUPPORT_GDI_ARB,
			WGL_SUPPORT_OPENGL_ARB,
			WGL_DOUBLE_BUFFER_ARB,
			WGL_STEREO_ARB,
			WGL_PIXEL_TYPE_ARB,
			WGL_COLOR_BITS_ARB,
			WGL_RED_BITS_ARB,
			WGL_RED_SHIFT_ARB,
			WGL_GREEN_BITS_ARB,
			WGL_GREEN_SHIFT_ARB,
			WGL_BLUE_BITS_ARB,
			WGL_BLUE_SHIFT_ARB,
			WGL_ALPHA_BITS_ARB,
			WGL_ALPHA_SHIFT_ARB,
			WGL_ACCUM_BITS_ARB,
			WGL_ACCUM_RED_BITS_ARB,
			WGL_ACCUM_GREEN_BITS_ARB,
			WGL_ACCUM_BLUE_BITS_ARB,
			WGL_ACCUM_ALPHA_BITS_ARB,
			WGL_DEPTH_BITS_ARB,
			WGL_STENCIL_BITS_ARB,
			WGL_AUX_BUFFERS_ARB,
			WGL_NO_ACCELERATION_ARB,
			WGL_GENERIC_ACCELERATION_ARB,
			WGL_FULL_ACCELERATION_ARB,
			WGL_SWAP_EXCHANGE_ARB,
			WGL_SWAP_COPY_ARB,
			WGL_SWAP_UNDEFINED_ARB,
			WGL_TYPE_RGBA_ARB,
			WGL_TYPE_COLORINDEX_ARB,
			WGL_SAMPLE_BUFFERS_ARB,
			WGL_SAMPLES_ARB
		};
		const char* sattributes[] = {
			"WGL_NUMBER_PIXEL_FORMATS_ARB",
			"WGL_DRAW_TO_WINDOW_ARB",
			"WGL_DRAW_TO_BITMAP_ARB",
			"WGL_ACCELERATION_ARB",
			"WGL_NEED_PALETTE_ARB",
			"WGL_NEED_SYSTEM_PALETTE_ARB",
			"WGL_SWAP_LAYER_BUFFERS_ARB",
			"WGL_SWAP_METHOD_ARB",
			"WGL_NUMBER_OVERLAYS_ARB",
			"WGL_NUMBER_UNDERLAYS_ARB",
			"WGL_TRANSPARENT_ARB",
			"WGL_TRANSPARENT_RED_VALUE_ARB",
			"WGL_TRANSPARENT_GREEN_VALUE_ARB",
			"WGL_TRANSPARENT_BLUE_VALUE_ARB",
			"WGL_TRANSPARENT_ALPHA_VALUE_ARB",
			"WGL_TRANSPARENT_INDEX_VALUE_ARB",
			"WGL_SHARE_DEPTH_ARB",
			"WGL_SHARE_STENCIL_ARB",
			"WGL_SHARE_ACCUM_ARB",
			"WGL_SUPPORT_GDI_ARB",
			"WGL_SUPPORT_OPENGL_ARB",
			"WGL_DOUBLE_BUFFER_ARB",
			"WGL_STEREO_ARB",
			"WGL_PIXEL_TYPE_ARB",
			"WGL_COLOR_BITS_ARB",
			"WGL_RED_BITS_ARB",
			"WGL_RED_SHIFT_ARB",
			"WGL_GREEN_BITS_ARB",
			"WGL_GREEN_SHIFT_ARB",
			"WGL_BLUE_BITS_ARB",
			"WGL_BLUE_SHIFT_ARB",
			"WGL_ALPHA_BITS_ARB",
			"WGL_ALPHA_SHIFT_ARB",
			"WGL_ACCUM_BITS_ARB",
			"WGL_ACCUM_RED_BITS_ARB",
			"WGL_ACCUM_GREEN_BITS_ARB",
			"WGL_ACCUM_BLUE_BITS_ARB",
			"WGL_ACCUM_ALPHA_BITS_ARB",
			"WGL_DEPTH_BITS_ARB",
			"WGL_STENCIL_BITS_ARB",
			"WGL_AUX_BUFFERS_ARB",
			"WGL_NO_ACCELERATION_ARB",
			"WGL_GENERIC_ACCELERATION_ARB",
			"WGL_FULL_ACCELERATION_ARB",
			"WGL_SWAP_EXCHANGE_ARB",
			"WGL_SWAP_COPY_ARB",
			"WGL_SWAP_UNDEFINED_ARB",
			"WGL_TYPE_RGBA_ARB",
			"WGL_TYPE_COLORINDEX_ARB",
			"WGL_SAMPLE_BUFFERS_ARB",
			"WGL_SAMPLES_ARB",
		};
		int ivalues[sizeof(iattributes) / sizeof(int)];
		int mattrib = sizeof(iattributes) / sizeof(int);
		bool ok = true;
		int format = 1;
		fprintf(stdout, "ShowPixelFormatCapabilities(x%" PRIxPTR ")\n", (ULONG_PTR)g_hDC);
		do {
			if (wglGetPixelFormatAttribivARB(g_hDC, format, 0, sizeof(iattributes) / sizeof(int), iattributes, ivalues)) {
				fprintf(stdout, "PBuffer pixel format ARB %d\n", format);
				for (int i = 0; i < mattrib; i++) {
					fprintf(stdout, "   %s = %d\n", sattributes[i], ivalues[i]);
				}
				format++;
			}
			else
				ok = false;

		} while (ok);
		fprintf(stdout, "number of pixel format ARB read: %d\n", format);
	}

}

void GL_PBuffer::Init(HDC g_hDC, int pbuffer_width, int pbuffer_height)
{

	//
	// If the required extensions are present, get the addresses for the
	// functions that we wish to use...
	//

	wglGetExtensionsStringARB = (PFNWGLGETEXTENSIONSSTRINGARBPROC)wglGetProcAddress("wglGetExtensionsStringARB");
	char *ext = NULL;

	if (wglGetExtensionsStringARB)
		ext = (char*)wglGetExtensionsStringARB(g_hDC);//wglGetCurrentDC() );
	else
	{
		fprintf(stderr, "Unable to get address for wglGetExtensionsStringARB!\n");
		return;
	}

	//
	// WGL_ARB_pbuffer
	//

	if (strstr(ext, "WGL_ARB_pbuffer") == NULL)
	{
		fprintf(stderr, "WGL_ARB_pbuffer extension was not found\n");
	}
	else
	{
		wglCreatePbufferARB = (PFNWGLCREATEPBUFFERARBPROC)wglGetProcAddress("wglCreatePbufferARB");
		wglGetPbufferDCARB = (PFNWGLGETPBUFFERDCARBPROC)wglGetProcAddress("wglGetPbufferDCARB");
		wglReleasePbufferDCARB = (PFNWGLRELEASEPBUFFERDCARBPROC)wglGetProcAddress("wglReleasePbufferDCARB");
		wglDestroyPbufferARB = (PFNWGLDESTROYPBUFFERARBPROC)wglGetProcAddress("wglDestroyPbufferARB");
		wglQueryPbufferARB = (PFNWGLQUERYPBUFFERARBPROC)wglGetProcAddress("wglQueryPbufferARB");

		if (!wglCreatePbufferARB || !wglGetPbufferDCARB || !wglReleasePbufferDCARB ||
			!wglDestroyPbufferARB || !wglQueryPbufferARB)
		{
			fprintf(stderr, "One or more WGL_ARB_pbuffer functions were not found\n");
			return;
		}
	}

	//
	// WGL_ARB_pixel_format
	//

	if (strstr(ext, "WGL_ARB_pixel_format") == NULL)
	{
		fprintf(stderr, "WGL_ARB_pixel_format extension was not found\n");
		return;
	}
	else
	{
		wglGetPixelFormatAttribivARB = (PFNWGLGETPIXELFORMATATTRIBIVARBPROC)wglGetProcAddress("wglGetPixelFormatAttribivARB");
		wglGetPixelFormatAttribfvARB = (PFNWGLGETPIXELFORMATATTRIBFVARBPROC)wglGetProcAddress("wglGetPixelFormatAttribfvARB");
		wglChoosePixelFormatARB = (PFNWGLCHOOSEPIXELFORMATARBPROC)wglGetProcAddress("wglChoosePixelFormatARB");

		if (!wglGetExtensionsStringARB || !wglCreatePbufferARB || !wglGetPbufferDCARB)
		{
			fprintf(stderr, "One or more WGL_ARB_pixel_format functions were not found\n");
			return;
		}
	}

	//
	// WGL_ARB_render_texture
	//

	if (strstr(ext, "WGL_ARB_render_texture") == NULL)
	{
		fprintf(stderr, "WGL_ARB_render_texture extension was not found\n");
		return;
	}
	else
	{
		wglBindTexImageARB = (PFNWGLBINDTEXIMAGEARBPROC)wglGetProcAddress("wglBindTexImageARB");
		wglReleaseTexImageARB = (PFNWGLRELEASETEXIMAGEARBPROC)wglGetProcAddress("wglReleaseTexImageARB");
		wglSetPbufferAttribARB = (PFNWGLSETPBUFFERATTRIBARBPROC)wglGetProcAddress("wglSetPbufferAttribARB");

		if (!wglBindTexImageARB || !wglReleaseTexImageARB || !wglSetPbufferAttribARB)
		{
			fprintf(stderr, "One or more WGL_ARB_render_texture functions were not found\n");
			return;
		}
	}

	//ShowPixelFormatCapabilities(g_hDC);

	//-------------------------------------------------------------------------
	// Create a GL_PBuffer for off-screen rendering.
	//-------------------------------------------------------------------------

	m_pbuffer.hPBuffer = NULL;
	m_pbuffer.nWidth = pbuffer_width;
	m_pbuffer.nHeight = pbuffer_height;

	//
	// Define the minimum pixel format requirements we will need for our 
	// GL_PBuffer. A GL_PBuffer is just like a frame buffer, it can have a depth 
	// buffer associated with it and it can be double buffered.
	//

	int current = 0;
	Config_Modaris::get(_T("ogl_antialiasing2"), current);

	float fAttributes[] = { 0, 0 };

	int pf_attr[] =
	{
		WGL_SUPPORT_OPENGL_ARB, TRUE,       // GL_PBuffer will be used with OpenGL
		WGL_DRAW_TO_PBUFFER_ARB, TRUE,      // Enable render to GL_PBuffer
//		WGL_BIND_TO_TEXTURE_RGBA_ARB, TRUE, // GL_PBuffer will be used as a texture
		WGL_RED_BITS_ARB, 8,                // At least 8 bits for RED channel
		WGL_GREEN_BITS_ARB, 8,              // At least 8 bits for GREEN channel
		WGL_BLUE_BITS_ARB, 8,               // At least 8 bits for BLUE channel
		WGL_ALPHA_BITS_ARB, 8,              // At least 8 bits for ALPHA channel
		WGL_DEPTH_BITS_ARB, 24,             // At least 24 bits for depth buffer
		WGL_DOUBLE_BUFFER_ARB, FALSE,       // We don't require double buffering
		WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
		WGL_SUPPORT_OPENGL_ARB, TRUE,
		WGL_SAMPLE_BUFFERS_ARB, current == 0 ? 0 : 1,	  // does not works on all board!, keep it at last position
		WGL_SAMPLES_ARB, current,
		0, 0                                   // Zero terminates the list
	};

	unsigned int count = 0;
	int pixelFormat;
	wglChoosePixelFormatARB(g_hDC, (const int*)pf_attr, fAttributes, 1, &pixelFormat, &count);

	if (count == 0)
	{
		fprintf(stderr, "Could not find an acceptable pixel format with antialising! Give second chance\n");
		// search the antialiasPos
		int nbParam = sizeof(pf_attr) / sizeof(pf_attr[0]);
		for (int i = 0; i < sizeof(pf_attr) / sizeof(pf_attr[0]); i += 2) {
			if (pf_attr[i] == WGL_SAMPLE_BUFFERS_ARB) {
				pf_attr[i] = 0;
				ASSERT( (i + 1) < nbParam);
				pf_attr[i + 1] = 0;
				break;
			}
		}
		wglChoosePixelFormatARB(g_hDC, (const int*)pf_attr, fAttributes, 1, &pixelFormat, &count);
		if (count == 0) {
			fprintf(stderr, "Could not find an acceptable pixel format without antialising!\n");
			return;
		}
	}
	//fprintf(stdout, "PBuffer pixel format ARB %d\n", pixelFormat);

	//
	// Create a generic GL_PBuffer...
	//

	int pb_attr[] =
	{
		WGL_TEXTURE_FORMAT_ARB, WGL_NO_TEXTURE_ARB,
		WGL_TEXTURE_TARGET_ARB, WGL_NO_TEXTURE_ARB,

		0                                           // Zero terminates the list
	};

	//
	// Create the GL_PBuffer...
	//

	int q_attr[] =
	{
		WGL_MAX_PBUFFER_PIXELS_ARB,
		WGL_MAX_PBUFFER_WIDTH_ARB,
		WGL_MAX_PBUFFER_HEIGHT_ARB
	};

	int max[3] = { 0, 0, 0 };

	wglGetPixelFormatAttribivARB(g_hDC, pixelFormat, 0, 3, q_attr, max);

	static int gMaxPBufferSize = 2048;
	static bool alreadydone = false;
	if (!alreadydone) {
		Config_Modaris::get_or_set(_T("PBufferMaxSize"), gMaxPBufferSize, gMaxPBufferSize);
		alreadydone = true;
	}

	if (m_pbuffer.nWidth > max[1] || m_pbuffer.nHeight > max[2] || m_pbuffer.nWidth > gMaxPBufferSize || m_pbuffer.nHeight > gMaxPBufferSize) {

		fprintf(stderr, "Could not create the GL_PBuffer for size %d %d, max gl size: %d %d, user limit %d\n", m_pbuffer.nWidth, m_pbuffer.nHeight, max[1], max[2], gMaxPBufferSize);
		return;
	}

	m_pbuffer.hPBuffer = wglCreatePbufferARB(g_hDC, pixelFormat, m_pbuffer.nWidth, m_pbuffer.nHeight, pb_attr);

	m_pbuffer.hDC = wglGetPbufferDCARB(m_pbuffer.hPBuffer);
	m_pbuffer.hRC = wglCreateContext(m_pbuffer.hDC);
	m_glRC = new GL_RC();
	m_glRC->Attach(m_pbuffer.hRC);

	if (!m_pbuffer.hPBuffer)
	{
		fprintf(stderr, "Could not create the GL_PBuffer for size %d %d\n", m_pbuffer.nWidth, m_pbuffer.nHeight);
		return;
	}

	//
	// We were successful in creating a GL_PBuffer. We can now make its context 
	// current and set it up just like we would a regular context 
	// attached to a window.
	//

	if (!wglMakeCurrent(m_pbuffer.hDC, m_pbuffer.hRC))
	{
		fprintf(stderr, "Could not make the GL_PBuffer's context current!\n");
		return;
	}

}

bool GL_PBuffer::IsValid() const
{
	return m_pbuffer.hPBuffer != NULL;
}



void GL_PBuffer::MakeCurrent()
{
	if (m_pbuffer.hPBuffer) {
		if (!wglMakeCurrent(m_pbuffer.hDC, m_pbuffer.hRC))  {
			fprintf(stderr, "Could not make the GL_PBuffer's context current!\n");
		}
		//glEnable(GL_MULTISAMPLE_ARB);
	}
}

void GL_PBuffer::Release()
{
	if (m_pbuffer.hRC != NULL)
	{
		wglMakeCurrent(m_pbuffer.hDC, m_pbuffer.hRC);
		wglReleasePbufferDCARB(m_pbuffer.hPBuffer, m_pbuffer.hDC);
		wglDestroyPbufferARB(m_pbuffer.hPBuffer);
		m_pbuffer.hPBuffer = NULL;
		m_pbuffer.hRC = NULL;
		//glDisable(GL_MULTISAMPLE_ARB);
	}

	if (m_pbuffer.hDC != NULL)
	{
		//DeleteDC( g_hWnd, m_pbuffer.hDC );
		m_pbuffer.hDC = NULL;
	}
	delete m_glRC;
	m_glRC = NULL;

}

int GL_PBuffer::GetPixelFormat() const
{
	ASSERT(m_glRC);
	int pf = -1;
	if (m_glRC)
		pf = m_glRC->GetPixelFormat();
	return pf;
}

ChaineInc GL_PBuffer::GetType() const
{
	return ChaineInc("GL_PBuffer");
}