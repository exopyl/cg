#pragma once

#include <map>
using namespace std;

class Img;

class TexturesManager
{
private:
	TexturesManager () {};
	~TexturesManager () {};

public:
	static TexturesManager* getInstance (void) { return m_pInstance; };

	unsigned int addTexture (Img *img);

private:
	static TexturesManager *m_pInstance;

	int m_idCurrent;
	std::map<Img*,int> m_mapTextures;
};
