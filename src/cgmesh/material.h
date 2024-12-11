#ifndef __MATERIAL_H__
#define __MATERIAL_H__

#include "../cgimg/cgimg.h"

// material definition
enum MaterialType {
	MATERIAL_NONE = ((unsigned int)-1),
	MATERIAL_COLOR = 0,
	MATERIAL_COLOR_ADV = 1,
	MATERIAL_TEXTURE = 2
};

//
//
//
class Material
{
public:
	Material () { m_pName = NULL; };
	Material (const Material &m) {}; // constructor of copy
	//virtual ~Material ();

	virtual MaterialType GetType (void) { return MATERIAL_NONE; };
	virtual void Dump (void) {};
	
	void SetName (char const *name) { m_pName = strdup (name); };
	char *GetName (void) { return m_pName; };

protected:
	char *m_pName;
};

//
//
//
class MaterialColor : public Material
{
	friend class Mesh;
public:
	MaterialColor ();
	MaterialColor (unsigned char r, unsigned char g, unsigned char b, unsigned char a=255);
	MaterialColor (const MaterialColor &m); // constructor of copy

	//virtual ~MaterialColor () {};
	MaterialType GetType (void);
	void Dump (void);
private:
	unsigned char m_r, m_g, m_b, m_a;
};

//
// Material color extended
//
class MaterialColorExt : public Material
{
	friend class Mesh;

public:
	// material color extended library
	enum MaterialColorExtType {
		EMERALD = 0,
		JADE,
		OBSIDIAN,
		PEARL,
		RUBY,
		TURQUOISE,
		BRASS,
		BRONZE,
		CHROME,
		COPPER,
		GOLD,
		SILVER,
		BLACK_PLASTIC,
		CYAN_PLASTIC,
		GREEN_PLASTIC,
		RED_PLASTIC,
		WHITE_PLASTIC,
		YELLOW_PLASTIC,
		BLACK_RUBBER,
		CYAN_RUBBER,
		GREEN_RUBBER,
		RED_RUBBER,
		WHITE_RUBBER,
		YELLOW_RUBBER
	};

	MaterialColorExt ()
		{
			{
				for (int i=0; i<4; i++)
				{
					m_fAmbient[i] = 0.;
					m_fDiffuse[i] = 0.;
					m_fSpecular[i] = 0.;
					m_fEmission[i] = 0.;
				}
				m_fShininess[0] = 0.;
			}
		};
	MaterialColorExt (const MaterialColorExt &m) // constructor of copy
		{
			memcpy (m_fAmbient, m.m_fAmbient, 4*sizeof(float));
			memcpy (m_fDiffuse, m.m_fDiffuse, 4*sizeof(float));
			memcpy (m_fSpecular, m.m_fSpecular, 4*sizeof(float));
			memcpy (m_fEmission, m.m_fEmission, 4*sizeof(float));
			memcpy (m_fShininess, m.m_fShininess, 1*sizeof(float));
		};
	//virtual ~MaterialColorExt () {};

	inline void SetAmbient (float fR, float fG, float fB, float fA)
	{
		m_fAmbient[0] = fR;
		m_fAmbient[1] = fG;
		m_fAmbient[2] = fB;
		m_fAmbient[3] = fA;
	};
	inline void SetDiffuse (float fR, float fG, float fB, float fA)
	{
		m_fDiffuse[0] = fR;
		m_fDiffuse[1] = fG;
		m_fDiffuse[2] = fB;
		m_fDiffuse[3] = fA;
	};
	inline void GetDiffuse (float diffuse[4])
	{
		memcpy (diffuse, m_fDiffuse, 4*sizeof(float));
	};
	inline void SetSpecular (float fR, float fG, float fB, float fA)
	{
		m_fSpecular[0] = fR;
		m_fSpecular[1] = fG;
		m_fSpecular[2] = fB;
		m_fSpecular[3] = fA;
	};
	inline void SetEmission (float fR, float fG, float fB, float fA)
	{
		m_fEmission[0] = fR;
		m_fEmission[1] = fG;
		m_fEmission[2] = fB;
		m_fEmission[3] = fA;
	};
	inline void SetShininess (float fPower)
	{
		m_fShininess[0] = fPower;
	};
	void Init_From_Library (MaterialColorExtType eType);

	inline MaterialType GetType () { return MATERIAL_COLOR_ADV; };
	void Dump () {
		printf ("MATERIAL_COLOR_ADV :\n");
		printf ("   ambient : %f %f %f %f\n", m_fAmbient[0], m_fAmbient[1], m_fAmbient[2], m_fAmbient[3]);
		printf ("   diffuse : %f %f %f %f\n", m_fDiffuse[0], m_fDiffuse[1], m_fDiffuse[2], m_fDiffuse[3]);
		printf ("   specular : %f %f %f %f\n", m_fSpecular[0], m_fSpecular[1], m_fSpecular[2], m_fSpecular[3]);
		printf ("   emission : %f %f %f %f\n", m_fEmission[0], m_fEmission[1], m_fEmission[2], m_fEmission[3]);
		printf ("   shininess : %f\n", m_fShininess[0]);
	};
public:
	float m_fAmbient[4];
	float m_fDiffuse[4];
	float m_fSpecular[4];
	float m_fEmission[4];
	float m_fShininess[1];
};


//
//
//
class MaterialTexture : public Material
{
public:
	MaterialTexture (char const *filename, char const *path = NULL);
	MaterialTexture (unsigned int nWidth, unsigned int nHeight);
	MaterialTexture (const MaterialTexture &m); // constructor of copy
	virtual ~MaterialTexture ();

	MaterialType GetType (void);
	void Dump (void);
	char* GetFilename ();
	Img* GetImage ();
private:
	char *m_pFilename;
	Img *m_pImage;
	unsigned int m_nWidth, m_nHeight;
	unsigned char *m_pPixels;
};

#endif // __MATERIAL_H__
