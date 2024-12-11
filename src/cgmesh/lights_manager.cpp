#include <stdlib.h>
#include <stdio.h>

#include "lights_manager.h"

LightsManager *LightsManager::m_pInstance = new LightsManager;

LightsManager::LightsManager()
{
}

LightsManager::~LightsManager()
{
}

void LightsManager::Initialize (void)
{
	m_nLights = 0;
	for (int i=0; i<8; i++)
		m_pLights[i] = NULL;
}

int LightsManager::AddLight (Light* light)
{
	if (m_nLights == 8)
		return -1;

	m_pLights[m_nLights] = light;
	m_nLights++;
	return m_nLights-1;
}

Light* LightsManager::GetLight (unsigned int id)
{
	return m_pLights[id];
}

void LightsManager::EnableLight (unsigned int id, bool Flag)
{
	if (id <= 0 || id > 8) return;

	Light *light = m_pLights[id];
	light->SetEnable (Flag);
}

void LightsManager::dump (void)
{
	for (int i=1; i<9; i++)
	{
		printf ("%d / 8 :\n", i);
		Light *light = m_pLights[i];
		if (light != NULL)
			light->Dump ();
		else
			printf (" no light stored\n");
	}
}
