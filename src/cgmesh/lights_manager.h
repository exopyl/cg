#pragma once
#include "light.h"

class LightsManager
{
private:
	LightsManager();
	~LightsManager ();

public:
	static LightsManager* getInstance (void) { return m_pInstance; };

	void Initialize (void);

	int		AddLight		(Light* light);
	
	void	EnableLight		(unsigned int id, bool bFlag);

	Light*	GetLight (unsigned int id);

	void	dump (void);

public:
	static LightsManager *m_pInstance;

	int m_nLights;
	Light* m_pLights[8];
};
