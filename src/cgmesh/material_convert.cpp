#include "material_convert.h"

#include <cmath>

namespace {

// Reflectance normale d'un dielectrique non metallique. Valeur de Schlick
// retenue par la specification glTF 2.0 pour le coeur du modele.
const float kDielectricF0 = 0.04f;

// Marge sous laquelle un speculaire est tenu pour EGAL a F0.
//
// Le passage lineaire -> sRGB -> lineaire n'est exact qu'aux erreurs
// d'arrondi pres : un F0 exact ressort a quelques 1e-7 de 0,04, tantot au
// dessus tantot en dessous. Le test « ce speculaire est-il sous F0 ? » doit
// donc etre franc, sinon un dielectrique noir bascule en metal a cause d'un
// bit de mantisse.
const float kF0Margin = 1e-3f;

// Seuil sRGB de la specification, et son image exacte par la branche lineaire.
//
// La litterature ecrit d'ordinaire 0,0031308 pour le seuil inverse. Ce n'est PAS
// l'image exacte de 0,04045 par la division par 12,92 : utiliser la constante
// arrondie rendrait les deux courbes non inversibles au genou, avec une erreur
// de l'ordre de 2e-5. On derive donc le seuil au lieu de le recopier.
const double kSrgbKnee   = 0.04045;
const double kLinearKnee = kSrgbKnee / 12.92;

constexpr float Clamp01 (float v)
{
	return (v < 0.f) ? 0.f : ((v > 1.f) ? 1.f : v);
}

// Luminance perceptuelle Rec. 709, celle qui sert deja de reference au depot
// pour reduire une couleur a un scalaire.
float Luminance (float r, float g, float b)
{
	return 0.2126f * r + 0.7152f * g + 0.0722f * b;
}

float ToSrgb   (float v) { return (float) cgpbr::linearToSrgb ((double) v); }
float ToLinear (float v) { return (float) cgpbr::srgbToLinear ((double) v); }

// Les quatre canaux Phong d'un materiau du depot, quel que soit son type
// concret, EN sRGB -- l'espace des couleurs de materiau du depot. Un type
// inconnu rend le neutre : diffuse blanche, pas de speculaire.
struct PhongView
{
	float diffuse[4]  { 1.f, 1.f, 1.f, 1.f };
	float specular[4] { 0.f, 0.f, 0.f, 1.f };
	float emission[3] { 0.f, 0.f, 0.f };
	float shininess   { 0.f };
};

PhongView ReadPhong (const Material& src)
{
	PhongView v;

	if (const MaterialColorExt* ext = dynamic_cast<const MaterialColorExt*> (&src))
	{
		for (int i = 0; i < 4; ++i)
		{
			v.diffuse[i]  = ext->GetDiffuse()[i];
			v.specular[i] = ext->GetSpecular()[i];
		}
		for (int i = 0; i < 3; ++i)
			v.emission[i] = ext->GetEmission()[i];
		v.shininess = ext->GetShininess();

		// MaterialColorExt nait a zero sur les quatre canaux : un alpha nul n'y
		// est pas une intention, il rendrait le modele invisible. Meme garde que
		// les ecrivains MTL et GLB.
		if (v.diffuse[3] <= 0.f)
			v.diffuse[3] = 1.f;
	}
	else if (const MaterialTexture* tex = dynamic_cast<const MaterialTexture*> (&src))
	{
		for (int i = 0; i < 4; ++i)
		{
			v.diffuse[i]  = tex->GetDiffuse()[i];
			v.specular[i] = tex->GetSpecular()[i];
		}
		v.shininess = tex->GetShininess();
	}
	else if (const MaterialColor* col = dynamic_cast<const MaterialColor*> (&src))
	{
		v.diffuse[0] = col->GetFloatRed();
		v.diffuse[1] = col->GetFloatGreen();
		v.diffuse[2] = col->GetFloatBlue();
		v.diffuse[3] = col->GetFloatAlpha();
	}

	return v;
}

// Les canaux Phong d'un MaterialPbr, deja convertis en sRGB. Partage entre les
// deux types concrets que toPhong peut rendre, pour que la formule n'existe
// qu'une fois.
struct PhongProjection
{
	float ambient[4];
	float diffuse[4];
	float specular[4];
	float emission[4];
	float shininess;
};

PhongProjection Project (const cgpbr::Factors& f)
{
	const float metallic    = Clamp01 (f.metallic);
	const float roughness   = Clamp01 (f.roughness);
	const float oneMinusMet = 1.f - metallic;

	PhongProjection p {};
	for (int i = 0; i < 3; ++i)
	{
		// Le melange dielectrique/metal est une somme d'ENERGIES : il se calcule
		// en lineaire, et c'est son resultat qui se convertit, pas ses termes.
		p.ambient[i]  = ToSrgb (0.2f * f.baseColor[i]);
		p.diffuse[i]  = ToSrgb (f.baseColor[i] * oneMinusMet);
		p.specular[i] = ToSrgb (f.baseColor[i] * metallic + kDielectricF0 * oneMinusMet);
		p.emission[i] = ToSrgb (f.emissive[i]);
	}
	p.ambient[3]  = 1.f;
	p.diffuse[3]  = f.baseColor[3];   // l'alpha n'est pas un signal lumineux
	p.specular[3] = 1.f;
	p.emission[3] = 1.f;
	p.shininess   = (1.f - roughness) * (1.f - roughness);
	return p;
}

} // namespace

namespace cgpbr {

std::unique_ptr<Material> toPhong (const MaterialPbr& src)
{
	const PhongProjection p = Project (src.GetFactors());
	const TextureRef& base  = src.GetMap (MapSlot::base_color);

	if (base.image)
	{
		// L'image est PARTAGEE avec le materiau source : le constructeur prend
		// le shared_ptr, il ne recopie aucun pixel.
		auto out = std::make_unique<MaterialTexture> (base.name, base.image);
		out->SetName (src.GetName());
		out->SetAmbient  (p.ambient[0],  p.ambient[1],  p.ambient[2],  p.ambient[3]);
		out->SetDiffuse  (p.diffuse[0],  p.diffuse[1],  p.diffuse[2],  p.diffuse[3]);
		out->SetSpecular (p.specular[0], p.specular[1], p.specular[2], p.specular[3]);
		out->SetShininess (p.shininess);
		// MaterialTexture n'a pas de canal d'emission : p.emission est perdu ici.
		return out;
	}

	auto out = std::make_unique<MaterialColorExt> ();
	out->SetName (src.GetName());
	out->SetAmbient  (p.ambient[0],  p.ambient[1],  p.ambient[2],  p.ambient[3]);
	out->SetDiffuse  (p.diffuse[0],  p.diffuse[1],  p.diffuse[2],  p.diffuse[3]);
	out->SetSpecular (p.specular[0], p.specular[1], p.specular[2], p.specular[3]);
	out->SetEmission (p.emission[0], p.emission[1], p.emission[2], p.emission[3]);
	out->SetShininess (p.shininess);
	return out;
}

std::unique_ptr<MaterialPbr> fromPhong (const Material& src)
{
	// Un materiau deja PBR n'a rien a projeter : sans cette garde, aucune des
	// trois branches de ReadPhong ne le reconnait et il ressort neutre.
	if (const MaterialPbr* pbr = dynamic_cast<const MaterialPbr*> (&src))
		return std::make_unique<MaterialPbr> (*pbr);

	PhongView v = ReadPhong (src);

	// Les canaux lus sont en sRGB ; toute l'algebre qui suit est celle de
	// toPhong, donc lineaire.
	for (int i = 0; i < 3; ++i)
	{
		v.diffuse[i]  = ToLinear (v.diffuse[i]);
		v.specular[i] = ToLinear (v.specular[i]);
		v.emission[i] = ToLinear (v.emission[i]);
	}

	auto out = std::make_unique<MaterialPbr> ();
	out->SetName (src.GetName());
	Factors& f = out->EditFactors();

	// Facteur metallique par la luminance. En posant Ld et Ls les luminances de
	// la diffuse et du speculaire, les equations de toPhong donnent
	// Ls - F0 = metallic * (Lbase - F0) et Ld = Lbase * (1 - metallic), soit
	// Ld + Ls - F0 = Lbase - F0 * metallic. Le quotient ci-dessous vaut donc
	// exactement 0 pour un dielectrique (Ls = F0, Ld = Lbase) et exactement 1
	// pour un metal (Ls = Lbase, Ld = 0) ; entre les deux il approche.
	const float ld = Luminance (v.diffuse[0],  v.diffuse[1],  v.diffuse[2]);
	const float ls = Luminance (v.specular[0], v.specular[1], v.specular[2]);
	const float denom = ld + ls - kDielectricF0;

	// LE CAS SOMBRE. Quand la diffuse est nulle, le denominateur s'annule ou
	// devient negatif, et la formule ci-dessus ne dit plus rien. Deux materiaux
	// s'y confondent, que seul le NIVEAU du speculaire separe :
	//
	//   Ls >= F0 : une base sombre suffit a l'expliquer, le facteur metallique
	//              est indetermine et 0 redonne exactement le meme Phong ;
	//   Ls <  F0 : AUCUN dielectrique ne descend sous F0 -- son speculaire vaut
	//              F0 quelle que soit sa couleur. C'est donc un METAL plus
	//              sombre que F0, et le rendre dielectrique le noircirait
	//              definitivement (base = diffuse = 0).
	float metallic;
	if (denom > 1e-6f)
		metallic = Clamp01 ((ls - kDielectricF0) / denom);
	else if (ld < 1e-4f && ls < kDielectricF0 - kF0Margin)
		metallic = 1.f;
	else
		metallic = 0.f;
	f.metallic = metallic;

	// Couleur de base : diffuse = base * (1 - metallic) s'inverse EXACTEMENT
	// tant que le materiau n'est pas quasi metallique. Au voisinage de 1 la
	// division devient mal conditionnee (la diffuse y tend vers zero) et c'est
	// le speculaire qui porte la couleur.
	const float oneMinusMet = 1.f - metallic;
	for (int i = 0; i < 3; ++i)
	{
		const float base = (oneMinusMet > 1e-3f) ? (v.diffuse[i] / oneMinusMet)
		                                         : v.specular[i];
		f.baseColor[i] = Clamp01 (base);
	}
	f.baseColor[3] = Clamp01 (v.diffuse[3]);

	for (int i = 0; i < 3; ++i)
		f.emissive[i] = Clamp01 (v.emission[i]);

	// shininess = (1 - roughness)^2, donc roughness = 1 - sqrt(shininess).
	f.roughness = Clamp01 (1.f - std::sqrt (Clamp01 (v.shininess)));

	out->SetAlphaMode ((f.baseColor[3] < 1.f) ? AlphaMode::blend : AlphaMode::opaque);

	return out;
}

double srgbToLinear (double c)
{
	if (c <= 0.0)
		return 0.0;
	if (c >= 1.0)
		return 1.0;
	return (c <= kSrgbKnee) ? (c / 12.92) : std::pow ((c + 0.055) / 1.055, 2.4);
}

double linearToSrgb (double c)
{
	if (c <= 0.0)
		return 0.0;
	if (c >= 1.0)
		return 1.0;
	return (c <= kLinearKnee) ? (c * 12.92) : (1.055 * std::pow (c, 1.0 / 2.4) - 0.055);
}

} // namespace cgpbr
