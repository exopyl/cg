#ifndef GL_PBUFFER_HXX
#define GL_PBUFFER_HXX

#include	<Win32_GL_utils/GL.hxx>
#include	"Win32_GL_utils/GL_RC_Util.hxx"

//
// Interface for OpenGL Buffer
//
class GL_IBuffer
{
public:
	virtual ~GL_IBuffer();
	virtual void Init(HDC hDC, int pbuffer_width, int pbuffer_height) = 0;
	virtual void MakeCurrent() = 0;
	virtual bool IsValid() const = 0;
	virtual void Release() = 0;
	virtual int	GetPixelFormat() const = 0;
	virtual ChaineInc GetType() const = 0;
};

class GL_PBuffer : public GL_IBuffer
{
protected:
	typedef struct {
		HPBUFFERARB hPBuffer;
		HDC         hDC;
		HGLRC       hRC;
		int         nWidth;
		int         nHeight;

	} PBUFFER;

	GL_RC* m_glRC;
	PBUFFER m_pbuffer;

public:
	GL_PBuffer();
	~GL_PBuffer();

	virtual void Init(HDC hDC, int pbuffer_width, int pbuffer_height) override;
	virtual void MakeCurrent() override;
	virtual bool IsValid() const override;
	virtual void Release() override;
	virtual int	GetPixelFormat() const override;
	virtual ChaineInc GetType() const override;
};

#endif // GL_PBUFFER_HXX
