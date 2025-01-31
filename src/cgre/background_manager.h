#ifndef __BACKGROUND_MANAGER_H__
#define __BACKGROUND_MANAGER_H__

#include "skybox.h"

class BackgroundManager
{
public:
	enum BackgroundType { BACKGROUND_COLOR, BACKGROUND_GRADIENT, BACKGROUND_SKYBOX };

	static BackgroundManager* getInstance (void) { return m_pInstance; };

	void select (BackgroundType eBg, char *path = nullptr);
	void display ();

	BackgroundType get_type (void) { return m_eBgType; };

private:
	static BackgroundManager *m_pInstance;

	BackgroundType m_eBgType;
	float m_color[3];
	float m_gradient_color1[3], m_gradient_color2[3];
	Skybox *m_pSkybox;

	BackgroundManager ();
	~BackgroundManager ();

	void DrawColor (float r, float g, float b);
	void DrawGradient (float rtop, float gtop, float btop, float rbottom, float gbottom, float bbottom);
	void DrawSkybox (void);
};

#endif // __BACKGROUND_MANAGER_H__
