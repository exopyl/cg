#pragma once
#include "material.h"

class MaterialsManager
{
private:
	MaterialsManager();
	~MaterialsManager ();

public:
	static MaterialsManager* getInstance (void) { return m_pInstance; };

	void Initialize (void);

	int		AddMaterial		(Material* pMaterial);
	
	void	EnableMaterial		(unsigned int id, bool bFlag);

	Material*	GetMaterial (unsigned int id);

	void	dump (void);

public:
	static MaterialsManager *m_pInstance;

	int m_nMaterials;
	Material* m_pMaterials[8];
};
