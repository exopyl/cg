#include <stdio.h>

#include "palette.h"

Palette::Palette ()
{
	m_nMaxColors = 256;
	m_nColors = 0;
	m_pColors = (Color*)malloc(m_nMaxColors*sizeof(Color));
}

Palette::~Palette ()
{
	if (m_pColors)
		free (m_pColors);
}

unsigned int Palette::AddColor (unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
	int index = IsPresent (r, g, b, a);
	if (index != -1)
		return index;

	if (m_nColors >= m_nMaxColors)
		return 0;
	m_pColors[m_nColors].SetRGBA (r, g, b, a);
	m_nColors++;

	return m_nColors-1;
}

int Palette::IsPresent (unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
	for (unsigned int i=0; i<m_nColors; i++)
	{
		if (m_pColors[i].r() == r &&
		    m_pColors[i].g() == g && 
		    m_pColors[i].b() == b && 
		    m_pColors[i].a() == a)
			return i;
	}
	return -1;
}

void Palette::dump (void)
{
	printf ("palette size : %d\n", m_nColors);
	for (unsigned int i=0; i<m_nColors; i++)
		printf ("color %d / %d : %d %d %d %d\n", i, m_nColors,
			m_pColors[i].r(), m_pColors[i].g(), m_pColors[i].b(), m_pColors[i].a() );
}

