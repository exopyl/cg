#include "material_renderer.h"

MaterialRenderer *MaterialRenderer::m_pInstance = new MaterialRenderer;

MaterialRenderer::MaterialRenderer()
{
	m_nMaterials = 0;
}

MaterialRenderer::~MaterialRenderer()
{
}

int MaterialRenderer::AddMaterial (Material *pMaterial)
{
	if (!pMaterial) return -1;

	// Search if already added
	for (unsigned int i = 0; i < m_nMaterials; i++)
	{
		if (m_pMaterials[i] == pMaterial)
			return (int)i;
	}

	if (m_nMaterials >= 256)
		return - 1;

	m_pMaterials[m_nMaterials] = pMaterial;

	if (pMaterial->GetType () == MATERIAL_TEXTURE)
	{
		MaterialTexture *pMaterialTexture = dynamic_cast<MaterialTexture*> (pMaterial);
		if (pMaterialTexture && pMaterialTexture->GetImage())
		{
			glGenTextures(1, &m_pTexturesId[m_nMaterials]);
			glBindTexture(GL_TEXTURE_2D, m_pTexturesId[m_nMaterials]);

			Img *pImage = pMaterialTexture->GetImage ();
			glTexImage2D(GL_TEXTURE_2D, 0, 4, pImage->m_iWidth, pImage->m_iHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, pImage->m_pPixels);
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
		}
	}
	m_nMaterials++;
	return m_nMaterials-1;
}

void MaterialRenderer::ActivateMaterial (unsigned int id)
{
	if (id >= m_nMaterials || m_pMaterials[id] == NULL)
		return;

	Material *pMaterial = m_pMaterials[id];
	if (pMaterial->GetType () == MATERIAL_TEXTURE)
	{
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, m_pTexturesId[id]);
	}
	else if (pMaterial->GetType () == MATERIAL_COLOR_ADV)
	{
		glDisable(GL_TEXTURE_2D);
		glDisable(GL_COLOR_MATERIAL);
		MaterialColorExt *pMatColExt = dynamic_cast<MaterialColorExt*>(pMaterial);
		if (pMatColExt)
		{
			// For when lighting is ON
			glMaterialfv (GL_FRONT, GL_AMBIENT, pMatColExt->m_fAmbient);
			glMaterialfv (GL_FRONT, GL_DIFFUSE, pMatColExt->m_fDiffuse);
			glMaterialfv (GL_FRONT, GL_SPECULAR, pMatColExt->m_fSpecular);
			glMaterialf (GL_FRONT, GL_SHININESS, 128. * pMatColExt->m_fShininess[0]);
			glMaterialfv (GL_FRONT, GL_EMISSION, pMatColExt->m_fEmission);

			// For when lighting is OFF
			glColor4fv(pMatColExt->m_fDiffuse);
		}
	}
	else if (pMaterial->GetType() == MATERIAL_COLOR)
	{
		glDisable(GL_TEXTURE_2D);
		glEnable(GL_COLOR_MATERIAL);
		glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
		MaterialColor* pMatCol = dynamic_cast<MaterialColor*>(pMaterial);
		if (pMatCol)
		{
			glColor4f(pMatCol->GetFloatRed(), pMatCol->GetFloatGreen(), pMatCol->GetFloatBlue(), pMatCol->GetFloatAlpha());
		}
	}
}

void MaterialRenderer::SetMaterial (MaterialColorExt::MaterialColorExtType eType)
{
	MaterialColorExt *pMaterial = new MaterialColorExt ();
	pMaterial->Init_From_Library (eType);

	MaterialColorExt *pMatColExt = dynamic_cast<MaterialColorExt*>(pMaterial);
 
	GLfloat mat[4];

	//glEnable (GL_COLOR_MATERIAL);
	glColorMaterial (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE | GL_SPECULAR | GL_SHININESS);
 
	// ambient
	/*
	mat[0] = material_parameters[10*index];
	mat[1] = material_parameters[10*index+1];
	mat[2] = material_parameters[10*index+2];
	mat[3] = 1.;
	*/
	glMaterialfv (GL_FRONT, GL_AMBIENT, pMatColExt->m_fAmbient);
  
	// diffuse
	/*
	mat[0] = material_parameters[10*index+3];
	mat[1] = material_parameters[10*index+4];
	mat[2] = material_parameters[10*index+5];
	*/
	glMaterialfv (GL_FRONT, GL_DIFFUSE, pMatColExt->m_fDiffuse);
  
	// specular
	/*
	mat[0] = material_parameters[10*index+6];
	mat[1] = material_parameters[10*index+7];
	mat[2] = material_parameters[10*index+8];
	*/
	glMaterialfv (GL_FRONT, GL_SPECULAR, pMatColExt->m_fSpecular);
  
	// shininess
	//mat[0] = 128 * material_parameters[10*index+9];
	glMaterialf (GL_FRONT, GL_SHININESS, 128. * pMatColExt->m_fShininess[0]);
  
	glMaterialfv (GL_FRONT, GL_EMISSION, pMatColExt->m_fEmission);

	delete pMaterial;
}



#ifndef WIN32
#include "cgmesh/cgmesh.h"
#include "cgimg/cgimg.h"
#include "cgimg/image_tga.h"

// Load a TGA texture
GLuint LoadTexture(char *TexName)
{
	TGAImg Img;
	GLuint Texture;
	
	// Load our Texture
	if(Img.Load(TexName)!=IMG_OK)
		return -1;
	
	glGenTextures(1,&Texture);
	glBindTexture(GL_TEXTURE_2D,Texture);
	
	// Create the texture
	if(Img.GetBPP()==24)
		glTexImage2D(GL_TEXTURE_2D,0,3,
			     Img.GetWidth(),Img.GetHeight(),
			     0,GL_RGB,GL_UNSIGNED_BYTE,
			     Img.GetImg());
	else if(Img.GetBPP()==32)
		glTexImage2D(GL_TEXTURE_2D,0,4,
			     Img.GetWidth(),Img.GetHeight(),
			     0,GL_RGBA,GL_UNSIGNED_BYTE,
			     Img.GetImg());
	else
		return -1;
	
	// Specify filtering and edge actions
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
	
	return Texture;
}

#endif
