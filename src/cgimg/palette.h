#pragma once

#include "color.h"

class Palette
{
public:
	Palette ();
	Palette (const Palette &p);              // copie profonde de m_pColors
	Palette &operator= (const Palette &p);   // idem (règle des 3/5 : dtor libère m_pColors)
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
