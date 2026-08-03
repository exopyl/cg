#include "material.h"

#include <string>

// ---------------------------------------------------------------------------
//  Rattrapage des noms de texture TRONQUES par le format 3DS
// ---------------------------------------------------------------------------
// Le champ de nom de map d'un chunk MAT_MAPNAME est un nom DOS 8.3, soit
// 8 + 1 + 3 = 12 caracteres au plus. Un exportateur dont le nom de fichier
// depasse cette capacite le coupe a 12 caracteres, et c'est l'extension qui part
// en premier : « Bar_Chair_C.jpg » est ecrit « Bar_Chair_C. », point final sans
// extension.
//
// Consequence, avant ce rattrapage : ImgIO::load appelle
// std::filesystem::path::extension(), qui rend une chaine VIDE sur un nom
// terminant par un point. Aucune branche de format ne s'active, load renvoie -1
// sans jamais toucher au disque, et la texture est perdue alors que le fichier est
// juste a cote.
//
// Mecanisme retenu : une longueur de nom EXACTEMENT egale a la largeur du champ
// est le signal de la troncature -- le nom a rempli le champ jusqu'au bord. Dans
// ce cas seulement, on COMPLETE le nom par ce qui manque de ".jpg", selon ce que
// la coupure a laisse subsister :
//
//     "abcdefghijkl"  (rien)  -> + ".jpg"  -> "abcdefghijkl.jpg"
//     "abcdefghijk."  (point) -> + "jpg"   -> "abcdefghijk.jpg"
//     "abcdefghij.j"  (".j")  -> + "pg"    -> "abcdefghij.jpg"
//     "abcdefghi.jp"  (".jp") -> + "g"     -> "abcdefghi.jpg"
//
// jpg est le format d'image de loin le plus courant dans les jeux de donnees 3DS,
// et le seul qu'on puisse deviner sans lister le repertoire.
//
// Trois garde-fous. Le rattrapage n'est tente qu'APRES l'echec du chargement
// nominal, donc il ne peut pas detourner un nom qui fonctionne. Une extension
// deja complete (trois caracteres apres le point) exclut la troncature : l'echec
// vient alors d'un fichier reellement absent, qu'il ne faut pas renommer. Enfin ce
// qui subsiste de l'extension doit etre un PREFIXE de "jpg" : sans cela,
// "abcdefghij.x" donnerait ".xpg", un nom qui ne peut designer aucun fichier.
//
// Ce code vit dans MaterialTexture, partage avec l'import OBJ. Un nom d'OBJ de 12
// caracteres a extension incomplete subirait le meme traitement -- sans dommage,
// puisqu'il a de toute facon deja echoue a charger.
static const size_t k3dsMapNameWidth = 8 + 1 + 3;   // nom DOS 8.3

static std::string CompleteTruncated3dsMapName (const std::string& fn)
{
	if (fn.size() != k3dsMapNameWidth)
		return fn;                                  // pas a la largeur du champ

	const size_t dot = fn.find_last_of('.');
	if (dot == std::string::npos)
		return fn + ".jpg";                         // le point lui-meme a ete coupe

	const std::string ext = fn.substr(dot + 1);     // ce qui subsiste apres le point
	if (ext.size() >= 3)
		return fn;                                  // extension complete : pas une troncature

	const std::string jpg = "jpg";
	if (jpg.compare(0, ext.size(), ext) != 0)
		return fn;                                  // pas un ".jpg" coupe

	return fn + jpg.substr(ext.size());             // completer par ce qui manque
}

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
	0.0215f, 0.1745f, 0.0215f, 0.07568f, 0.61424f, 0.07568f, 0.633f, 0.727811f, 0.633f, 0.6f, // emerald
	0.135f, 0.2225f, 0.1575f, 0.54f, 0.89f, 0.63f, 0.316228f, 0.316228f, 0.316228f, 0.1f, // jade
	0.05375f, 0.05f, 0.06625f, 0.18275f, 0.17f, 0.22525f, 0.332741f, 0.328634f, 0.346435f, 0.3f, // obsidian
	0.25f, 0.20725f, 0.20725f, 1, 0.829f, 0.829f, 0.296648f, 0.296648f, 0.296648f, 0.088f, // pearl
	0.1745f, 0.01175f, 0.01175f, 0.61424f, 0.04136f, 0.04136f, 0.727811f, 0.626959f, 0.626959f, 0.6f, // ruby
	0.1f, 0.18725f, 0.1745f, 0.396f, 0.74151f, 0.69102f, 0.297254f, 0.30829f, 0.306678f, 0.1f,  // turquoise
	0.329412f, 0.223529f, 0.027451f, 0.780392f, 0.568627f, 0.113725f, 0.992157f, 0.941176f, 0.807843f, 0.21794872f,  // brass
	0.2125f, 0.1275f, 0.054f, 0.714f, 0.4284f, 0.18144f, 0.393548f, 0.271906f, 0.166721f, 0.2f,  // bronze
	0.25f, 0.25f, 0.25f, 0.4f, 0.4f, 0.4f, 0.774597f, 0.774597f, 0.774597f, 0.6f,  // chrome
	0.19125f, 0.0735f, 0.0225f, 0.7038f, 0.27048f, 0.0828f, 0.256777f, 0.137622f, 0.086014f, 0.1f,  // copper
	0.24725f, 0.1995f, 0.0745f, 0.75164f, 0.60648f, 0.22648f, 0.628281f, 0.555802f, 0.366065f, 0.4f,  // gold
	0.19225f, 0.19225f, 0.19225f, 0.50754f, 0.50754f, 0.50754f, 0.508273f, 0.508273f, 0.508273f, 0.4f,  // silver
	0.0f, 0.0f, 0.0f, 0.01f, 0.01f, 0.01f, 0.50f, 0.50f, 0.50f, .25f,  // black, plastic
	0.0f, 0.1f, 0.06f, 0.0f, 0.50980392f, 0.50980392f, 0.50196078f, 0.50196078f, 0.50196078f, .25f,  // cyan, plastic
	0.0f, 0.0f, 0.0f, 0.1f, 0.35f, 0.1f, 0.45f, 0.55f, 0.45f, .25f,  // green plastic
	0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f, 0.7f, 0.6f, 0.6f, .25f,  // red plastic
	0.0f, 0.0f, 0.0f, 0.55f, 0.55f, 0.55f, 0.70f, 0.70f, 0.70f, .25f,  // white, plastic
	0.0f, 0.0f, 0.0f, 0.5f, 0.5f, 0.0f, 0.60f, 0.60f, 0.50f, .25f,  // yellow, plastic
	0.02f, 0.02f, 0.02f, 0.01f, 0.01f, 0.01f, 0.4f, 0.4f, 0.4f, .078125f,  // black, rubber
	0.0f, 0.05f, 0.05f, 0.4f, 0.5f, 0.5f, 0.04f, 0.7f, 0.7f, .078125f,  // cyan, rubber
	0.0f, 0.05f, 0.0f, 0.4f, 0.5f, 0.4f, 0.04f, 0.7f, 0.04f, .078125f,  // green, rubber
	0.05f, 0.0f, 0.0f, 0.5f, 0.4f, 0.4f, 0.7f, 0.04f, 0.04f, .078125f,  // red, rubber
	0.05f, 0.05f, 0.05f, 0.5f, 0.5f, 0.5f, 0.7f, 0.7f, 0.7f, .078125f,  // white, rubber,
	0.05f, 0.05f, 0.0f, 0.5f, 0.5f, 0.4f, 0.7f, 0.7f, 0.04f, .078125f // yellow, rubber
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
// Charge une image de texture, avec le rattrapage de troncature 8.3. Rend
// l'image partagee, ou nullptr ; `name` recoit le nom qui a effectivement charge.
// Factorise entre la texture diffuse et la carte de reflexion : les deux viennent
// des memes fichiers, donc subissent la meme troncature.
static std::shared_ptr<Img> LoadTextureImage (char const *filename, char const *path,
                                              std::string &name)
{
	name = std::string (filename ? filename : "");
	if (name.empty())
		return nullptr;

	std::shared_ptr<Img> img = std::make_shared<Img> ();
	if (img->load(name.c_str(), path) == 0)
		return img;

	// Second essai : le nom est peut-etre tronque a la largeur du champ 8.3
	// d'un MAT_MAPNAME 3DS (cf. l'en-tete de ce fichier).
	const std::string alt = CompleteTruncated3dsMapName (name);
	if (alt != name && img->load(alt.c_str(), path) == 0)
	{
		name = alt;             // rapporter le nom qui a effectivement charge
		return img;
	}
	return nullptr;             // introuvable ou format inconnu
}

MaterialTexture::MaterialTexture (char const *filename, char const *path)
{
	m_pImage = LoadTextureImage (filename, path, m_filename);
}

bool MaterialTexture::SetReflectionMap (char const *filename, char const *path)
{
	std::string name;
	std::shared_ptr<Img> img = LoadTextureImage (filename, path, name);
	if (!img)
		return false;           // laisser le materiau intact

	m_reflFilename = name;
	m_pReflImage   = img;
	return true;
}

MaterialTexture::MaterialTexture (const std::string &name, unsigned int width, unsigned int height, const unsigned char *rgbaPixels)
{
	m_filename = name;
	if (!rgbaPixels || width == 0 || height == 0)
		return;

	m_pImage = std::make_shared<Img>(width, height, false);
	if (!m_pImage || !m_pImage->data())
	{
		m_pImage.reset();
		return;
	}

	memcpy(m_pImage->data(), rgbaPixels, 4 * width * height * sizeof(unsigned char));
}

MaterialTexture::MaterialTexture (unsigned int nWidth, unsigned int nHeight)
{
}

// Copy constructor: SHARE the decoded image (ref-counted) rather than duplicate
// its pixels, and carry over the filename, modulation colours and name (the
// previous empty body silently produced a blank, unnamed, image-less material).
MaterialTexture::MaterialTexture (const MaterialTexture &m)
{
	m_name     = m.m_name;
	m_filename = m.m_filename;
	m_pImage   = m.m_pImage;   // shared_ptr: +1 ref, no pixel copy
	m_nWidth   = m.m_nWidth;
	m_nHeight  = m.m_nHeight;
	// La carte de reflexion suit la meme regle : partagee, pas dupliquee.
	m_reflFilename = m.m_reflFilename;
	m_pReflImage   = m.m_pReflImage;
	memcpy(m_fAmbient,  m.m_fAmbient,  4 * sizeof(float));
	memcpy(m_fDiffuse,  m.m_fDiffuse,  4 * sizeof(float));
	memcpy(m_fSpecular, m.m_fSpecular, 4 * sizeof(float));
	m_fShininess = m.m_fShininess;
}

MaterialTexture::~MaterialTexture ()
{
	// m_pImage is a shared_ptr: the image is released automatically when the
	// last MaterialTexture referencing it is destroyed.
}

MaterialType MaterialTexture::GetType (void)
{
	return MATERIAL_TEXTURE;
}

void MaterialTexture::Dump (void)
{
	printf ("MATERIAL_TEXTURE : %s\n", m_filename.c_str());
}

std::string MaterialTexture::GetFilename ()
{
	return m_filename;
}

Img* MaterialTexture::GetImage ()
{
	return m_pImage.get();
}

