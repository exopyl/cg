#pragma once

#include "color.h"

class Palette
{
public:
	Palette ();
	~Palette ();

	unsigned int AddColor (unsigned char r, unsigned char g, unsigned char b, unsigned char a);
	int IsPresent (unsigned char r, unsigned char g, unsigned char b, unsigned char a);

	inline unsigned int NColors (void) { return m_nColors; };

	void dump (void);
	
public:
	unsigned int m_nColors;
	unsigned int m_nMaxColors;
	Color *m_pColors;
};
