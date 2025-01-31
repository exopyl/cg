#pragma once

#include "gl_wrapper.h"

#include "../cgmesh/cgmesh.h"

class MaterialRenderer
{
private:
	MaterialRenderer();
	~MaterialRenderer ();
public:
	static MaterialRenderer* getInstance (void) { return m_pInstance; };

	int AddMaterial (Material *pMaterial);

	static void SetMaterial (MaterialColorExt::MaterialColorExtType eType);
	void ActivateMaterial (unsigned int id);

private:
	static MaterialRenderer *m_pInstance;
	
	unsigned int m_nMaterials;
	Material *m_pMaterials[8];
	GLuint m_pTexturesId[8];
};

extern GLuint LoadTexture(char *TexName);
