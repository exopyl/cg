#pragma once

#include <memory>

#include "../cgimg/cgimg.h"

// material definition
enum MaterialType {
	MATERIAL_NONE = ((unsigned int)-1),
	MATERIAL_COLOR = 0,
	MATERIAL_COLOR_ADV = 1,
	MATERIAL_TEXTURE = 2
};

//
// MaterialPtr -- un pointeur possedant qui se COPIE en clonant.
//
// Confiner la copie polymorphe dans un type valeur est ce qui permet aux classes
// qui detiennent des materiaux d'etre copiables sans constructeur de copie ecrit
// a la main. Meme surface que unique_ptr<Material> : get, operator->,
// operator bool, reset.
//
class Material;

class MaterialPtr
{
public:
	MaterialPtr () = default;
	explicit MaterialPtr (Material *p) : m_p (p) {}

	MaterialPtr (const MaterialPtr &o);
	MaterialPtr& operator= (const MaterialPtr &o);
	MaterialPtr (MaterialPtr &&) = default;
	MaterialPtr& operator= (MaterialPtr &&) = default;

	Material* get (void) const { return m_p.get (); }
	Material* operator-> (void) const { return m_p.get (); }
	Material& operator* (void) const { return *m_p; }
	explicit operator bool (void) const { return (bool)m_p; }
	void reset (Material *p = nullptr) { m_p.reset (p); }

private:
	std::unique_ptr<Material> m_p;
};

//
//
//
class Material
{
public:
	Material () = default;
	virtual ~Material () = default; // required: subclasses (MaterialTexture) hold resources

	// ⚠ NE PAS ECRIRE DE CONSTRUCTEUR DE COPIE ICI ni dans les sous-classes : les
	// implicites sont corrects et complets. Ceux qui s'y trouvaient oubliaient
	// m_name, chacun a sa maniere.

	// Copie POLYMORPHE, sans laquelle une copie par valeur trancherait les
	// sous-objets.
	//
	// ⚠ L'IMAGE d'une MaterialTexture est PARTAGEE, pas dupliquee (cf. le
	// shared_ptr<Img>) : elle est traitee comme immuable. Tout l'etat MUTABLE, lui,
	// est bien duplique.
	virtual std::unique_ptr<Material> clone (void) const
		{ return std::make_unique<Material> (*this); }

	virtual MaterialType GetType (void) { return MATERIAL_NONE; };
	virtual void Dump (void) {};
	
	void SetName (const std::string & name) { m_name = name; };
	std::string GetName (void) { return m_name; };

protected:
	std::string m_name;
};

// Apres Material : clone () doit etre connue.
inline MaterialPtr::MaterialPtr (const MaterialPtr &o)
	: m_p (o.m_p ? o.m_p->clone () : nullptr) {}

inline MaterialPtr& MaterialPtr::operator= (const MaterialPtr &o)
{
	if (this != &o)
		m_p = o.m_p ? o.m_p->clone () : nullptr;
	return *this;
}

//
//
//
class MaterialColor : public Material
{
public:
	MaterialColor ();
	MaterialColor (unsigned char r, unsigned char g, unsigned char b, unsigned char a=255);

	//virtual ~MaterialColor () {};
	std::unique_ptr<Material> clone (void) const override
		{ return std::make_unique<MaterialColor> (*this); }
	MaterialType GetType (void);

	float GetFloatRed() const { return m_r / 255.f; };
	float GetFloatGreen() const { return m_g / 255.f; };
	float GetFloatBlue() const { return m_b / 255.f; };
	float GetFloatAlpha() const { return m_a / 255.f; };

	void Dump (void);
private:
	unsigned char m_r, m_g, m_b, m_a;
};

//
// Material color extended
//
class MaterialColorExt : public Material
{

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
	//virtual ~MaterialColorExt () {};
	std::unique_ptr<Material> clone (void) const override
		{ return std::make_unique<MaterialColorExt> (*this); }

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
	MaterialTexture (char const *filename, char const *path = nullptr);
	MaterialTexture (const std::string &name, unsigned int width, unsigned int height, const unsigned char *rgbaPixels);
	MaterialTexture (unsigned int nWidth, unsigned int nHeight);
	MaterialTexture (const MaterialTexture &m); // constructor of copy
	virtual ~MaterialTexture ();

	std::unique_ptr<Material> clone (void) const override
		{ return std::make_unique<MaterialTexture> (*this); }
	MaterialType GetType (void);
	void Dump (void);
	std::string GetFilename ();
	Img* GetImage ();

	// Carte de REFLEXION, facultative, en plus de la texture diffuse.
	//
	// Un materiau peut la declarer par `MAT_REFLMAP` en 3DS ou `refl` en MTL. Le
	// rendu l'applique en sphere mapping : les coordonnees sont GENEREES depuis la
	// normale en espace oeil (GL_SPHERE_MAP), la carte n'a donc pas besoin d'UV et
	// suit le point de vue, ce qui est le propre d'un reflet.
	//
	// Renvoie true si l'image a pu etre decodee. Un echec laisse le materiau
	// inchange : une reflexion introuvable ne doit pas invalider sa diffuse.
	bool SetReflectionMap (char const *filename, char const *path = nullptr);
	std::string GetReflectionFilename () const { return m_reflFilename; }
	Img* GetReflectionImage () const { return m_pReflImage.get(); }
	bool HasReflectionMap () const { return m_pReflImage != nullptr; }

	// Optional material colours that MODULATE the texture under lighting. 3DS
	// textured materials carry a diffuse tint (e.g. a light rubber tread
	// texture darkened by a grey diffuse). Defaults are white/neutral so a
	// plain texture renders unchanged when no colours are set.
	inline void SetAmbient  (float r, float g, float b, float a) { m_fAmbient[0]=r; m_fAmbient[1]=g; m_fAmbient[2]=b; m_fAmbient[3]=a; }
	inline void SetDiffuse  (float r, float g, float b, float a) { m_fDiffuse[0]=r; m_fDiffuse[1]=g; m_fDiffuse[2]=b; m_fDiffuse[3]=a; }
	inline void SetSpecular (float r, float g, float b, float a) { m_fSpecular[0]=r; m_fSpecular[1]=g; m_fSpecular[2]=b; m_fSpecular[3]=a; }
	inline void SetShininess(float p) { m_fShininess = p; }
	inline const float* GetAmbient  () const { return m_fAmbient; }
	inline const float* GetDiffuse  () const { return m_fDiffuse; }
	inline const float* GetSpecular () const { return m_fSpecular; }
	inline float        GetShininess() const { return m_fShininess; }
private:
	std::string m_filename;
	// Reference-counted so several MaterialTexture instances (e.g. the per-object
	// submeshes produced when splitting a multi-object OBJ) can share a single
	// decoded image instead of each owning a duplicate. The image is freed when
	// the last referencing material dies; the owning Mesh stays self-contained.
	std::shared_ptr<Img> m_pImage;
	unsigned int m_nWidth = 0, m_nHeight = 0;
	unsigned char *m_pPixels = nullptr;

	// Carte de reflexion : meme partage par comptage de references que la diffuse,
	// plusieurs materiaux d'un meme fichier la designant en general.
	std::string m_reflFilename;
	std::shared_ptr<Img> m_pReflImage;

	float m_fAmbient[4]  = { 1.f, 1.f, 1.f, 1.f };
	float m_fDiffuse[4]  = { 1.f, 1.f, 1.f, 1.f };
	float m_fSpecular[4] = { 0.f, 0.f, 0.f, 1.f };
	float m_fShininess   = 0.f;
};
