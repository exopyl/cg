#pragma once

class Light;

class LightRenderer
{
private:
	LightRenderer();
	~LightRenderer ();
public:
	static LightRenderer* getInstance (void) { return m_pInstance; };

	static void EnableLighting (bool bFlag);
	static void EnableLight (unsigned int id);
	static void DisplayLight (Light *pLight);

private:
	static LightRenderer *m_pInstance;
};
