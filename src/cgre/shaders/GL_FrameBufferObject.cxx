#ifdef _WIN32
#include <windows.h>
#endif

#include	<COREVP/ImageStorage.h>
#include	<Win32_GL_utils/GL_Wrapper.hxx>
#include	"Win32_GL_utils/GL_FrameBufferObject.hxx"
#include	"Win32_GL_utils/GL_CapabilitiesManager.hxx"
#include	"utils/getconfig.hxx"
#include	"utils/2d3dAssert.hxx"

// #define DEBUG_DRW_OBJ
#ifdef DEBUG_DRW_OBJ
#include	"teapot.h"
#endif

extern PFNGLISRENDERBUFFERPROC glIsRenderbuffer = NULL;
extern PFNGLBINDRENDERBUFFERPROC glBindRenderbuffer = NULL;
extern PFNGLDELETERENDERBUFFERSPROC glDeleteRenderbuffers = NULL;
extern PFNGLGENRENDERBUFFERSPROC glGenRenderbuffers = NULL;
extern PFNGLRENDERBUFFERSTORAGEPROC glRenderbufferStorage = NULL;
extern PFNGLGETRENDERBUFFERPARAMETERIVPROC glGetRenderbufferParameteriv = NULL;
extern PFNGLISFRAMEBUFFERPROC glIsFramebuffer = NULL;
extern PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer = NULL;
extern PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers = NULL;
extern PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers = NULL;
extern PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus = NULL;
extern PFNGLFRAMEBUFFERTEXTURE1DPROC glFramebufferTexture1D = NULL;
extern PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D = NULL;
extern PFNGLFRAMEBUFFERTEXTURE3DPROC glFramebufferTexture3D = NULL;
extern PFNGLFRAMEBUFFERRENDERBUFFERPROC glFramebufferRenderbuffer = NULL;
extern PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC glGetFramebufferAttachmentParameteriv = NULL;
extern PFNGLGENERATEMIPMAPPROC glGenerateMipmap = NULL;
extern PFNGLDRAWBUFFERSPROC glDrawBuffers = NULL;
extern PFNGLTEXIMAGE2DMULTISAMPLEPROC glTexImage2DMultisample = NULL;
extern PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC glRenderbufferStorageMultisample = NULL;

GL_FrameBufferObject::GL_FrameBufferObject()
	: m_frameBuffer(0), m_depthRenderBuffer(0), m_dynamicTextureID(0), m_autoDeleteTexture(false), m_depthRenderBufferIsTexture(false), m_buffer_width(0), m_buffer_height(0), m_format(0)
{

}
GL_FrameBufferObject::~GL_FrameBufferObject()
{
	Release();
	if (m_autoDeleteTexture && m_dynamicTextureID)
		glDeleteTextures(1, &m_dynamicTextureID);
}

bool GL_FrameBufferObject::DefineProc()
{
	if (!GL_CapabilitiesManager::GetInstance()->HasExtensions("GL_ARB_framebuffer_object"))
	{
		fprintf(stderr, "framebuffer_object extension was not found\n");
		return false;
	}
	else
	{
		glIsRenderbuffer = (PFNGLISRENDERBUFFERPROC)wglGetProcAddress("glIsRenderbuffer");
		glBindRenderbuffer = (PFNGLBINDRENDERBUFFERPROC)wglGetProcAddress("glBindRenderbuffer");
		glDeleteRenderbuffers = (PFNGLDELETERENDERBUFFERSPROC)wglGetProcAddress("glDeleteRenderbuffers");
		glGenRenderbuffers = (PFNGLGENRENDERBUFFERSPROC)wglGetProcAddress("glGenRenderbuffers");
		glRenderbufferStorage = (PFNGLRENDERBUFFERSTORAGEPROC)wglGetProcAddress("glRenderbufferStorage");
		glGetRenderbufferParameteriv = (PFNGLGETRENDERBUFFERPARAMETERIVPROC)wglGetProcAddress("glGetRenderbufferParameteriv");
		glIsFramebuffer = (PFNGLISFRAMEBUFFERPROC)wglGetProcAddress("glIsFramebuffer");
		glBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC)wglGetProcAddress("glBindFramebuffer");
		glDeleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSPROC)wglGetProcAddress("glDeleteFramebuffers");
		glGenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC)wglGetProcAddress("glGenFramebuffers");
		glCheckFramebufferStatus = (PFNGLCHECKFRAMEBUFFERSTATUSPROC)wglGetProcAddress("glCheckFramebufferStatus");
		glFramebufferTexture1D = (PFNGLFRAMEBUFFERTEXTURE1DPROC)wglGetProcAddress("glFramebufferTexture1D");
		glFramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DPROC)wglGetProcAddress("glFramebufferTexture2D");
		glFramebufferTexture3D = (PFNGLFRAMEBUFFERTEXTURE3DPROC)wglGetProcAddress("glFramebufferTexture3D");
		glFramebufferRenderbuffer = (PFNGLFRAMEBUFFERRENDERBUFFERPROC)wglGetProcAddress("glFramebufferRenderbuffer");
		glGetFramebufferAttachmentParameteriv = (PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC)wglGetProcAddress("glGetFramebufferAttachmentParameteriv");
		glGenerateMipmap = (PFNGLGENERATEMIPMAPPROC)wglGetProcAddress("glGenerateMipmap");
		glDrawBuffers = (PFNGLDRAWBUFFERSPROC)wglGetProcAddress("glDrawBuffers");
		glTexImage2DMultisample = (PFNGLTEXIMAGE2DMULTISAMPLEPROC)wglGetProcAddress("glTexImage2DMultisample");
		glRenderbufferStorageMultisample = (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC)wglGetProcAddress("glTexImage2DMultisample");
	}
	if (!glIsRenderbuffer || !glBindRenderbuffer || !glDeleteRenderbuffers ||
		!glGenRenderbuffers || !glRenderbufferStorage || !glGetRenderbufferParameteriv ||
		!glIsFramebuffer || !glBindFramebuffer || !glDeleteFramebuffers ||
		!glGenFramebuffers || !glCheckFramebufferStatus || !glFramebufferTexture1D ||
		!glFramebufferTexture2D || !glFramebufferTexture3D || !glFramebufferRenderbuffer ||
		!glGetFramebufferAttachmentParameteriv || !glGenerateMipmap || !glTexImage2DMultisample || !glRenderbufferStorageMultisample)
	{
		fprintf(stderr, "One or more framebuffer_object functions were not found\n");
		return false;
	}

	return true;
}

// check FBO completeness
bool GL_FrameBufferObject::CheckFramebufferStatus()
{
	// check FBO status
	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	switch (status)
	{
	case GL_FRAMEBUFFER_COMPLETE:
		//printf("Framebuffer complete.\n");
		return true;

	case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
		printf("[ERROR] Framebuffer incomplete: Attachment is NOT complete.\n");
		return false;

	case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
		printf("[ERROR] Framebuffer incomplete: No image is attached to FBO.\n");
		return false;
	case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
		printf("[ERROR] Framebuffer incomplete : Draw buffer.\n");
		return false;

	case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
		printf("[ERROR] Framebuffer incomplete: Read buffer.\n");
		return false;

	case GL_FRAMEBUFFER_UNSUPPORTED:
		printf("[ERROR] Framebuffer incomplete: Unsupported by FBO implementation.\n");
		return false;

	default:
		printf("[ERROR] Framebuffer incomplete: Unknown error %x.\n", status);
		return false;
	}
}

void GL_FrameBufferObject::Init(HDC hDC, int buffer_width, int buffer_height)
{
	Init(buffer_width, buffer_height, false, 0);
}

bool GL_FrameBufferObject::Init(int buffer_width, int buffer_height, bool depthTexture, int msaa)
{
	glCheckError(__FUNCTION__, __LINE__);

	if (!DefineProc())
		return false;

	// backup initial bindings
	GLint initialFrameBufferBinding, initialTexture2DBinding;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &initialFrameBufferBinding);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &initialTexture2DBinding);

	m_buffer_width = buffer_width;
	m_buffer_height = buffer_height;

	//
	// Create a frame-buffer object and a render-buffer object...
	//
	glGenFramebuffers(1, &m_frameBuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, m_frameBuffer);
	glCheckError(__FUNCTION__, __LINE__);

	//
	// Now, dynamic texture. It doesn't actually get loaded with any 
	// pixel data, but its texture ID becomes associated with the pixel data
	// contained in the frame-buffer object. This allows us to bind to this data
	// like we would any regular texture.
	//
	if (m_dynamicTextureID == 0) {
		glGenTextures(1, &m_dynamicTextureID);
		m_autoDeleteTexture = true;
	}
	if (m_dynamicTextureID > 0) {
		glBindTexture(msaa>0 ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D, m_dynamicTextureID);
		glCheckError(__FUNCTION__, __LINE__);
		if (depthTexture) {
			m_format = GL_DEPTH_COMPONENT;
			glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, buffer_width, buffer_height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);	//depth
		}
		else {
			m_format = GL_RGBA;
			if (msaa>0)
				glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, msaa, GL_RGBA, buffer_width, buffer_height, GL_FALSE);				// color
			else
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, buffer_width, buffer_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);					// color
		}
		glCheckError(__FUNCTION__, __LINE__);

		if (depthTexture)
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
			GLfloat borderColor[] = { 1.0, 1.0, 1.0, 1.0 };
			glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
		}
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, depthTexture ? GL_NEAREST : GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, depthTexture ? GL_NEAREST : GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE); // automatic mipmap generation included in OpenGL v1.4
		glCheckError(__FUNCTION__, __LINE__);

		// attach a texture to FBO color attachement point
		if (depthTexture)
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_dynamicTextureID, 0);				// depth
		else
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, msaa>0 ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D, m_dynamicTextureID, 0);			// color
		glCheckError(__FUNCTION__, __LINE__);
	}
	glFlush();

	// Initialize the render-buffer for usage as a depth buffer.
	// We don't really need this to render things into the frame-buffer object,
	// but without it the geometry will not be sorted properly.
	if (msaa > 0) {
		glGenTextures(1, &m_depthRenderBuffer);
		glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, m_depthRenderBuffer);
		m_depthRenderBufferIsTexture = true;

		glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, msaa, GL_DEPTH_COMPONENT, buffer_width, buffer_height, GL_FALSE);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D_MULTISAMPLE, m_depthRenderBuffer, 0);
		glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);
	}
	else {
		glGenRenderbuffers(1, &m_depthRenderBuffer);
		glBindRenderbuffer(GL_RENDERBUFFER, m_depthRenderBuffer);
		m_depthRenderBufferIsTexture = false;

		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, buffer_width, buffer_height);
		glBindRenderbuffer(GL_RENDERBUFFER, 0);
	}
	glCheckError(__FUNCTION__, __LINE__);

	if (depthTexture) {
		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);
	}
	else {
		// attach a renderbuffer to depth attachment point
		if (msaa==0)
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depthRenderBuffer);
	}
	glCheckError(__FUNCTION__, __LINE__);


#ifdef TRACE_FBO
	printFramebufferInfo();
#endif
	//
	// Check for errors...
	//

	bool status = CheckFramebufferStatus();

	// restore initial bindings
	glBindFramebuffer(GL_FRAMEBUFFER, initialFrameBufferBinding);
	glBindTexture(GL_TEXTURE_2D, initialTexture2DBinding);

	if (!status)
	{
		Release();
		return false;
	}
	return true;
}

void GL_FrameBufferObject::SetDynamicTextureID(GLuint dynamicTextureID)
{
	m_dynamicTextureID = dynamicTextureID;
}

GLuint GL_FrameBufferObject::GetCurrentFrameBufferBinding()
{
	GLint currentFrameBufferBinding;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &currentFrameBufferBinding);
	return currentFrameBufferBinding;
}

void GL_FrameBufferObject::MakeCurrent()
{
	if (m_frameBuffer) {
		//
		// Bind the frame-buffer object and attach to it a render-buffer object 
		// set up as a depth-buffer.
		//
		glBindFramebuffer(GL_FRAMEBUFFER, m_frameBuffer);
		glDepthMask(GL_TRUE);
		glEnable(GL_DEPTH_TEST);
	}
}

bool GL_FrameBufferObject::IsValid() const
{
	return m_frameBuffer != 0 && m_depthRenderBuffer != 0;
}

GLuint GL_FrameBufferObject::GetId() const
{
	return m_frameBuffer;
}

GLuint GL_FrameBufferObject::GetTextureId() const
{
	return m_dynamicTextureID;
}

void GL_FrameBufferObject::Release()
{
	if (glDeleteFramebuffers && m_frameBuffer)
		glDeleteFramebuffers(1, &m_frameBuffer);
	if (m_depthRenderBuffer) {
		if (m_depthRenderBufferIsTexture)
			glDeleteTextures(1, &m_depthRenderBuffer);
		else if (glDeleteRenderbuffers)
			glDeleteRenderbuffers(1, &m_depthRenderBuffer);
	}
	m_frameBuffer = m_depthRenderBuffer = 0;
}

bool GL_FrameBufferObject::BindTexImageARB()
{
	glBindTexture(GL_TEXTURE_2D, m_dynamicTextureID);
	return true;
}

bool GL_FrameBufferObject::ReleaseTexImageARB(GLuint old)
{
	//
	// Unbind the frame-buffer and render-buffer objects.
	//
	// trigger mipmaps generation explicitly
	// NOTE: If GL_GENERATE_MIPMAP is set to GL_TRUE, then glCopyTexSubImage2D()
	// triggers mipmap generation automatically. However, the texture attached
	// onto a FBO should generate mipmaps manually via glGenerateMipmap().
	glBindFramebuffer(GL_FRAMEBUFFER, old);
	if (m_dynamicTextureID > 0) {
		glBindTexture(GL_TEXTURE_2D, m_dynamicTextureID);
		glGenerateMipmap(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
	return true;
}

int	GL_FrameBufferObject::GetPixelFormat() const
{
	return ::GetPixelFormat(wglGetCurrentDC());
}

ChaineInc GL_FrameBufferObject::GetType() const
{
	return ChaineInc("GL_FrameBufferObject");
}

#ifdef TRACE_FBO
void GL_FrameBufferObject::debugDrawDirect()
{
#ifdef DEBUG_DRW_OBJ
	static float angle = 0.0f;
	glClearColor(1, 1, 1, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glScalef(0.1f, 0.1f, 0.1f);
	angle += 10.0f;
	glRotatef(angle*0.5f, 1, 0, 0);
	glRotatef(angle, 0, 1, 0);
	glRotatef(angle*0.7f, 0, 0, 1);
	glTranslatef(0, -1.575f, 0);
	drawTeapot();
	glPopMatrix();
#endif
}

void GL_FrameBufferObject::debugExportBuffer(const ChaineInc& filename)
{
	if (m_buffer_width <= 0 || m_buffer_height <= 0 || m_format == 0)
		return;

	// backup
	GLuint oldFrameBufferBinding = GetCurrentFrameBufferBinding();
	MakeCurrent();

	int nChannels = 0;
	switch (m_format)
	{
	case GL_R:		nChannels = 1; break;
	case GL_RGB:	nChannels = 3; break;
	case GL_RGBA:	nChannels = 4; break;
	case GL_DEPTH_COMPONENT: 
	case GL_STENCIL_INDEX:
		nChannels = 1; break;
	}
	GLubyte* pixels = (GLubyte*)malloc(nChannels * sizeof(GLubyte) * m_buffer_width * m_buffer_height);
	if (pixels)
	{
		glReadPixels(0, 0, m_buffer_width, m_buffer_height, m_format, GL_UNSIGNED_BYTE, pixels);

		// flip the bitmap
		int halfHeight = (int)(.5f * m_buffer_height);
		int size = m_buffer_width*nChannels;
		unsigned char* buffer = (unsigned char*)malloc(size*sizeof(unsigned char));
		if (buffer)
		{
			for (int i = 0; i < halfHeight; i++)
			{
				unsigned char* line1 = pixels + i*size;
				unsigned char* line2 = pixels + (m_buffer_height - i - 1)*size;
				memcpy(buffer, line1, size*sizeof(unsigned char));
				memcpy(line1, line2, size*sizeof(unsigned char));
				memcpy(line2, buffer, size*sizeof(unsigned char));
			}
			free(buffer);
		}

		// save the bitmap
		ImageStorage stg;
		stg.Create(filename.chars());
		stg.Save(nullptr, 0, 0, m_buffer_width, m_buffer_height, nChannels, (const unsigned char *)pixels, -1);

		free(pixels);
	}

	// restore
	glBindFramebuffer(GL_FRAMEBUFFER, oldFrameBufferBinding);
}

// return renderbuffer parameters as string using glGetRenderbufferParameteriv
ChaineInc GL_FrameBufferObject::getRenderbufferParameters(GLuint id)
{
	if (glIsRenderbuffer(id) == GL_FALSE)
		return "Not Renderbuffer object";

	int width, height, format;
	ChaineInc formatName;
	glBindRenderbuffer(GL_RENDERBUFFER, id);
	glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &width);    // get renderbuffer width
	glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &height);  // get renderbuffer height
	glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_INTERNAL_FORMAT, &format); // get renderbuffer internal format
	glBindRenderbuffer(GL_RENDERBUFFER, 0);

	formatName = convertInternalFormatToString(format);

	ChaineInc ss;
	ss.sprintf("width %d height %d %s\n", width, height, formatName.chars());
	return ss;
}

// convert OpenGL internal format enum to string
ChaineInc GL_FrameBufferObject::convertInternalFormatToString(GLenum format)
{
	ChaineInc formatName;

	switch (format)
	{
	case GL_STENCIL_INDEX:      // 0x1901
		formatName = "GL_STENCIL_INDEX";
		break;
	case GL_DEPTH_COMPONENT:    // 0x1902
		formatName = "GL_DEPTH_COMPONENT";
		break;
	case GL_ALPHA:              // 0x1906
		formatName = "GL_ALPHA";
		break;
	case GL_RGB:                // 0x1907
		formatName = "GL_RGB";
		break;
	case GL_RGBA:               // 0x1908
		formatName = "GL_RGBA";
		break;
	case GL_LUMINANCE:          // 0x1909
		formatName = "GL_LUMINANCE";
		break;
	case GL_LUMINANCE_ALPHA:    // 0x190A
		formatName = "GL_LUMINANCE_ALPHA";
		break;
	case GL_R3_G3_B2:           // 0x2A10
		formatName = "GL_R3_G3_B2";
		break;
	case GL_ALPHA4:             // 0x803B
		formatName = "GL_ALPHA4";
		break;
	case GL_ALPHA8:             // 0x803C
		formatName = "GL_ALPHA8";
		break;
	case GL_ALPHA12:            // 0x803D
		formatName = "GL_ALPHA12";
		break;
	case GL_ALPHA16:            // 0x803E
		formatName = "GL_ALPHA16";
		break;
	case GL_LUMINANCE4:         // 0x803F
		formatName = "GL_LUMINANCE4";
		break;
	case GL_LUMINANCE8:         // 0x8040
		formatName = "GL_LUMINANCE8";
		break;
	case GL_LUMINANCE12:        // 0x8041
		formatName = "GL_LUMINANCE12";
		break;
	case GL_LUMINANCE16:        // 0x8042
		formatName = "GL_LUMINANCE16";
		break;
	case GL_LUMINANCE4_ALPHA4:  // 0x8043
		formatName = "GL_LUMINANCE4_ALPHA4";
		break;
	case GL_LUMINANCE6_ALPHA2:  // 0x8044
		formatName = "GL_LUMINANCE6_ALPHA2";
		break;
	case GL_LUMINANCE8_ALPHA8:  // 0x8045
		formatName = "GL_LUMINANCE8_ALPHA8";
		break;
	case GL_LUMINANCE12_ALPHA4: // 0x8046
		formatName = "GL_LUMINANCE12_ALPHA4";
		break;
	case GL_LUMINANCE12_ALPHA12:// 0x8047
		formatName = "GL_LUMINANCE12_ALPHA12";
		break;
	case GL_LUMINANCE16_ALPHA16:// 0x8048
		formatName = "GL_LUMINANCE16_ALPHA16";
		break;
	case GL_INTENSITY:          // 0x8049
		formatName = "GL_INTENSITY";
		break;
	case GL_INTENSITY4:         // 0x804A
		formatName = "GL_INTENSITY4";
		break;
	case GL_INTENSITY8:         // 0x804B
		formatName = "GL_INTENSITY8";
		break;
	case GL_INTENSITY12:        // 0x804C
		formatName = "GL_INTENSITY12";
		break;
	case GL_INTENSITY16:        // 0x804D
		formatName = "GL_INTENSITY16";
		break;
	case GL_RGB4:               // 0x804F
		formatName = "GL_RGB4";
		break;
	case GL_RGB5:               // 0x8050
		formatName = "GL_RGB5";
		break;
	case GL_RGB8:               // 0x8051
		formatName = "GL_RGB8";
		break;
	case GL_RGB10:              // 0x8052
		formatName = "GL_RGB10";
		break;
	case GL_RGB12:              // 0x8053
		formatName = "GL_RGB12";
		break;
	case GL_RGB16:              // 0x8054
		formatName = "GL_RGB16";
		break;
	case GL_RGBA2:              // 0x8055
		formatName = "GL_RGBA2";
		break;
	case GL_RGBA4:              // 0x8056
		formatName = "GL_RGBA4";
		break;
	case GL_RGB5_A1:            // 0x8057
		formatName = "GL_RGB5_A1";
		break;
	case GL_RGBA8:              // 0x8058
		formatName = "GL_RGBA8";
		break;
	case GL_RGB10_A2:           // 0x8059
		formatName = "GL_RGB10_A2";
		break;
	case GL_RGBA12:             // 0x805A
		formatName = "GL_RGBA12";
		break;
	case GL_RGBA16:             // 0x805B
		formatName = "GL_RGBA16";
		break;
	case GL_DEPTH_COMPONENT16:  // 0x81A5
		formatName = "GL_DEPTH_COMPONENT16";
		break;
	case GL_DEPTH_COMPONENT24:  // 0x81A6
		formatName = "GL_DEPTH_COMPONENT24";
		break;
	case GL_DEPTH_COMPONENT32:  // 0x81A7
		formatName = "GL_DEPTH_COMPONENT32";
		break;
	case GL_DEPTH_STENCIL:      // 0x84F9
		formatName = "GL_DEPTH_STENCIL";
		break;
	case GL_RGBA32F:            // 0x8814
		formatName = "GL_RGBA32F";
		break;
	case GL_RGB32F:             // 0x8815
		formatName = "GL_RGB32F";
		break;
	case GL_RGBA16F:            // 0x881A
		formatName = "GL_RGBA16F";
		break;
	case GL_RGB16F:             // 0x881B
		formatName = "GL_RGB16F";
		break;
	case GL_DEPTH24_STENCIL8:   // 0x88F0
		formatName = "GL_DEPTH24_STENCIL8";
		break;
	default:
		ChaineInc ss;
		ss.sprintf("Unknown Format(0x%x)\n", format);
		formatName = ss;
	}

	return formatName;
}

// return texture parameters as string using glGetTexLevelParameteriv()
ChaineInc GL_FrameBufferObject::getTextureParameters(GLuint id)
{
	if (glIsTexture(id) == GL_FALSE)
		return "Not texture object";

	int width, height, format;
	ChaineInc formatName;
	glBindTexture(GL_TEXTURE_2D, id);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);            // get texture width
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);          // get texture height
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &format); // get texture internal format
	glBindTexture(GL_TEXTURE_2D, 0);

	formatName = convertInternalFormatToString(format);

	ChaineInc ss;
	ss.sprintf("width %d height %d formatName %s\n", width, height, formatName.chars());
	return ss;
}

// print out the FBO infos
void GL_FrameBufferObject::printFramebufferInfo()
{
	printf("===== FBO STATUS =====\n");

	// print max # of colorbuffers supported by FBO
	int colorBufferCount = 0;
	glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &colorBufferCount);
	printf("Max Number of Color Buffer Attachment Points: %d \n", colorBufferCount);

	int objectType;
	int objectId;

	// print info of the colorbuffer attachable image
	for (int i = 0; i < colorBufferCount; ++i)
	{
		glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
			GL_COLOR_ATTACHMENT0 + i,
			GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
			&objectType);
		if (objectType != GL_NONE)
		{
			glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
				GL_COLOR_ATTACHMENT0 + i,
				GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
				&objectId);

			ChaineInc formatName;

			printf("Color Attachment %d :", i);
			if (objectType == GL_TEXTURE)
			{
				printf("GL_TEXTURE, %s\n", getTextureParameters(objectId).chars());
			}
			else if (objectType == GL_RENDERBUFFER)
			{
				printf("GL_RENDERBUFFER, %s\n", getRenderbufferParameters(objectId).chars());
			}
		}
	}

	// print info of the depthbuffer attachable image
	glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
		GL_DEPTH_ATTACHMENT,
		GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
		&objectType);
	if (objectType != GL_NONE)
	{
		glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
			GL_DEPTH_ATTACHMENT,
			GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
			&objectId);

		printf("Depth Attachment: ");
		switch (objectType)
		{
		case GL_TEXTURE:
			printf("GL_TEXTURE, %s\n", getTextureParameters(objectId).chars());
			break;
		case GL_RENDERBUFFER:
			printf("GL_RENDERBUFFER, %s\n", getRenderbufferParameters(objectId).chars());
			break;
		}
	}

	// print info of the stencilbuffer attachable image
	glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
		GL_STENCIL_ATTACHMENT,
		GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
		&objectType);
	if (objectType != GL_NONE)
	{
		glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
			GL_STENCIL_ATTACHMENT,
			GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
			&objectId);

		printf("Stencil Attachment: ");
		switch (objectType)
		{
		case GL_TEXTURE:
			printf("GL_TEXTURE, %s\n", getTextureParameters(objectId).chars());
			break;
		case GL_RENDERBUFFER:
			printf("GL_RENDERBUFFER, %s\n", getRenderbufferParameters(objectId).chars());
			break;
		}
	}
}
#endif
