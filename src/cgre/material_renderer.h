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
	Material *m_pMaterials[256];
	GLuint m_pTexturesId[256];
	// Carte de reflexion du materiau, 0 si aucune. Objet GL distinct : un
	// materiau peut porter les deux cartes, liees a deux unites de texture.
	GLuint m_pReflTexturesId[256];
	// Taux de reflexion, dans [0,1] : proportion du reflet dans la couleur finale.
	// Ni le 3DS ni le MTL ne fournissent de montant pour la map, on le derive donc
	// du niveau speculaire du materiau (cf. ActivateMaterial).
	float  m_pReflAmount[256];
};

extern GLuint LoadTexture(char *TexName);
