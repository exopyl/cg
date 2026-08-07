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

// LoadTexture(char*) a ete retiree avec le decodeur TGAImg qu'elle etait seule a
// utiliser : jamais appelee, et compilee sous #ifndef WIN32 alors que cgre n'est
// construit qu'avec ENABLE_SINAIA, donc sous Windows. Pour charger une texture,
// passer par Img::load(), qui dispatche sur l'extension (TGA compris).
