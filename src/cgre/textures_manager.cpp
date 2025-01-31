#include "textures_manager.h"

#include "../cgimg/cgimg.h"

#include "gl_wrapper.h"

TexturesManager *TexturesManager::m_pInstance = new TexturesManager;

unsigned int TexturesManager::addTexture (Img *pImg)
{
	unsigned int idImg;

	unsigned char* data = pImg->m_pPixels;
	int img_width = pImg->width ();
	int img_height = pImg->height ();

	glGenTextures(1, &idImg); // create a texture
	glBindTexture(GL_TEXTURE_2D, idImg);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,  img_width, img_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	//delete pImg;
	//m_mapTextures[idImg] = pImg;

	return idImg;
}
