#include <stdlib.h>
#include <stdio.h>

#include "materials_manager.h"

MaterialsManager *MaterialsManager::m_pInstance = new MaterialsManager;

MaterialsManager::MaterialsManager()
{
	m_nMaterials = 0;
	for (int i=0; i<8; i++)
		m_pMaterials[i] = NULL;
}

MaterialsManager::~MaterialsManager()
{
}

void MaterialsManager::Initialize (void)
{
}

int MaterialsManager::AddMaterial (Material* pMapterial)
{
	if (m_nMaterials == 8)
		return -1;

	m_pMaterials[m_nMaterials] = pMapterial;
	m_nMaterials++;
	return m_nMaterials-1;
}

Material* MaterialsManager::GetMaterial (unsigned int id)
{
	return m_pMaterials[id];
}

void MaterialsManager::EnableMaterial (unsigned int id, bool Flag)
{
	if (id <= 0 || id > 8) return;

	Material *pMaterial = m_pMaterials[id];
	//pMaterial->SetEnable (Flag);
}

void MaterialsManager::dump (void)
{
	for (int i=0; i<8; i++)
	{
		printf ("%d / 8 :\n", i);
		Material *pMaterial = m_pMaterials[i];
		if (pMaterial != NULL)
			pMaterial->Dump ();
		else
			printf (" no material stored\n");
	}
}
