#pragma once

/*
*
* Reference :
* http://archive.gamedev.net/archive/reference/programming/features/fbo1/index.html
* http://archive.gamedev.net/archive/reference/programming/features/fbo2/index.html
*
*/

/**
* class FrameBufferObject : manage extension
*/
class FrameBufferObject
{
public:
	FrameBufferObject (unsigned int width = 256, unsigned int height = 256);
	~FrameBufferObject ();

	bool isOK (void) { return m_status; };

	void activate (void);
	void desactivate (void);

	void bindTexture (void);

	bool status(void);

private:
	bool m_status;

	GLuint m_fbo;
	GLuint m_depthbuffer;
	unsigned int m_width, m_height;
	GLuint m_img;
};
