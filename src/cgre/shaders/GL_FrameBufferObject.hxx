#ifndef GL_FRAME_BUFFER_OBJECT_HXX
#define GL_FRAME_BUFFER_OBJECT_HXX

#include <Win32_GL_utils/GL.hxx>
#include <Win32_GL_utils/GL_PBuffer.hxx>
#include <utils/ChaineInc.hxx>

//#define TRACE_FBO // FBO utils

class GL_FrameBufferObject : public GL_IBuffer
{
protected:
	GLuint m_frameBuffer;
	GLuint m_depthRenderBuffer;

	GLuint	m_dynamicTextureID;

	bool	m_autoDeleteTexture;
	bool	m_depthRenderBufferIsTexture;
public:
	GL_FrameBufferObject();
	~GL_FrameBufferObject();

	// overrides
	virtual void Init(HDC hDC/* not used but kept for compatibility with GL_PBuffer*/, int buffer_width, int buffer_height) override;
	virtual void MakeCurrent() override;
	virtual bool IsValid() const override;
	virtual void Release() override;
	virtual int	GetPixelFormat() const override;
	virtual ChaineInc GetType() const override;

	bool Init(int buffer_width, int buffer_height, bool depthTexture = false, int msaa = 0);
	static GLuint GetCurrentFrameBufferBinding();

	GLuint GetId() const;
	GLuint GetTextureId() const;

	void SetDynamicTextureID(GLuint dynamicTextureID);
	bool BindTexImageARB();
	bool ReleaseTexImageARB(GLuint old);

#ifdef TRACE_FBO
	static void debugDrawDirect();
	void debugExportBuffer(const ChaineInc& filename);
	static void printFramebufferInfo();
	static ChaineInc convertInternalFormatToString(GLenum format);
	static ChaineInc getTextureParameters(GLuint id);
	static ChaineInc getRenderbufferParameters(GLuint id);
#endif

private:
	static bool DefineProc();
	static bool CheckFramebufferStatus();

	int m_buffer_width, m_buffer_height;
	GLenum m_format;

friend class GL_FrameBufferObjectTest;
};

#endif // GL_FRAME_BUFFER_OBJECT_HXX
