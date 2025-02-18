#include "material.h"

//
// Material Color
//
MaterialColor::MaterialColor ()
{
}

MaterialColor::MaterialColor (unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
	m_r = r;
	m_g = g;
	m_b = b;
	m_a = a;
}

MaterialColor::MaterialColor (const MaterialColor &m)
{
	m_r = m.m_r;
	m_g = m.m_g;
	m_b = m.m_b;
	m_a = m.m_a;
}

MaterialType MaterialColor::GetType (void)
{
	return MATERIAL_COLOR;
}

void MaterialColor::Dump (void)
{
	printf ("MATERIAL_COLOR : %d %d %d %d\n", m_r, m_g, m_b, m_a);
}

//
// Material Color Extended
//
static float _material_extended_parameters[] = 
{   // Ambient Diffuse Specular Shininess
	0.0215, 0.1745, 0.0215, 0.07568, 0.61424, 0.07568, 0.633, 0.727811, 0.633, 0.6, // emerald
	0.135, 0.2225, 0.1575, 0.54, 0.89, 0.63, 0.316228, 0.316228, 0.316228, 0.1, // jade
	0.05375, 0.05, 0.06625, 0.18275, 0.17, 0.22525, 0.332741, 0.328634, 0.346435, 0.3, // obsidian
	0.25, 0.20725, 0.20725, 1, 0.829, 0.829, 0.296648, 0.296648, 0.296648, 0.088, // pearl
	0.1745, 0.01175, 0.01175, 0.61424, 0.04136, 0.04136, 0.727811, 0.626959, 0.626959, 0.6, // ruby
	0.1, 0.18725, 0.1745, 0.396, 0.74151, 0.69102, 0.297254, 0.30829, 0.306678, 0.1,  // turquoise
	0.329412, 0.223529, 0.027451, 0.780392, 0.568627, 0.113725, 0.992157, 0.941176, 0.807843, 0.21794872,  // brass
	0.2125, 0.1275, 0.054, 0.714, 0.4284, 0.18144, 0.393548, 0.271906, 0.166721, 0.2,  // bronze
	0.25, 0.25, 0.25, 0.4, 0.4, 0.4, 0.774597, 0.774597, 0.774597, 0.6,  // chrome
	0.19125, 0.0735, 0.0225, 0.7038, 0.27048, 0.0828, 0.256777, 0.137622, 0.086014, 0.1,  // copper
	0.24725, 0.1995, 0.0745, 0.75164, 0.60648, 0.22648, 0.628281, 0.555802, 0.366065, 0.4,  // gold
	0.19225, 0.19225, 0.19225, 0.50754, 0.50754, 0.50754, 0.508273, 0.508273, 0.508273, 0.4,  // silver
	0.0, 0.0, 0.0, 0.01, 0.01, 0.01, 0.50, 0.50, 0.50, .25,  // black, plastic
	0.0, 0.1, 0.06, 0.0, 0.50980392, 0.50980392, 0.50196078, 0.50196078, 0.50196078, .25,  // cyan, plastic
	0.0, 0.0, 0.0, 0.1, 0.35, 0.1, 0.45, 0.55, 0.45, .25,  // green plastic
	0.0, 0.0, 0.0, 0.5, 0.0, 0.0, 0.7, 0.6, 0.6, .25,  // red plastic
	0.0, 0.0, 0.0, 0.55, 0.55, 0.55, 0.70, 0.70, 0.70, .25,  // white, plastic
	0.0, 0.0, 0.0, 0.5, 0.5, 0.0, 0.60, 0.60, 0.50, .25,  // yellow, plastic
	0.02, 0.02, 0.02, 0.01, 0.01, 0.01, 0.4, 0.4, 0.4, .078125,  // black, rubber
	0.0, 0.05, 0.05, 0.4, 0.5, 0.5, 0.04, 0.7, 0.7, .078125,  // cyan, rubber
	0.0, 0.05, 0.0, 0.4, 0.5, 0.4, 0.04, 0.7, 0.04, .078125,  // green, rubber
	0.05, 0.0, 0.0, 0.5, 0.4, 0.4, 0.7, 0.04, 0.04, .078125,  // red, rubber
	0.05, 0.05, 0.05, 0.5, 0.5, 0.5, 0.7, 0.7, 0.7, .078125,  // white, rubber,
	0.05, 0.05, 0.0, 0.5, 0.5, 0.4, 0.7, 0.7, 0.04, .078125 // yellow, rubber
};

void MaterialColorExt::Init_From_Library (MaterialColorExtType index)
{
	SetAmbient (_material_extended_parameters[10*index], _material_extended_parameters[10*index+1], _material_extended_parameters[10*index+2], 1.);
	SetDiffuse (_material_extended_parameters[10*index+3], _material_extended_parameters[10*index+4], _material_extended_parameters[10*index+5], 1.);
	SetSpecular (_material_extended_parameters[10*index+6], _material_extended_parameters[10*index+7], _material_extended_parameters[10*index+8], 1.);
	SetShininess (_material_extended_parameters[10*index+9]);
	SetEmission (0., 0., 0., 1.);
}



//
// Texture
//
MaterialTexture::MaterialTexture (char const *filename, char const *path)
{
	m_pFilename = strdup (filename);
	/*
	m_pImage = new Img ();
	m_pImage->load (m_pFilename, path);
	*/
	
}

MaterialTexture::MaterialTexture (unsigned int nWidth, unsigned int nHeight)
{
}

MaterialTexture::MaterialTexture (const MaterialTexture &m) // constructor of copy
{
}

MaterialTexture::~MaterialTexture ()
{
}

MaterialType MaterialTexture::GetType (void)
{
	return MATERIAL_TEXTURE;
}

void MaterialTexture::Dump (void)
{
	printf ("MATERIAL_TEXTURE : %s\n", m_pFilename);
}

char* MaterialTexture::GetFilename ()
{
	return m_pFilename;
}

Img* MaterialTexture::GetImage ()
{
	return m_pImage;
}

