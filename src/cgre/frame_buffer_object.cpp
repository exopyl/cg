#include <stdlib.h>
#include <stdio.h>
#include "gl_wrapper.h"


#include "frame_buffer_object.h"
#include "textures_manager.h"
#include "../cgimg/cgimg.h"

FrameBufferObject::FrameBufferObject (unsigned int width, unsigned int height)
{
	m_status = true;
	
	m_width		= width;
	m_height	= height;

	// frame buffer object
	glGenFramebuffers(1, &m_fbo);
	glGenFramebuffers(1, &m_fbo); // create an handle to a framebuffer
	glBindFramebuffer(GL_FRAMEBUFFER, m_fbo); // bind

	// render buffer
	glGenRenderbuffers(1, &m_depthbuffer); // create an handle to a renderbuffer
	glBindRenderbuffer(GL_RENDERBUFFER, m_depthbuffer); // bind the renderbuffer
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, m_width, m_height); // init the storage space
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depthbuffer); // attach the renderbuffer to the FBO

	/* colour information
	*  Two ways :
	*  1 - attach a colour renderbuffer to the FBO
	*  2 - attach a texture to the FBO
	* in the following, we consider the second method
	*/

	// create a texture
	Img *pImg = new Img ();
	pImg->load ("../data/lena_monochrome.bmp");
	m_img = TexturesManager::getInstance()->addTexture(pImg);

	// The following 3 lines enable mipmap filtering and generate the mipmap data so rendering works
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	//glGenerateMipmapEXT(GL_TEXTURE_2D);
	
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_img, 0); // attach the texture to the FBO

	glBindFramebuffer(GL_FRAMEBUFFER, 0); // Unbind the FBO for now

	// status of the currently bound FBO
	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	m_status = (status == GL_FRAMEBUFFER_COMPLETE)? true : false;
}

/**
* Destructor
*/
FrameBufferObject::~FrameBufferObject ()
{
	if (isOK())
	{
		glDeleteFramebuffers (1, &m_fbo);
		glDeleteRenderbuffers (1, &m_depthbuffer);
		glDeleteTextures (1, &m_img);
	}
}

/**
* Activates the rendering
*/
void FrameBufferObject::activate (void)
{
	glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
	glPushAttrib(GL_VIEWPORT_BIT);
	glViewport(0, 0, m_width, m_height);
}

/**
* Desactivates the rendering
*/
void FrameBufferObject::desactivate (void)
{
	glPopAttrib();
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

/**
* Binds the current texture
*/
void FrameBufferObject::bindTexture (void)
{
	glBindTexture(GL_TEXTURE_2D, m_img);
}


bool FrameBufferObject::status()
{
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    switch(status)
    {
    case GL_FRAMEBUFFER_COMPLETE:
        std::cout << "Framebuffer complete." << std::endl;
        return true;

	case GL_FRAMEBUFFER_UNSUPPORTED:
		std::cout << "[ERROR] Framebuffer incomplete: Unsupported by FBO implementation." << std::endl;
		return false;

    case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
        std::cout << "[ERROR] Framebuffer incomplete: Attachment is NOT complete." << std::endl;
        return false;

    case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
        std::cout << "[ERROR] Framebuffer incomplete: No image is attached to FBO." << std::endl;
        return false;

    case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
        std::cout << "[ERROR] GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE." << std::endl;
        return false;

    case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
        std::cout << "[ERROR] GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS." << std::endl;
        return false;

    case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
        std::cout << "[ERROR] Framebuffer incomplete: Draw buffer." << std::endl;
        return false;

    case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
        std::cout << "[ERROR] Framebuffer incomplete: Read buffer." << std::endl;
        return false;

    default:
        std::cout << "[ERROR] Framebuffer incomplete: Unknown error." << std::endl;
        return false;
    }
}
